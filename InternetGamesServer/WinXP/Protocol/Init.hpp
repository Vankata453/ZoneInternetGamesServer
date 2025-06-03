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

	MessageConnectionKeepAlive = 0x80000000,
	MessageConnectionPing = 0x80000001,
	MessageConnectionPingResponse = 0x80000002
};

struct MsgBaseInternal
{
	uint32 signature = XPInternalProtocolSignature;
	uint32 totalLength = 0;
	uint16 messageType = 0;
	uint16 intLength = 0;
};

#define BASE_INTERNAL_MESSAGE(CLASS, TYPE) CLASS() { messageType = TYPE; intLength = sizeof(CLASS); totalLength = sizeof(CLASS); }


struct MsgBaseGeneric final : public MsgBaseInternal
{
	BASE_INTERNAL_MESSAGE(MsgBaseGeneric, MessageConnectionGeneric)

	uint32 sequenceID = 0;
	uint32 checksum = 0;
};

struct MsgFooterGeneric final
{
	// Not encrypted
	uint32 status;

	enum Status
	{
		STATUS_CANCELLED = 0,
		STATUS_OK
	};
};

struct MsgBaseApplication final
{
	uint32 signature = XPInternalProtocolSignatureApp;
	uint32 channel = 0;
	uint32 messageType = 0;
	uint32 dataLength = 0;
};

template<size_t Len>
struct MsgApplication final
{
	MsgApplication(uint32 dataLen) :
		data(),
		dataLength(dataLen)
	{}

	char data[Len];
	const uint32 dataLength;
};


struct MsgConnectionHi final : public MsgBaseInternal
{
	BASE_INTERNAL_MESSAGE(MsgConnectionHi, MessageConnectionHi)

	uint32 protocolVersion = 0;
	uint32 productSignature = 0;
	uint32 optionFlagsMask = 0;
	uint32 optionFlags = 0;
	uint32 clientKey = 0;
	GUID machineGUID;
};

struct MsgConnectionHello final : public MsgBaseInternal
{
	BASE_INTERNAL_MESSAGE(MsgConnectionHello, MessageConnectionHello)

	uint32 firstSequenceID = 0;
	uint32 key = 0;
	uint32 optionFlags = 0;
	GUID machineGUID;
};


static std::ostream& operator<<(std::ostream& out, const MsgBaseInternal& m)
{
	return out
		<< "  signature = 0x" << std::hex << m.signature << std::dec
		<< "  totalLength = " << m.totalLength
		<< "  messageType = " << m.messageType
		<< "  intLength = " << m.intLength;
}

static std::ostream& operator<<(std::ostream& out, const MsgBaseGeneric& m)
{
	out << "MsgBaseGeneric:";
	return out << static_cast<const MsgBaseInternal&>(m)
		<< "  sequenceID = " << m.sequenceID
		<< "  checksum = 0x" << std::hex << m.checksum << std::dec;
}

static std::ostream& operator<<(std::ostream& out, const MsgFooterGeneric& m)
{
	out << "MsgFooterGeneric:";
	return out <<
		"  status = " << m.status;
}

static std::ostream& operator<<(std::ostream& out, const MsgBaseApplication& m)
{
	out << "MsgBaseApplication:";
	return out
		<< "  signature = 0x" << std::hex << m.signature << std::dec
		<< "  channel = " << m.channel
		<< "  messageType = " << m.messageType
		<< "  dataLength = " << m.dataLength;
}

template<size_t Len>
static std::ostream& operator<<(std::ostream& out, const MsgApplication<Len>& m)
{
	out << "MsgApplication:"
	    << "  data = ";
	out.write(m.data, m.dataLength);
	return out
		<< "  dataLength = " << m.dataLength;
}

static std::ostream& operator<<(std::ostream& out, const MsgConnectionHi& m)
{
	out << "MsgConnectionHi:";
	return out << static_cast<const MsgBaseInternal&>(m)
		<< "  protocolVersion = " << m.protocolVersion
		<< "  productSignature = 0x" << std::hex << m.productSignature << std::dec
		<< "  optionFlagsMask = 0x" << std::hex << m.optionFlagsMask << std::dec
		<< "  optionFlags = 0x" << std::hex << m.optionFlags << std::dec
		<< "  clientKey = 0x" << std::hex << m.clientKey << std::dec
		<< "  machineGUID = " << m.machineGUID;
}

static std::ostream& operator<<(std::ostream& out, const MsgConnectionHello& m)
{
	out << "MsgConnectionHello:";
	return out << static_cast<const MsgBaseInternal&>(m)
		<< "  firstSequenceID = " << m.firstSequenceID
		<< "  key = 0x" << std::hex << m.key << std::dec
		<< "  optionFlags = 0x" << std::hex << m.optionFlags << std::dec
		<< "  machineGUID = " << m.machineGUID;
}


template<uint16 Type, typename T>
static bool ValidateInternalMessage(const T& msg, uint32 totalLength = sizeof(T))
{
	static_assert(std::is_base_of_v<MsgBaseInternal, T>, "T must inherit MsgBaseInternal!");

	return msg.signature == XPInternalProtocolSignature &&
		msg.totalLength == totalLength &&
		msg.intLength == sizeof(T) &&
		msg.messageType == Type;
}

template<uint16 Type, typename T>
static bool ValidateInternalMessageNoTotalLength(const T& msg)
{
	static_assert(std::is_base_of_v<MsgBaseInternal, T>, "T must inherit MsgBaseInternal!");

	return msg.signature == XPInternalProtocolSignature &&
		msg.intLength == sizeof(T) &&
		msg.messageType == Type;
}

}
