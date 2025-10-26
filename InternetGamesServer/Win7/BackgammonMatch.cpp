#include "BackgammonMatch.hpp"

#include "PlayerSocket.hpp"
#include "../Util.hpp"

static const std::uniform_int_distribution<> s_dieDistribution(1, 6);

namespace Win7 {

BackgammonMatch::BackgammonMatch(PlayerSocket& player) :
	Match(player),
	m_homeTableStones(0, 0),
	m_lastMovePlayer(-1),
	m_lastMoveHomeTableStone(false),
	m_initialRollStarted(false),
	m_doubleRequested(false),
	m_resignRequested(false),
	m_startNextGameState(StartNextGameState::DENIED),
	m_startNextGameRequestedOnceBy(-1)
{}


void
BackgammonMatch::ClearGameState()
{
	m_homeTableStones = { 0, 0 };
	m_lastMovePlayer = -1;
	m_lastMoveHomeTableStone = false;
	m_initialRollStarted = false;
	m_doubleRequested = false;
	m_resignRequested = false;
	m_startNextGameState = StartNextGameState::DENIED;
	m_startNextGameRequestedOnceBy = -1;
}


std::vector<BackgammonMatch::QueuedEvent>
BackgammonMatch::ProcessEvent(const tinyxml2::XMLElement& elEvent, const PlayerSocket& caller)
{
	if (!strcmp(elEvent.Name(), "DiceManager"))
	{
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

			if (sourceX >= 0 && sourceX < 25 && targetX >= 0 && targetX < 25)
			{
				m_lastMovePlayer = caller.m_role;
				m_lastMoveHomeTableStone = false;
				return {
					sanitizedMoveMessage.print()
				};
			}
			if (((caller.m_role == 0 && targetX == 25) || (caller.m_role == 1 && targetX == 26)) &&
				targetY >= 0 && targetY < 15)
			{
				m_lastMovePlayer = caller.m_role;
				m_lastMoveHomeTableStone = true;

				int& homeTableStones = caller.m_role == 0 ? m_homeTableStones.first : m_homeTableStones.second;
				++homeTableStones;

				if (homeTableStones >= 15) // All stones are in the player's home board
				{
					m_startNextGameState = StartNextGameState::ALLOWED;
					return {
						sanitizedMoveMessage.print(),
						QueuedEvent(
							StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver", "BearOff," + std::to_string(caller.m_role)),
							true)
					};
				}
				return {
					sanitizedMoveMessage.print()
				};
			}
			throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"Move\": Invalid source and/or target values!");
		}
	}
	else if (!strcmp(elEvent.Name(), "GameLogic"))
	{
		const tinyxml2::XMLElement* elMethod = elEvent.FirstChildElement("Method");
		if (!elMethod || !elMethod->GetText())
			throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\": No 'Method' field or inner text!");

		if (!strcmp(elMethod->GetText(), "DoneMoving")) // Player has done all their moves this round
		{
			return {
				StateSTag::ConstructMethodMessage("GameLogic", "DoneMoving")
			};
		}
		if (!strcmp(elMethod->GetText(), "UndoLast")) // Player has undone their last move
		{
			if (m_lastMovePlayer == caller.m_role && m_lastMoveHomeTableStone) // Player has moved stone from home table
			{
				int& homeTableStones = m_lastMovePlayer == 0 ? m_homeTableStones.first : m_homeTableStones.second;
				--homeTableStones;
				m_lastMoveHomeTableStone = false;
			}
			return {
				StateSTag::ConstructMethodMessage("GameLogic", "UndoLast")
			};
		}
		if (!strcmp(elMethod->GetText(), "AcceptDouble")) // Player has accepted other player's double
		{
			if (!m_doubleRequested)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"AcceptDouble\": No double has been requested!");

			m_doubleRequested = false;
			return {
				StateSTag::ConstructMethodMessage("GameLogic", "AcceptDouble")
			};
		}
		if (!strcmp(elMethod->GetText(), "RejectDouble")) // Player has rejected other player's double
		{
			if (!m_doubleRequested)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"RejectDouble\": No double has been requested!");

			m_doubleRequested = false;
			m_startNextGameState = StartNextGameState::ALLOWED;
			return {
				QueuedEvent(
					StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver", "DoubleDecline," + std::string(caller.m_role == 0 ? "1" : "0")),
					true)
			};
		}
		if (!strcmp(elMethod->GetText(), "RequestResign")) // Player has requested resign
		{
			if (m_resignRequested)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"RequestResign\": Resign was already requested!");

			const tinyxml2::XMLElement* elParams = elEvent.FirstChildElement("Params");
			if (elParams && elParams->GetText())
			{
				const int pointsOffered = std::stoi(elParams->GetText());
				if (pointsOffered >= 0)
				{
					m_resignRequested = true;
					return {
						StateSTag::ConstructMethodMessage("GameLogic", "RequestResign", std::to_string(pointsOffered))
					};
				}
			}
		}
		if (!strcmp(elMethod->GetText(), "AcceptResign")) // Player has accepted resign request of other player
		{
			if (!m_resignRequested)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"AcceptResign\": No resign has been requested!");

			return {
				StateSTag::ConstructMethodMessage("GameLogic", "AcceptResign")
			};
		}
		if (!strcmp(elMethod->GetText(), "RejectResign")) // Player has rejected resign request of other player
		{
			if (!m_resignRequested)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\" -> \"RejectResign\": No resign has been requested!");

			m_resignRequested = false;
			return {
				StateSTag::ConstructMethodMessage("GameLogic", "RejectResign")
			};
		}
		if (!strcmp(elMethod->GetText(), "StartNextGame")) // Client has requested the start of the next game
		{
			switch (m_startNextGameState)
			{
				case StartNextGameState::DENIED:
				{
					break;
				}
				case StartNextGameState::ALLOWED:
				{
					m_startNextGameState = StartNextGameState::REQUESTED_ONCE;
					m_startNextGameRequestedOnceBy = caller.m_role;
					break;
				}
				case StartNextGameState::REQUESTED_ONCE:
				{
					if (m_startNextGameRequestedOnceBy != caller.m_role)
					{
						ClearGameState();
					}
					break;
				}
			}
			return {};
		}
		throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameLogic\": Unknown method!");
	}
	else if (!strcmp(elEvent.Name(), "GameManagement"))
	{
		const tinyxml2::XMLElement* elMethod = elEvent.FirstChildElement("Method");
		if (!elMethod || !elMethod->GetText())
			throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameManagement\": No 'Method' field or inner text!");

		if (!strcmp(elMethod->GetText(), "ResignGiven")) // Player has resigned
		{
			if (!m_resignRequested)
				throw std::runtime_error("BackgammonMatch::ProcessEvent(): \"GameManagement\" -> \"ResignGiven\": No resign has been requested!");

			m_resignRequested = false;
			m_startNextGameState = StartNextGameState::ALLOWED;
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
