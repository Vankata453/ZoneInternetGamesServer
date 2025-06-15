#pragma once

#include "../Defines.hpp"
#include "../Endian.hpp"

#include <ostream>

namespace WinXP {

namespace Backgammon {

enum
{
	MessageChatMessage = 256,
	MessageGameTransaction,
	MessageCheckIn = 1024
};


struct MsgCheckIn final
{
	uint32 protocolSignature = 0;
	uint32 protocolVersion = 0;
	uint32 clientVersion = 0;
	uint32 playerID = 0;
	int16 seat = 0;
	int16 playerType = 0;

	void ConvertToHostEndian()
	{
		HOST_ENDIAN_LONG(protocolSignature)
		HOST_ENDIAN_LONG(protocolVersion)
		HOST_ENDIAN_LONG(clientVersion)
		HOST_ENDIAN_LONG(playerID)
		HOST_ENDIAN_SHORT(seat)
		HOST_ENDIAN_SHORT(playerType)
	}
	void ConvertToNetworkEndian()
	{
		NETWORK_ENDIAN_LONG(protocolSignature)
		NETWORK_ENDIAN_LONG(protocolVersion)
		NETWORK_ENDIAN_LONG(clientVersion)
		NETWORK_ENDIAN_LONG(playerID)
		NETWORK_ENDIAN_SHORT(seat)
		NETWORK_ENDIAN_SHORT(playerType)
	}
};

struct MsgChatMessage final
{
	uint32 userID = 0;
	int16 seat = 0;
	uint16 messageLength = 0;
	// char[]

	void ConvertToHostEndian()
	{
		HOST_ENDIAN_LONG(userID)
		HOST_ENDIAN_SHORT(seat)
		HOST_ENDIAN_SHORT(messageLength)
	}
	void ConvertToNetworkEndian()
	{
		NETWORK_ENDIAN_LONG(userID)
		NETWORK_ENDIAN_SHORT(seat)
		NETWORK_ENDIAN_SHORT(messageLength)
	}
};


static std::ostream& operator<<(std::ostream& out, const MsgCheckIn& m)
{
	out << "Backgammon::MsgCheckIn:";
	return out
		<< "  protocolSignature = 0x" << std::hex << m.protocolSignature << std::dec
		<< "  protocolVersion = " << m.protocolVersion
		<< "  clientVersion = " << m.clientVersion
		<< "  playerID = " << m.playerID
		<< "  seat = " << m.seat
		<< "  playerType = " << m.playerType;
}

static std::ostream& operator<<(std::ostream& out, const MsgChatMessage& m)
{
	out << "Backgammon::MsgChatMessage:";
	return out
		<< "  userID = " << m.userID
		<< "  seat = " << m.seat
		<< "  messageLength = " << m.messageLength;
}

}

}
