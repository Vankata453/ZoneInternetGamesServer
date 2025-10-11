#include "CheckersMatch.hpp"

#include "PlayerSocket.hpp"

namespace WinXP {

#define XPCheckersInvertSeat(seat) (seat == 0 ? 1 : 0)
#define XPCheckersIsSeatHost(seat) (seat == 0)

#define XPCheckersProtocolSignature 0x43484B52
#define XPCheckersProtocolVersion 2
#define XPCheckersClientVersion 66050

CheckersMatch::CheckersMatch(PlayerSocket& player) :
	Match(player),
	m_matchState(MatchState::AWAITING_START),
	m_playersCheckedIn({ false, false }),
	m_playerCheckInMsgs(),
	m_playerTurn(0),
	m_drawOfferedBy(-1),
	m_drawAccepted(false),
	m_playerResigned(-1)
{}

void
CheckersMatch::Reset()
{
	m_playersCheckedIn = { false, false };
	m_playerCheckInMsgs = {};
	m_playerTurn = 0;
	m_drawOfferedBy = -1;
	m_drawAccepted = false;
	m_playerResigned = -1;
}


void
CheckersMatch::ProcessIncomingGameMessage(PlayerSocket& player, uint32 type)
{
	using namespace Checkers;

	switch (m_matchState)
	{
		case MatchState::AWAITING_START:
		{
			if (m_playersCheckedIn[player.m_seat])
				break;

			MsgCheckIn msgCheckIn = player.OnMatchAwaitGameMessage<MsgCheckIn, MessageCheckIn>();
			if (msgCheckIn.protocolSignature != XPCheckersProtocolSignature)
				throw std::runtime_error("Checkers::MsgCheckIn: Invalid protocol signature!");
			if (msgCheckIn.protocolVersion != XPCheckersProtocolVersion)
				throw std::runtime_error("Checkers::MsgCheckIn: Incorrect protocol version!");
			if (msgCheckIn.clientVersion != XPCheckersClientVersion)
				throw std::runtime_error("Checkers::MsgCheckIn: Incorrect client version!");
			// msgCheckIn.playerID should be undefined
			if (msgCheckIn.seat != player.m_seat)
				throw std::runtime_error("Checkers::MsgCheckIn: Incorrect player seat!");

			msgCheckIn.playerID = player.GetID();

			m_playersCheckedIn[player.m_seat] = true;
			m_playerCheckInMsgs[player.m_seat] = std::move(msgCheckIn);
			if (m_playersCheckedIn[0] && m_playersCheckedIn[1])
			{
				for (const MsgCheckIn& msg : m_playerCheckInMsgs)
					BroadcastGameMessage<MessageCheckIn>(msg);
				m_playerCheckInMsgs = {};
				m_matchState = MatchState::PLAYING;
			}
			return;
		}
		case MatchState::PLAYING:
		{
			switch (type)
			{
				case MessageMovePiece:
				{
					if (m_drawAccepted)
						throw std::runtime_error("Checkers::MsgMovePiece: Draw was accepted!");
					if (m_drawOfferedBy != -1)
						throw std::runtime_error("Checkers::MsgMovePiece: Draw was offered!");
					if (m_playerResigned != -1)
						throw std::runtime_error("Checkers::MsgMovePiece: Player has resigned!");
					if (player.m_seat != m_playerTurn)
						throw std::runtime_error("Checkers::MsgMovePiece: Not this player's turn!");

					MsgMovePiece msgMovePiece = player.OnMatchAwaitGameMessage<MsgMovePiece, MessageMovePiece>();
					if (msgMovePiece.seat != player.m_seat)
						throw std::runtime_error("Checkers::MsgMovePiece: Incorrect player seat!");

					BroadcastGameMessage<MessageMovePiece>(msgMovePiece, player.m_seat);
					return;
				}
				case MessageFinishMove:
				{
					if (m_drawAccepted)
						throw std::runtime_error("Checkers::MsgFinishMove: Draw was accepted!");
					if (m_playerResigned != -1)
						throw std::runtime_error("Checkers::MsgFinishMove: Player has resigned!");

					MsgFinishMove msgFinishMove = player.OnMatchAwaitGameMessage<MsgFinishMove, MessageFinishMove>();
					if (msgFinishMove.seat != player.m_seat)
						throw std::runtime_error("Checkers::MsgFinishMove: Incorrect player seat!");
					if (msgFinishMove.drawSeat != -1)
					{
						if (m_drawOfferedBy != -1)
							throw std::runtime_error("Checkers::MsgFinishMove: Draw already offered!");
						if (msgFinishMove.drawSeat != player.m_seat)
							throw std::runtime_error("Checkers::MsgFinishMove: Incorrect draw player seat!");
						
						m_drawOfferedBy = player.m_seat;
					}

					m_playerTurn = XPCheckersInvertSeat(m_playerTurn);

					BroadcastGameMessage<MessageFinishMove>(msgFinishMove, player.m_seat);
					return;
				}
				case MessageDrawResponse:
				{
					if (m_drawAccepted)
						throw std::runtime_error("Checkers::MsgDrawResponse: Draw was accepted!");
					if (m_drawOfferedBy == -1)
						throw std::runtime_error("Checkers::MsgDrawResponse: No draw was offered!");
					if (m_playerResigned != -1)
						throw std::runtime_error("Checkers::MsgDrawResponse: Player has resigned!");

					MsgDrawResponse msgDrawResponse = player.OnMatchAwaitGameMessage<MsgDrawResponse, MessageDrawResponse>();
					if (msgDrawResponse.seat != player.m_seat)
						throw std::runtime_error("Checkers::MsgDrawResponse: Incorrect player seat!");
					if (msgDrawResponse.seat != XPCheckersInvertSeat(m_drawOfferedBy))
						throw std::runtime_error("Checkers::MsgDrawResponse: Draw was not requested from this player!");

					if (msgDrawResponse.response == MsgDrawResponse::RESPONSE_ACCEPT)
						m_drawAccepted = true;
					else if (msgDrawResponse.response == MsgDrawResponse::RESPONSE_REJECT)
						m_drawOfferedBy = -1;
					else
						throw std::runtime_error("Checkers::MsgDrawResponse: Invalid response!");

					BroadcastGameMessage<MessageDrawResponse>(msgDrawResponse);
					return;
				}
				case MessageEndGame:
				{
					if (m_playerResigned != -1)
						throw std::runtime_error("Checkers::MsgEndGame: Player has resigned!");

					MsgEndGame msgEndGame = player.OnMatchAwaitGameMessage<MsgEndGame, MessageEndGame>();
					if (msgEndGame.seat != player.m_seat)
						throw std::runtime_error("Checkers::MsgEndGame: Incorrect player seat!");
					if (m_drawAccepted && msgEndGame.flags != MsgEndGame::FLAG_DRAW)
						throw std::runtime_error("Checkers::MsgEndGame: Match should have ended in draw!");

					if (msgEndGame.flags == MsgEndGame::FLAG_RESIGN)
						m_playerResigned = player.m_seat;

					BroadcastGameMessage<MessageEndGame>(msgEndGame, player.m_seat);
					return;
				}
				case MessageEndMatch:
				{
					if (!XPCheckersIsSeatHost(player.m_seat))
						throw std::runtime_error("Checkers::MsgEndMatch: Only the host (seat 0) can send this message!");

					MsgEndMatch msgEndMatch = player.OnMatchAwaitGameMessage<MsgEndMatch, MessageEndMatch>();
					if (msgEndMatch.seatLost < 0 || msgEndMatch.seatLost > 2)
						throw std::runtime_error("Checkers::MsgEndMatch: Invalid lost seat!");
					if (m_drawAccepted && (msgEndMatch.reason != MsgEndMatch::REASON_GAMEOVER || msgEndMatch.seatLost != 2))
						throw std::runtime_error("Checkers::MsgEndMatch: Match should have ended in draw!");
					if (m_playerResigned != -1 && (msgEndMatch.reason != MsgEndMatch::REASON_GAMEOVER || msgEndMatch.seatLost != m_playerResigned))
						throw std::runtime_error("Checkers::MsgEndMatch: Match should have ended in resign of player " + std::to_string(m_playerResigned) + "!");

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
			if (msgCheckIn.protocolSignature != XPCheckersProtocolSignature)
				throw std::runtime_error("Checkers::MsgCheckIn: Invalid protocol signature!");
			if (msgCheckIn.protocolVersion != XPCheckersProtocolVersion)
				throw std::runtime_error("Checkers::MsgCheckIn: Incorrect protocol version!");
			// msgCheckIn.playerID should be undefined
			if (msgCheckIn.seat != player.m_seat)
				throw std::runtime_error("Checkers::MsgCheckIn: Incorrect player seat!");

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
				throw std::runtime_error("Checkers::MsgChatMessage: Incorrect player seat!");

			// userID may be 0, if chat message was sent before client check-in completed
			if (msgChat.first.userID == 0)
				msgChat.first.userID = player.GetID();
			else if (msgChat.first.userID != player.GetID())
				throw std::runtime_error("Checkers::MsgChatMessage: Incorrect user ID!");

			const WCHAR* chatMsgRaw = reinterpret_cast<const WCHAR*>(msgChat.second.raw);
			const size_t chatMsgLen = msgChat.second.GetLength() / sizeof(WCHAR) - 1;
			if (chatMsgLen <= 1)
				throw std::runtime_error("Checkers::MsgChatMessage: Empty chat message!");
			if (chatMsgRaw[chatMsgLen - 1] != L'\0')
				throw std::runtime_error("Checkers::MsgChatMessage: Non-null-terminated chat message!");

			const std::wstring chatMsg(chatMsgRaw, chatMsgLen - 1);
			const uint8_t msgID = ValidateChatMessage(chatMsg, 40, 43);
			if (!msgID)
				throw std::runtime_error("Checkers::MsgChatMessage: Invalid chat message!");

			// XP games initially check for a wide character at the end of the message string, which is equal to the message ID.
			// Since we have already verified that the message ID (/{id}) at the start of the string is valid,
			// we can safely just send over the corresponding character.
			Array<char, ROUND_DATA_LENGTH_UINT32(4)> msgIDArr;
			msgIDArr[2] = msgID; // 1st byte of 2nd WCHAR
			msgIDArr.SetLength(4);
			msgChat.first.messageLength = 4;
			BroadcastGameMessage<MessageChatMessage>(msgChat.first, msgIDArr);
			break;
		}
		default:
			throw std::runtime_error("CheckersMatch::ProcessIncomingGameMessage(): Game message of unknown type received: " + std::to_string(type));
	}
}

}
