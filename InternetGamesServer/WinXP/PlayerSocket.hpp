#pragma once

#include "../PlayerSocket.hpp"

#include "Defines.hpp"
#include "Match.hpp"
#include "Protocol/Init.hpp"

namespace WinXP {

class PlayerSocket final : public ::PlayerSocket
{
public:
	enum State {
		STATE_INITIALIZED,
		STATE_JOINING,
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
	State m_state;

	Match::Game m_game;
	GUID m_machineGUID;
	uint32 m_securityKey;

	Match* m_match;

private:
	PlayerSocket(const PlayerSocket&) = delete;
	PlayerSocket operator=(const PlayerSocket&) = delete;
};

}
