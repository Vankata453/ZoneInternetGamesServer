#pragma once

#include <memory>
#include <vector>

#include "Match.hpp"
#include "PlayerSocket.hpp"

class MatchManager final
{
private:
	static MatchManager s_instance;

public:
	inline static MatchManager& Get() { return s_instance; }

public:
	MatchManager();

	/** Find a lobby (pending match) to join a player in, based on their game.
		If none is available, create one. */
	Match* FindLobby(PlayerSocket& player, Match::Game game);

private:
	Match* CreateLobby(PlayerSocket& player, Match::Game game);

private:
	// TODO: Derived classes for each match type
	std::vector<std::unique_ptr<Match>> m_backgammonMatches;
	std::vector<std::unique_ptr<Match>> m_checkersMatches;
	std::vector<std::unique_ptr<Match>> m_spadesMatches;
};
