#pragma once

#include "Match.hpp"

class BackgammonMatch final : public Match
{
public:
	BackgammonMatch(PlayerSocket& player);

	/** Construct "STag" messages */
	std::string ConstructEndMatchMessage() const override { return ""; } // TODO

protected:
	std::string ConstructGameInitXML(PlayerSocket* caller) const override { return ""; } // TODO
};

