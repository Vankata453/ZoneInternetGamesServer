#pragma once

#include <memory>
#include <vector>

#include "PlayerSocket.hpp"

class Match;

class MatchManager final
{
private:
	static MatchManager s_instance;

public:
	inline static MatchManager& Get() { return s_instance; }
	static DWORD WINAPI UpdateHandler(void*);

public:
	MatchManager();
	~MatchManager();

	/** Update the logic of all matches */
	void Update();

	/** Find a lobby (pending match) to join a player in, based on their game.
		If none is available, create one. */
	Match* FindLobby(PlayerSocket& player);

private:
	Match* CreateLobby(PlayerSocket& player);

private:
	HANDLE m_mutex; // Mutex to prevent simultaneous updating and creation of matches

	std::vector<std::unique_ptr<Match>> m_matches;
};
