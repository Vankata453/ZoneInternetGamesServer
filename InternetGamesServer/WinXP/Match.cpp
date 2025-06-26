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
	m_endTime(0)
{
	JoinPlayer(player);
}

Match::~Match()
{
	// Match has ended, so disconnect any remaining players
	for (PlayerSocket* p : m_players)
		p->Disconnect();

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

	// End the match on no players, marking it as to-be-removed from MatchManager
	if (m_players.empty())
	{
		m_state = STATE_ENDED;
		return;
	}

	// If the game has already ended, notify all players that someone has left the match and "Play Again" is now impossible
	if (m_state == STATE_GAMEOVER && m_players.size() == GetRequiredPlayerCount() - 1)
	{
		for (PlayerSocket* p : m_players)
			p->OnMatchServiceInfo(MsgProxyServiceInfo::SERVICE_DISCONNECT);

		m_state = STATE_ENDED;
		return;
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
		// Disconnect any remaining players
		for (PlayerSocket* p : m_players)
			p->Disconnect();

		m_state = STATE_ENDED;
	}
#endif
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


bool
Match::ValidateCommonChatMessage(const std::wstring& chatMsg)
{
	return chatMsg == L"/1 Nice try" || chatMsg == L"/2 Good job" ||
		chatMsg == L"/3 Good game" || chatMsg == L"/4 Good luck!" ||
		chatMsg == L"/5 It's your turn" || chatMsg == L"/6 I'm thinking..." ||
		chatMsg == L"/7 Play again?" || chatMsg == L"/8 Yes" ||
		chatMsg == L"/9 No" || chatMsg == L"/10 Hello" ||
		chatMsg == L"/11 Goodbye" || chatMsg == L"/12 Thank you" ||
		chatMsg == L"/13 You're welcome" || chatMsg == L"/14 It was luck" ||
		chatMsg == L"/15 Be right back..." || chatMsg == L"/16 Okay, I'm back" ||
		chatMsg == L"/17 Are you still there?" || chatMsg == L"/18 Sorry, I have to go now" ||
		chatMsg == L"/19 I'm going to play at zone.com" || chatMsg == L"/20 :-)" ||
		chatMsg == L"/21 :-(" || chatMsg == L"/22 Uh oh..." ||
		chatMsg == L"/23 Oops!" || chatMsg == L"/24 Ouch!";
}

}
