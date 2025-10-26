#pragma once

#include "../Defines.hpp"
#include "../Endian.hpp"

#include <ostream>

namespace WinXP {

namespace Backgammon {

#define XPBackgammonProtocolSignature 0x42434B47
#define XPBackgammonProtocolVersion 3
#define XPBackgammonClientVersion 65536

enum
{
	MessageChatMessage = 256,
	MessageStateTransaction,
	MessageDiceRollRequest = 261,
	MessageDiceRollResponse,
	MessageEndMatch,
	MessageNewMatch,
	MessageFirstMove,
	MessageEndTurn = 267,
	MessageEndGame,
	MessageFirstRoll,
	MessageTieRoll,
	MessageCheckIn = 1024
};

enum SharedStateTransactions
{
	TransactionStateChange = 0,
	TransactionInitialSettings,
	TransactionDice,
	TransactionBoard = 4,
	TransactionAcceptDouble,
	TransactionNoMoreLegalMoves = 11,
	TransactionReadyForNewMatch
};

enum SharedState
{
	SharedState = 0,
	SharedGameOverReason = 6,
	SharedActiveSeat = 8,
	SharedAutoDouble,
	SharedHostBrown,
	SharedTargetScore,
	SharedSettingsDone,
	SharedDoubleCubeValue,
	SharedDoubleCubeOwner,
	SharedResignPoints,
	SharedDice = 19,
	SharedDiceSize,
	SharedReady,
	SharedPieces
};

enum State
{
	StateGameSettings = 4
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


struct DiceInfo final
{
	int16 val = 0;
	int32 encodedVal = 0;
	int16 encoderMul = 0;
	int16 encoderAdd = 0;
	int32 numUses = 0;

	void ConvertToHostEndian()
	{
		HOST_ENDIAN_SHORT(val)
		HOST_ENDIAN_LONG(encodedVal)
		HOST_ENDIAN_SHORT(encoderMul)
		HOST_ENDIAN_SHORT(encoderAdd)
		HOST_ENDIAN_LONG(numUses)
	}
	void ConvertToNetworkEndian()
	{
		NETWORK_ENDIAN_SHORT(val)
		NETWORK_ENDIAN_LONG(encodedVal)
		NETWORK_ENDIAN_SHORT(encoderMul)
		NETWORK_ENDIAN_SHORT(encoderAdd)
		NETWORK_ENDIAN_LONG(numUses)
	}
};

static std::ostream& operator<<(std::ostream& out, const DiceInfo& r)
{
	out << r.val;
	if (!r.encodedVal && !r.encoderMul && !r.encoderAdd && !r.numUses)
		return out;
	return out << " (" << r.encodedVal << ", " << r.encoderMul << ", " << r.encoderAdd << ", " << r.numUses << ')';
}


struct MsgDiceRollRequest final
{
	int16 seat = 0;

	void ConvertToHostEndian()
	{
		HOST_ENDIAN_SHORT(seat)
	}
	void ConvertToNetworkEndian()
	{
		NETWORK_ENDIAN_SHORT(seat)
	}

	STRUCT_PADDING(2)
};

struct MsgDiceRollResponse final
{
	int16 seat = 0;
	DiceInfo dice1;
	DiceInfo dice2;

	void ConvertToHostEndian()
	{
		HOST_ENDIAN_SHORT(seat)
		dice1.ConvertToHostEndian();
		dice2.ConvertToHostEndian();
	}
	void ConvertToNetworkEndian()
	{
		NETWORK_ENDIAN_SHORT(seat)
		dice1.ConvertToNetworkEndian();
		dice2.ConvertToNetworkEndian();
	}
};

struct MsgFirstMove final
{
	int32 numPoints = 0;
	int32 rollWinnerSeat = 0;

	void ConvertToHostEndian()
	{
		HOST_ENDIAN_LONG(numPoints)
	}
	void ConvertToNetworkEndian()
	{
		NETWORK_ENDIAN_LONG(numPoints)
	}
};

struct MsgEndTurn final
{
	int16 seat = 0;

	void ConvertToHostEndian() {}
	void ConvertToNetworkEndian() {}

	STRUCT_PADDING(2)
};

struct MsgEndMatch final
{
	int32 numPoints = 0;
	int16 reason = 0;
	int16 seatLost = 0;
	int16 seatQuit = 0;

	enum Reason
	{
		REASON_TIMEOUT = 1,
		REASON_FORFEIT,
		REASON_GAMEOVER
	};

	void ConvertToHostEndian() {}
	void ConvertToNetworkEndian() {}
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

static std::ostream& operator<<(std::ostream& out, const MsgDiceRollRequest& m)
{
	out << "Backgammon::MsgDiceRollRequest:";
	return out
		<< "  seat = " << m.seat;
}

static std::ostream& operator<<(std::ostream& out, const MsgDiceRollResponse& m)
{
	out << "Backgammon::MsgDiceRollResponse:";
	return out
		<< "  seat = " << m.seat
		<< "  dice1 = " << m.dice1
		<< "  dice2 = " << m.dice2;
}

static std::ostream& operator<<(std::ostream& out, const MsgFirstMove& m)
{
	out << "Backgammon::MsgFirstMove:";
	return out
		<< "  numPoints = " << m.numPoints
		<< "  rollWinnerSeat = " << m.rollWinnerSeat;
}

static std::ostream& operator<<(std::ostream& out, const MsgEndTurn& m)
{
	out << "Backgammon::MsgEndTurn:";
	return out
		<< "  seat = " << m.seat;
}

static std::ostream& operator<<(std::ostream& out, const MsgEndMatch& m)
{
	out << "Backgammon::MsgEndMatch:";
	return out
		<< "  numPoints = " << m.numPoints
		<< "  reason = " << m.reason
		<< "  seatLost = " << m.seatLost
		<< "  seatQuit = " << m.seatQuit;
}

}

}
