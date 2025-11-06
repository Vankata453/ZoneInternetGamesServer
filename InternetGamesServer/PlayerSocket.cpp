#include "PlayerSocket.hpp"

#include <stdexcept>
#include <sstream>

#include "MatchManager.hpp"
#include "Util.hpp"

PlayerSocket::PlayerSocket(Socket& socket) :
	m_socket(socket),
	m_disconnected(false)
{
}


void
PlayerSocket::Disconnect()
{
	if (m_disconnected)
		return;

	m_disconnected = true; // Set early on to prevent another thread from disconnecting this socket again.

	OnDisconnected();
	m_socket.Disconnect();
}
