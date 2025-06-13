#pragma once

#include "Match.hpp"

namespace WinXP {

class BackgammonMatch final : public Match
{
public:
	BackgammonMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::BACKGAMMON; }

	/** Processing messages */
	void ProcessIncomingGameMessage(PlayerSocket& player, uint32 type) override;

private:
	BackgammonMatch(const BackgammonMatch&) = delete;
	BackgammonMatch operator=(const BackgammonMatch&) = delete;
};

}
