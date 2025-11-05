#pragma once

#include <winsock2.h>

#include <string>
#include <vector>

#include "Socket.hpp"

class PlayerSocket
{
public:
	PlayerSocket(Socket& socket);
	virtual ~PlayerSocket() = default;

	virtual void ProcessMessages() = 0;

	void Disconnect();

	inline Socket::Type GetType() const { return m_socket.GetType(); }
	inline std::string GetAddressString() const { return m_socket.GetAddressString(); }

protected:
	virtual void OnDisconnected() {}

protected:
	Socket& m_socket;

private:
	PlayerSocket(const PlayerSocket&) = delete;
	PlayerSocket operator=(const PlayerSocket&) = delete;
};
