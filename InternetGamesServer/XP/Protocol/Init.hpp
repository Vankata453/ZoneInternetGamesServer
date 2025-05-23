#include "../Defines.hpp"

#include <guiddef.h>

namespace XP {

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

}
