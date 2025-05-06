#pragma once

#include "Match.hpp"

#include <array>
#include <random>

class SpadesMatch final : public Match
{
public:
	SpadesMatch(PlayerSocket& player);

	Game GetGame() const override { return Game::SPADES; }

	std::vector<std::string> ConstructGameStartMessagesXML(const PlayerSocket& caller) const override;

protected:
	size_t GetRequiredPlayerCount() const override { return 4; }

	std::vector<QueuedEvent> ProcessEvent(const tinyxml2::XMLElement& elEvent, const PlayerSocket& caller) override;

private:
	void ResetHand();

	void UpdateTeamPoints();

private:
	enum BidValues
	{
		BID_HAND_START = -4,
		BID_SHOWN_CARDS = -3,
		BID_DOUBLE_NIL = -2
	};

public:
	typedef uint16_t Card;
	typedef std::vector<Card> CardArray;

	class CardTrick final
	{
	public:
		CardTrick();

		void Reset();
		void Set(int player, Card card);

		bool FollowsSuit(Card card, const CardArray& hand) const;

		bool IsFinished() const;
		int GetWinner() const;

	private:
		Card m_leadCard;
		std::array<Card, 4> m_playerCards;
	};

private:
	std::mt19937 m_rng;

	enum class MatchState
	{
		BIDDING,
		PLAYING
	};
	MatchState m_match_state;

	std::array<int, 2> m_teamPoints;
	std::array<uint8_t, 2> m_teamBags;

	int m_handDealer;
	int m_nextBidPlayer;
	std::array<int, 4> m_playerBids;
	std::array<CardArray, 4> m_playerCards;
	int m_playerTurn;
	int m_playerTrickTurn;
	std::array<uint8_t, 4> m_playerTricksTaken;

	CardTrick m_currentTrick;

private:
	SpadesMatch(const SpadesMatch&) = delete;
	SpadesMatch operator=(const SpadesMatch&) = delete;
};
