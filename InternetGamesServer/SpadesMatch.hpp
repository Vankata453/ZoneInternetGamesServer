#pragma once

#include "Match.hpp"

class SpadesMatch final : public Match
{
public:
	SpadesMatch(PlayerSocket& player);

	/** Construct "STag" messages */
	std::string ConstructEndMatchMessage() const override { return ""; } // TODO

protected:
	size_t GetRequiredPlayerCount() const override { return 4; }

	std::string ConstructGameInitXML(PlayerSocket* caller) const override { return ""; } // TODO
};

