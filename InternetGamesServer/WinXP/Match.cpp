#include "Match.hpp"

#include "PlayerSocket.hpp"

namespace WinXP {

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
		default:
			return "Invalid";
	}
}


#define MATCH_NO_DISCONNECT_ON_PLAYER_LEAVE 0 // DEBUG: If a player leaves a match, do not disconnect other players.

Match::Match(PlayerSocket& player) :
	::Match<PlayerSocket>(player)
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
	AddPlayer(player);
}

void
Match::DisconnectedPlayer(PlayerSocket& player)
{
	RemovePlayer(player);
}


void
Match::Update()
{

}

}
