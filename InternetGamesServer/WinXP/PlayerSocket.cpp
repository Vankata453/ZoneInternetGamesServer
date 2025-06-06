#include "PlayerSocket.hpp"

#include <cassert>
#include <random>
#include <stdexcept>
#include <sstream>

#include "../../MatchManager.hpp"
#include "Protocol/Game.hpp"
#include "Protocol/Proxy.hpp"

namespace WinXP {

PlayerSocket::PlayerSocket(Socket& socket, const MsgConnectionHi& hiMessage) :
	::PlayerSocket(socket),
	m_state(STATE_INITIALIZED),
	m_game(Match::Game::INVALID),
	m_machineGUID(hiMessage.machineGUID),
	m_securityKey(),
	m_sequenceID(0),
	m_inLobby(false),
	m_serviceName(),
	m_match(),
	m_ID(-1)
{
	std::mt19937 keyRng(std::random_device{}());
	const_cast<uint32&>(m_securityKey) = std::uniform_int_distribution<uint32>{}(keyRng);
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
				m_inLobby = true;

				AwaitClientConfig();
				SendServerStatus();

				m_match = MatchManager::Get().FindLobby(*this);
				m_state = STATE_WAITINGFOROPPONENTS;
				break;
			}
			case STATE_PLAYING:
			{
				break;
			}
		}
	}
}


void
PlayerSocket::OnGameStart(const std::vector<PlayerSocket*>& matchPlayers)
{
	if (m_state != STATE_WAITINGFOROPPONENTS)
		return;

	MsgUserInfoResponse msgUserInfo;
	msgUserInfo.ID = m_ID;
	msgUserInfo.language = m_config.userLanguage;
	snprintf(msgUserInfo.username, sizeof(msgUserInfo.username), "Player %d", m_ID);

	const int16 totalPlayerCount = static_cast<int16>(m_match->GetRequiredPlayerCount());
	MsgGameStart msgGameStart;
	msgGameStart.gameID = 1; // TODO
	msgGameStart.seat = m_ID;
	msgGameStart.totalSeats = totalPlayerCount;

	assert(matchPlayers.size() == totalPlayerCount);
	for (int16 i = 0; i < totalPlayerCount; ++i)
	{
		MsgGameStart::User& user = msgGameStart.users[i];
		const Config& config = matchPlayers[i]->m_config;

		user.ID = matchPlayers[i]->m_ID;
		user.language = config.userLanguage;
		user.chatEnabled = config.chatEnabled;
		user.skill = static_cast<int16>(config.skillLevel);

		assert(config.skillLevel != Match::SkillLevel::INVALID);
	}

	SendGenericMessage<MessageUserInfoResponse>(std::move(msgUserInfo));
	SendGenericMessage<MessageGameStart>(std::move(msgGameStart),
		sizeof(MsgGameStart) - sizeof(MsgGameStart::User) * (XPMaxMatchPlayers - totalPlayerCount));

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
PlayerSocket::AwaitClientConfig()
{
	MsgClientConfig msg = AwaitGenericMessage<MsgClientConfig, MessageClientConfig>();
	if (msg.protocolSignature != XPRoomProtocolSignature)
		throw std::runtime_error("MsgClientConfig: Invalid protocol signature!");
	if (msg.protocolVersion != XPRoomClientVersion)
		throw std::runtime_error("MsgClientConfig: Unsupported client version!");

	// Parse config string
	const char* ch = msg.config;
	while (*ch)
	{
		const char* keyStart = ch;
		const char* equalChar = strchr(ch, '=');
		if (!equalChar)
			throw std::runtime_error("MsgClientConfig: Config parse error: Expected '='!");
		const std::string key(keyStart, equalChar);

		if (*(equalChar + 1) != '<')
			throw std::runtime_error("MsgClientConfig: Config parse error: Expected '<' after '='!");
		const char* valStart = equalChar + 2;
		const char* valEnd = strchr(valStart, '>');
		if (!valEnd)
			throw std::runtime_error("MsgClientConfig: Config parse error: Expected '>'!");
		const std::string value(valStart, valEnd);

		if (key == "SLCID" || key == "ILCID") // System and application LCIDs
		{
			// Ignore
		}
		else if (key == "ULCID") // User LCID
		{
			m_config.userLanguage = std::stoi(value);
		}
		else if (key == "UTCOFFSET") // UTC offset
		{
			m_config.offsetUTC = std::stoi(value);
		}
		else if (key == "Skill") // Skill level
		{
			if (value == "Beginner")
				m_config.skillLevel = Match::SkillLevel::BEGINNER;
			else if (value == "Intermediate")
				m_config.skillLevel = Match::SkillLevel::INTERMEDIATE;
			else if (value == "Expert")
				m_config.skillLevel = Match::SkillLevel::EXPERT;
			else
				throw std::runtime_error("MsgClientConfig: Config: Unknown value for 'Skill': '" + value + "'!");
		}
		else if (key == "Chat") // Chat enabled
		{
			if (value == "Off")
				m_config.chatEnabled = false;
			else if (value == "On")
				m_config.chatEnabled = true;
			else
				throw std::runtime_error("MsgClientConfig: Config: Unknown value for 'Chat': '" + value + "'!");
		}
		else if (key == "Exit")
		{
			// Ignore
		}
		else
		{
			throw std::runtime_error("MsgClientConfig: Config: Unknown property '" + key + "'!");
		}

		ch = valEnd + 1; // Parse next entry
	}
}


void
PlayerSocket::SendProxyHelloMessages()
{
	Util::MsgProxyHelloCollection msgsHello;
	msgsHello.settings.chat = MsgProxySettings::CHAT_FULL;
	msgsHello.settings.statistics = MsgProxySettings::STATS_ALL;
	msgsHello.basicServiceInfo.flags = MsgProxyServiceInfo::SERVICE_AVAILABLE | MsgProxyServiceInfo::SERVICE_LOCAL;
	msgsHello.basicServiceInfo.minutesRemaining = 0;
	memcpy(msgsHello.basicServiceInfo.serviceName, m_serviceName, sizeof(m_serviceName));

	Util::MsgProxyServiceInfoCollection msgsServiceInfo;
	msgsServiceInfo.serviceInfo = msgsHello.basicServiceInfo;
	msgsServiceInfo.serviceInfo.reason = MsgProxyServiceInfo::SERVICE_CONNECT;
	msgsServiceInfo.basicServiceInfo = msgsHello.basicServiceInfo;

	SendGenericMessage<3>(std::move(msgsHello));
	SendGenericMessage<2>(std::move(msgsServiceInfo));
}

void
PlayerSocket::SendServerStatus()
{
	MsgServerStatus msg;
	msg.playersWaiting = 1; // TODO

	SendGenericMessage<MessageServerStatus>(std::move(msg));
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
