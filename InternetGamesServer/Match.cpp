#include "Match.hpp"

#include <rpcdce.h>

#include "PlayerSocket.hpp"

Match::Game
Match::GameFromString(const std::string& str)
{
	if (str == "wnbk")
		return Game::BACKGAMMON;
	else if (str == "wnck")
		return Game::CHECKERS;
	else if (str == "wnsp")
		return Game::SPADES;

	return Game::INVALID;
}


Match::Match(PlayerSocket& player) :
	m_state(STATE_WAITINGFORPLAYERS),
	m_guid(),
	m_players()
{
	// Generate a unique GUID for the match
	UuidCreate(&m_guid);

	JoinPlayer(player);
}


void
Match::JoinPlayer(PlayerSocket& player)
{
	if (m_state != STATE_WAITINGFORPLAYERS)
		return;

	// Add to players array
	m_players.push_back(&player);

	// Start the game, if enough players have joined
	if (m_players.size() == GetRequiredPlayerCount())
	{
		m_state = STATE_PLAYING;
		for (PlayerSocket* p : m_players)
			p->OnGameStart();
	}
}

void
Match::DisconnectedPlayer(PlayerSocket& player)
{
	// Remove from players array
	m_players.erase(std::remove(m_players.begin(), m_players.end(), &player), m_players.end());

	// TODO: Indicate that to remaining players in some way?
}
