#pragma once

#include "Match.hpp"

class CheckersMatch final : public Match
{
public:
	CheckersMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::CHECKERS; }

	/** Construct XML data for STag messages */
	std::string ConstructGameInitXML(PlayerSocket* caller) const override;

protected:
	std::vector<QueuedEvent> ProcessEvent(const std::string& xml, const PlayerSocket* caller) override;

private:
	bool m_drawOffered;

private:
	CheckersMatch(const CheckersMatch&) = delete;
	CheckersMatch operator=(const CheckersMatch&) = delete;
};
