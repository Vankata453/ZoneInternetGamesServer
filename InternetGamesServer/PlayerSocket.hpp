#pragma once

#include <winsock2.h>

#include <string>
#include <vector>

class PlayerSocket
{
public:
	PlayerSocket(SOCKET socket);

	std::vector<std::string> GetResponse(const std::vector<std::string>& receivedData);
	void OnDisconnected();

public:
	enum State {
		STATE_NOT_INITIALIZED,
		STATE_INITIALIZED,
		STATE_JOINED
	};

private:
	SOCKET m_socket;
	State m_state;

	std::string m_guid;
};
