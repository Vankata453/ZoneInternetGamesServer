#pragma once

#include "Match.hpp"

#include <array>

namespace Win7 {

#define BackgammonMatchPoints 5
#define BackgammonPlayerStones 15

#define BackgammonBarX 24

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

	void AddGamePoints(int role, uint8_t points);

private:
	std::array<uint8_t, 2> m_playerPoints;
	bool m_crawfordGame; // "Crawford Rule", enabled for the game where the leading player is one point away (4) from winning the match

	std::array<std::pair<int8_t, int8_t>, BackgammonPlayerStones * 2> m_stones; // Stones and their positions on the board
	int8_t m_lastMoveStone;
	std::pair<int8_t, int8_t> m_lastMoveSourcePos;
	std::pair<int8_t, int8_t> m_lastMoveBlotSourcePos;
	std::array<bool, 2> m_playersBorneOff;

	bool m_initialRollStarted;
	bool m_doubleRequested;
	uint8_t m_doubleCubeValue;
	int m_doubleCubeOwner;
	uint8_t m_resignPointsOffered;

	enum class GameState
	{
		PLAYING,
		START_NEXT,
		START_NEXT_REQUESTED_ONCE
	};
	GameState m_gameState;
	int m_startNextGameRequestedOnceBy;

private:
	BackgammonMatch(const BackgammonMatch&) = delete;
	BackgammonMatch operator=(const BackgammonMatch&) = delete;
};

};
