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

std::string
Match::GameToNameString(Match::Game game)
{
	switch (game)
	{
		case Game::BACKGAMMON:
			return "Backgammon";
		case Game::CHECKERS:
			return "Checkers";
		case Game::SPADES:
			return "Spades";
		default:
			return "Invalid";
	}
}

Match::Level
Match::LevelFromPublicELO(const std::string& str)
{
	if (str == "1000")
		return Level::BEGINNER;
	else if (str == "2000")
		return Level::INTERMEDIATE;
	else if (str == "3000")
		return Level::EXPERT;

	return Level::INVALID;
}


Match::Match(PlayerSocket& player) :
	m_state(STATE_WAITINGFORPLAYERS),
	m_guid(),
	m_level(player.GetLevel()),
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
	if (!m_players.empty())
		m_players.erase(std::remove(m_players.begin(), m_players.end(), &player), m_players.end());

	// End the match on no players, marking it as to-be-removed from MatchManager
	if (m_players.empty())
	{
		m_state = STATE_ENDED;
		return;
	}

	// Originally, servers replaced players who have left the game with AI.
	// However, since there is no logic support for any of the games on this server currently,
	// if currently playing a game, we end the game directly.
	if (m_state == STATE_PLAYING)
	{
		m_state = STATE_ENDED;
		for (PlayerSocket* p : m_players)
		{
			if (p != &player)
				p->OnMatchEnded();
		}
	}
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
Match::EventSend(const PlayerSocket* caller, const std::string& xml)
{
	const QueuedEvent ev = ProcessEvent(xml);
	if (ev.xml.empty()) return;

	// Send the event to all players, including the sender only if specified by the event
	for (PlayerSocket* p : m_players)
	{
		if (ev.includeSender || p != caller)
			p->OnEventReceive(ev.xml);
	}
}

void
Match::Chat(const StateChatTag tag)
{
	// Send the event to all other players
	for (PlayerSocket* p : m_players)
	{
		p->OnChat(&tag);
	}
}


Match::QueuedEvent
Match::ProcessEvent(const std::string& xml)
{
	return xml;
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
Match::ConstructStateXML(const std::vector<const StateTag*> tags) const
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
	for (const StateTag* tag : tags)
		tag->AppendToTags(elArTags);
	elStateMessage->InsertEndChild(&elArTags);

	return PrintXML(doc);
}


StateSTag
Match::ConstructGameInitSTag(PlayerSocket* caller) const
{
	StateSTag sTag;
	sTag.msgID = "GameInit";
	sTag.msgIDSbky = "GameInit";
	sTag.msgD = ConstructGameInitXML(caller);
	return sTag;
}

StateSTag
Match::ConstructGameStartSTag() const
{
	StateSTag sTag;
	sTag.msgID = "GameStart";
	sTag.msgIDSbky = "GameStart";
	return sTag;
}

StateSTag
Match::ConstructEventReceiveSTag(const std::string& xml) const
{
	StateSTag sTag;
	sTag.msgID = "EventReceive";
	sTag.msgIDSbky = "EventReceive";
	sTag.msgD = xml;
	return sTag;
}
