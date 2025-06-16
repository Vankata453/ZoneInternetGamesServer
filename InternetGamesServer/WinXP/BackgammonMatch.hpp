#pragma once

#include "Match.hpp"

#include <array>

namespace WinXP {

class BackgammonMatch final : public Match
{
public:
	BackgammonMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::BACKGAMMON; }

	/** Processing messages */
	void ProcessIncomingGameMessage(PlayerSocket& player, uint32 type) override;

private:
	enum class MatchState
	{
		INITIALIZING,
		PLAYING
	};
	MatchState m_matchState;

	enum class MatchPlayerState
	{
		AWAITING_CHECKIN,
		AWAITING_INITIAL_TRANSACTION, // Seat 0 only
		AWAITING_MATCH_START, // Seat 0 only
		WAITING_FOR_MATCH_START, // Seat 1 only
		PLAYING
	};
	std::array<MatchPlayerState, 2> m_playerStates;

private:
	BackgammonMatch(const BackgammonMatch&) = delete;
	BackgammonMatch operator=(const BackgammonMatch&) = delete;
};

}
