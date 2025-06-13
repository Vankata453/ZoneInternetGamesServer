#pragma once

#include "../PlayerSocket.hpp"

#include "Defines.hpp"
#include "Match.hpp"
#include "Protocol/Game.hpp"
#include "Protocol/Init.hpp"
#include "Security.hpp"

namespace WinXP {

class PlayerSocket final : public ::PlayerSocket
{
public:
	enum State {
		STATE_INITIALIZED,
		STATE_UNCONFIGURED,
		STATE_WAITINGFOROPPONENTS,
		STATE_PLAYING
	};

public:
	PlayerSocket(Socket& socket, const MsgConnectionHi& hiMessage);
	~PlayerSocket() override;

	void ProcessMessages() override;

	/** Event handling */
	void OnGameStart(const std::vector<PlayerSocket*>& matchPlayers);
	void OnDisconnected();
	template<typename T, uint32 Type>
	T OnMatchAwaitGameMessage()
	{
		return AwaitIncomingGameMessage<T, Type>();
	}
	template<typename T, uint32 Type, uint16 MessageLen>
	std::pair<T, CharArray<MessageLen>> OnMatchAwaitGameMessage()
	{
		return AwaitIncomingGameMessage<T, Type, MessageLen>();
	}
	template<uint32 Type, typename T>
	void OnMatchGenericMessage(const T& msgApp, int len = sizeof(T))
	{
		SendGenericMessage<Type>(msgApp, len);
	}
	template<uint32 Type, typename T>
	void OnMatchGameMessage(const T& msgGame, int len = sizeof(T))
	{
		SendGameMessage<Type>(msgGame, len);
	}
	template<uint32 Type, typename T, uint16 MessageLen> // Another arbitrary message right after T
	void OnMatchGameMessage(const T& msgGame, const CharArray<MessageLen>& msgGameSecond)
	{
		SendGameMessage<Type>(msgGame, msgGameSecond);
	}

	Socket::Type GetType() const override { return Socket::WIN7; }
	inline State GetState() const { return m_state; }
	inline Match::Game GetGame() const { return m_game; }
	inline Match::SkillLevel GetSkillLevel() const { return m_config.skillLevel; }
	inline uint32 GetSecurityKey() const { return m_securityKey; }

