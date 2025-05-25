#pragma once

#include <ctime>
#include <string>
#include <vector>

template<typename P>
class Match
{
public:
	Match(P& player) :
		m_players(),
		m_creationTime(std::time(nullptr))
	{}
	virtual ~Match() = default;

	/** Update match logic */
	virtual void Update() = 0;

protected:
	virtual size_t GetRequiredPlayerCount() const { return 2; }

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
	std::vector<P*> m_players;

	const std::time_t m_creationTime;

private:
	Match(const Match&) = delete;
	Match operator=(const Match&) = delete;
};
