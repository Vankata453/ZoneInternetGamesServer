#include "CheckersMatch.hpp"

#include "tinyxml2.h"

#include "PlayerSocket.hpp"
#include "Util.hpp"

CheckersMatch::CheckersMatch(PlayerSocket& player) :
	Match(player),
	m_drawOffered(false)
{}


CheckersMatch::QueuedEvent
CheckersMatch::ProcessEvent(const std::string& xml)
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError status = doc.Parse(xml.c_str());
	if (status != tinyxml2::XML_SUCCESS)
		return xml;

	tinyxml2::XMLElement* elMessage = doc.RootElement();
	if (!elMessage || strcmp(elMessage->Name(), "Message"))
		return xml;

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
				m_state = STATE_GAMEOVER;
				return StateSTag::ConstructGameManagementMessage("ServerGameOver", "PlayerQuit");
			}
			else if (!strcmp(elMethod->GetText(), "OfferDraw")) // Player has offered a draw to their opponent
			{
				m_drawOffered = true;
				return xml;
			}
			else if (!strcmp(elMethod->GetText(), "DrawReject")) // Player has rejected a draw from their opponent
			{
				m_drawOffered = false;
				return xml;
			}
			else if (!strcmp(elMethod->GetText(), "DrawAccept")) // Player has accepted a draw by the opponent
			{
				if (!m_drawOffered)
					return xml;

				m_state = STATE_GAMEOVER;
				return QueuedEvent(StateSTag::ConstructGameManagementMessage("ServerGameOver", "DrawOffered"), true);
			}
		}
	}

	return xml;
}


std::string
CheckersMatch::ConstructGameInitXML(PlayerSocket* caller) const
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

	// Board
	tinyxml2::XMLElement* elBoard = doc.NewElement("Board");
	NewElementWithText(elBoard, "Row", "0,1,0,1,0,1,0,1");
	NewElementWithText(elBoard, "Row", "1,0,1,0,1,0,1,0");
	NewElementWithText(elBoard, "Row", "0,1,0,1,0,1,0,1");
	NewElementWithText(elBoard, "Row", "0,0,0,0,0,0,0,0");
	NewElementWithText(elBoard, "Row", "0,0,0,0,0,0,0,0");
	NewElementWithText(elBoard, "Row", "3,0,3,0,3,0,3,0");
	NewElementWithText(elBoard, "Row", "0,3,0,3,0,3,0,3");
	NewElementWithText(elBoard, "Row", "3,0,3,0,3,0,3,0");
	elGameInit->InsertEndChild(elBoard);

	NewElementWithText(elGameInit, "GameType", "Standard");

	return PrintXML(doc);
}