	MsgConnectionHello ConstructHelloMessage() const;

private:
	/* Awaiting utilities */
	void AwaitGenericMessageHeader();
	template<typename T, uint32 Type>
	T AwaitIncomingGenericMessage()
	{
		assert(m_incomingGenericMsg.valid && !m_incomingGameMsg.valid);

		const MsgBaseGeneric& msgBaseGeneric = m_incomingGenericMsg.base;
		const MsgBaseApplication& msgBaseApp = m_incomingGenericMsg.info;

		if (msgBaseGeneric.totalLength != sizeof(MsgBaseGeneric) + sizeof(MsgBaseApplication) + ROUND_DATA_LENGTH_UINT32(msgBaseApp.dataLength) + sizeof(MsgFooterGeneric))
			throw std::runtime_error("MsgBaseGeneric: totalLength is invalid!");

		if (msgBaseApp.signature != (m_inLobby ? XPProxyProtocolSignature : XPInternalProtocolSignatureApp))
			throw std::runtime_error("MsgBaseApplication: Invalid protocol signature!");
		if (msgBaseApp.messageType != Type)
			throw std::runtime_error("MsgBaseApplication: Incorrect message type! Expected: " + std::to_string(Type));
		if (msgBaseApp.dataLength != sizeof(T))
			throw std::runtime_error("MsgBaseApplication: Data is of incorrect size! Expected: " + std::to_string(sizeof(T)));

		T msgApp;
		m_socket.ReceiveData(msgApp, DecryptMessage, m_securityKey);

		AwaitIncomingGenericFooter();

		// Validate checksum
		const uint32 checksum = GenerateChecksum({
				{ &msgBaseApp, sizeof(msgBaseApp) },
				{ &msgApp, sizeof(msgApp) }
			});
		if (checksum != msgBaseGeneric.checksum)
			throw std::runtime_error("MsgBaseGeneric: Checksums don't match! Generated: " + std::to_string(checksum));

		return msgApp;
	}
	void AwaitIncomingGameMessageHeader();
	template<typename T, uint32 Type>
	T AwaitIncomingGameMessage()
	{
		assert(m_incomingGenericMsg.valid && m_incomingGameMsg.valid);

		const MsgBaseGeneric& msgBaseGeneric = m_incomingGenericMsg.base;
		const MsgBaseApplication& msgBaseApp = m_incomingGenericMsg.info;
		const MsgGameMessage& msgGameMessage = m_incomingGameMsg.info;

		if (msgGameMessage.length != msgBaseApp.dataLength - sizeof(msgGameMessage))
			throw std::runtime_error("MsgGameMessage: length is invalid!");
		if (msgGameMessage.length != sizeof(T))
			throw std::runtime_error("MsgGameMessage: Data is of incorrect size! Expected: " + std::to_string(sizeof(T)));
		if (msgGameMessage.type != Type)
			throw std::runtime_error("MsgGameMessage: Incorrect message type! Expected: " + std::to_string(Type));

		T msgGame;
		char msgGameRaw[sizeof(T)];
		m_socket.ReceiveData(msgGame, &T::ConvertToHostEndian, DecryptMessage, m_securityKey, msgGameRaw);

		AwaitIncomingGenericFooter();
		m_incomingGameMsg.valid = false;

		// Validate checksum
		const uint32 checksum = GenerateChecksum({
				{ &msgBaseApp, sizeof(msgBaseApp) },
				{ &msgGameMessage, sizeof(msgGameMessage) },
				{ msgGameRaw, sizeof(msgGameRaw) }
			});
		if (checksum != msgBaseGeneric.checksum)
			throw std::runtime_error("MsgBaseGeneric: Checksums don't match! Generated: " + std::to_string(checksum));

		return msgGame;
	}
	template<typename T, uint32 Type, uint16 MessageLen> // Another arbitrary message right after T
	std::pair<T, CharArray<MessageLen>> AwaitIncomingGameMessage()
	{
		assert(m_incomingGenericMsg.valid && m_incomingGameMsg.valid);

		const MsgBaseGeneric& msgBaseGeneric = m_incomingGenericMsg.base;
		const MsgBaseApplication& msgBaseApp = m_incomingGenericMsg.info;
		const MsgGameMessage& msgGameMessage = m_incomingGameMsg.info;

		if (msgGameMessage.length != msgBaseApp.dataLength - sizeof(msgGameMessage))
			throw std::runtime_error("MsgGameMessage: length is invalid!");
		if (msgGameMessage.type != Type)
			throw std::runtime_error("MsgGameMessage: Incorrect message type! Expected: " + std::to_string(Type));

		T msgGame;
		char msgGameRaw[sizeof(T)];
		m_socket.ReceiveData(msgGame, &T::ConvertToHostEndian, DecryptMessage, m_securityKey, msgGameRaw);

		if (msgGameMessage.length != sizeof(T) + msgGame.messageLength)
			throw std::runtime_error("MsgGameMessage: Data is of incorrect size! Expected: " + std::to_string(sizeof(T) + msgGame.messageLength));
		if (msgGame.messageLength > MessageLen)
			throw std::runtime_error("MsgGameMessage: Child message is too long! Expected less or equal than: " + std::to_string(MessageLen));

		const int msgLength = static_cast<int>(msgGame.messageLength);

		// First, receive the full message, including additional data due to uint32 rounding.
		// Must be in one buffer as to not split DWORD blocks for checksum generation.
		CharArray<MessageLen + sizeof(uint32)> msgGameSecondFull;
		msgGameSecondFull.len = ROUND_DATA_LENGTH_UINT32(msgLength);
		m_socket.ReceiveData(msgGameSecondFull, DecryptMessage, m_securityKey, msgGameSecondFull.len);

		AwaitIncomingGenericFooter();
		m_incomingGameMsg.valid = false;

		// Validate checksum
		const uint32 checksum = GenerateChecksum({
				{ &msgBaseApp, sizeof(msgBaseApp) },
				{ &msgGameMessage, sizeof(msgGameMessage) },
				{ msgGameRaw, sizeof(msgGameRaw) },
				{ msgGameSecondFull.raw, msgGameSecondFull.len }
			});
		if (checksum != msgBaseGeneric.checksum)
			throw std::runtime_error("MsgBaseGeneric: Checksums don't match! Generated: " + std::to_string(checksum));

		// Move the actual message data to the array we're about to return
		CharArray<MessageLen> msgGameSecond;
		msgGameSecond.len = msgLength;
		std::memmove(msgGameSecond.raw, msgGameSecondFull.raw, msgLength);

		return {
			std::move(msgGame),
			std::move(msgGameSecond)
		};
	}
	void AwaitIncomingGenericFooter();

