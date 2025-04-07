#pragma once

#include "Match.hpp"

class BackgammonMatch final : public Match
{
public:
	BackgammonMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::BACKGAMMON; }

	/** Construct XML data for STag messages */
	std::string ConstructGameInitXML(PlayerSocket* caller) const;

protected:
	QueuedEvent ProcessEvent(const std::string& xml, const PlayerSocket* caller) override;

private:
	bool m_initialRollStarted;

private:
	BackgammonMatch(const BackgammonMatch&) = delete;
	BackgammonMatch operator=(const BackgammonMatch&) = delete;
};
