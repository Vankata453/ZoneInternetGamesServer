#include "PlayerSocket.hpp"

#include <cassert>
#include <random>
#include <stdexcept>
#include <sstream>

namespace WinXP {

PlayerSocket::PlayerSocket(Socket& socket, const MsgConnectionHi& hiMessage) :
	::PlayerSocket(socket),
	m_state(STATE_INITIALIZED),
	m_game(Match::Game::INVALID),
	m_machineGUID(hiMessage.machineGUID),
	m_securityKey(),
	m_match()
{
	std::mt19937 keyRng(std::random_device{}());
	m_securityKey = std::uniform_int_distribution<uint32>{}(keyRng);
}

PlayerSocket::~PlayerSocket()
{
	OnDisconnected();
}


void
PlayerSocket::ProcessMessages()
{
	while (true)
	{
		// TODO: Process received messages
	}
}


void
PlayerSocket::OnGameStart()
{
	if (m_state != STATE_WAITINGFOROPPONENTS)
		return;

	m_state = STATE_PLAYING;
}

void
PlayerSocket::OnDisconnected()
{
	if (m_match)
	{
		m_match->DisconnectedPlayer(*this);
		m_match = nullptr;
	}
}


MsgConnectionHello
PlayerSocket::ConstructHelloMessage() const
{
	WinXP::MsgConnectionHello helloMessage;
	helloMessage.signature = WinXP::InternalProtocolSignature;
	helloMessage.totalLength = sizeof(helloMessage);
	helloMessage.intLength = sizeof(helloMessage);
	helloMessage.messageType = WinXP::MessageConnectionHello;

	helloMessage.key = m_securityKey;
	helloMessage.machineGUID = m_machineGUID;

	return helloMessage;
}

}
