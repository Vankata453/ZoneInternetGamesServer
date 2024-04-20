#include "CheckersMatch.hpp"

#include "tinyxml2.h"

#include "PlayerSocket.hpp"
#include "Util.hpp"

CheckersMatch::CheckersMatch(PlayerSocket& player) :
	Match(player)
{}


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
