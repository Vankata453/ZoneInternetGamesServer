#pragma once

#include <winsock2.h>

#include <string>
#include <vector>

#include "Match.hpp"

class PlayerSocket
{
public:
	enum State {
		STATE_NOT_INITIALIZED,
		STATE_INITIALIZED,
		STATE_JOINING,
		STATE_WAITINGFOROPPONENTS,
		STATE_PLAYING
	};

public:
	PlayerSocket(SOCKET socket);

	std::vector<std::string> GetResponse(const std::vector<std::string>& receivedData);

	void OnGameStart();
	void OnDisconnected();

	inline State GetState() const { return m_state; }

private:
	void ParseGasTicket(const std::string& xml);

	/** Construct protocol messages */
	std::string ConstructJoinContextMessage();
	std::string ConstructReadyMessage();
	std::string ConstructStateMessage(const std::string& xml);

private:
	SOCKET m_socket;
	State m_state;

	std::string m_guid;
	Match::Game m_game;

	Match* m_match;

public:
	// Match-related variables
	std::string m_name;
	int m_role;
};
