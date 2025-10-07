#include "ReversiMatch.hpp"

#include "PlayerSocket.hpp"
#include "Protocol/Reversi.hpp"

namespace WinXP {

#define XPReversiInvertSeat(seat) (seat == 0 ? 1 : 0)
#define XPReversiIsSeatHost(seat) (seat == 0)

#define XPReversiProtocolSignature 0x72767365
#define XPReversiProtocolVersion 3
#define XPReversiClientVersion 0x00010204

ReversiMatch::ReversiMatch(PlayerSocket& player) :
	Match(player),
	m_matchState(MatchState::AWAITING_START),
	m_playersCheckedIn({ false, false }),
	m_playerTurn(1),
	m_playerResigned(-1)
{}

void
ReversiMatch::Reset()
{
	m_playersCheckedIn = { false, false };
	m_playerTurn = 1;
	m_playerResigned = -1;
}


void
ReversiMatch::ProcessIncomingGameMessage(PlayerSocket& player, uint32 type)
{
	using namespace Reversi;

	switch (m_matchState)
	{
		case MatchState::AWAITING_START:
		{
			if (m_playersCheckedIn[player.m_seat])
				break;

			MsgCheckIn msgCheckIn = player.OnMatchAwaitGameMessage<MsgCheckIn, MessageCheckIn>();
			if (msgCheckIn.protocolSignature != XPReversiProtocolSignature)
				throw std::runtime_error("Reversi::MsgCheckIn: Invalid protocol signature!");
			if (msgCheckIn.protocolVersion != XPReversiProtocolVersion)
				throw std::runtime_error("Reversi::MsgCheckIn: Incorrect protocol version!");
			if (msgCheckIn.clientVersion != XPReversiClientVersion)
				throw std::runtime_error("Reversi::MsgCheckIn: Incorrect client version!");
			// msgCheckIn.playerID should be undefined
			if (msgCheckIn.seat != player.m_seat)
				throw std::runtime_error("Reversi::MsgCheckIn: Incorrect player seat!");

			msgCheckIn.playerID = player.GetID();
			BroadcastGameMessage<MessageCheckIn>(msgCheckIn);

			m_playersCheckedIn[player.m_seat] = true;
			if (m_playersCheckedIn[0] && m_playersCheckedIn[1])
				m_matchState = MatchState::PLAYING;
			return;
		}
		case MatchState::PLAYING:
		{
			switch (type)
			{
				case MessageMovePiece:
				{
					if (m_playerResigned != -1)
						throw std::runtime_error("Reversi::MsgMovePiece: Player has resigned!");
					if (player.m_seat != m_playerTurn)
						throw std::runtime_error("Reversi::MsgMovePiece: Not this player's turn!");

					MsgMovePiece msgMovePiece = player.OnMatchAwaitGameMessage<MsgMovePiece, MessageMovePiece>();
					if (msgMovePiece.seat != player.m_seat)
						throw std::runtime_error("Reversi::MsgMovePiece: Incorrect player seat!");

					m_playerTurn = XPReversiInvertSeat(m_playerTurn);

					BroadcastGameMessage<MessageMovePiece>(msgMovePiece, player.m_seat);
					return;
				}
				case MessageEndGame:
				{
					if (m_playerResigned != -1)
						throw std::runtime_error("Reversi::MsgEndGame: Player has resigned!");

					MsgEndGame msgEndGame = player.OnMatchAwaitGameMessage<MsgEndGame, MessageEndGame>();
					if (msgEndGame.seat != player.m_seat)
						throw std::runtime_error("Reversi::MsgEndGame: Incorrect player seat!");

					if (msgEndGame.flags == MsgEndGame::FLAG_RESIGN)
						m_playerResigned = player.m_seat;

					BroadcastGameMessage<MessageEndGame>(msgEndGame, player.m_seat);
					return;
				}
				case MessageEndMatch:
				{
					if (!XPReversiIsSeatHost(player.m_seat))
						throw std::runtime_error("Reversi::MsgEndMatch: Only the host (seat 0) can send this message!");

					MsgEndMatch msgEndMatch = player.OnMatchAwaitGameMessage<MsgEndMatch, MessageEndMatch>();
					if (msgEndMatch.seatLost < 0 || msgEndMatch.seatLost > 2)
						throw std::runtime_error("Reversi::MsgEndMatch: Invalid lost seat!");
					if (m_playerResigned != -1 && (msgEndMatch.reason != MsgEndMatch::REASON_GAMEOVER || msgEndMatch.seatLost != m_playerResigned))
						throw std::runtime_error("Reversi::MsgEndMatch: Match should have ended in resign of player " + std::to_string(m_playerResigned) + "!");

					m_matchState = MatchState::ENDED;
					m_state = STATE_GAMEOVER;

					Reset();
					return;
				}
			}
			break;
		}
		case MatchState::ENDED:
		{
			MsgCheckIn msgCheckIn = player.OnMatchAwaitGameMessage<MsgCheckIn, MessageCheckIn>();
			if (msgCheckIn.protocolSignature != XPReversiProtocolSignature)
				throw std::runtime_error("Reversi::MsgCheckIn: Invalid protocol signature!");
			if (msgCheckIn.protocolVersion != XPReversiProtocolVersion)
				throw std::runtime_error("Reversi::MsgCheckIn: Incorrect protocol version!");
			// msgCheckIn.playerID should be undefined
			if (msgCheckIn.seat != player.m_seat)
				throw std::runtime_error("Reversi::MsgCheckIn: Incorrect player seat!");

			MsgNewGameVote msgNewGameVote;
			msgNewGameVote.seat = player.m_seat;
			BroadcastGameMessage<MessageNewGameVote>(msgNewGameVote);

			msgCheckIn.playerID = player.GetID();
			BroadcastGameMessage<MessageCheckIn>(msgCheckIn);

			m_playersCheckedIn[player.m_seat] = true;
			if (m_playersCheckedIn[0] && m_playersCheckedIn[1])
			{
				m_matchState = MatchState::PLAYING;
				m_state = STATE_PLAYING;
			}
			return;
		}
	}

	// Miscellaneous messages
	switch (type)
	{
		case MessageChatMessage:
		{
			std::pair<MsgChatMessage, Array<char, 128>> msgChat =
				player.OnMatchAwaitGameMessage<MsgChatMessage, MessageChatMessage, char, 128>();
			if (msgChat.first.seat != player.m_seat)
				throw std::runtime_error("Reversi::MsgChatMessage: Incorrect player seat!");

			// userID may be 0, if chat message was sent before client check-in completed
			if (msgChat.first.userID == 0)
				msgChat.first.userID = player.GetID();
			else if (msgChat.first.userID != player.GetID())
				throw std::runtime_error("Reversi::MsgChatMessage: Incorrect user ID!");

			const WCHAR* chatMsgRaw = reinterpret_cast<const WCHAR*>(msgChat.second.raw);
			const size_t chatMsgLen = msgChat.second.GetLength() / sizeof(WCHAR) - 1;
			if (chatMsgLen <= 1)
				throw std::runtime_error("Reversi::MsgChatMessage: Empty chat message!");
			if (chatMsgRaw[chatMsgLen - 1] != L'\0')
				throw std::runtime_error("Reversi::MsgChatMessage: Non-null-terminated chat message!");

			const std::wstring chatMsg(chatMsgRaw, chatMsgLen - 1);
			if (!ValidateCommonChatMessage(chatMsg) &&
				chatMsg != L"/70 Nice move" &&
				chatMsg != L"/71 Didn't see that coming!" &&
				chatMsg != L"/72 I got the corner!" &&
				chatMsg != L"/73 Nice comeback")
			{
				throw std::runtime_error("Reversi::MsgChatMessage: Invalid chat message!");
			}

			BroadcastGameMessage<MessageChatMessage>(msgChat.first, msgChat.second);
			break;
		}
		default:
			throw std::runtime_error("ReversiMatch::ProcessIncomingGameMessage(): Game message of unknown type received: " + std::to_string(type));
	}
}

}
