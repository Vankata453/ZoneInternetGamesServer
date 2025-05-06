#include "BackgammonMatch.hpp"

#include "PlayerSocket.hpp"
#include "Util.hpp"

static const std::uniform_int_distribution<> s_dieDistribution(1, 6);

BackgammonMatch::BackgammonMatch(PlayerSocket& player) :
	Match(player),
	m_rng(std::random_device()()),
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
		if (elMethod && elMethod->GetText())
		{
			if (!strcmp(elMethod->GetText(), "GenerateValues")) // Generate dice values
			{
				const std::pair<int, int> dieValues = { s_dieDistribution(m_rng), s_dieDistribution(m_rng) };

				return {
					QueuedEvent(
						StateSTag::ConstructMethodMessage("DiceManager", "DieRolls",
							[&caller, &dieValues](XMLPrinter& printer) {
								NewElementWithText(printer, "Seat", std::to_string(caller.m_role));
								NewElementWithText(printer, "FirstDie", std::to_string(dieValues.first));
								NewElementWithText(printer, "SecondDie", std::to_string(dieValues.second));
							}),
						true)
				};
			}
			else if (!strcmp(elMethod->GetText(), "GenerateInitialValues")) // Generate initial dice values
			{
				if (!m_initialRollStarted)
				{
					m_initialRollStarted = true;
					return {};
				}

				const std::pair<int, int> firstDieValues = { s_dieDistribution(m_rng), s_dieDistribution(m_rng) };
				std::pair<int, int> secondDieValues = { s_dieDistribution(m_rng), s_dieDistribution(m_rng) };
				while (firstDieValues.first == firstDieValues.second && secondDieValues.first == secondDieValues.second)
				{
					secondDieValues = { s_dieDistribution(m_rng), s_dieDistribution(m_rng) };
				}

				return {
					QueuedEvent(
						StateSTag::ConstructMethodMessage("DiceManager", "InitialDieRoles",
							[&firstDieValues, &secondDieValues](XMLPrinter& printer) {
								NewElementWithText(printer, "PlayersFirstDieValue", std::to_string(firstDieValues.first));
								NewElementWithText(printer, "OpponentsFirstDieValue", std::to_string(firstDieValues.second));
								NewElementWithText(printer, "PlayersSecondDieValue", std::to_string(secondDieValues.first));
								NewElementWithText(printer, "OpponentsSecondDieValue", std::to_string(secondDieValues.second));
							}),
						StateSTag::ConstructMethodMessage("DiceManager", "InitialDieRoles",
							[&firstDieValues, &secondDieValues](XMLPrinter& printer) {
								NewElementWithText(printer, "PlayersFirstDieValue", std::to_string(firstDieValues.second));
								NewElementWithText(printer, "OpponentsFirstDieValue", std::to_string(firstDieValues.first));
								NewElementWithText(printer, "PlayersSecondDieValue", std::to_string(secondDieValues.second));
								NewElementWithText(printer, "OpponentsSecondDieValue", std::to_string(secondDieValues.first));
							}))
				};
			}
		}
	}
	else if (!strcmp(elEvent.Name(), "Move"))
	{
		XMLPrinter sanitizedMoveMessage;
		sanitizedMoveMessage.OpenElement("Message");
		sanitizedMoveMessage.OpenElement("Move");

		const tinyxml2::XMLElement* elDouble = elEvent.FirstChildElement("Double");
		if (elDouble && elDouble->GetText()) // Player has requested double
		{
			if (m_doubleRequested)
			{
				return {};
			}
			try
			{
				const int doubleBy = std::stoi(elDouble->GetText());
				if (doubleBy >= 2 && doubleBy % 2 == 0)
				{
					m_doubleRequested = true;

					sanitizedMoveMessage.OpenElement("Source");
					NewElementWithText(sanitizedMoveMessage, "X", "-1");
					NewElementWithText(sanitizedMoveMessage, "Y", "-1");
					sanitizedMoveMessage.CloseElement("Source");

					sanitizedMoveMessage.OpenElement("Target");
					NewElementWithText(sanitizedMoveMessage, "X", "-1");
					NewElementWithText(sanitizedMoveMessage, "Y", "-1");
					sanitizedMoveMessage.CloseElement("Target");

					NewElementWithText(sanitizedMoveMessage, "Double", std::to_string(doubleBy));

					sanitizedMoveMessage.CloseElement("Move");
					sanitizedMoveMessage.CloseElement("Message");
					return {
						sanitizedMoveMessage.print()
					};
				}
			}
			catch (const std::exception&) {}
		}
		else // Player has performed a regular move
		{
			const tinyxml2::XMLElement* elSource = elEvent.FirstChildElement("Source");
			const tinyxml2::XMLElement* elTarget = elEvent.FirstChildElement("Target");
			if (elSource && elTarget)
			{
				const tinyxml2::XMLElement* elSourceX = elSource->FirstChildElement("X");
				const tinyxml2::XMLElement* elSourceY = elSource->FirstChildElement("Y");
				const tinyxml2::XMLElement* elTargetX = elTarget->FirstChildElement("X");
				const tinyxml2::XMLElement* elTargetY = elTarget->FirstChildElement("Y");
				if (elSourceX && elSourceX->GetText() && elSourceY && elSourceY->GetText() &&
					elTargetX && elTargetX->GetText() && elTargetY && elTargetY->GetText())
				{
					try
					{
						const int sourceX = std::stoi(elSourceX->GetText());
						const int sourceY = std::stoi(elSourceY->GetText());
						const int targetX = std::stoi(elTargetX->GetText());
						const int targetY = std::stoi(elTargetY->GetText());

						sanitizedMoveMessage.OpenElement("Source");
						NewElementWithText(sanitizedMoveMessage, "X", std::to_string(sourceX));
						NewElementWithText(sanitizedMoveMessage, "Y", std::to_string(sourceY));
						sanitizedMoveMessage.CloseElement("Source");

						sanitizedMoveMessage.OpenElement("Target");
						NewElementWithText(sanitizedMoveMessage, "X", std::to_string(targetX));
						NewElementWithText(sanitizedMoveMessage, "Y", std::to_string(targetY));
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
					}
					catch (const std::exception&) {}
				}
			}
		}
	}
	else if (!strcmp(elEvent.Name(), "GameLogic"))
	{
		const tinyxml2::XMLElement* elMethod = elEvent.FirstChildElement("Method");
		if (elMethod && elMethod->GetText())
		{
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
				{
					return {};
				}
				m_doubleRequested = false;
				return {
					StateSTag::ConstructMethodMessage("GameLogic", "AcceptDouble")
				};
			}
			if (!strcmp(elMethod->GetText(), "RejectDouble")) // Player has rejected other player's double
			{
				if (!m_doubleRequested)
				{
					return {};
				}
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
				{
					return {};
				}
				const tinyxml2::XMLElement* elParams = elEvent.FirstChildElement("Params");
				if (elParams && elParams->GetText())
				{
					try
					{
						const int pointsOffered = std::stoi(elParams->GetText());
						if (pointsOffered >= 0)
						{
							m_resignRequested = true;
							return {
								StateSTag::ConstructMethodMessage("GameLogic", "RequestResign", elParams->GetText())
							};
						}
					}
					catch (const std::exception&) {}
				}
			}
			if (!strcmp(elMethod->GetText(), "AcceptResign")) // Player has accepted resign request of other player
			{
				if (!m_resignRequested)
				{
					return {};
				}
				return {
					StateSTag::ConstructMethodMessage("GameLogic", "AcceptResign")
				};
			}
			if (!strcmp(elMethod->GetText(), "RejectResign")) // Player has rejected resign request of other player
			{
				if (!m_resignRequested)
				{
					return {};
				}
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
			}
		}
	}
	else if (!strcmp(elEvent.Name(), "GameManagement"))
	{
		const tinyxml2::XMLElement* elMethod = elEvent.FirstChildElement("Method");
		if (elMethod && elMethod->GetText())
		{
			if (!strcmp(elMethod->GetText(), "ResignGiven")) // Player has resigned
			{
				if (!m_resignRequested)
				{
					return {};
				}
				m_resignRequested = false;
				m_startNextGameState = StartNextGameState::ALLOWED;
				return {
					QueuedEvent(
						StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver", "PlayerResigned," + std::string(caller.m_role == 0 ? "1" : "0")),
						true)
				};
			}
		}
	}
	return {};
}
