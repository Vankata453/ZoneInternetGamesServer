#include "Match.hpp"

#include <algorithm>
#include <rpcdce.h>
#include <sstream>

#include "tinyxml2.h"

#include "PlayerSocket.hpp"
#include "Util.hpp"

Match::Game
Match::GameFromString(const std::string& str)
{
	if (str == "wnbk")
		return Game::BACKGAMMON;
	else if (str == "wnck")
		return Game::CHECKERS;
	else if (str == "wnsp")
		return Game::SPADES;

	return Game::INVALID;
}


Match::Match(PlayerSocket& player) :
	m_state(STATE_WAITINGFORPLAYERS),
	m_guid(),
	m_players(),
	m_creationTime(std::time(nullptr))
{
	// Generate a unique GUID for the match
	UuidCreate(&m_guid);

	JoinPlayer(player);
}


void
Match::JoinPlayer(PlayerSocket& player)
{
	if (m_state != STATE_WAITINGFORPLAYERS)
		return;

	// Add to players array
	m_players.push_back(&player);

	// Switch state, if enough players have joined
	if (m_players.size() == GetRequiredPlayerCount())
		m_state = STATE_PENDINGSTART;
}

void
Match::DisconnectedPlayer(PlayerSocket& player)
{
	// Remove from players array
	m_players.erase(std::remove(m_players.begin(), m_players.end(), &player), m_players.end());

	// TODO: Indicate that to remaining players in some way?
}


void
Match::Update()
{
	switch (m_state)
	{
		case STATE_PENDINGSTART:
		{
			// Start the game, if all players are waiting for opponents
			if (std::all_of(m_players.begin(), m_players.end(),
				[](const auto& player) { return player->GetState() == PlayerSocket::STATE_WAITINGFOROPPONENTS; }))
			{
				// Distribute unique role IDs for each player, starting from 0
				const int playerCount = static_cast<int>(m_players.size());
				const std::vector<int> roles = GenerateUniqueRandomNums(0, playerCount - 1);
				for (int i = 0; i < playerCount; i++)
					m_players[i]->m_role = roles[i];

				for (PlayerSocket* p : m_players)
					p->OnGameStart();

				m_state = STATE_PLAYING;
			}
			break;
		}

		default:
			break;
	}
}


void
Match::EventSend(const PlayerSocket* caller, const std::string& XMLDataString) const
{
	// Send the event to all other players
	for (PlayerSocket* p : m_players)
	{
		if (p != caller)
			p->OnEventReceive(XMLDataString);
	}
}


std::string
Match::ConstructReadyXML() const
{
	tinyxml2::XMLDocument doc;

	tinyxml2::XMLElement* elReadyMessage = doc.NewElement("ReadyMessage");
	doc.InsertFirstChild(elReadyMessage);

	NewElementWithText(elReadyMessage, "eStatus", "Ready");
	NewElementWithText(elReadyMessage, "sMode", "normal");

	return PrintXML(doc);
}

std::string
Match::ConstructStateXML(const std::vector<std::unique_ptr<StateTag>>& tags) const
{
	tinyxml2::XMLDocument doc;

	tinyxml2::XMLElement* elStateMessage = doc.NewElement("StateMessage");
	doc.InsertFirstChild(elStateMessage);

	NewElementWithText(elStateMessage, "nSeq", "4"); // TODO: Figure out what "nSeq" is for. Currently it doesn't seem to matter
	NewElementWithText(elStateMessage, "nRole", "0"); // TODO: Figure out what "nRole" is for. Currently it doesn't seem to matter
	NewElementWithText(elStateMessage, "eStatus", "Ready");
	NewElementWithText(elStateMessage, "nTimestamp", std::to_string(std::time(nullptr) - m_creationTime));
	NewElementWithText(elStateMessage, "sMode", "normal");

	// Tags
	tinyxml2::XMLElement& elArTags = *doc.NewElement("arTags");
	for (const auto& tag : tags)
		tag->AppendToTags(elArTags);
	elStateMessage->InsertEndChild(&elArTags);

	return PrintXML(doc);
}


std::unique_ptr<StateTag> 
Match::ConstructGameInitSTag(PlayerSocket* caller) const
{
	auto sTag = std::make_unique<StateSTag>();
	sTag->msgID = "GameInit";
	sTag->msgIDSbky = "GameInit";
	sTag->msgD = ConstructGameInitXML(caller);
	return sTag;
}

std::unique_ptr<StateTag>
Match::ConstructGameStartSTag() const
{
	auto sTag = std::make_unique<StateSTag>();
	sTag->msgID = "GameStart";
	sTag->msgIDSbky = "GameStart";
	return sTag;
}

std::unique_ptr<StateTag>
Match::ConstructEventReceiveSTag(const std::string& xml) const
{
	auto sTag = std::make_unique<StateSTag>();
	sTag->msgID = "EventReceive";
	sTag->msgIDSbky = "EventReceive";
	sTag->msgD = xml;
	return sTag;
}
