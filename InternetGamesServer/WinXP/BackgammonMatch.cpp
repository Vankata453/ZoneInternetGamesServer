#include "BackgammonMatch.hpp"

#include "PlayerSocket.hpp"
#include "Protocol/Backgammon.hpp"

namespace WinXP {

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
			player.OnMatchAwaitGameMessage<MsgCheckIn, MessageCheckIn>(); // TODO
			break;
		}
		case MessageChatMessage:
		{
			const std::pair<MsgChatMessage, CharArray<128>> msgChat = player.OnMatchAwaitGameMessage<MsgChatMessage, MessageChatMessage, 128>();
			if (msgChat.first.userID != player.m_ID)
				throw std::runtime_error("Backgammon::MsgChatMessage: Incorrect user ID!");

			const WCHAR* chatMsgRaw = reinterpret_cast<const WCHAR*>(msgChat.second.raw);
			const int chatMsgLen = msgChat.second.len / sizeof(WCHAR) - 1;
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
