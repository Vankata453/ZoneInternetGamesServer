#include "SpadesUtil.hpp"

#include <windows.h>

#define SPADES_LOG_TEAM_POINTS 0 // DEBUG: Print data about points/bags calculation for a team when a hand ends, to the console

#if SPADES_LOG_TEAM_POINTS
#include <iostream>
#endif

namespace Spades {

std::array<TrickScore, 2> CalculateTrickScore(const std::array<int8_t, 4>& playerBids,
	const std::array<int16_t, 4>& playerTricksTaken,
	const std::array<int16_t, 2>& teamBags,
	const int8_t doubleNilBid,
	bool countNilOvertricks)
{
	// Handle Nil/Double Nil for each player
	std::array<int16_t, 4> playerNilBonuses = { 0, 0, 0, 0 };
	std::array<int16_t, 4> playerNilBags = { 0, 0, 0, 0 };
	for (int i = 0; i < 4; ++i)
	{
		if (playerBids[i] == 0) // Nil
		{
			if (playerTricksTaken[i] == 0)
			{
				playerNilBonuses[i] = 100;
			}
			else
			{
				playerNilBonuses[i] = -100;
				playerNilBags[i] = playerTricksTaken[i];
			}
		}
		else if (playerBids[i] == doubleNilBid) // Double Nil
		{
			if (playerTricksTaken[i] == 0)
			{
				playerNilBonuses[i] = 200;
			}
			else
			{
				playerNilBonuses[i] = -200;
				playerNilBags[i] = playerTricksTaken[i];
			}
		}
	}

	// Score each team
	std::array<TrickScore, 2> score;
	for (BYTE team = 0; team < 2; ++team)
	{
		const BYTE p1 = team == 0 ? 0 : 1;
		const BYTE p2 = p1 + 2;

		TrickScore& teamScore = score[team];
		teamScore.pointsNil = playerNilBonuses[p1] + playerNilBonuses[p2];

		int16_t points = teamScore.pointsNil;
		int16_t bags = teamBags[team] + playerNilBags[p1] + playerNilBags[p2];

		// Contract scoring
		const int16_t contract = max(0, playerBids[p1]) + max(0, playerBids[p2]);
		const int16_t tricksNonNil = (playerBids[p1] > 0 ? playerTricksTaken[p1] : 0) +
			(playerBids[p2] > 0 ? playerTricksTaken[p2] : 0);
		if (tricksNonNil >= contract)
		{
			teamScore.pointsBase = contract * 10;
			points += teamScore.pointsBase;

			const int16_t overtricksNonNil = tricksNonNil - contract;
			bags += overtricksNonNil; // Only non-Nil overtricks count towards bags
			if (!countNilOvertricks) // WinXP: Overtricks for points are calculated using only non-Nil overtricks
			{
				teamScore.pointsBagBonus = overtricksNonNil;
				points += overtricksNonNil;
			}
		}
		else
		{
			teamScore.pointsBase = contract * -10;
			points += teamScore.pointsBase;
		}

		if (countNilOvertricks) // Win7: Overtricks for points are calculated using all tricks taken, Nil or not
		{
			const int16_t tricksAll = playerTricksTaken[p1] + playerTricksTaken[p2];
			const int16_t overtricksAll = tricksAll - contract;
			if (overtricksAll > 0)
			{
				teamScore.pointsBagBonus = overtricksAll;
				points += overtricksAll;
			}
		}

		// Bag penalty
		if (bags >= 10)
		{
			teamScore.pointsBagPenalty = (bags / 10) * -100;
			points += teamScore.pointsBagPenalty;
			bags %= 10;
		}

#if SPADES_LOG_TEAM_POINTS
		std::cout << std::dec
			<< "Team " << (team + 1)
			<< " Bid: " << contract
			<< " TricksNonNil: " << tricksNonNil;
		if (countNilOvertricks)
		{
			std::cout << " Tricks: " << (playerTricksTaken[p1] + playerTricksTaken[p2])
				<< " Overtricks: " << (playerTricksTaken[p1] + playerTricksTaken[p2] - contract);
		}
		std::cout << " NilPenalty: " << (playerNilBonuses[p1] + playerNilBonuses[p2])
			<< " BagsBefore: " << static_cast<int>(teamBags[team])
			<< " BagsAfter: " << static_cast<int>(bags)
			<< " ScoreDelta: " << points
			<< std::endl;
#endif

		teamScore.points = points;
		teamScore.bags = bags;
	}
	return score;
}

}
