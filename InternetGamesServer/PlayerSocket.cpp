#include "PlayerSocket.hpp"

#include <stdexcept>
#include <sstream>

#include "MatchManager.hpp"
#include "Util.hpp"

PlayerSocket::PlayerSocket(Socket& socket) :
	m_socket(socket)
{
}


void
PlayerSocket::Disconnect()
{
	if (m_socket.IsDisconnected())
		return;

	OnDisconnected();
	m_socket.Disconnect();
}
