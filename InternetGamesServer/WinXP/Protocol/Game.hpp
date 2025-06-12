#pragma once

#include "../Defines.hpp"

#include <ostream>

namespace WinXP {

enum
{
	MessageGameMessage = 9,

	MessageUserInfoResponse = 23,

	MessageClientConfig = 30,
	MessageServerStatus,
	MessageGameStart,
	MessageChatSwitch,
	MessagePlayerReplaced
};


struct MsgClientConfig final
{
	uint32 protocolSignature = 0;
	uint32 protocolVersion = 0;
	char config[256];
};

struct MsgServerStatus final
{
	uint32 status = 0;
	uint32 playersWaiting = 0;
};


struct MsgUserInfoResponse final
{
	uint32 ID = 0;
	char username[32];
	LCID language = 0;
};

struct MsgGameStart final
{
	uint32 gameID = 0;
	int16 table = 0;
	int16 seat = 0;
	int16 totalSeats = 0;

	struct User final
	{
		uint32 ID = 0;
		LCID language = 0;
		bool chatEnabled = false;
		int16 skill = 0;
	};
	User users[XPMaxMatchPlayers];
};


struct MsgGameMessage final
{
	uint32 gameID = 0;
	uint32 type = 0;
	uint16 length = 0;
	int16 _unused = 0;
};

struct MsgChatSwitch final
{
	uint32 userID = 0;
	bool chatEnabled = false;
};


static std::ostream& operator<<(std::ostream& out, const MsgClientConfig& m)
{
	out << "MsgClientConfig:";
	return out
		<< "  protocolSignature = 0x" << std::hex << m.protocolSignature << std::dec
		<< "  protocolVersion = " << m.protocolVersion
		<< "  config = " << m.config;
}

static std::ostream& operator<<(std::ostream& out, const MsgServerStatus& m)
{
	out << "MsgServerStatus:";
	return out
		<< "  status = " << m.status
		<< "  playersWaiting = " << m.playersWaiting;
}

static std::ostream& operator<<(std::ostream& out, const MsgUserInfoResponse& m)
{
	out << "MsgUserInfoResponse:";
	return out
		<< "  ID = " << m.ID
		<< "  username = " << m.username
		<< "  language = " << m.language;
}

static std::ostream& operator<<(std::ostream& out, const MsgGameStart& m)
{
	out << "MsgGameStart:"
		<< "  gameID = " << m.gameID
		<< "  table = " << m.table
		<< "  seat = " << m.seat
		<< "  totalSeats = " << m.totalSeats;
	for (int16 i = 0; i < m.totalSeats; ++i)
	{
		out << std::endl
			<< "user " << i << ':'
			<< "  ID = " << m.users[i].ID
			<< "  language = " << m.users[i].language
			<< "  chatEnabled = " << m.users[i].chatEnabled
			<< "  skill = " << m.users[i].skill;
	}
	return out;
}

static std::ostream& operator<<(std::ostream& out, const MsgGameMessage& m)
{
	out << "MsgGameMessage:";
	return out
		<< "  gameID = " << m.gameID
		<< "  type = " << m.type
		<< "  length = " << m.length;
}

static std::ostream& operator<<(std::ostream& out, const MsgChatSwitch& m)
{
	out << "MsgChatSwitch:";
	return out
		<< "  userID = " << m.userID
		<< "  chatEnabled = " << m.chatEnabled;
}

}
