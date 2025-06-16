#include "BackgammonMatch.hpp"

#include "PlayerSocket.hpp"
#include "Protocol/Backgammon.hpp"
#include <iostream>
namespace WinXP {

#define XPBackgammonProtocolSignature 0x42434B47
#define XPBackgammonProtocolVersion 3
#define XPBackgammonClientVersion 65536


BackgammonMatch::BackgammonMatch(PlayerSocket& player) :
	Match(player),
	m_matchState(MatchState::INITIALIZING),
	m_playerStates({ MatchPlayerState::AWAITING_CHECKIN, MatchPlayerState::AWAITING_CHECKIN })
{}


void
BackgammonMatch::ProcessIncomingGameMessage(PlayerSocket& player, uint32 type)
{
	using namespace Backgammon;

	switch (m_matchState)
	{
		case MatchState::INITIALIZING:
		{
			switch (m_playerStates[player.m_seat])
			{
				case MatchPlayerState::AWAITING_CHECKIN:
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
					m_playerStates[player.m_seat] = player.m_seat == 0 ? MatchPlayerState::AWAITING_INITIAL_TRANSACTION : MatchPlayerState::WAITING_FOR_MATCH_START;
					return;
				}
				case MatchPlayerState::AWAITING_INITIAL_TRANSACTION:
				{
					assert(player.m_seat == 0);

					const std::pair<MsgStateTransaction, Array<MsgStateTransaction::Transaction, 2>> msgTransaction =
						player.OnMatchAwaitGameMessage<MsgStateTransaction, MessageStateTransaction, MsgStateTransaction::Transaction, 2>();
					if (msgTransaction.first.userID != player.GetID())
						throw std::runtime_error("MsgStateTransaction: Incorrect user ID!");
					if (msgTransaction.first.seat != 0)
						throw std::runtime_error("MsgStateTransaction: Incorrect seat!");

					const auto& transactions = msgTransaction.second;
					if (transactions.GetLength() != 2 ||
						transactions[0].tag != 0 || transactions[1].tag != 8)
					{
						throw std::runtime_error("BackgammonMatch::ProcessIncomingGameMessage(): Invalid initial state transaction!");
					}

					BroadcastGameMessage<MessageStateTransaction>(msgTransaction.first, transactions, 0);
					m_playerStates[0] = MatchPlayerState::AWAITING_MATCH_START;
					return;
				}
				case MatchPlayerState::AWAITING_MATCH_START:
				{
					assert(player.m_seat == 0);
					assert(m_playerStates[1] == MatchPlayerState::WAITING_FOR_MATCH_START);

					player.OnMatchAwaitEmptyGameMessage(MessageNewMatch);

					m_playerStates[0] = MatchPlayerState::PLAYING;
					m_playerStates[1] = MatchPlayerState::PLAYING;
					m_matchState = MatchState::PLAYING;
					return;
				}
			}
			break;
		}
		case MatchState::PLAYING:
		{
			assert(m_playerStates[0] == MatchPlayerState::PLAYING &&
				m_playerStates[1] == MatchPlayerState::PLAYING);

			switch (type)
			{
				case MessageStateTransaction:
				{
					const std::pair<MsgStateTransaction, Array<MsgStateTransaction::Transaction, 8>> msgTransaction =
						player.OnMatchAwaitGameMessage<MsgStateTransaction, MessageStateTransaction, MsgStateTransaction::Transaction, 8>();
					if (msgTransaction.first.userID != player.GetID())
						throw std::runtime_error("MsgStateTransaction: Incorrect user ID!");
					if (msgTransaction.first.seat != player.m_seat)
						throw std::runtime_error("MsgStateTransaction: Incorrect seat!");

					// TODO: Transaction type validation?

					BroadcastGameMessage<MessageStateTransaction>(msgTransaction.first, msgTransaction.second, player.m_seat);
					return;
				}
			}
			break;
		}
	}

	// Miscellaneous messages
	switch (type)
	{
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
