#include "PlayerSocket.hpp"

#include <cassert>
#include <random>
#include <stdexcept>
#include <sstream>

#include "Protocol/Proxy.hpp"

namespace WinXP {

PlayerSocket::PlayerSocket(Socket& socket, const MsgConnectionHi& hiMessage) :
	::PlayerSocket(socket),
	m_state(STATE_INITIALIZED),
	m_game(Match::Game::INVALID),
	m_machineGUID(hiMessage.machineGUID),
	m_securityKey(),
	m_serviceName(),
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
		switch (m_state)
		{
			case STATE_INITIALIZED:
			{
				AwaitProxyHiMessages();
				SendProxyHelloMessages();
				m_state = STATE_WAITINGFOROPPONENTS;
				break;
			}
			case STATE_WAITINGFOROPPONENTS:
			{
				AwaitProxyHiMessages(); // TEMP (TODO)
				break;
			}
		}
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


void
PlayerSocket::AwaitProxyHiMessages()
{
	Util::MsgProxyHiCollection msg = AwaitGenericMessage<Util::MsgProxyHiCollection, 3>();
	if (!ValidateProxyMessage<MessageProxyHi>(msg.hi))
		throw std::runtime_error("MsgProxyHi: Message is invalid!");
	if (!ValidateProxyMessage<MessageProxyID>(msg.ID))
		throw std::runtime_error("MsgProxyID: Message is invalid!");
	if (!ValidateProxyMessage<MessageProxyServiceRequest>(msg.serviceRequest))
		throw std::runtime_error("MsgProxyServiceRequest: Message is invalid!");

	if (msg.hi.protocolVersion != XPProxyProtocolVersion)
		throw std::runtime_error("MsgProxyHi: Incorrect protocol version!");
	if (msg.hi.clientVersion != XPProxyClientVersion)
		throw std::runtime_error("MsgProxyHi: Unsupported client version!");

	// Determine game
	const std::string gameToken(msg.hi.gameToken, 6);
	m_game = Match::GameFromString(gameToken);
	if (m_game == Match::Game::INVALID)
		throw std::runtime_error("Invalid game token: " + gameToken + "!");

	memmove(m_serviceName, msg.serviceRequest.serviceName, sizeof(msg.serviceRequest.serviceName));
}


void
PlayerSocket::SendProxyHelloMessages()
{
	Util::MsgProxyHelloCollection msg;
	msg.settings.chat = MsgProxySettings::CHAT_FULL;
	msg.settings.statistics = MsgProxySettings::STATS_ALL;
	msg.serviceInfo.reason = MsgProxyServiceInfo::SERVICE_CONNECT;
	msg.serviceInfo.flags = MsgProxyServiceInfo::SERVICE_AVAILABLE | MsgProxyServiceInfo::SERVICE_LOCAL;
	msg.serviceInfo.minutesRemaining = 0;
	memcpy(msg.serviceInfo.serviceName, m_serviceName, sizeof(m_serviceName));

	SendGenericMessage<3>(std::move(msg));
}


MsgConnectionHello
PlayerSocket::ConstructHelloMessage() const
{
	WinXP::MsgConnectionHello helloMessage;
	helloMessage.key = m_securityKey;
	helloMessage.machineGUID = m_machineGUID;
	return helloMessage;
}

}
