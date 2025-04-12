#include "CheckersMatch.hpp"

#include "PlayerSocket.hpp"
#include "Util.hpp"

CheckersMatch::CheckersMatch(PlayerSocket& player) :
	Match(player),
	m_drawOfferedBy(-1)
{}


std::vector<CheckersMatch::QueuedEvent>
CheckersMatch::ProcessEvent(const tinyxml2::XMLElement& elEvent, const PlayerSocket& caller)
{
	if (!strcmp(elEvent.Name(), "Move"))
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
					if (sourceX >= 0 && sourceX < 8 && sourceY >= 0 && sourceY < 8 &&
						targetX >= 0 && targetX < 8 && targetY >= 0 && targetY < 8)
					{
						XMLPrinter sanitizedMoveMessage;
						sanitizedMoveMessage.OpenElement("Message");
						sanitizedMoveMessage.OpenElement("Move");

						sanitizedMoveMessage.OpenElement("Source");
						NewElementWithText(sanitizedMoveMessage, "X", elSourceX->GetText());
						NewElementWithText(sanitizedMoveMessage, "Y", elSourceY->GetText());
						sanitizedMoveMessage.CloseElement("Source");

						sanitizedMoveMessage.OpenElement("Target");
						NewElementWithText(sanitizedMoveMessage, "X", elTargetX->GetText());
						NewElementWithText(sanitizedMoveMessage, "Y", elTargetY->GetText());
						sanitizedMoveMessage.CloseElement("Target");

						sanitizedMoveMessage.CloseElement("Move");
						sanitizedMoveMessage.CloseElement("Message");
						return {
							sanitizedMoveMessage.print()
						};
					}
				}
				catch (const std::exception&) {}
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
				m_state = STATE_GAMEOVER;
				return {
					StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver", "PlayerQuit")
				};
			}
			if (!strcmp(elMethod->GetText(), "OfferDraw")) // Player has offered a draw to their opponent
			{
				if (m_drawOfferedBy >= 0)
				{
					return {};
				}
				m_drawOfferedBy = caller.m_role;
				return {
					StateSTag::ConstructMethodMessage("GameManagement", "OfferDraw")
				};
			}
			if (!strcmp(elMethod->GetText(), "DrawAccept")) // Player has accepted a draw request by the opponent
			{
				if (m_drawOfferedBy < 0 || m_drawOfferedBy == caller.m_role)
				{
					return {};
				}
				m_drawOfferedBy = -1;
				m_state = STATE_GAMEOVER;
				return {
					QueuedEvent(
						StateSTag::ConstructMethodMessage("GameManagement", "ServerGameOver", "DrawOffered"),
						true)
				};
			}
			if (!strcmp(elMethod->GetText(), "DrawReject")) // Player has rejected a draw request by their opponent
			{
				if (m_drawOfferedBy < 0 || m_drawOfferedBy == caller.m_role)
				{
					return {};
				}
				m_drawOfferedBy = -1;
				return {
					StateSTag::ConstructMethodMessage("GameManagement", "DrawReject")
				};
			}
		}
	}
	return {};
}


std::string
CheckersMatch::ConstructGameInitXML(PlayerSocket* caller) const
{
	XMLPrinter printer;
	printer.OpenElement("GameInit");

	NewElementWithText(printer, "Role", std::to_string(caller->m_role));

	// Players
	printer.OpenElement("Players");
	for (PlayerSocket* player : m_players)
	{
		printer.OpenElement("Player");
		NewElementWithText(printer, "Role", std::to_string(player->m_role));
		NewElementWithText(printer, "Name", player->GetPUID());
		NewElementWithText(printer, "Type", "Human");
		printer.CloseElement("Player");
	}
	printer.CloseElement("Players");

	// Board
	printer.OpenElement("Board");
	NewElementWithText(printer, "Row", "0,1,0,1,0,1,0,1");
	NewElementWithText(printer, "Row", "1,0,1,0,1,0,1,0");
	NewElementWithText(printer, "Row", "0,1,0,1,0,1,0,1");
	NewElementWithText(printer, "Row", "0,0,0,0,0,0,0,0");
	NewElementWithText(printer, "Row", "0,0,0,0,0,0,0,0");
	NewElementWithText(printer, "Row", "3,0,3,0,3,0,3,0");
	NewElementWithText(printer, "Row", "0,3,0,3,0,3,0,3");
	NewElementWithText(printer, "Row", "3,0,3,0,3,0,3,0");
	printer.CloseElement("Board");

	NewElementWithText(printer, "GameType", "Standard");

	printer.CloseElement("GameInit");
	return printer;
}
