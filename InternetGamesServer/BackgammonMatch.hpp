#pragma once

#include "Match.hpp"

class BackgammonMatch final : public Match
{
public:
	BackgammonMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::BACKGAMMON; }

	/** Construct "STag" messages */
	std::string ConstructEndMatchMessage() const override;

protected:
	QueuedEvent ProcessEvent(const std::string& xml) override;

	std::string ConstructGameInitXML(PlayerSocket* caller) const override;

private:
	bool m_initialRollStarted;
};

