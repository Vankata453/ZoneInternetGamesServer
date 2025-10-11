#include "PlayerSocket.hpp"

#include <array>
#include <cassert>
#include <random>
#include <stdexcept>
#include <sstream>
#include <iostream>

#include "../../MatchManager.hpp"

namespace WinXP {

PlayerSocket::PlayerSocket(Socket& socket, const MsgConnectionHi& hiMessage) :
	::PlayerSocket(socket),
	m_state(STATE_INITIALIZED),
	m_ID(),
	m_game(Match::Game::INVALID),
	m_machineGUID(hiMessage.machineGUID),
	m_securityKey(),
	m_sequenceID(0),
	m_inLobby(false),
	m_genericMessageMutex(CreateMutex(nullptr, false, nullptr)),
	m_acceptsGameMessagesEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr)),
	m_incomingGenericMsg(),
	m_incomingGameMsg(),
	m_serviceName(),
	m_match(),
	m_seat(-1)
{
	std::mt19937 rng(std::random_device{}());
	const_cast<uint32&>(m_ID) = std::uniform_int_distribution<uint32>{}(rng);
	const_cast<uint32&>(m_securityKey) = std::uniform_int_distribution<uint32>{}(rng);
}

PlayerSocket::~PlayerSocket()
{
	if (!m_socket.IsDisconnected())
		OnDisconnected();

	CloseHandle(m_acceptsGameMessagesEvent);
	CloseHandle(m_genericMessageMutex);
}


#define XPPlayerSocketMatchGuard \
	if (!m_match) \
		throw std::runtime_error("WinXP::PlayerSocket::ProcessMessages(): Attempted to access destroyed match!"); \
	if (m_match->GetState() == Match::STATE_ENDED) \
		throw std::runtime_error("WinXP::PlayerSocket::ProcessMessages(): Attempted to access ended match!");

void
PlayerSocket::ProcessMessages()
{
	while (true)
	{
		AwaitGenericMessageHeader();
		if (m_incomingGenericMsg.GetType() == MessageConnectionKeepAlive)
		{
			AwaitIncomingGenericFooter();
			continue;
		}

		switch (m_state)
		{
			case STATE_INITIALIZED:
			{
				AwaitProxyHiMessages();
				SendProxyHelloMessages();
				break;
			}
			case STATE_UNCONFIGURED:
			{
				AwaitClientConfig();

				m_match = MatchManager::Get().FindLobby(*this);
				m_state = STATE_WAITINGFOROPPONENTS;
				break;
			}
			case STATE_PROXY_DISCONNECTED: // Disconnected proxy (after: disconnected from match, find new opponent, received corrupted data)
			{
				assert(!m_inLobby);

				/* Await connect request and send response */
				const MsgProxyServiceRequest msgServiceRequestConnect = AwaitIncomingGenericMessage<MsgProxyServiceRequest, 1>();
				if (!ValidateProxyMessage<MessageProxyServiceRequest>(msgServiceRequestConnect))
					throw std::runtime_error("MsgProxyServiceRequest: Message is invalid!");

				if (msgServiceRequestConnect.reason != MsgProxyServiceRequest::REASON_CONNECT)
				{
					if (msgServiceRequestConnect.reason == MsgProxyServiceRequest::REASON_DISCONNECT)
						break;
					throw std::runtime_error("MsgProxyServiceRequest: Reason is not client connection or disconnection!");
				}

				SendProxyServiceInfoMessages(MsgProxyServiceInfo::SERVICE_CONNECT);
				break;
			}
			case STATE_STARTING_GAME:
			{
				switch (WaitForSingleObject(m_acceptsGameMessagesEvent, 5000))
				{
					case WAIT_OBJECT_0:
						break;
					case WAIT_TIMEOUT:
						throw std::runtime_error("WinXP::PlayerSocket::ProcessMessages(): Timed out waiting for \"accepts game messages\" event!");
					default:
						throw std::runtime_error("WinXP::PlayerSocket::ProcessMessages(): An error occured waiting for \"accepts game messages\" event: " + std::to_string(GetLastError()));
				}
				// Fallthrough (game messages are now accepted)
			}
			case STATE_PLAYING:
			{
				switch (m_incomingGenericMsg.GetType())
				{
					case MessageGameMessage:
					{
						AwaitIncomingGameMessageHeader();

						XPPlayerSocketMatchGuard
						m_match->ProcessIncomingGameMessage(*this, m_incomingGameMsg.GetType());
						break;
					}
					case MessageChatSwitch:
					{
						const MsgChatSwitch msgChatSwitch = AwaitIncomingGenericMessage<MsgChatSwitch, MessageChatSwitch>();
						if (msgChatSwitch.userID != m_ID)
							throw std::runtime_error("MsgChatSwitch: Incorrect user ID!");

						if (m_config.chatEnabled == msgChatSwitch.chatEnabled)
							break;
						m_config.chatEnabled = msgChatSwitch.chatEnabled;

						XPPlayerSocketMatchGuard
						m_match->ProcessMessage(msgChatSwitch);
						break;
					}
					case 1: // Disconnect proxy (find new opponent, received corrupted data)
					{
						assert(m_inLobby);

						m_inLobby = false;

						/* Await disconnect request and send response */
						const MsgProxyServiceRequest msgServiceRequestDisconnect = AwaitIncomingGenericMessage<MsgProxyServiceRequest, 1>();
						if (!ValidateProxyMessage<MessageProxyServiceRequest>(msgServiceRequestDisconnect))
							throw std::runtime_error("MsgProxyServiceRequest: Message is invalid!");

						if (msgServiceRequestDisconnect.reason != MsgProxyServiceRequest::REASON_DISCONNECT)
							throw std::runtime_error("MsgProxyServiceRequest: Reason is not client disconnection!");

						SendProxyServiceInfoMessages(MsgProxyServiceInfo::SERVICE_DISCONNECT);
						break;
					}
					default:
						throw std::runtime_error("WinXP::PlayerSocket::ProcessMessages(): Generic message of unknown type received: " + std::to_string(m_incomingGenericMsg.GetType()));
				}
				break;
			}
			default:
				throw std::runtime_error("WinXP::PlayerSocket::ProcessMessages(): Message was received, but current state (" + std::to_string(m_state) + ") does not process any!");
		}
	}
}

