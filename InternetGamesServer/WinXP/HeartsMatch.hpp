#pragma once

#include "Match.hpp"

#include <array>
#include <random>

namespace WinXP {

#define HeartsNumPlayers 4

class HeartsMatch final : public Match
{
public:
	HeartsMatch(PlayerSocket& player);

	int8_t GetRequiredPlayerCount() const override { return HeartsNumPlayers; }
	Game GetGame() const override { return Game::HEARTS; }

protected:
	/** Processing messages */
	void ProcessIncomingGameMessageImpl(PlayerSocket& player, uint32 type) override;

private:
	void Reset();
	void ResetHand();

private:
	typedef int8_t Card;
	typedef std::vector<Card> CardArray;

	class CardTrick final
	{
	public:
		CardTrick();

		bool IsEmpty() const;

		void Reset();
		void Set(int16 player, Card card);

		bool FollowsSuit(Card card, const CardArray& hand) const;

		bool IsFinished() const;
		int16 GetWinner() const;
		int16 GetPoints() const;

	private:
		Card m_leadCard;
		std::array<Card, HeartsNumPlayers> m_playerCards;
	};

private:
	enum class MatchState
	{
		INITIALIZING,
		PASSING,
		PLAYING,
		ENDED
	};
	MatchState m_matchState;

	std::array<bool, 4> m_playersCheckedIn;
	std::array<CardArray, 4> m_playerCards;

	int16 m_passDirection;
	std::array<bool, 4> m_playersPassedCards;

	int16 m_playerTurn;
	bool m_pointsBroken;
	std::array<int16, 4> m_playerHandPoints;
	std::array<int16, 4> m_playerTotalPoints;

	CardTrick m_currentTrick;

private:
	HeartsMatch(const HeartsMatch&) = delete;
	HeartsMatch operator=(const HeartsMatch&) = delete;
};

}
