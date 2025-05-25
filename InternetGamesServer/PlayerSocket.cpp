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
PlayerSocket::OnMatchEnded()
{
	// The match has ended, so disconnect the player
	m_socket.Disconnect();
}
