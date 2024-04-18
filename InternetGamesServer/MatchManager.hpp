#pragma once

#include <memory>
#include <vector>

#include "Match.hpp"
#include "PlayerSocket.hpp"

class BackgammonMatch;
class CheckersMatch;
class SpadesMatch;

class MatchManager final
{
private:
	static MatchManager s_instance;

public:
	inline static MatchManager& Get() { return s_instance; }
	static DWORD WINAPI UpdateHandler(void*);

public:
	MatchManager();

	/** Update the logic of all matches */
	void Update();

	/** Find a lobby (pending match) to join a player in, based on their game.
		If none is available, create one. */
	Match* FindLobby(PlayerSocket& player, Match::Game game);

private:
	Match* CreateLobby(PlayerSocket& player, Match::Game game);

private:
	std::vector<std::unique_ptr<BackgammonMatch>> m_backgammonMatches;
	std::vector<std::unique_ptr<CheckersMatch>> m_checkersMatches;
	std::vector<std::unique_ptr<SpadesMatch>> m_spadesMatches;
};
