#pragma once

#include <winsock2.h>

#include <string>
#include <vector>

#include "Match.hpp"

class PlayerSocket
{
public:
	enum State {
		STATE_NOT_INITIALIZED,
		STATE_INITIALIZED,
		STATE_JOINING,
		STATE_WAITINGFOROPPONENTS,
		STATE_PLAYING
	};

public:
	PlayerSocket(SOCKET socket);

	std::vector<std::string> GetResponse(const std::vector<std::string>& receivedData);

	/** Event handling */
	void OnGameStart();
	void OnDisconnected();
	void OnEventReceive(const std::string& xml) const;
	void OnChat(const StateChatTag* tag);

	inline State GetState() const { return m_state; }
	inline std::string GetPUID() const { return m_puid; }
	inline Match::Game GetGame() const { return m_game; }
	inline Match::Level GetLevel() const { return m_level; }

private:
	void ParseSasTicket(const std::string& xml);
	void ParseGasTicket(const std::string& xml);
	void ParsePasTicket(const std::string& xml);

	/** Construct protocol messages */
	std::string ConstructJoinContextMessage() const;
	std::string ConstructReadyMessage() const;
	std::string ConstructStateMessage(const std::string& xml) const;

private:
	SOCKET m_socket;
	State m_state;

	std::string m_guid;
	std::string m_puid;
	Match::Game m_game;
	Match::Level m_level;

	Match* m_match;

public:
	// Variables, set by the match
	int m_role;
};
