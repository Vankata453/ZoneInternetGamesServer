#pragma once

#include "../Defines.hpp"

#include <ostream>

#include "../../Util.hpp"

namespace WinXP {

enum
{
	MessageConnectionGeneric = 0,
	MessageConnectionHi,
	MessageConnectionHello,
	MessageConnectionGoodbye,
	MessageConnectionKeepAlive,
	MessageConnectionPing
};

struct MsgBaseInternal
{
	uint32 signature = 0;
	uint32 totalLength = 0;
	uint16 messageType = 0;
	uint16 intLength = 0;
};


struct MsgConnectionHi final : public MsgBaseInternal
{
	uint32 protocolVersion = 0;
	uint32 productSignature = 0;
	uint32 optionFlagsMask = 0;
	uint32 optionFlags = 0;
	uint32 clientKey = 0;
	GUID machineGUID;
};

struct MsgConnectionHello final : public MsgBaseInternal
{
	uint32 firstSequenceID = 0;
	uint32 key = 0;
	uint32 optionFlags = 0;
	GUID machineGUID;
};


static std::ostream& operator<<(std::ostream& out, const MsgBaseInternal& m)
{
	return out
		<< "  signature      = 0x" << std::hex << m.signature << std::dec
		<< "  totalLength    = " << m.totalLength
		<< "  messageType    = " << m.messageType
		<< "  intLength      = " << m.intLength;
}

static std::ostream& operator<<(std::ostream& out, const MsgConnectionHi& m)
{
	out << "MsgConnectionHi:";
	return out << static_cast<const MsgBaseInternal&>(m)
		<< "  protocolVersion  = " << m.protocolVersion
		<< "  productSignature = 0x" << std::hex << m.productSignature << std::dec
		<< "  optionFlagsMask  = 0x" << std::hex << m.optionFlagsMask << std::dec
		<< "  optionFlags      = 0x" << std::hex << m.optionFlags << std::dec
		<< "  clientKey        = 0x" << std::hex << m.clientKey << std::dec
		<< "  machineGUID      = " << m.machineGUID;
}

static std::ostream& operator<<(std::ostream& out, const MsgConnectionHello& m)
{
	out << "MsgConnectionHello:";
	return out << static_cast<const MsgBaseInternal&>(m)
		<< "  firstSequenceID = " << m.firstSequenceID
		<< "  key             = 0x" << std::hex << m.key << std::dec
		<< "  optionFlags     = 0x" << std::hex << m.optionFlags << std::dec
		<< "  machineGUID     = " << m.machineGUID;
}

}