#undef XPPlayerSocketMatchGuard


void
PlayerSocket::OnGameStart(const std::vector<PlayerSocket*>& matchPlayers)
{
	if (m_state != STATE_WAITINGFOROPPONENTS)
		return;

	MsgUserInfoResponse msgUserInfo;
	msgUserInfo.ID = m_ID;
	msgUserInfo.language = m_config.userLanguage;
	snprintf(msgUserInfo.username, sizeof(msgUserInfo.username), "Player %d", m_seat);

	const int16 totalPlayerCount = static_cast<int16>(m_match->GetRequiredPlayerCount());
	MsgGameStart msgGameStart;
	msgGameStart.gameID = m_match->GetGameID();
	msgGameStart.seat = m_seat;
	msgGameStart.totalSeats = totalPlayerCount;

	assert(matchPlayers.size() == totalPlayerCount);
	for (int16 i = 0; i < totalPlayerCount; ++i)
	{
		MsgGameStart::User& user = msgGameStart.users[matchPlayers[i]->m_seat];
		const Config& config = matchPlayers[i]->m_config;

		user.ID = matchPlayers[i]->GetID();
		user.language = config.userLanguage;
		user.chatEnabled = config.chatEnabled;
		user.skill = static_cast<int16>(config.skillLevel);

		assert(config.skillLevel != Match::SkillLevel::INVALID);
	}

	m_state = STATE_STARTING_GAME;

	SendGenericMessage<MessageUserInfoResponse>(std::move(msgUserInfo));
	SendGenericMessage<MessageGameStart>(std::move(msgGameStart),
		sizeof(MsgGameStart) - sizeof(MsgGameStart::User) * (XPMaxMatchPlayers - totalPlayerCount));

	// The match can now send game messages to this client - game start messages have been sent
	m_state = STATE_PLAYING;
	SetEvent(m_acceptsGameMessagesEvent);
}