	/* Awaiting messages */
	void AwaitProxyHiMessages();
	void AwaitClientConfig();

	/* Sending utilities */
	template<uint32 Type, typename T>
	void SendGenericMessage(T msgApp, int len = sizeof(T))
	{
		MsgBaseApplication msgBaseApp;
		msgBaseApp.signature = (m_inLobby ? XPProxyProtocolSignature : XPInternalProtocolSignatureApp);
		msgBaseApp.messageType = Type;
		msgBaseApp.dataLength = len;

		MsgBaseGeneric msgBaseGeneric;
		msgBaseGeneric.totalLength = sizeof(MsgBaseGeneric) + sizeof(MsgBaseApplication) + len + sizeof(MsgFooterGeneric);
		msgBaseGeneric.sequenceID = m_sequenceID++;
		msgBaseGeneric.checksum = GenerateChecksum({
				{ &msgBaseApp, sizeof(msgBaseApp) },
				{ &msgApp, len }
			});

		MsgFooterGeneric msgFooterGeneric;
		msgFooterGeneric.status = MsgFooterGeneric::STATUS_OK;

		m_socket.SendData(std::move(msgBaseGeneric), EncryptMessage, m_securityKey);
		m_socket.SendData(std::move(msgBaseApp), EncryptMessage, m_securityKey);
		m_socket.SendData(std::move(msgApp), EncryptMessage, m_securityKey, len);
		m_socket.SendData(msgFooterGeneric);
	}
	template<uint32 Type, typename T>
	void SendGameMessage(T msgGame, int len = sizeof(T))
	{
		assert(m_inLobby);

		MsgBaseApplication msgBaseApp;
		msgBaseApp.signature = XPProxyProtocolSignature;
		msgBaseApp.messageType = MessageGameMessage;
		msgBaseApp.dataLength = sizeof(MsgGameMessage) + len;

		MsgGameMessage msgGameMessage;
		msgGameMessage.type = Type;
		msgGameMessage.length = len;

		// Convert T to network endian so we can properly calculate the checksum.
		T msgGameNetworkEndian = msgGame;
		msgGameNetworkEndian.ConvertToNetworkEndian();

		MsgBaseGeneric msgBaseGeneric;
		msgBaseGeneric.totalLength = sizeof(MsgBaseGeneric) + sizeof(MsgBaseApplication) + sizeof(MsgGameMessage) + len + sizeof(MsgFooterGeneric);
		msgBaseGeneric.sequenceID = m_sequenceID++;
		msgBaseGeneric.checksum = GenerateChecksum({
				{ &msgBaseApp, sizeof(msgBaseApp) },
				{ &msgGameMessage, sizeof(msgGameMessage) },
				{ &msgGameNetworkEndian, len }
			});

		MsgFooterGeneric msgFooterGeneric;
		msgFooterGeneric.status = MsgFooterGeneric::STATUS_OK;

		m_socket.SendData(std::move(msgBaseGeneric), EncryptMessage, m_securityKey);
		m_socket.SendData(std::move(msgBaseApp), EncryptMessage, m_securityKey);
		m_socket.SendData(std::move(msgGameMessage), EncryptMessage, m_securityKey);
		m_socket.SendData(std::move(msgGame), &T::ConvertToNetworkEndian, EncryptMessage, m_securityKey, len);
		m_socket.SendData(msgFooterGeneric);
	}
	template<uint32 Type, typename T, uint16 MessageLen> // Another arbitrary message right after T
	void SendGameMessage(T msgGame, const CharArray<MessageLen>& msgGameSecond)
	{
		assert(m_inLobby);

		MsgBaseApplication msgBaseApp;
		msgBaseApp.signature = XPProxyProtocolSignature;
		msgBaseApp.messageType = MessageGameMessage;
		msgBaseApp.dataLength = sizeof(MsgGameMessage) + sizeof(msgGame) + msgGameSecond.len;

		MsgGameMessage msgGameMessage;
		msgGameMessage.type = Type;
		msgGameMessage.length = sizeof(msgGame) + msgGameSecond.len;

		// Copy the message data to an array which has additional space for padding due to uint32 rounding.
		// Must be in one buffer as to not split DWORD blocks for checksum generation.
		CharArray<MessageLen + sizeof(uint32)> msgGameSecondFull;
		msgGameSecondFull.len = ROUND_DATA_LENGTH_UINT32(msgGameSecond.len);
		std::memcpy(msgGameSecondFull.raw, msgGameSecond.raw, msgGameSecond.len);

		// Convert T to network endian so we can properly calculate the checksum.
		T msgGameNetworkEndian = msgGame;
		msgGameNetworkEndian.ConvertToNetworkEndian();

		MsgBaseGeneric msgBaseGeneric;
		msgBaseGeneric.totalLength = sizeof(MsgBaseGeneric) + sizeof(MsgBaseApplication) + sizeof(MsgGameMessage) + sizeof(msgGame) + msgGameSecondFull.len + sizeof(MsgFooterGeneric);
		msgBaseGeneric.sequenceID = m_sequenceID++;
		msgBaseGeneric.checksum = GenerateChecksum({
				{ &msgBaseApp, sizeof(msgBaseApp) },
				{ &msgGameMessage, sizeof(msgGameMessage) },
				{ &msgGameNetworkEndian, sizeof(msgGameNetworkEndian) },
				{ msgGameSecondFull.raw, msgGameSecondFull.len }
			});

		MsgFooterGeneric msgFooterGeneric;
		msgFooterGeneric.status = MsgFooterGeneric::STATUS_OK;

		m_socket.SendData(std::move(msgBaseGeneric), EncryptMessage, m_securityKey);
		m_socket.SendData(std::move(msgBaseApp), EncryptMessage, m_securityKey);
		m_socket.SendData(std::move(msgGameMessage), EncryptMessage, m_securityKey);
		m_socket.SendData(std::move(msgGame), &T::ConvertToNetworkEndian, EncryptMessage, m_securityKey);
		m_socket.SendData(std::move(msgGameSecondFull), EncryptMessage, m_securityKey, msgGameSecondFull.len);
		m_socket.SendData(msgFooterGeneric);
	}

	/* Sending messages */
	void SendProxyHelloMessages();

private:
	struct IncomingGenericMessage final
	{
		bool valid = false;
		MsgBaseGeneric base;
		MsgBaseApplication info;

		inline uint32 GetType() const { return info.messageType; }
	};
	struct IncomingGameMessage final
	{
		bool valid = false;
		MsgGameMessage info;

		inline uint32 GetType() const { return info.type; }
	};

	struct Config final
	{
		LCID userLanguage = 0;
		LONG offsetUTC = 0;
		Match::SkillLevel skillLevel = Match::SkillLevel::INVALID;
		bool chatEnabled = false;
	};

private:
	State m_state;

	Match::Game m_game;
	GUID m_machineGUID;
	const uint32 m_securityKey;
	uint32 m_sequenceID;
	bool m_inLobby;

	IncomingGenericMessage m_incomingGenericMsg;
	IncomingGameMessage m_incomingGameMsg;

	char m_serviceName[16];
	Config m_config;

	Match* m_match;

public:
	// Variables, set by the match
	const int m_ID;

private:
	PlayerSocket(const PlayerSocket&) = delete;
	PlayerSocket operator=(const PlayerSocket&) = delete;
};

}
