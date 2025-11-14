#pragma once

#include "Match.hpp"

#include <array>

#include "Protocol/Reversi.hpp"

namespace WinXP {

class ReversiMatch final : public Match
{
public:
	ReversiMatch(unsigned int index, PlayerSocket& player);

	Game GetGame() const override { return Game::REVERSI; }

protected:
	/** Processing messages */
	void ProcessIncomingGameMessageImpl(PlayerSocket& player, uint32 type) override;

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
	std::array<Reversi::MsgCheckIn, 2> m_playerCheckInMsgs;

	int16 m_playerResigned;

private:
	ReversiMatch(const ReversiMatch&) = delete;
	ReversiMatch operator=(const ReversiMatch&) = delete;
};

}