void
PlayerSocket::OnDisconnected()
{
	if (m_match)
	{
		m_match->DisconnectedPlayer(*this);
		m_match = nullptr;
	}
	m_inLobby = false;
	m_state = STATE_DISCONNECTING;
	ResetEvent(m_acceptsGameMessagesEvent);
}

void
PlayerSocket::OnMatchDisconnect()
{
	if (!m_match)
		return;
	m_match = nullptr;
	SendProxyServiceInfoMessages(MsgProxyServiceInfo::SERVICE_DISCONNECT);
}


void
PlayerSocket::AwaitGenericMessageHeader()
{
	assert(!m_incomingGenericMsg.valid && !m_incomingGameMsg.valid);

	m_socket.ReceiveData(m_incomingGenericMsg.base, DecryptMessage, m_securityKey);
	if (!ValidateInternalMessageNoTotalLength<MessageConnectionGeneric>(m_incomingGenericMsg.base))
		throw std::runtime_error("MsgBaseGeneric: Message is invalid!");

	if (WaitForSingleObject(m_genericMessageMutex, 5000) == WAIT_ABANDONED) // Acquired ownership of an abandoned generic message mutex
		throw std::runtime_error("WinXP::PlayerSocket::AwaitGenericMessageHeader(): Got ownership of an abandoned generic message mutex: " + std::to_string(GetLastError()));

	try
	{
		m_socket.ReceiveData(m_incomingGenericMsg.info, DecryptMessage, m_securityKey);
	}
	catch (...)
	{
		ReleaseMutex(m_genericMessageMutex);
		throw;
	}

	m_incomingGenericMsg.valid = true;
}

void
PlayerSocket::AwaitIncomingGameMessageHeader()
{
	assert(m_incomingGenericMsg.valid && !m_incomingGameMsg.valid);

	assert(m_inLobby);
	assert(m_state == STATE_PLAYING); // The client can send game messages only in STATE_PLAYING

	const MsgBaseGeneric& msgBaseGeneric = m_incomingGenericMsg.base;
	const MsgBaseApplication& msgBaseApp = m_incomingGenericMsg.info;

	if (msgBaseGeneric.totalLength != sizeof(MsgBaseGeneric) + sizeof(MsgBaseApplication) + ROUND_DATA_LENGTH_UINT32(msgBaseApp.dataLength) + sizeof(MsgFooterGeneric))
		throw std::runtime_error("MsgBaseGeneric: totalLength is invalid!");

	if (msgBaseApp.signature != XPLobbyProtocolSignature)
		throw std::runtime_error("MsgBaseApplication: Invalid protocol signature!");
	if (msgBaseApp.messageType != MessageGameMessage)
		throw std::runtime_error("MsgBaseApplication: Incorrect message type! Expected: Game message");
	if (msgBaseApp.dataLength < sizeof(MsgGameMessage))
		throw std::runtime_error("MsgBaseApplication: Data is of incorrect size! Expected: equal or more than " + std::to_string(sizeof(MsgGameMessage)));

	try
	{
		m_socket.ReceiveData(m_incomingGameMsg.info, DecryptMessage, m_securityKey);
	}
	catch (...)
	{
		ReleaseMutex(m_genericMessageMutex);
		throw;
	}

	m_incomingGameMsg.valid = true;
}

void
PlayerSocket::AwaitIncomingEmptyGameMessage(uint32 type)
{
	assert(m_incomingGenericMsg.valid && m_incomingGameMsg.valid);

	const MsgBaseGeneric& msgBaseGeneric = m_incomingGenericMsg.base;
	const MsgBaseApplication& msgBaseApp = m_incomingGenericMsg.info;
	const MsgGameMessage& msgGameMessage = m_incomingGameMsg.info;

	if (msgGameMessage.gameID != m_match->GetGameID())
		throw std::runtime_error("MsgGameMessage: Incorrect game ID!");
	if (msgGameMessage.type != type)
		throw std::runtime_error("MsgGameMessage: Incorrect message type! Expected: " + std::to_string(type));
	if (msgGameMessage.length != 0)
		throw std::runtime_error("MsgGameMessage: length is invalid: Expected empty message!");

	AwaitIncomingGenericFooter();
	m_incomingGameMsg.valid = false;

	// Validate checksum
	const uint32 checksum = GenerateChecksum({
			{ &msgBaseApp, sizeof(msgBaseApp) },
			{ &msgGameMessage, sizeof(msgGameMessage) }
		});
	if (checksum != msgBaseGeneric.checksum)
		throw std::runtime_error("MsgBaseGeneric: Checksums don't match! Generated: " + std::to_string(checksum));
}

