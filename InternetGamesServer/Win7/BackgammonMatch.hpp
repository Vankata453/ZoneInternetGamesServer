#pragma once

#include "Match.hpp"

#include <random>

namespace Win7 {

class BackgammonMatch final : public Match
{
public:
	BackgammonMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::BACKGAMMON; }

protected:
	std::pair<uint8_t, uint8_t> GetCustomChatMessagesRange() const override { return { 80, 82 }; }

	std::vector<QueuedEvent> ProcessEvent(const tinyxml2::XMLElement& elEvent, const PlayerSocket& caller) override;

private:
	void ClearGameState();

private:
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

};
