#include "BackgammonMatch.hpp"

#include "tinyxml2.h"

#include "PlayerSocket.hpp"
#include "Util.hpp"

BackgammonMatch::BackgammonMatch(PlayerSocket& player) :
	Match(player),
	m_initialRollStarted(false)
{}


std::string
BackgammonMatch::ConstructEndMatchMessage() const
{
	// Give the win to the other player (same as if the opponent resigned)
	return "";// StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver", "PlayerResigned");
}


BackgammonMatch::QueuedEvent
BackgammonMatch::ProcessEvent(const std::string& xml)
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError status = doc.Parse(xml.c_str());
	if (status != tinyxml2::XML_SUCCESS)
		return xml;

	tinyxml2::XMLElement* elMessage = doc.RootElement();
	if (!elMessage || strcmp(elMessage->Name(), "Message"))
		return xml;

	// Dice management events
	tinyxml2::XMLElement* elDiceManager = elMessage->FirstChildElement("DiceManager");
	if (elDiceManager)
	{
		// Method
		tinyxml2::XMLElement* elMethod = elDiceManager->FirstChildElement("Method");
		if (elMethod && elMethod->GetText())
		{
			if (!strcmp(elMethod->GetText(), "GenerateInitialValues")) // Generate initial dice values
			{
				if (!m_initialRollStarted)
				{
					m_initialRollStarted = true;
					return QueuedEvent("");
				}
				return QueuedEvent(StateSTag::ConstructMethodMessage("DiceManager", "DieRolls", "41234124"), true);
			}
		}
	}

	return xml;
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