void
PlayerSocket::AwaitIncomingGenericFooter()
{
	MsgFooterGeneric msgFooterGeneric;
	try
	{
		m_socket.ReceiveData(msgFooterGeneric);
	}
	catch (...)
	{
		ReleaseMutex(m_genericMessageMutex);
		throw;
	}
	if (msgFooterGeneric.status == MsgFooterGeneric::STATUS_CANCELLED)
		throw std::runtime_error("MsgFooterGeneric: Status is CANCELLED!");
	else if (msgFooterGeneric.status != MsgFooterGeneric::STATUS_OK)
		throw std::runtime_error("MsgFooterGeneric: Status is not OK! - " + std::to_string(msgFooterGeneric.status));

	m_incomingGenericMsg.valid = false;

	if (!ReleaseMutex(m_genericMessageMutex))
		throw std::runtime_error("WinXP::PlayerSocket::AwaitIncomingGenericFooter(): Couldn't release generic message mutex: " + std::to_string(GetLastError()));
}


void
PlayerSocket::AwaitProxyHiMessages()
{
	Util::MsgProxyHiCollection msg = AwaitIncomingGenericMessage<Util::MsgProxyHiCollection, 3>();
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

	if (msg.serviceRequest.reason != MsgProxyServiceRequest::REASON_CONNECT)
		throw std::runtime_error("MsgProxyServiceRequest: Reason is not client connection!");

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
	MsgClientConfig msg = AwaitIncomingGenericMessage<MsgClientConfig, MessageClientConfig>();
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
	std::memcpy(msgsHello.basicServiceInfo.serviceName, m_serviceName, sizeof(m_serviceName));

	Util::MsgProxyServiceInfoCollection msgsServiceInfo;
	msgsServiceInfo.serviceInfo = msgsHello.basicServiceInfo;
	msgsServiceInfo.serviceInfo.reason = MsgProxyServiceInfo::SERVICE_CONNECT;
	msgsServiceInfo.basicServiceInfo = msgsHello.basicServiceInfo;

	SendGenericMessage<3>(std::move(msgsHello));
	SendGenericMessage<2>(std::move(msgsServiceInfo));

	m_inLobby = true;
	m_state = STATE_UNCONFIGURED;
}

void
PlayerSocket::SendProxyServiceInfoMessages(MsgProxyServiceInfo::Reason reason)
{
	Util::MsgProxyServiceInfoCollection msgsServiceInfo;

	msgsServiceInfo.basicServiceInfo.flags = MsgProxyServiceInfo::SERVICE_AVAILABLE | MsgProxyServiceInfo::SERVICE_LOCAL;
	msgsServiceInfo.basicServiceInfo.minutesRemaining = 0;
	std::memcpy(msgsServiceInfo.basicServiceInfo.serviceName, m_serviceName, sizeof(m_serviceName));

	msgsServiceInfo.serviceInfo = msgsServiceInfo.basicServiceInfo;
	msgsServiceInfo.serviceInfo.reason = reason;

	SendGenericMessage<2>(std::move(msgsServiceInfo), AdjustedSize<Util::MsgProxyServiceInfoCollection>, true);

	if (reason == MsgProxyServiceInfo::SERVICE_CONNECT)
	{
		assert(m_state != STATE_PLAYING);

		m_inLobby = true;
		m_state = STATE_UNCONFIGURED;
	}
	else if (reason == MsgProxyServiceInfo::SERVICE_DISCONNECT)
	{
		if (m_match)
		{
			m_match->DisconnectedPlayer(*this);
			m_match = nullptr;
		}
		m_inLobby = false;
		m_state = STATE_PROXY_DISCONNECTED;
		ResetEvent(m_acceptsGameMessagesEvent);
	}
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
