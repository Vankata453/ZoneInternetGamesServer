#pragma once

#include "Match.hpp"

class BackgammonMatch final : public Match
{
public:
	BackgammonMatch(PlayerSocket& player);

protected:
	std::string ConstructGameInitXML(PlayerSocket* caller) const override { return ""; } // TODO
};

