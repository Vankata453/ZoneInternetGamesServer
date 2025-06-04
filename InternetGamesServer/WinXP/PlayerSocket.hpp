#pragma once

#include "../PlayerSocket.hpp"

#include "Defines.hpp"
#include "Match.hpp"
#include "Protocol/Init.hpp"
#include "Security.hpp"

namespace WinXP {

class PlayerSocket final : public ::PlayerSocket
{
public:
	enum State {
		STATE_INITIALIZED,
		STATE_WAITINGFOROPPONENTS,
		STATE_PLAYING
	};

public:
	PlayerSocket(Socket& socket, const MsgConnectionHi& hiMessage);
	~PlayerSocket() override;

	void ProcessMessages() override;

	/** Event handling */
	void OnGameStart();
	void OnDisconnected();

	Socket::Type GetType() const override { return Socket::WIN7; }
	inline State GetState() const { return m_state; }
	inline Match::Game GetGame() const { return m_game; }
	inline uint32 GetSecurityKey() const { return m_securityKey; }

	MsgConnectionHello ConstructHelloMessage() const;

private:
	/* Awaiting utilities */
	template<typename T, uint32 Type>
	T AwaitGenericMessage()
	{
		MsgBaseGeneric msgBaseGeneric;
		MsgBaseApplication msgBaseApp;
		while (true)
		{
			m_socket.ReceiveData(msgBaseGeneric, DecryptMessage, m_securityKey);
			if (!ValidateInternalMessageNoTotalLength<MessageConnectionGeneric>(msgBaseGeneric))
				throw std::runtime_error("MsgBaseGeneric: Message is invalid!");

			// Wait for a message which is not keep alive
			m_socket.ReceiveData(msgBaseApp, DecryptMessage, m_securityKey);
			if (msgBaseApp.messageType != MessageConnectionKeepAlive)
				break;

			// TODO: Handle ping messages

			MsgFooterGeneric dummyFooter;
			m_socket.ReceiveData(dummyFooter);
		}

		if (msgBaseGeneric.totalLength != sizeof(MsgBaseGeneric) + sizeof(MsgBaseApplication) + msgBaseApp.dataLength + sizeof(MsgFooterGeneric))
			throw std::runtime_error("MsgBaseGeneric: totalLength is invalid!");

		if (msgBaseApp.signature != XPInternalProtocolSignatureApp || msgBaseApp.messageType != Type)
			throw std::runtime_error("MsgBaseApplication: Message is invalid!");
		if (msgBaseApp.dataLength != sizeof(T))
			throw std::runtime_error("MsgBaseApplication: Data is of incorrect size! Expected: " + std::to_string(sizeof(T)));

		T msgApp;
		m_socket.ReceiveData(msgApp, DecryptMessage, m_securityKey, sizeof(T));

		MsgFooterGeneric msgFooterGeneric;
		m_socket.ReceiveData(msgFooterGeneric);
		if (msgFooterGeneric.status == MsgFooterGeneric::STATUS_CANCELLED)
			throw std::runtime_error("MsgFooterGeneric: Status is CANCELLED!");
		else if (msgFooterGeneric.status != MsgFooterGeneric::STATUS_OK)
			throw std::runtime_error("MsgFooterGeneric: Status is not OK! - " + std::to_string(msgFooterGeneric.status));

		// Validate checksum
		const uint32 checksum = GenerateChecksum({
				{ &msgBaseApp, sizeof(msgBaseApp) },
				{ &msgApp, sizeof(T) }
			});
		if (checksum != msgBaseGeneric.checksum)
			throw std::runtime_error("MsgBaseGeneric: Checksums don't match! Generated: " + std::to_string(checksum));

		return msgApp;
	}

	/* Awaiting messages */
	void AwaitProxyHiMessages();

	/* Sending utilities */
	template<uint32 Type, typename T>
	void SendGenericMessage(T data, int len = sizeof(T))
	{
		MsgBaseApplication msgBaseApp;
		msgBaseApp.messageType = Type;
		msgBaseApp.dataLength = len;

		MsgBaseGeneric msgBaseGeneric;
		msgBaseGeneric.totalLength = sizeof(MsgBaseGeneric) + sizeof(MsgBaseApplication) + len + sizeof(MsgFooterGeneric);
		msgBaseGeneric.sequenceID = m_sequenceID++;
		msgBaseGeneric.checksum = GenerateChecksum({
				{ &msgBaseApp, sizeof(msgBaseApp) },
				{ &data, len }
			});

		MsgFooterGeneric msgFooterGeneric;
		msgFooterGeneric.status = MsgFooterGeneric::STATUS_OK;

		m_socket.SendData(msgBaseGeneric, EncryptMessage, m_securityKey);
		m_socket.SendData(msgBaseApp, EncryptMessage, m_securityKey);
		m_socket.SendData(data, EncryptMessage, m_securityKey, len);
		m_socket.SendData(msgFooterGeneric);
	}

	/* Sending messages */
	void SendProxyHelloMessages();

private:
	State m_state;

	Match::Game m_game;
	GUID m_machineGUID;
	uint32 m_securityKey;
	uint32 m_sequenceID;

	char m_serviceName[16];

	Match* m_match;

private:
	PlayerSocket(const PlayerSocket&) = delete;
	PlayerSocket operator=(const PlayerSocket&) = delete;
};

}
