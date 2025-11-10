#include "PlayerSocket.hpp"

#include <stdexcept>
#include <sstream>

#include "MatchManager.hpp"
#include "Util.hpp"

PlayerSocket::PlayerSocket(Socket& socket) :
	m_socket(socket),
	m_disconnected(false)
{
	m_socket.m_playerSocket = this;
}


void
PlayerSocket::Disconnect()
{
	if (m_disconnected)
		return;

	m_disconnected = true; // Set early on to prevent another thread from disconnecting this socket again.

	m_socket.m_playerSocket = nullptr;

	OnDisconnected();
	m_socket.Disconnect();
}

void
PlayerSocket::Destroy()
{
	if (m_disconnected)
		return;

	m_disconnected = true; // Set early on to prevent another thread from disconnecting this socket again.

	m_socket.m_playerSocket = nullptr;

	OnDisconnected();
}
