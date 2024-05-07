#pragma once

#include "Match.hpp"

class SpadesMatch final : public Match
{
public:
	SpadesMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::SPADES; }

	/** Construct XML data for STag messages */
	std::string ConstructGameInitXML(PlayerSocket* caller) const override { return ""; } // TODO

protected:
	size_t GetRequiredPlayerCount() const override { return 4; }

private:
	SpadesMatch(const SpadesMatch&) = delete;
	SpadesMatch operator=(const SpadesMatch&) = delete;
};
