#pragma once

#include "Match.hpp"

#include <array>
#include <random>

#include "Protocol/Backgammon.hpp"

namespace WinXP {

#define XPBackgammonMaxStateTransactionsSize 12

class BackgammonMatch final : public Match
{
public:
	BackgammonMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::BACKGAMMON; }

	/** Processing messages */
	void ProcessIncomingGameMessage(PlayerSocket& player, uint32 type) override;

private:
	void ValidateStateTransaction(int tag,
		const Array<MsgStateTransaction::Transaction, XPBackgammonMaxStateTransactionsSize>& trans, int16 seat);

private:
	enum class MatchState
	{
		INITIALIZING,
		PLAYING,
		ENDED
	};
	MatchState m_matchState;

	enum class MatchPlayerState
	{
		AWAITING_CHECKIN,
		AWAITING_INITIAL_TRANSACTION, // Seat 0 only
		AWAITING_MATCH_START, // Seat 0 only
		WAITING_FOR_MATCH_START, // Seat 1 only
		IN_GAME,
		END_GAME,
		END_MATCH
	};
	std::array<MatchPlayerState, 2> m_playerStates;

	std::array<Backgammon::MsgCheckIn, 2> m_playerCheckInMsgs;

	std::mt19937 m_rng;

	bool m_initialRollStarted;
	int m_doubleCubeValue;
	int16 m_doubleCubeOwnerSeat;

private:
	BackgammonMatch(const BackgammonMatch&) = delete;
	BackgammonMatch operator=(const BackgammonMatch&) = delete;
};

}
