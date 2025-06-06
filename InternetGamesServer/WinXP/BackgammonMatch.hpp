#pragma once

#include "Match.hpp"

namespace WinXP {

class BackgammonMatch final : public Match
{
public:
	BackgammonMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::BACKGAMMON; }

private:
	BackgammonMatch(const BackgammonMatch&) = delete;
	BackgammonMatch operator=(const BackgammonMatch&) = delete;
};

}
