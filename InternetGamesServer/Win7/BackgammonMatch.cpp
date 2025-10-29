#include "BackgammonMatch.hpp"

#include "PlayerSocket.hpp"
#include "../Util.hpp"

#define BackgammonInvertRole(role) (role == 0 ? 1 : 0)
#define BackgammonIsStoneIdxPlayer0(stoneIdx) (stoneIdx < BackgammonPlayerStones)
#define BackgammonIsStoneIdxPlayer1(stoneIdx) (stoneIdx >= BackgammonPlayerStones && stoneIdx < BackgammonPlayerStones * 2)

static const std::uniform_int_distribution<> s_dieDistribution(1, 6);

namespace Win7 {

BackgammonMatch::BackgammonMatch(PlayerSocket& player) :
	Match(player),
	m_playerPoints(),
	m_crawfordGame(),
	m_stones(),
	m_lastMoveStone(),
	m_lastMoveSourcePos(),
	m_lastMoveBlotSourcePos(),
	m_playersBorneOff(),
	m_initialRollStarted(),
	m_doubleRequested(),
	m_doubleCubeValue(),
	m_doubleCubeOwner(),
	m_resignPointsOffered(),
	m_gameState(),
	m_startNextGameRequestedOnceBy(-1)
{
	ClearGameState();
}


void
BackgammonMatch::ClearGameState()
{
	m_stones = {
		{
			// Player 1
			{ 18, 0 }, { 18, 1 }, { 18, 2 }, { 18, 3 }, { 18, 4 },
			{ 16, 0 }, { 16, 1 }, { 16, 2 },
			{ 11, 0 }, { 11, 1 }, { 11, 2 }, { 11, 3 }, { 11, 4 },
			{ 0, 0 }, { 0, 1 },

			// Player 2
			{ 5, 0 }, { 5, 1 }, { 5, 2 }, { 5, 3 }, { 5, 4 },
			{ 7, 0 }, { 7, 1 }, { 7, 2 },
			{ 12, 0 }, { 12, 1 }, { 12, 2 }, { 12, 3 }, { 12, 4 },
			{ 23, 0 }, { 23, 1 }
		}
	};
	m_lastMoveStone = -1;
	m_lastMoveSourcePos = { -1, -1 };
	m_lastMoveBlotSourcePos = { -1, -1 };
	m_playersBorneOff = {};
	m_initialRollStarted = false;
	m_doubleRequested = false;
	m_doubleRequested = false;
	m_doubleCubeValue = 1;
	m_doubleCubeOwner = -1;
	m_resignPointsOffered = 0;
	m_gameState = GameState::PLAYING;
	m_startNextGameRequestedOnceBy = -1;
}

void
BackgammonMatch::AddGamePoints(int role, uint8_t points)
{
	assert(points > 0);
	if ((m_playerPoints[role] += points) >= BackgammonMatchPoints)
	{
		m_state = STATE_GAMEOVER;
		return;
	}

	m_crawfordGame = m_playerPoints[role] == BackgammonMatchPoints - 1 &&
		m_playerPoints[BackgammonInvertRole(role)] < BackgammonMatchPoints - 1;
}


std::vector<BackgammonMatch::QueuedEvent>
BackgammonMatch::ProcessEvent(const tinyxml2::XMLElement& elEvent, const PlayerSocket& caller)
{
	if (!strcmp(elEvent.Name(), "DiceManager"))
	{
		if (m_gameState != GameState::PLAYING)
			throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"DiceManager\": Not in a game!");

		const tinyxml2::XMLElement* elMethod = elEvent.FirstChildElement("Method");
		if (!elMethod || !elMethod->GetText())
			throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"DiceManager\": No 'Method' field or inner text!");

