#include "Match.hpp"

#include <algorithm>
#include <iostream>

#include "PlayerSocket.hpp"

namespace WinXP {

Match::Game
Match::GameFromString(const std::string& str)
{
	if (str == "BCKGZM")
		return Game::BACKGAMMON;
	else if (str == "CHKRZM")
		return Game::CHECKERS;
	else if (str == "SHVLZM")
		return Game::SPADES;
	else if (str == "HRTZZM")
		return Game::HEARTS;
	else if (str == "RVSEZM")
		return Game::REVERSI;

	return Game::INVALID;
}

std::string
Match::GameToNameString(Match::Game game)
{
	switch (game)
	{
		case Game::BACKGAMMON:
			return "Backgammon";
		case Game::CHECKERS:
			return "Checkers";
		case Game::SPADES:
			return "Spades";
		case Game::HEARTS:
			return "Hearts";
		case Game::REVERSI:
			return "Reversi";
		default:
			return "Invalid";
	}
}


#define MATCH_NO_DISCONNECT_ON_PLAYER_LEAVE 0 // DEBUG: If a player leaves a match, do not disconnect other players.

Match::Match(PlayerSocket& player) :
	::Match<PlayerSocket>(player),
	m_state(STATE_WAITINGFORPLAYERS),
	m_skillLevel(player.GetSkillLevel()),
	m_broadcastMutex(CreateMutex(nullptr, false, nullptr)),
	m_disconnectMutex(CreateMutex(nullptr, false, nullptr)),
	m_endTime(0)
{
	JoinPlayer(player);
}

Match::~Match()
{
	// Match has ended, so disconnect any remaining players
	for (PlayerSocket* p : m_players)
		p->Disconnect();

	CloseHandle(m_disconnectMutex);
	CloseHandle(m_broadcastMutex);
}


void
Match::JoinPlayer(PlayerSocket& player)
{
	if (m_state != STATE_WAITINGFORPLAYERS)
		return;

	AddPlayer(player);

	// Switch state, if enough players have joined
	if (m_players.size() == GetRequiredPlayerCount())
		m_state = STATE_PENDINGSTART;
}

void
Match::DisconnectedPlayer(PlayerSocket& player)
{
	RemovePlayer(player);

	if (WaitForSingleObject(m_disconnectMutex, 5000) == WAIT_ABANDONED) // Acquired ownership of an abandoned disconnect mutex
		throw std::runtime_error("WinXP::Match::DisconnectedPlayer(): Got ownership of an abandoned disconnect mutex: " + std::to_string(GetLastError()));

	try
	{
		// End the match on no players, marking it as to-be-removed from MatchManager
		if (m_players.empty())
		{
			m_state = STATE_ENDED;
			goto disconnectEnd;
		}

		// If the game has already ended, notify all players that someone has left the match and "Play Again" is now impossible
		if (m_state == STATE_GAMEOVER && m_players.size() == GetRequiredPlayerCount() - 1)
		{
			m_state = STATE_ENDED;

			for (PlayerSocket* p : m_players)
				p->OnMatchServiceInfo(MsgProxyServiceInfo::SERVICE_DISCONNECT);

			goto disconnectEnd;
		}

#if not MATCH_NO_DISCONNECT_ON_PLAYER_LEAVE
		// Originally, servers replaced players who have left the game with AI.
		// However, since there is no logic support for any of the games on this server currently,
		// if currently playing a game, we end the game directly by disconnecting everyone.
		// NOTE: The server doesn't know when a game has finished with a win, so this has the drawback of causing
		//       an "Internet connection to the game server was broken" message after a game has finished with a win
		//       (even though since the game has ended anyway, it's not really important).
		if (m_state == STATE_PLAYING)
		{
			m_state = STATE_ENDED;

			// Disconnect any remaining players
			for (PlayerSocket* p : m_players)
				p->Disconnect();
		}
#endif
	}
	catch (...)
	{
		ReleaseMutex(m_disconnectMutex);
		throw;
	}

disconnectEnd:
	if (!ReleaseMutex(m_disconnectMutex))
		throw std::runtime_error("WinXP::Match::DisconnectedPlayer(): Couldn't release disconnect mutex: " + std::to_string(GetLastError()));
}


void
Match::Update()
{
	switch (m_state)
	{
		case STATE_PENDINGSTART:
		{
			// Start the game, if all players are waiting for opponents
			if (std::all_of(m_players.begin(), m_players.end(),
				[](const auto& player) { return player->GetState() == PlayerSocket::STATE_WAITINGFOROPPONENTS; }))
			{
				// Distribute unique IDs for each player, starting from 0
				const int playerCount = static_cast<int>(m_players.size());
				const std::vector<int> seats = GenerateUniqueRandomNums(0, playerCount - 1);
				for (int i = 0; i < playerCount; i++)
					const_cast<int16&>(m_players[i]->m_seat) = seats[i];

				for (PlayerSocket* p : m_players)
					p->OnGameStart(m_players);

				std::cout << "[MATCH] " << m_guid << ": Started match!" << std::endl;
				m_state = STATE_PLAYING;
			}
			break;
		}
		case STATE_PLAYING:
		{
			if (m_endTime != 0)
			{
				assert(m_players.size() == GetRequiredPlayerCount());

				std::cout << "[MATCH] " << m_guid << ": Playing state restored, cancelling game over close timer." << std::endl;
				m_endTime = 0;
			}
			break;
		}
		case STATE_GAMEOVER:
		{
			if (m_endTime == 0)
			{
				std::cout << "[MATCH] " << m_guid << ": Game over, match will automatically close in 60 seconds!" << std::endl;
				m_endTime = std::time(nullptr);
			}
			else if (std::time(nullptr) - m_endTime >= 60) // A minute has passed since the match ended
			{
				std::cout << "[MATCH] " << m_guid << ": Match ended a minute ago, closing!" << std::endl;
				m_state = STATE_ENDED;
			}
			break;
		}

		default:
			break;
	}
}


void
Match::ProcessMessage(const MsgChatSwitch& msg)
{
	BroadcastGenericMessage<MessageChatSwitch>(msg);
}


uint8_t // Returns message ID, 0 on failure
Match::ValidateChatMessage(const std::wstring& chatMsg, uint8_t customRangeStart, uint8_t customRangeEnd)
{
	if (chatMsg.empty() || chatMsg[0] != L'/')
		return 0;

	try
	{
		size_t lastIDPos;
		const int msgID = std::stoi(chatMsg.substr(1), &lastIDPos);

		if (lastIDPos + 1 < chatMsg.size() && !std::isspace(chatMsg[lastIDPos + 1]))
			return 0;

		if ((msgID >= 1 && msgID <= 24) || // Common messages
			(msgID >= customRangeStart && msgID <= customRangeEnd)) // Custom (per-game) messages
			return msgID;
	}
	catch (const std::exception&)
	{
		return 0;
	}

	return 0;
}

}
