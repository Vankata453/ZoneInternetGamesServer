#pragma once

#include "../Defines.hpp"

#include <ostream>

#include "../../Socket.hpp"

namespace WinXP {

enum
{
	MessageProxyHi = 0,
	MessageProxyHello,
	MessageProxyGoodbye,
	MessageProxyWrongVersion,
	MessageProxyServiceRequest,
	MessageProxyServiceInfo,
	MessageProxyID,
	MessageProxySettings
};

struct MsgBaseProxy
{
	uint16 messageType = 0;
	uint16 length = 0;
};

#define BASE_PROXY_MESSAGE(CLASS, TYPE) CLASS() { messageType = TYPE; length = sizeof(CLASS); }


struct MsgProxyHi final : public MsgBaseProxy
{
	BASE_PROXY_MESSAGE(MsgProxyHi, MessageProxyHi)

	uint32 protocolVersion = 0;
	char gameToken[64];
	uint32 clientVersion = 0;
};

struct MsgProxyID final : public MsgBaseProxy
{
	BASE_PROXY_MESSAGE(MsgProxyID, MessageProxyID)

	LANGID systemLanguage = 0;
	LANGID userLanguage = 0;
	LANGID appLanguage = 0;
	int16 timeZoneMinutes = 0;
};

struct MsgProxyServiceRequest final : public MsgBaseProxy
{
	BASE_PROXY_MESSAGE(MsgProxyServiceRequest, MessageProxyServiceRequest)

	uint32 reason = 0;
	char serviceName[16];
	uint32 channel = 0;
};

namespace Util {

struct MsgProxyHiCollection final
{
	MsgProxyHi hi;
	MsgProxyID ID;
	MsgProxyServiceRequest serviceRequest;
};

}


struct MsgProxyHello final : public MsgBaseProxy
{
	BASE_PROXY_MESSAGE(MsgProxyHello, MessageProxyHello)
};

struct MsgProxySettings final : public MsgBaseProxy
{
	BASE_PROXY_MESSAGE(MsgProxySettings, MessageProxySettings)

	uint16 chat = 0;
	uint16 statistics = 0;

	enum Chat
	{
		CHAT_FULL = 1,
		CHAT_RESTRICTED,
		CHAT_NONE
	};
	enum Statistics
	{
		STATS_ALL = 1,
		STATS_MOST,
		STATS_MINIMAL
	};
};

struct MsgProxyServiceInfo final : public MsgBaseProxy
{
	BASE_PROXY_MESSAGE(MsgProxyServiceInfo, MessageProxyServiceInfo)

	uint32 reason = 0;
	char serviceName[16];
	uint32 channel = 0;
	uint32 flags = 0;
	union
	{
		uint32 minutesRemaining;
		uint32 minutesDowntime;
		struct
		{
			IN_ADDR ip;
			uint16 port;
		} address;
	};

	enum Reason
	{
		SERVICE_INFO = 0,
		SERVICE_CONNECT,
		SERVICE_DISCONNECT,
		SERVICE_STOP_,
		SERVICE_BROADCAST
	};
	enum Flags
	{
		SERVICE_AVAILABLE = 0x01,
		SERVICE_LOCAL = 0x02,
		SERVICE_CONNECTED = 0x04,
		SERVICE_STOPPING = 0x08
	};
};

namespace Util {

struct MsgProxyHelloCollection final
{
	MsgProxyHello hello;
	MsgProxySettings settings;
	MsgProxyServiceInfo serviceInfo;
};

}


static std::ostream& operator<<(std::ostream& out, const MsgBaseProxy& m)
{
	return out
		<< "  messageType = " << m.messageType
		<< "  length = " << m.length;
}

static std::ostream& operator<<(std::ostream& out, const MsgProxyHi& m)
{
	out << "MsgProxyHi:";
	return out << static_cast<const MsgBaseProxy&>(m)
		<< "  protocolVersion = " << m.protocolVersion
		<< "  setupToken = " << m.gameToken
		<< "  clientVersion = 0x" << std::hex << m.clientVersion << std::dec;
}

static std::ostream& operator<<(std::ostream& out, const MsgProxyServiceRequest& m)
{
	out << "MsgProxyServiceRequest:";
	return out << static_cast<const MsgBaseProxy&>(m)
		<< "  reason = " << m.reason
		<< "  serviceName = " << m.serviceName
		<< "  channel = " << m.channel;
}

static std::ostream& operator<<(std::ostream& out, const MsgProxyID& m)
{
	out << "MsgProxyID:";
	return out << static_cast<const MsgBaseProxy&>(m)
		<< "  systemLanguage = " << m.systemLanguage
		<< "  userLanguage = " << m.userLanguage
		<< "  appLanguage = " << m.appLanguage
		<< "  timeZoneMinutes = " << m.timeZoneMinutes;
}

namespace Util {

static std::ostream& operator<<(std::ostream& out, const MsgProxyHiCollection& m)
{
	return out
		<< m.hi << std::endl
		<< m.ID << std::endl
		<< m.serviceRequest;
}

}

static std::ostream& operator<<(std::ostream& out, const MsgProxyHello& m)
{
	out << "MsgProxyHello:";
	return out << static_cast<const MsgBaseProxy&>(m);
}

static std::ostream& operator<<(std::ostream& out, const MsgProxySettings& m)
{
	out << "MsgProxySettings:";
	return out << static_cast<const MsgBaseProxy&>(m)
			<< "  chat = " << m.chat
			<< "  statistics = " << m.statistics;
}

static std::ostream& operator<<(std::ostream& out, const MsgProxyServiceInfo& m)
{
	out << "MsgProxyServiceInfo:"
		<< static_cast<const MsgBaseProxy&>(m)
		<< "  reason = " << m.reason
		<< "  serviceName = " << m.serviceName
		<< "  channel = " << m.channel
		<< "  flags = " << std::hex << m.flags << std::dec;

	if (m.flags == MsgProxyServiceInfo::SERVICE_AVAILABLE)
		return out << "  address = " << Socket::GetAddressString(m.address.ip) << ':' << m.address.port;
	if (!(m.flags & MsgProxyServiceInfo::SERVICE_AVAILABLE))
		return out << "  minutesDowntime = " << m.minutesDowntime;
	return out << "  minutesRemaining = " << m.minutesRemaining;
}

namespace Util {

static std::ostream& operator<<(std::ostream& out, const MsgProxyHelloCollection& m)
{
	return out
		<< m.hello << std::endl
		<< m.settings << std::endl
		<< m.serviceInfo;
}

}


template<uint16 Type, typename T>
static bool ValidateProxyMessage(const T& msg)
{
	static_assert(std::is_base_of_v<MsgBaseProxy, T>, "T must inherit MsgBaseProxy!");

	return msg.messageType == Type &&
		msg.length == sizeof(T);
}

}
