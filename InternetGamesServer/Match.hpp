#pragma once

#include <string>
#include <vector>

#include <winsock2.h>

class PlayerSocket;

class Match
{
public:
	enum class Game {
		INVALID = 0,
		BACKGAMMON,
		CHECKERS,
		SPADES
	};
	static Game GameFromString(const std::string& str);

	enum State {
		STATE_WAITINGFORPLAYERS,
		STATE_PLAYING
	};

public:
	Match(PlayerSocket& player);

	void JoinPlayer(PlayerSocket& player);
	void DisconnectedPlayer(PlayerSocket& player);

	inline State GetState() const { return m_state; }
	inline REFGUID GetGUID() const { return m_guid; }

protected:
	virtual size_t GetRequiredPlayerCount() const { return 2; }

private:
	State m_state;

	GUID m_guid;
	std::vector<PlayerSocket*> m_players;
};
