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
	m_skillLevel(player.GetSkillLevel())
{
	JoinPlayer(player);
}

Match::~Match()
{
	for (PlayerSocket* p : m_players)
		p->OnMatchEnded();
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
				const std::vector<int> ids = GenerateUniqueRandomNums(2, playerCount + 1);
				for (int i = 0; i < playerCount; i++)
					const_cast<int&>(m_players[i]->m_ID) = ids[i];

				for (PlayerSocket* p : m_players)
					p->OnGameStart(m_players);

				std::cout << "[MATCH] " << m_guid << ": Started match!" << std::endl;
				m_state = STATE_PLAYING;
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
Match::ValidateCommonChatMessage(const WCHAR* chatMsg)
{
	return WCHARSTR_EQUAL(chatMsg, L"/1 Nice try\0") || WCHARSTR_EQUAL(chatMsg, L"/2 Good job\0") ||
		WCHARSTR_EQUAL(chatMsg, L"/3 Good game\0") || WCHARSTR_EQUAL(chatMsg, L"/4 Good luck!\0") ||
		WCHARSTR_EQUAL(chatMsg, L"/5 It's your turn\0") || WCHARSTR_EQUAL(chatMsg, L"/6 I'm thinking...\0") ||
		WCHARSTR_EQUAL(chatMsg, L"/7 Play again?\0") || WCHARSTR_EQUAL(chatMsg, L"/8 Yes\0") ||
		WCHARSTR_EQUAL(chatMsg, L"/9 No\0") || WCHARSTR_EQUAL(chatMsg, L"/10 Hello\0") ||
		WCHARSTR_EQUAL(chatMsg, L"/11 Goodbye\0") || WCHARSTR_EQUAL(chatMsg, L"/12 Thank you\0") ||
		WCHARSTR_EQUAL(chatMsg, L"/13 You're welcome\0") || WCHARSTR_EQUAL(chatMsg, L"/14 It was luck\0") ||
		WCHARSTR_EQUAL(chatMsg, L"/15 Be right back...\0") || WCHARSTR_EQUAL(chatMsg, L"/16 Okay, I'm back\0") ||
		WCHARSTR_EQUAL(chatMsg, L"/17 Are you still there?\0") || WCHARSTR_EQUAL(chatMsg, L"/18 Sorry, I have to go now\0") ||
		WCHARSTR_EQUAL(chatMsg, L"/19 I'm going to play at zone.com\0") || WCHARSTR_EQUAL(chatMsg, L"/20 :-)\0") ||
		WCHARSTR_EQUAL(chatMsg, L"/21 :-(\0") || WCHARSTR_EQUAL(chatMsg, L"/22 Uh oh...\0") ||
		WCHARSTR_EQUAL(chatMsg, L"/23 Oops!\0") || WCHARSTR_EQUAL(chatMsg, L"/24 Ouch!\0");
}

}
