#pragma once

#include <winsock2.h>

#include <string>
#include <vector>

#include "Match.hpp"

class PlayerSocket
{
public:
	PlayerSocket(SOCKET socket);

	std::vector<std::string> GetResponse(const std::vector<std::string>& receivedData);

	void OnGameStart();
	void OnDisconnected();

public:
	enum State {
		STATE_NOT_INITIALIZED,
		STATE_INITIALIZED,
		STATE_JOINED
	};

private:
	void ParseGasTicket(const std::string& xml);

	std::string ConstructJoinContextMessage();
	std::string ConstructReadyMessage();
	std::string ConstructStateMessage(const std::string& xml);

	static std::string ConstructReadyXML();

private:
	SOCKET m_socket;
	State m_state;

	std::string m_guid;
	Match::Game m_game;

	Match* m_match;
};
