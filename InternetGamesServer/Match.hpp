#pragma once

#define WIN32_LEAN_AND_MEAN

#include <ctime>
#include <string>
#include <vector>

#include <windows.h>
#include <rpc.h>
#include <rpcdce.h>

template<typename P>
class Match
{
public:
	Match(P& player) :
		m_guid(),
		m_creationTime(std::time(nullptr)),
		m_players()
	{
		// Generate a unique GUID for the match
		UuidCreate(const_cast<GUID*>(&m_guid));
	}
	virtual ~Match() = default;

	/** Update match logic */
	virtual void Update() = 0;

	virtual size_t GetRequiredPlayerCount() const { return 2; }
	inline REFGUID GetGUID() const { return m_guid; }

protected:
	void AddPlayer(P& player)
	{
		// Add to players array
		m_players.push_back(&player);
	}
	void RemovePlayer(const P& player)
	{
		// Remove from players array
		if (!m_players.empty())
			m_players.erase(std::remove(m_players.begin(), m_players.end(), &player), m_players.end());
	}

protected:
	const GUID m_guid;
	const std::time_t m_creationTime;

	std::vector<P*> m_players;

private:
	Match(const Match&) = delete;
	Match operator=(const Match&) = delete;
};
