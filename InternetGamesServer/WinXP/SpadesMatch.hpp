#pragma once

#include "Match.hpp"

#include "..\WinCommon\SpadesUtil.hpp"

namespace WinXP {

using namespace ::Spades;

class SpadesMatch final : public Match
{
public:
	SpadesMatch(PlayerSocket& player);

	size_t GetRequiredPlayerCount() const override { return SpadesNumPlayers; }
	Game GetGame() const override { return Game::SPADES; }

protected:
	/** Processing messages */
	void ProcessIncomingGameMessageImpl(PlayerSocket& player, uint32 type) override;

private:
	void Reset();
	void ResetHand();

private:
	typedef int8_t Card;
	typedef std::vector<Card> CardArray;

private:
	std::mt19937 m_rng;

	enum class MatchState
	{
		INITIALIZING,
		BIDDING,
		PLAYING,
		ENDED
	};
	MatchState m_matchState;

	std::array<bool, 4> m_playersCheckedIn;
	std::array<CardArray, 4> m_playerCards;

	std::array<int16_t, 2> m_teamPoints;
	std::array<int16_t, 2> m_teamBags;

	int16 m_handDealer;
	int16 m_nextBidPlayer;
	enum BidValues
	{
		BID_HAND_START = -2,
		BID_SHOWN_CARDS = -1
	};
	std::array<int8_t, 4> m_playerBids;

	int16 m_playerTurn;
	int16 m_playerTrickTurn;
	std::array<int16_t, 4> m_playerTricksTaken;

	CardTrick<Card, SpadesUnsetCard> m_currentTrick;

private:
	SpadesMatch(const SpadesMatch&) = delete;
	SpadesMatch operator=(const SpadesMatch&) = delete;
};

}
