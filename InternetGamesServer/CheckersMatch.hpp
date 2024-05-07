#pragma once

#include "Match.hpp"

class CheckersMatch final : public Match
{
public:
	CheckersMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::CHECKERS; }

protected:
	QueuedEvent ProcessEvent(const std::string& xml) override;

	std::string ConstructGameInitXML(PlayerSocket* caller) const override;

private:
	bool m_drawOffered;
};
