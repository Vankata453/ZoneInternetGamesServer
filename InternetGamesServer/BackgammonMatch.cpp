#include "BackgammonMatch.hpp"

#include "tinyxml2.h"

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
BackgammonMatch::ClearState()
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
BackgammonMatch::ProcessEvent(const std::string& xml, const PlayerSocket* caller)
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError status = doc.Parse(xml.c_str());
	if (status != tinyxml2::XML_SUCCESS)
		return { xml };

	tinyxml2::XMLElement* elMessage = doc.RootElement();
	if (!elMessage || strcmp(elMessage->Name(), "Message"))
		return { xml };

	// Dice management events
	tinyxml2::XMLElement* elDiceManager = elMessage->FirstChildElement("DiceManager");
	if (elDiceManager)
	{
		// Method
		tinyxml2::XMLElement* elMethod = elDiceManager->FirstChildElement("Method");
		if (elMethod && elMethod->GetText())
		{
			if (!strcmp(elMethod->GetText(), "GenerateValues")) // Generate dice values
			{
				const std::pair<int, int> dieValues = { s_dieDistribution(m_rng), s_dieDistribution(m_rng) };

				return {
					QueuedEvent(
						StateSTag::ConstructMethodMessage("DiceManager", "DieRolls",
						[caller, &dieValues](tinyxml2::XMLElement* elParams) {
							NewElementWithText(elParams, "Seat", std::to_string(caller->m_role));
							NewElementWithText(elParams, "FirstDie", std::to_string(dieValues.first));
							NewElementWithText(elParams, "SecondDie", std::to_string(dieValues.second));
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
							[&firstDieValues, &secondDieValues](tinyxml2::XMLElement* elParams) {
								NewElementWithText(elParams, "PlayersFirstDieValue", std::to_string(firstDieValues.first));
								NewElementWithText(elParams, "OpponentsFirstDieValue", std::to_string(firstDieValues.second));
								NewElementWithText(elParams, "PlayersSecondDieValue", std::to_string(secondDieValues.first));
								NewElementWithText(elParams, "OpponentsSecondDieValue", std::to_string(secondDieValues.second));
							}),
						StateSTag::ConstructMethodMessage("DiceManager", "InitialDieRoles",
							[&firstDieValues, &secondDieValues](tinyxml2::XMLElement* elParams) {
								NewElementWithText(elParams, "PlayersFirstDieValue", std::to_string(firstDieValues.second));
								NewElementWithText(elParams, "OpponentsFirstDieValue", std::to_string(firstDieValues.first));
								NewElementWithText(elParams, "PlayersSecondDieValue", std::to_string(secondDieValues.second));
								NewElementWithText(elParams, "OpponentsSecondDieValue", std::to_string(secondDieValues.first));
							}))
				};
			}
		}

		return { xml };
	}
	// Move events
	tinyxml2::XMLElement* elMove = elMessage->FirstChildElement("Move");
	if (elMove)
	{
		// Method
		tinyxml2::XMLElement* elDouble = elMove->FirstChildElement("Double");
		if (elDouble && elDouble->GetText()) // Player has requested double
		{
			if (m_doubleRequested)
			{
				return {};
			}
			m_doubleRequested = true;
		}
		else
		{
			m_lastMovePlayer = caller->m_role;
			m_lastMoveHomeTableStone = false;

			tinyxml2::XMLElement* elTarget = elMove->FirstChildElement("Target");
			if (elTarget)
			{
				tinyxml2::XMLElement* elTargetX = elTarget->FirstChildElement("X");
				tinyxml2::XMLElement* elTargetY = elTarget->FirstChildElement("Y");
				if (elTargetX && elTargetX->GetText() && elTargetY && elTargetY->GetText())
				{
					if ((caller->m_role == 0 && !strcmp(elTargetX->GetText(), "25")) ||
						(caller->m_role == 1 && !strcmp(elTargetX->GetText(), "26"))) // Player has moved stone to home table
					{
						try
						{
							const int targetY = std::stoi(elTargetY->GetText());
							if (targetY >= 0 && targetY < 15)
							{
								int& homeTableStones = caller->m_role == 0 ? m_homeTableStones.first : m_homeTableStones.second;
								++homeTableStones;
								m_lastMoveHomeTableStone = true;

								if (homeTableStones >= 15) // All stones are in the player's home board
								{
									m_startNextGameState = StartNextGameState::ALLOWED;
									return {
										QueuedEvent(
											StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver", "BearOff," + std::to_string(caller->m_role)),
											true)
									};
								}
							}
						}
						catch (const std::exception&) {}
					}
				}
			}
		}

		return { xml };
	}
	// Game logic events
	tinyxml2::XMLElement* elGameLogic = elMessage->FirstChildElement("GameLogic");
	if (elGameLogic)
	{
		// Method
		tinyxml2::XMLElement* elMethod = elGameLogic->FirstChildElement("Method");
		if (elMethod && elMethod->GetText())
		{
			if (!strcmp(elMethod->GetText(), "UndoLast")) // Player has undone their last move
			{
				if (m_lastMovePlayer == caller->m_role && m_lastMoveHomeTableStone) // Player has moved stone from home table
				{
					int& homeTableStones = m_lastMovePlayer == 0 ? m_homeTableStones.first : m_homeTableStones.second;
					--homeTableStones;
					m_lastMoveHomeTableStone = false;
				}
			}
			else if (!strcmp(elMethod->GetText(), "AcceptDouble")) // Player has accepted other player's double
			{
				if (!m_doubleRequested)
				{
					return {};
				}
				m_doubleRequested = false;
			}
			else if (!strcmp(elMethod->GetText(), "RejectDouble")) // Player has rejected other player's double
			{
				if (!m_doubleRequested)
				{
					return {};
				}
				m_doubleRequested = false;
				m_startNextGameState = StartNextGameState::ALLOWED;
				return {
					QueuedEvent(
						StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver", "DoubleDecline," + std::string(caller->m_role == 0 ? "1" : "0")),
						true)
				};
			}
			else if (!strcmp(elMethod->GetText(), "RequestResign")) // Player has requested resign
			{
				if (m_resignRequested)
				{
					return {};
				}
				m_resignRequested = true;
			}
			else if (!strcmp(elMethod->GetText(), "AcceptResign")) // Player has accepted resign of other player
			{
				if (!m_resignRequested)
				{
					return {};
				}
			}
			else if (!strcmp(elMethod->GetText(), "RejectResign")) // Player has rejected resign of other player
			{
				if (!m_resignRequested)
				{
					return {};
				}
				m_resignRequested = false;
			}
			else if (!strcmp(elMethod->GetText(), "StartNextGame")) // Client has requested the start of the next game
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
						m_startNextGameRequestedOnceBy = caller->m_role;
						break;
					}
					case StartNextGameState::REQUESTED_ONCE:
					{
						if (m_startNextGameRequestedOnceBy != caller->m_role)
						{
							ClearState();
						}
						break;
					}
				}
				return {};
			}
		}

		return { xml };
	}
	// Game management events
	tinyxml2::XMLElement* elGameManagement = elMessage->FirstChildElement("GameManagement");
	if (elGameManagement)
	{
		// Method
		tinyxml2::XMLElement* elMethod = elGameManagement->FirstChildElement("Method");
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
						StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver", "PlayerResigned," + std::string(caller->m_role == 0 ? "1" : "0")),
						true)
				};
			}
		}
	}

	return { xml };
}


std::string
BackgammonMatch::ConstructGameInitXML(PlayerSocket* caller) const
{
	tinyxml2::XMLDocument doc;

	tinyxml2::XMLElement* elGameInit = doc.NewElement("GameInit");
	doc.InsertFirstChild(elGameInit);
	NewElementWithText(elGameInit, "Role", std::to_string(caller->m_role));

	// Players
	tinyxml2::XMLElement* elPlayers = doc.NewElement("Players");
	for (PlayerSocket* player : m_players)
	{
		tinyxml2::XMLElement* elPlayer = doc.NewElement("Player");
		NewElementWithText(elPlayer, "Role", std::to_string(player->m_role));
		NewElementWithText(elPlayer, "Name", player->GetPUID());
		NewElementWithText(elPlayer, "Type", "Human");
		elPlayers->InsertEndChild(elPlayer);
	}
	elGameInit->InsertEndChild(elPlayers);

	return PrintXML(doc);
}
