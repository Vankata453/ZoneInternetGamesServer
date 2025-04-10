#pragma once

#include "Match.hpp"

#include <array>
#include <random>

class BackgammonMatch final : public Match
{
public:
	BackgammonMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::BACKGAMMON; }

	/** Construct XML data for STag messages */
	std::string ConstructGameInitXML(PlayerSocket* caller) const;

protected:
	std::vector<QueuedEvent> ProcessEvent(const std::string& xml, const PlayerSocket* caller) override;

private:
	void ClearState();

private:
	std::mt19937 m_rng;

	std::pair<int, int> m_homeTableStones;
	int m_lastMovePlayer;
	bool m_lastMoveHomeTableStone;

	bool m_initialRollStarted;
	bool m_doubleRequested;
	bool m_resignRequested;

	enum class StartNextGameState
	{
		DENIED,
		ALLOWED,
		REQUESTED_ONCE
	};
	StartNextGameState m_startNextGameState;
	int m_startNextGameRequestedOnceBy;

private:
	BackgammonMatch(const BackgammonMatch&) = delete;
	BackgammonMatch operator=(const BackgammonMatch&) = delete;
};
