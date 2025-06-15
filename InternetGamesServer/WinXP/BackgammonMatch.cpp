#include "BackgammonMatch.hpp"

#include "PlayerSocket.hpp"
#include "Protocol/Backgammon.hpp"

namespace WinXP {

#define XPBackgammonProtocolSignature 0x42434B47
#define XPBackgammonProtocolVersion 3
#define XPBackgammonClientVersion 65536


BackgammonMatch::BackgammonMatch(PlayerSocket& player) :
	Match(player)
{}


void
BackgammonMatch::ProcessIncomingGameMessage(PlayerSocket& player, uint32 type)
{
	using namespace Backgammon;

	switch (type)
	{
		case MessageCheckIn:
		{
			const MsgCheckIn msgCheckIn = player.OnMatchAwaitGameMessage<MsgCheckIn, MessageCheckIn>();
			if (msgCheckIn.protocolSignature != XPBackgammonProtocolSignature)
				throw std::runtime_error("Backgammon::MsgCheckIn: Invalid protocol signature!");
			if (msgCheckIn.protocolVersion != XPBackgammonProtocolVersion)
				throw std::runtime_error("Backgammon::MsgCheckIn: Incorrect protocol version!");
			if (msgCheckIn.clientVersion != XPBackgammonClientVersion)
				throw std::runtime_error("Backgammon::MsgCheckIn: Incorrect client version!");
			if (msgCheckIn.playerID != player.GetID())
				throw std::runtime_error("Backgammon::MsgCheckIn: Incorrect player ID!");
			if (msgCheckIn.seat != player.m_seat)
				throw std::runtime_error("Backgammon::MsgCheckIn: Incorrect player seat!");
			if (msgCheckIn.playerType != 1)
				throw std::runtime_error("Backgammon::MsgCheckIn: Incorrect player type!");

			BroadcastGameMessage<MessageCheckIn>(msgCheckIn);
			break;
		}
		case MessageGameTransaction:
		{
			const std::pair<MsgGameTransaction, Array<MsgGameTransaction::Transaction, 8>> msgTransaction =
				player.OnMatchAwaitGameMessage<MsgGameTransaction, MessageGameTransaction, MsgGameTransaction::Transaction, 8>();
			if (msgTransaction.first.userID != player.GetID())
				throw std::runtime_error("MsgGameTransaction: Incorrect user ID!");
			if (msgTransaction.first.seat != player.m_seat)
				throw std::runtime_error("MsgGameTransaction: Incorrect seat!");

			// TODO: Transaction type validation?

			BroadcastGameMessage<MessageGameTransaction>(msgTransaction.first, msgTransaction.second, player.m_seat);
			break;
		}
		case MessageChatMessage:
		{
			const std::pair<MsgChatMessage, Array<char, 128>> msgChat =
				player.OnMatchAwaitGameMessage<MsgChatMessage, MessageChatMessage, char, 128>();
			if (msgChat.first.userID != player.GetID())
				throw std::runtime_error("Backgammon::MsgChatMessage: Incorrect user ID!");

			const WCHAR* chatMsgRaw = reinterpret_cast<const WCHAR*>(msgChat.second.raw);
			const size_t chatMsgLen = msgChat.second.GetLength() / sizeof(WCHAR) - 1;
			if (chatMsgLen <= 1)
				throw std::runtime_error("Backgammon::MsgChatMessage: Empty chat message!");
			if (chatMsgRaw[chatMsgLen - 1] != L'\0')
				throw std::runtime_error("Backgammon::MsgChatMessage: Non-null-terminated chat message!");

			const std::wstring chatMsg(chatMsgRaw, chatMsgLen - 1);
			if (!ValidateCommonChatMessage(chatMsg) &&
				chatMsg != L"/80 Nice move" &&
				chatMsg != L"/81 Nice roll" &&
				chatMsg != L"/82 Not again!")
			{
				throw std::runtime_error("Backgammon::MsgChatMessage: Invalid chat message!");
			}

			BroadcastGameMessage<MessageChatMessage>(msgChat.first, msgChat.second);
			break;
		}
		default:
			throw std::runtime_error("BackgammonMatch::ProcessIncomingGameMessage(): Game message of unknown type received: " + std::to_string(type));
	}
}

}