		if (!strcmp(elMethod->GetText(), "GenerateValues")) // Generate dice values
		{
			const std::pair<int, int> dieValues = { s_dieDistribution(g_rng), s_dieDistribution(g_rng) };

			return {
				QueuedEvent(
					StateSTag::ConstructMethodMessage("DiceManager", "DieRolls",
						[&caller, &dieValues](XMLPrinter& printer) {
							NewElementWithText(printer, "Seat", caller.m_role);
							NewElementWithText(printer, "FirstDie", dieValues.first);
							NewElementWithText(printer, "SecondDie", dieValues.second);
						}),
					true)
			};
		}
		if (!strcmp(elMethod->GetText(), "GenerateInitialValues")) // Generate initial dice values
		{
			if (!m_initialRollStarted)
			{
				m_initialRollStarted = true;
				return {};
			}

			const std::pair<int, int> firstDieValues = { s_dieDistribution(g_rng), s_dieDistribution(g_rng) };
			std::pair<int, int> secondDieValues = { s_dieDistribution(g_rng), s_dieDistribution(g_rng) };
			while (firstDieValues.first == firstDieValues.second && secondDieValues.first == secondDieValues.second)
				secondDieValues = { s_dieDistribution(g_rng), s_dieDistribution(g_rng) };

			return {
				QueuedEvent(
					StateSTag::ConstructMethodMessage("DiceManager", "InitialDieRoles",
						[&firstDieValues, &secondDieValues](XMLPrinter& printer) {
							NewElementWithText(printer, "PlayersFirstDieValue", firstDieValues.first);
							NewElementWithText(printer, "OpponentsFirstDieValue", firstDieValues.second);
							NewElementWithText(printer, "PlayersSecondDieValue", secondDieValues.first);
							NewElementWithText(printer, "OpponentsSecondDieValue", secondDieValues.second);
						}),
					StateSTag::ConstructMethodMessage("DiceManager", "InitialDieRoles",
						[&firstDieValues, &secondDieValues](XMLPrinter& printer) {
							NewElementWithText(printer, "PlayersFirstDieValue", firstDieValues.second);
							NewElementWithText(printer, "OpponentsFirstDieValue", firstDieValues.first);
							NewElementWithText(printer, "PlayersSecondDieValue", secondDieValues.second);
							NewElementWithText(printer, "OpponentsSecondDieValue", secondDieValues.first);
						}))
			};
		}
		throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"DiceManager\": Unknown method!");
	}
	else if (!strcmp(elEvent.Name(), "Move"))
	{
		if (m_gameState != GameState::PLAYING)
			throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": Not in a game!");

		XMLPrinter sanitizedMoveMessage;
		sanitizedMoveMessage.OpenElement("Message");
		sanitizedMoveMessage.OpenElement("Move");

		const tinyxml2::XMLElement* elDouble = elEvent.FirstChildElement("Double");
		if (elDouble) // Player has requested double
		{
			if (!elDouble->GetText())
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": No 'Double' inner text!");

			if (m_doubleRequested)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": Double was already requested!");
			if (m_doubleCubeValue >= 64)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": Double is being requested after the limit of 64 has been reached!");
			if (m_doubleCubeOwner != caller.m_role && m_doubleCubeOwner != -1)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": This player cannot request double as they do not own the cube!");
			if (m_crawfordGame)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": Cannot request double during Crawford game!");

			int doubleBy;
			try
			{
				doubleBy = std::stoi(elDouble->GetText());
			}
			catch (const std::exception& err)
			{
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": Couldn't parse double value as integer: " + std::string(err.what()));
			}
			if (doubleBy < 2 || doubleBy % 2 != 0)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": Invalid double value!");

			m_doubleRequested = true;

			sanitizedMoveMessage.OpenElement("Source");
			NewElementWithText(sanitizedMoveMessage, "X", "-1");
			NewElementWithText(sanitizedMoveMessage, "Y", "-1");
			sanitizedMoveMessage.CloseElement("Source");

			sanitizedMoveMessage.OpenElement("Target");
			NewElementWithText(sanitizedMoveMessage, "X", "-1");
			NewElementWithText(sanitizedMoveMessage, "Y", "-1");
			sanitizedMoveMessage.CloseElement("Target");

			NewElementWithText(sanitizedMoveMessage, "Double", doubleBy);

			sanitizedMoveMessage.CloseElement("Move");
			sanitizedMoveMessage.CloseElement("Message");
			return {
				sanitizedMoveMessage.print()
			};
		}
		else // Player has performed a regular move
		{
			const tinyxml2::XMLElement* elSource = elEvent.FirstChildElement("Source");
			const tinyxml2::XMLElement* elTarget = elEvent.FirstChildElement("Target");
			if (!elSource)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": No 'Source' field!");
			if (!elTarget)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": No 'Target' field!");

			const tinyxml2::XMLElement* elSourceX = elSource->FirstChildElement("X");
			const tinyxml2::XMLElement* elSourceY = elSource->FirstChildElement("Y");
			const tinyxml2::XMLElement* elTargetX = elTarget->FirstChildElement("X");
			const tinyxml2::XMLElement* elTargetY = elTarget->FirstChildElement("Y");
			if (!elSourceX || !elSourceX->GetText())
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": No 'Source' -> 'X' field or inner text!");
			if (!elSourceY || !elSourceY->GetText())
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": No 'Source' -> 'Y' field or inner text!");
			if (!elTargetX || !elTargetX->GetText())
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": No 'Target' -> 'X' field or inner text!");
			if (!elTargetY || !elTargetY->GetText())
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": No 'Target' -> 'Y' field or inner text!");

			int sourceX, sourceY, targetX, targetY;
			try
			{
				sourceX = std::stoi(elSourceX->GetText());
				sourceY = std::stoi(elSourceY->GetText());
				targetX = std::stoi(elTargetX->GetText());
				targetY = std::stoi(elTargetY->GetText());
			}
			catch (const std::exception& err)
			{
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": Couldn't parse source/target X/Y value(s) as integer(s): " + std::string(err.what()));
			}

			sanitizedMoveMessage.OpenElement("Source");
			NewElementWithText(sanitizedMoveMessage, "X", sourceX);
			NewElementWithText(sanitizedMoveMessage, "Y", sourceY);
			sanitizedMoveMessage.CloseElement("Source");

			sanitizedMoveMessage.OpenElement("Target");
			NewElementWithText(sanitizedMoveMessage, "X", targetX);
			NewElementWithText(sanitizedMoveMessage, "Y", targetY);
			sanitizedMoveMessage.CloseElement("Target");

			sanitizedMoveMessage.CloseElement("Move");
			sanitizedMoveMessage.CloseElement("Message");

			if (((sourceX >= 0 && sourceX < 25 && targetX >= 0 && targetX < 25) ||
					targetX == (caller.m_role == 0 ? 25 : 26)) &&
				targetY >= 0 && targetY < BackgammonPlayerStones)
			{
				int8_t stoneIdx = BackgammonPlayerStones * caller.m_role;
				for (; stoneIdx < BackgammonPlayerStones * (1 + caller.m_role); ++stoneIdx)
				{
					const auto& stonePos = m_stones[stoneIdx];
					if (stonePos.first == sourceX && (stonePos.second == sourceY || sourceX == BackgammonBarX))
						break;
				}
				if (stoneIdx == BackgammonPlayerStones * (1 + caller.m_role))
					throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": No stone of this player at provided source position!");

				int8_t targetStoneIdx = 0;
				for (; targetStoneIdx < BackgammonPlayerStones * 2; ++targetStoneIdx)
				{
					const auto& stonePos = m_stones[targetStoneIdx];
					if (stonePos.first == targetX && stonePos.second == targetY && targetX != BackgammonBarX)
						break;
				}
				bool blot = false;
				if (targetStoneIdx != BackgammonPlayerStones * 2)
				{
					/* Check if a blot is taking place */
					auto& stonePos = m_stones[targetStoneIdx];
					if (stonePos.second == 0 && // Y == 0
						(caller.m_role == 0 ? BackgammonIsStoneIdxPlayer1(targetStoneIdx) :
							BackgammonIsStoneIdxPlayer0(targetStoneIdx)) && // Owned by other player
						std::none_of(m_stones.begin(), m_stones.end(),
							[t = stonePos](const auto& s) { return s.first == t.first && s.second != t.second; })) // No other stone on the same point (X)
					{
						blot = true;
						m_lastMoveBlotSourcePos = stonePos;
						stonePos.first = BackgammonBarX; // Set only X, we don't care about Y in bar
					}
					else
					{
						throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": Target position is occupied by another stone!");
					}
				}
				if (!blot)
					m_lastMoveBlotSourcePos = { -1, -1 };

				m_lastMoveStone = stoneIdx;
				m_lastMoveSourcePos = { sourceX, sourceY };
				m_stones[stoneIdx] = { targetX, targetY };

				const int8_t bearOffPos = (caller.m_role == 0 ? 25 : 26);
				if (targetX == bearOffPos) // Bearing off?
				{
					m_playersBorneOff[caller.m_role] = true;

					bool borneOffAll = true;
					for (int8_t i = BackgammonPlayerStones * caller.m_role; i < BackgammonPlayerStones * (1 + caller.m_role); ++i)
					{
						const auto& stonePos = m_stones[i];
						if (stonePos.first != bearOffPos)
						{
							borneOffAll = false;
							break;
						}
					}
					if (borneOffAll) // All stones are in the player's home board
					{
						uint8_t points = 1; // "Cube"
						if (!m_playersBorneOff[BackgammonInvertRole(caller.m_role)]) // Opponent hasn't borne off this game
						{
							points = 2; // "Gammon"

							for (int8_t i = BackgammonPlayerStones * BackgammonInvertRole(caller.m_role);
								i < BackgammonPlayerStones * (1 + BackgammonInvertRole(caller.m_role)); ++i)
							{
								const auto& stonePos = m_stones[i];
								if (stonePos.first == BackgammonBarX || // Opponent piece in bar
									(stonePos.first >= (caller.m_role == 0 ? 0 : 18) &&
										stonePos.second <= (caller.m_role == 0 ? 5 : 23))) // Opponent piece in our home board
								{
									points = 3; // "Backgammon"
									break;
								}
							}
						}

						AddGamePoints(caller.m_role, points * m_doubleCubeValue);
						m_gameState = GameState::START_NEXT;
						return {
							sanitizedMoveMessage.print(),
							QueuedEvent(
								StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver", "BearOff," + std::to_string(caller.m_role)),
								true)
						};
					}
				}
				return {
					sanitizedMoveMessage.print()
				};
			}
			if (sourceX == -1 && sourceY == -1 && targetX == -1 && targetY == -1) // Player has no legal moves
			{
				return {};
			}
			throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": Invalid source and/or target value(s)!");
		}
	}
	else if (!strcmp(elEvent.Name(), "GameLogic"))
	{
		const tinyxml2::XMLElement* elMethod = elEvent.FirstChildElement("Method");
		if (!elMethod || !elMethod->GetText())
			throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\": No 'Method' field or inner text!");

		if (m_gameState != GameState::PLAYING)
		{
			if (!strcmp(elMethod->GetText(), "StartNextGame")) // Client has requested the start of the next game
			{
				switch (m_gameState)
				{
					case GameState::START_NEXT:
					{
						m_gameState = GameState::START_NEXT_REQUESTED_ONCE;
						m_startNextGameRequestedOnceBy = caller.m_role;
						break;
					}
					case GameState::START_NEXT_REQUESTED_ONCE:
					{
						if (m_startNextGameRequestedOnceBy == caller.m_role)
							throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"StartNextGame\": This player has already requested next game!");

						ClearGameState();
						break;
					}
				}
				return {};
			}
			throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\": Not in a game!");
		}

		if (!strcmp(elMethod->GetText(), "DoneMoving")) // Player has done all their moves this round
		{
			return {
				StateSTag::ConstructMethodMessage("GameLogic", "DoneMoving")
			};
		}
		if (!strcmp(elMethod->GetText(), "UndoLast")) // Player has undone their last move
		{
			if (caller.m_role == 0 ? BackgammonIsStoneIdxPlayer1(m_lastMoveStone) : BackgammonIsStoneIdxPlayer0(m_lastMoveStone))
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"UndoLast\": Last move was not done by this player!");

			m_stones[m_lastMoveStone] = m_lastMoveSourcePos;
			m_lastMoveStone = -1;
			m_lastMoveSourcePos = { -1, -1 };
			if (m_lastMoveBlotSourcePos.first > -1)
			{
				for (int8_t i = BackgammonPlayerStones * BackgammonInvertRole(caller.m_role);
					i < BackgammonPlayerStones * (1 + BackgammonInvertRole(caller.m_role)); ++i)
				{
					auto& stonePos = m_stones[i];
					if (stonePos.first == BackgammonBarX)
					{
						stonePos = m_lastMoveBlotSourcePos;
						break;
					}
				}
				m_lastMoveBlotSourcePos = { -1, -1 };
			}
			return {
				StateSTag::ConstructMethodMessage("GameLogic", "UndoLast")
			};
		}
		if (!strcmp(elMethod->GetText(), "AcceptDouble")) // Player has accepted other player's double
		{
			if (!m_doubleRequested)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"AcceptDouble\": No double has been requested!");
			if (m_doubleCubeOwner == caller.m_role)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"AcceptDouble\": This player is already cube owner!");
			if (m_crawfordGame)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"AcceptDouble\": Cannot double during Crawford game!");

			m_doubleRequested = false;
			assert(m_doubleCubeValue < 64);
			m_doubleCubeValue *= 2;
			m_doubleCubeOwner = caller.m_role;
			return {
				StateSTag::ConstructMethodMessage("GameLogic", "AcceptDouble")
			};
		}
		if (!strcmp(elMethod->GetText(), "RejectDouble")) // Player has rejected other player's double
		{
			if (!m_doubleRequested)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"RejectDouble\": No double has been requested!");
			if (m_doubleCubeOwner == caller.m_role)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"RejectDouble\": This player is already cube owner!");
			if (m_crawfordGame)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"RejectDouble\": Cannot double during Crawford game!");

			m_doubleRequested = false;
			AddGamePoints(BackgammonInvertRole(caller.m_role), m_doubleCubeValue);
			m_gameState = GameState::START_NEXT;
			return {
				QueuedEvent(
					StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver", "DoubleDecline," + std::string(caller.m_role == 0 ? "1" : "0")),
					true)
			};
		}
		if (!strcmp(elMethod->GetText(), "RequestResign")) // Player has requested resign
		{
			if (m_resignPointsOffered)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"RequestResign\": Resign was already requested!");

			const tinyxml2::XMLElement* elParams = elEvent.FirstChildElement("Params");
			if (!elParams || !elParams->GetText())
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"RequestResign\": No 'Params' field or inner text!");

			try
			{
				m_resignPointsOffered = static_cast<uint8_t>(std::stoi(elParams->GetText()));
			}
			catch (const std::exception& err)
			{
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"RequestResign\": Couldn't parse points offered value as integer: " + std::string(err.what()));
			}
			if (m_resignPointsOffered != m_doubleCubeValue &&
				m_resignPointsOffered != m_doubleCubeValue * 2 &&
				m_resignPointsOffered != m_doubleCubeValue * 3)
			{
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"RequestResign\": Invalid points offered!");
			}

			return {
				StateSTag::ConstructMethodMessage("GameLogic", "RequestResign", std::to_string(m_resignPointsOffered))
			};
		}
		if (!strcmp(elMethod->GetText(), "AcceptResign")) // Player has accepted resign request of other player
		{
			if (!m_resignPointsOffered)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"AcceptResign\": No resign has been requested!");

			return {
				StateSTag::ConstructMethodMessage("GameLogic", "AcceptResign")
			};
		}
		if (!strcmp(elMethod->GetText(), "RejectResign")) // Player has rejected resign request of other player
		{
			if (!m_resignPointsOffered)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"RejectResign\": No resign has been requested!");

			m_resignPointsOffered = 0;
			return {
				StateSTag::ConstructMethodMessage("GameLogic", "RejectResign")
			};
		}
		throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\": Unknown method!");
	}
	else if (!strcmp(elEvent.Name(), "GameManagement"))
	{
		if (m_gameState != GameState::PLAYING)
			throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameManagement\": Not in a game!");

		const tinyxml2::XMLElement* elMethod = elEvent.FirstChildElement("Method");
		if (!elMethod || !elMethod->GetText())
			throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameManagement\": No 'Method' field or inner text!");

		if (!strcmp(elMethod->GetText(), "ResignGiven")) // Player has resigned
		{
			if (!m_resignPointsOffered)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameManagement\" -> \"ResignGiven\": No resign has been requested!");

			AddGamePoints(BackgammonInvertRole(caller.m_role), m_resignPointsOffered);
			m_gameState = GameState::START_NEXT;
			return {
				QueuedEvent(
					StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver", "PlayerResigned," + std::string(caller.m_role == 0 ? "1" : "0")),
					true)
			};
		}
		throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameManagement\": Unknown method!");
	}
	throw std::runtime_error("BackgammonMatch::ProcessEvent(): Unknown event!");
}

}
