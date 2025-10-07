#pragma once

#include "Match.hpp"

#include <array>

namespace WinXP {

class ReversiMatch final : public Match
{
public:
	ReversiMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::REVERSI; }

	/** Processing messages */
	void ProcessIncomingGameMessage(PlayerSocket& player, uint32 type) override;

private:
	void Reset();

private:
	enum class MatchState
	{
		AWAITING_START,
		PLAYING,
		ENDED
	};
	MatchState m_matchState;

	std::array<bool, 2> m_playersCheckedIn;
	int16 m_playerTurn;
	int16 m_playerResigned;

private:
	ReversiMatch(const ReversiMatch&) = delete;
	ReversiMatch operator=(const ReversiMatch&) = delete;
};

}
