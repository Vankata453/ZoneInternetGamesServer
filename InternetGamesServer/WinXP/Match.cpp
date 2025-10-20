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
	m_processMessageMutex(CreateMutex(nullptr, false, nullptr)),
	m_broadcastMutex(CreateMutex(nullptr, false, nullptr)),
	m_endTime(0)
{
	JoinPlayer(player);
}

Match::~Match()
{
	// Match has ended, so disconnect any remaining players
	for (PlayerSocket* p : m_players)
		p->OnMatchDisconnect();

	CloseHandle(m_broadcastMutex);
	CloseHandle(m_processMessageMutex);
}


void
Match::JoinPlayer(PlayerSocket& player)
{
	if (m_state != STATE_WAITINGFORPLAYERS)
		return;

	AddPlayer(player);

	MsgServerStatus msgServerStatus;
	msgServerStatus.playersWaiting = static_cast<int>(m_players.size());
	for (PlayerSocket* p : m_players)
		p->OnMatchGenericMessage<MessageServerStatus>(msgServerStatus);

	// Switch state, if enough players have joined
	if (m_players.size() == GetRequiredPlayerCount())
		m_state = STATE_PENDINGSTART;
}

void
Match::DisconnectedPlayer(PlayerSocket& player)
{
	RemovePlayer(player);

	// End the match on no players, marking it as to-be-removed from MatchManager
	if (m_players.empty())
	{
		m_state = STATE_ENDED;
		return;
	}

	if (m_state == STATE_WAITINGFORPLAYERS)
	{
		MsgServerStatus msgServerStatus;
		msgServerStatus.playersWaiting = static_cast<int>(m_players.size());
		for (PlayerSocket* p : m_players)
			p->OnMatchGenericMessage<MessageServerStatus>(msgServerStatus);
		return;
	}

#if MATCH_NO_DISCONNECT_ON_PLAYER_LEAVE
	if (
#else
	if (m_state == STATE_PLAYING ||
#endif
		// If the game has already ended, notify all players that someone has left the match and "Play Again" is now impossible
		(m_state == STATE_GAMEOVER && m_players.size() == GetRequiredPlayerCount() - 1))
	{
		std::cout << "[MATCH] " << m_guid << ": A player left, closing match!" << std::endl;

		for (PlayerSocket* p : m_players)
			p->OnMatchDisconnect();
		m_players.clear();

		m_state = STATE_ENDED;
	}
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

void
Match::ProcessIncomingGameMessage(PlayerSocket& player, uint32 type)
{
	if (WaitForSingleObject(m_processMessageMutex, 5000) == WAIT_ABANDONED) // Acquired ownership of an abandoned process message mutex
		throw std::runtime_error("WinXP::Match::ProcessIncomingGameMessage(): Got ownership of an abandoned process message mutex: " + std::to_string(GetLastError()));

	try
	{
		ProcessIncomingGameMessageImpl(player, type);
	}
	catch (...)
	{
		ReleaseMutex(m_processMessageMutex);
		throw;
	}

	if (!ReleaseMutex(m_processMessageMutex))
		throw std::runtime_error("WinXP::Match::ProcessIncomingGameMessage(): Couldn't release process message mutex: " + std::to_string(GetLastError()));
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
