#pragma once

#include "Match.hpp"

#include <array>

namespace WinXP {

class CheckersMatch final : public Match
{
public:
	CheckersMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::CHECKERS; }

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
	int16 m_drawOfferedBy;
	bool m_drawAccepted;
	int16 m_playerResigned;

private:
	CheckersMatch(const CheckersMatch&) = delete;
	CheckersMatch operator=(const CheckersMatch&) = delete;
};

}
