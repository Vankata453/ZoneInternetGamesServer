#pragma once

#include <memory>
#include <vector>

#include <winsock2.h>

#include "WinXP/Match.hpp"

class PlayerSocket;

namespace Win7 {
	class Match;
	class PlayerSocket;
};
namespace WinXP {
	class PlayerSocket;
};

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

	std::pair<const std::vector<std::unique_ptr<Win7::Match>>&, const std::vector<std::unique_ptr<WinXP::Match>>&>
		AcquireMatches();
	void FreeAcquiredMatches();

	/** Update the logic of all matches */
	void Update();

	GUID DestroyMatch(unsigned int index);

	/** Find a lobby (pending match) to join a player in, based on their game.
		If none is available, create one. */
	Win7::Match* FindLobby(Win7::PlayerSocket& player);
	WinXP::Match* FindLobby(WinXP::PlayerSocket& player);

private:
	Win7::Match* CreateLobby(Win7::PlayerSocket& player);
	WinXP::Match* CreateLobby(WinXP::PlayerSocket& player);

private:
	HANDLE m_mutex; // Mutex to prevent simultaneous updating and creation of matches

	unsigned int m_lastMatchIndex;

	std::vector<std::unique_ptr<Win7::Match>> m_matches_win7;
	std::vector<std::unique_ptr<WinXP::Match>> m_matches_winxp;

private:
	MatchManager(const MatchManager&) = delete;
	MatchManager operator=(const MatchManager&) = delete;
};
