#include "BackgammonMatch.hpp"

#include "PlayerSocket.hpp"

static const std::uniform_int_distribution<> s_dieDistribution(1, 6);

namespace WinXP {

#define XPBackgammonInvertSeat(seat) (seat == 0 ? 1 : 0)
#define XPBackgammonIsSeatHost(seat) (seat == 0)
#define XPBackgammonMatchStateToNumString(state) std::to_string(static_cast<int>(state))

#define XPBackgammonProtocolSignature 0x42434B47
#define XPBackgammonProtocolVersion 3
#define XPBackgammonClientVersion 65536

BackgammonMatch::BackgammonMatch(PlayerSocket& player) :
	Match(player),
	m_matchState(MatchState::INITIALIZING),
	m_playerStates({ MatchPlayerState::AWAITING_CHECKIN, MatchPlayerState::AWAITING_CHECKIN }),
	m_playerCheckInMsgs(),
	m_rng(std::random_device()()),
	m_initialRollStarted(false),
	m_doubleCubeValue(1),
	m_doubleCubeOwnerSeat(-1)
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

					m_playerStates[player.m_seat] = player.m_seat == 0 ? MatchPlayerState::AWAITING_INITIAL_TRANSACTION : MatchPlayerState::WAITING_FOR_MATCH_START;
					m_playerCheckInMsgs[player.m_seat] = std::move(msgCheckIn);
					if (m_playerStates[0] != MatchPlayerState::AWAITING_CHECKIN &&
						m_playerStates[1] != MatchPlayerState::AWAITING_CHECKIN)
					{
						for (const MsgCheckIn& msg : m_playerCheckInMsgs)
							BroadcastGameMessage<MessageCheckIn>(msg);
						m_playerCheckInMsgs = {};
					}
					return;
				}
				case MatchPlayerState::AWAITING_MATCH_START:
				{
					assert(XPBackgammonIsSeatHost(player.m_seat));

					if (m_playerStates[1] != MatchPlayerState::WAITING_FOR_MATCH_START)
						throw std::runtime_error("MessageNewMatch: Player at seat 1 was not ready for start!");

					player.OnMatchAwaitEmptyGameMessage(MessageNewMatch);

					m_playerStates[0] = MatchPlayerState::END_GAME;
					m_playerStates[1] = MatchPlayerState::END_GAME;
					m_matchState = MatchState::PLAYING;
					return;
				}
			}
			break;
		}
		case MatchState::PLAYING:
		{
			switch (type)
			{
				case MessageFirstMove:
				{
					if (!XPBackgammonIsSeatHost(player.m_seat))
						throw std::runtime_error("Backgammon::MsgFirstMove: Only the host (seat 0) can send this message!");

					const MsgFirstMove msgFirstMove = player.OnMatchAwaitGameMessage<MsgFirstMove, MessageFirstMove>();
					return;
				}
				case MessageFirstRoll:
				{
					if (m_playerStates[player.m_seat] != MatchPlayerState::END_GAME)
						throw std::runtime_error("Backgammon::MsgEndTurn (MessageFirstRoll): Incorrect player state: " + XPBackgammonMatchStateToNumString(player.m_seat));

					const MsgEndTurn msgFirstRoll = player.OnMatchAwaitGameMessage<MsgEndTurn, MessageFirstRoll>();
					if (msgFirstRoll.seat != player.m_seat)
						throw std::runtime_error("Backgammon::MsgEndTurn (MessageFirstRoll): Incorrect seat!");

					if (!m_initialRollStarted)
					{
						m_initialRollStarted = true;
						return;
					}

					m_playerStates[0] = MatchPlayerState::IN_GAME;
					m_playerStates[1] = MatchPlayerState::IN_GAME;
					m_initialRollStarted = false;
					return;
				}
				case MessageTieRoll:
				{
					if (!XPBackgammonIsSeatHost(player.m_seat))
						throw std::runtime_error("Backgammon::MsgEndTurn (MessageTieRoll): Only the host (seat 0) can send this message!");

					const MsgEndTurn msgTieRoll = player.OnMatchAwaitGameMessage<MsgEndTurn, MessageTieRoll>();
					if (msgTieRoll.seat != player.m_seat)
						throw std::runtime_error("Backgammon::MsgEndTurn (MessageTieRoll): Incorrect seat!");

					m_playerStates[0] = MatchPlayerState::END_GAME;
					m_playerStates[1] = MatchPlayerState::END_GAME;
					return;
				}
				case MessageDiceRollRequest:
				{
					const MsgDiceRollRequest msgDiceRollRequest = player.OnMatchAwaitGameMessage<MsgDiceRollRequest, MessageDiceRollRequest>();
					if (msgDiceRollRequest.seat != player.m_seat)
						throw std::runtime_error("Backgammon::MsgDiceRollRequest: Incorrect seat!");

					MsgDiceRollResponse msgDiceRollResponse;
					msgDiceRollResponse.seat = player.m_seat;
					msgDiceRollResponse.dice1.val = s_dieDistribution(m_rng);
					msgDiceRollResponse.dice2.val = s_dieDistribution(m_rng);

					BroadcastGameMessage<MessageDiceRollResponse>(msgDiceRollResponse);
					return;
				}
				case MessageEndTurn:
				{
					const MsgEndTurn msgEndTurn = player.OnMatchAwaitGameMessage<MsgEndTurn, MessageEndTurn>();
					if (msgEndTurn.seat != player.m_seat)
						throw std::runtime_error("Backgammon::MsgEndTurn: Incorrect seat!");
					return;
				}
				case MessageEndGame:
				{
					const MsgEndTurn msgEndGame = player.OnMatchAwaitGameMessage<MsgEndTurn, MessageEndGame>();
					if (msgEndGame.seat != player.m_seat)
						throw std::runtime_error("Backgammon::MsgEndTurn (MessageEndGame): Incorrect seat!");

					m_playerStates[player.m_seat] = MatchPlayerState::END_GAME;
					m_doubleCubeValue = 1;
					m_doubleCubeOwnerSeat = -1;
					return;
				}
				case MessageEndMatch:
				{
					const MsgEndMatch msgEndMatch = player.OnMatchAwaitGameMessage<MsgEndMatch, MessageEndMatch>();
					if (msgEndMatch.reason != MsgEndMatch::REASON_FORFEIT &&
						msgEndMatch.reason != MsgEndMatch::REASON_GAMEOVER)
						throw std::runtime_error("Backgammon::MsgEndMatch: Invalid reason!");
					if (msgEndMatch.reason == MsgEndMatch::REASON_GAMEOVER && !XPBackgammonIsSeatHost(player.m_seat))
						throw std::runtime_error("Backgammon::MsgEndMatch: Only the host (seat 0) can notify the server of game over!");

					m_playerStates[0] = MatchPlayerState::END_MATCH;
					m_playerStates[1] = MatchPlayerState::END_MATCH;
					m_matchState = MatchState::ENDED;
					m_state = STATE_GAMEOVER;

					m_doubleCubeValue = 1;
					m_doubleCubeOwnerSeat = -1;
					return;
				}
			}
			break;
		}
		case MatchState::ENDED:
		{
			assert(m_playerStates[0] == MatchPlayerState::END_MATCH &&
				m_playerStates[1] == MatchPlayerState::END_MATCH);

			switch (type)
			{
				case MessageEndGame:
				{
					const MsgEndTurn msgEndGame = player.OnMatchAwaitGameMessage<MsgEndTurn, MessageEndGame>();
					if (msgEndGame.seat != player.m_seat)
						throw std::runtime_error("Backgammon::MsgEndTurn (MessageEndGame): Incorrect seat!");
					return;
				}
				case MessageNewMatch:
				{
					if (m_players.size() != GetRequiredPlayerCount())
						throw std::runtime_error("Backgammon::MessageNewMatch: Cannot proceed as players are missing!");

					player.OnMatchAwaitEmptyGameMessage(MessageNewMatch);

					m_playerStates[0] = MatchPlayerState::END_GAME;
					m_playerStates[1] = MatchPlayerState::END_GAME;
					m_matchState = MatchState::PLAYING;
					m_state = STATE_PLAYING;
					return;
				}
			}
		}
	}

	// Miscellaneous messages
	switch (type)
	{
		case MessageStateTransaction:
		{
			const std::pair<MsgStateTransaction, Array<MsgStateTransaction::Transaction, XPBackgammonMaxStateTransactionsSize>> msgTransaction =
				player.OnMatchAwaitGameMessage<MsgStateTransaction, MessageStateTransaction, MsgStateTransaction::Transaction, XPBackgammonMaxStateTransactionsSize>();
			if (msgTransaction.first.userID != player.GetID())
				throw std::runtime_error("MsgStateTransaction: Incorrect user ID!");
			if (msgTransaction.first.seat != player.m_seat)
				throw std::runtime_error("MsgStateTransaction: Incorrect seat!");

			if (m_playerStates[player.m_seat] == MatchPlayerState::AWAITING_INITIAL_TRANSACTION)
			{
				assert(player.m_seat == 0);

				const auto& trans = msgTransaction.second;
				if (msgTransaction.first.tag != TransactionStateChange ||
					trans.GetLength() != 2 ||
					trans[0].tag != SharedState || trans[0].value != StateGameSettings ||
					trans[1].tag != SharedActiveSeat || trans[1].value != 0)
				{
					throw std::runtime_error("BackgammonMatch::ProcessIncomingGameMessage(): Invalid initial state transaction!");
				}
				m_playerStates[0] = MatchPlayerState::AWAITING_MATCH_START;
				return;
			}
			else if (m_playerStates[player.m_seat] <= MatchPlayerState::AWAITING_MATCH_START)
			{
				throw std::runtime_error("BackgammonMatch::ProcessIncomingGameMessage(): Invalid transaction during initialization phase!");
			}
			else
			{
				ValidateStateTransaction(msgTransaction.first.tag, msgTransaction.second, player.m_seat);
			}

			BroadcastGameMessage<MessageStateTransaction>(msgTransaction.first, msgTransaction.second, player.m_seat);
			return;
		}

		case MessageChatMessage:
		{
			const std::pair<MsgChatMessage, Array<char, 128>> msgChat =
				player.OnMatchAwaitGameMessage<MsgChatMessage, MessageChatMessage, char, 128>();
			if (msgChat.first.userID != player.GetID())
				throw std::runtime_error("Backgammon::MsgChatMessage: Incorrect user ID!");
			if (msgChat.first.seat != player.m_seat)
				throw std::runtime_error("Backgammon::MsgChatMessage: Incorrect seat!");

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

void
BackgammonMatch::ValidateStateTransaction(int tag,
	const Array<MsgStateTransaction::Transaction, XPBackgammonMaxStateTransactionsSize>& trans, int16 seat)
{
	using namespace Backgammon;

	switch (tag)
	{
		case TransactionStateChange:
		{
			if ((trans.GetLength() != 1 || trans[0].tag != SharedState) &&
				(trans.GetLength() != 2 || trans[0].tag != SharedState ||
					trans[1].tag != SharedActiveSeat) &&
				(trans.GetLength() != 2 || trans[0].tag != SharedState ||
					trans[1].tag != SharedResignPoints) &&
				(trans.GetLength() != 3 || trans[0].tag != SharedState ||
					trans[1].tag != SharedGameOverReason ||
					trans[2].tag != SharedActiveSeat))
			{
				throw std::runtime_error("BackgammonMatch::TransactionStateChange: Invalid transaction!");
			}
			break;
		}
		case TransactionInitialSettings:
		{
			if (seat != 0)
				throw std::runtime_error("BackgammonMatch::TransactionInitialSettings: Only the host (seat 0) can send this transaction!");

			if (trans.GetLength() != 4 ||
				trans[0].tag != SharedHostBrown || trans[0].value != TRUE ||
				trans[1].tag != SharedAutoDouble || trans[1].value != FALSE ||
				trans[2].tag != SharedTargetScore || trans[2].value != 3 ||
				trans[3].tag != SharedSettingsDone || trans[3].value != TRUE)
			{
				throw std::runtime_error("BackgammonMatch::TransactionInitialSettings: Invalid transaction!");
			}
			break;
		}
		case TransactionDice:
		{
			for (const auto& tr : trans)
			{
				if (tr.tag != SharedDice && tr.tag != SharedDiceSize)
					throw std::runtime_error("BackgammonMatch::TransactionDice: Invalid transaction tag!");
				if (tr.index < 0 || tr.index >= 4)
					throw std::runtime_error("BackgammonMatch::TransactionDice: Invalid transaction index!");
			}
			break;
		}
		case TransactionBoard:
		{
			if ((trans.GetLength() != 3 ||
				trans[0].tag != SharedPieces ||
				trans[1].tag != SharedDiceSize || trans[2].tag != SharedDiceSize) &&
				(trans.GetLength() != 4 ||
				trans[0].tag != SharedPieces || trans[1].tag != SharedPieces ||
				trans[2].tag != SharedDiceSize || trans[3].tag != SharedDiceSize))
			{
				throw std::runtime_error("BackgammonMatch::TransactionBoard: Invalid transaction!");
			}
			break;
		}
		case TransactionAcceptDouble:
		{
			if (trans.GetLength() != 2 ||
				trans[0].tag != SharedDoubleCubeValue || trans[0].value != (m_doubleCubeValue *= 2) || trans[0].value > 64 ||
				trans[1].tag != SharedDoubleCubeOwner || trans[1].value != XPBackgammonInvertSeat(seat) + 1)
			{
				throw std::runtime_error("BackgammonMatch::TransactionAcceptDouble: Invalid transaction!");
			}

			m_doubleCubeOwnerSeat = m_doubleCubeValue == 2 ? trans[1].value - 1 : XPBackgammonInvertSeat(m_doubleCubeOwnerSeat);
			if (trans[1].value != m_doubleCubeOwnerSeat + 1)
			{
				throw std::runtime_error("BackgammonMatch::TransactionAcceptDouble: Invalid transaction!");
			}
			break;
		}
		case TransactionNoMoreLegalMoves:
		{
			if (trans.GetLength() != 0)
			{
				throw std::runtime_error("BackgammonMatch::TransactionNoMoreLegalMoves: Invalid transaction: Expected to be empty!");
			}
			break;
		}
		case TransactionReadyForNewMatch:
		{
			if (trans.GetLength() != 1 ||
				trans[0].tag != SharedReady || trans[0].value != 1)
			{
				throw std::runtime_error("BackgammonMatch::TransactionReadyForNewMatch: Invalid transaction!");
			}
			break;
		}
		default:
			throw std::runtime_error("BackgammonMatch::ValidateStateTransaction(): Invalid state transaction tag!");
	}
}

}
