#include "Match.hpp"

#include <algorithm>
#include <iostream>
#include <rpcdce.h>
#include <synchapi.h>
#include <sstream>

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


Match::QueuedEvent::QueuedEvent(const std::string& xml_, bool includeSender_) :
	xml(xml_),
	xmlSender(),
	includeSender(includeSender_)
{}

Match::QueuedEvent::QueuedEvent(const std::string& xml_, const std::string& xmlSender_) :
	xml(xml_),
	xmlSender(xmlSender_),
	includeSender(false)
{}


#define MATCH_NO_DISCONNECT_ON_PLAYER_LEAVE 0 // DEBUG: If a player leaves a match, do not disconnect other players.

Match::Match(PlayerSocket& player) :
	m_state(STATE_WAITINGFORPLAYERS),
	m_guid(),
	m_level(player.GetLevel()),
	m_players(),
	m_eventMutex(CreateMutex(nullptr, false, nullptr)),
	m_creationTime(std::time(nullptr)),
	m_endTime(0)
{
	// Generate a unique GUID for the match
	UuidCreate(&m_guid);

	JoinPlayer(player);
}

Match::~Match()
{
	for (PlayerSocket* p : m_players)
		p->OnMatchEnded();

	CloseHandle(m_eventMutex);
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

#if not MATCH_NO_DISCONNECT_ON_PLAYER_LEAVE
	// Originally, servers replaced players who have left the game with AI.
	// However, since there is no logic support for any of the games on this server currently,
	// if currently playing a game, we end the game directly by disconnecting everyone.
	// NOTE: The server doesn't know when a game has finished with a win, so this has the drawback of causing
	//       an "Error communicating with server" message after a game has finished with a win
	//       (even though since the game has ended anyway, it's not really important).
	if (m_state == STATE_PLAYING)
		m_state = STATE_ENDED;
#endif
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

				std::cout << "[MATCH] " << m_guid << ": Started match!" << std::endl;
				m_state = STATE_PLAYING;
			}
			break;
		}
		case STATE_GAMEOVER:
		{
			if (m_endTime == 0)
			{
				std::cout << "[MATCH] " << m_guid << ": Game over, match will automatically close in 60 seconds!" << std::endl;
				m_endTime = std::time(nullptr);
			}
			else if (std::time(nullptr) - m_endTime >= 60) // A minute has passed since the match ended
			{
				std::cout << "[MATCH] " << m_guid << ": Match ended a minute ago, closing!" << std::endl;
				m_state = STATE_ENDED;
			}
			break;
		}

		default:
			break;
	}
}


void
Match::EventSend(const PlayerSocket& caller, const std::string& xml)
{
	/* Get event XML element */
	tinyxml2::XMLDocument eventDoc;
	tinyxml2::XMLError status = eventDoc.Parse(xml.c_str());
	if (status != tinyxml2::XML_SUCCESS)
		return;

	const tinyxml2::XMLElement* elMessage = eventDoc.RootElement();
	if (!elMessage || strcmp(elMessage->Name(), "Message"))
		return;

	const tinyxml2::XMLElement* elEvent = elMessage->FirstChildElement();
	if (!elEvent || !elEvent->Name())
		return;

	/* Process event */
	switch (WaitForSingleObject(m_eventMutex, 5000))
	{
		case WAIT_OBJECT_0: // Acquired ownership of the event mutex
		{
			const std::vector<QueuedEvent> events = ProcessEvent(*elEvent, caller);
			for (const QueuedEvent& ev : events)
			{
				if (!ev.xml.empty())
				{
					const bool includeSender = ev.includeSender && ev.xmlSender.empty();
					for (const PlayerSocket* p : m_players)
					{
						if (includeSender || p != &caller)
							p->OnEventReceive(ev.xml);
					}
				}
				if (!ev.xmlSender.empty())
				{
					caller.OnEventReceive(ev.xmlSender);
				}
			}

			if (!ReleaseMutex(m_eventMutex))
				throw std::runtime_error("Match::EventSend(): Couldn't release event mutex: " + GetLastError());
			break;
		}

		case WAIT_ABANDONED: // Acquired ownership of an abandoned event mutex
			throw std::runtime_error("Match::EventSend(): Got ownership of an abandoned event mutex: " + GetLastError());
	}
}

void
Match::Chat(StateChatTag tag)
{
	// Validate the chat event
	if (tag.text != "SYS_CHATON" && // Chat turned on
		tag.text != "SYS_CHATOFF" && // Chat turned off
		!IsValidChatNudgeMessage(tag.text)) // Nudge
	{
		try
		{
			// Ensure the chat message has a valid ID, as clients tend to accept any other custom messages
			const uint8_t msgID = static_cast<uint8_t>(std::stoi(tag.text));
			if (msgID < 1)
				return;
			if (msgID > 24)
			{
				const std::pair<uint8_t, uint8_t> customMsgRange = GetCustomChatMessagesRange();
				if (msgID < customMsgRange.first || msgID > customMsgRange.second)
					return;
			}

			tag.text = std::to_string(msgID);
		}
		catch (const std::exception&)
		{
			return;
		}
	}

	// Send the event to all other players
	for (PlayerSocket* p : m_players)
		p->OnChat(&tag);
}


std::vector<Match::QueuedEvent>
Match::ProcessEvent(const tinyxml2::XMLElement&, const PlayerSocket&)
{
	return {};
}


std::string
Match::ConstructReadyXML() const
{
	XMLPrinter printer;
	printer.OpenElement("ReadyMessage");

	NewElementWithText(printer, "eStatus", "Ready");
	NewElementWithText(printer, "sMode", "normal");

	printer.CloseElement("ReadyMessage");
	return printer;
}

std::string
Match::ConstructStateXML(const std::vector<const StateTag*> tags) const
{
	XMLPrinter printer;
	printer.OpenElement("StateMessage");

	NewElementWithText(printer, "nSeq", "4"); // TODO: Figure out what "nSeq" is for. Currently it doesn't seem to matter
	NewElementWithText(printer, "nRole", "0"); // TODO: Figure out what "nRole" is for. Currently it doesn't seem to matter
	NewElementWithText(printer, "eStatus", "Ready");
	NewElementWithText(printer, "nTimestamp", std::time(nullptr) - m_creationTime);
	NewElementWithText(printer, "sMode", "normal");

	// Tags
	printer.OpenElement("arTags");
	for (const StateTag* tag : tags)
		tag->AppendToTags(printer);
	printer.CloseElement("arTags");

	printer.CloseElement("StateMessage");
	return printer;
}

std::string
Match::ConstructGameInitXML(PlayerSocket* caller) const
{
	XMLPrinter printer;
	printer.OpenElement("GameInit");

	NewElementWithText(printer, "Role", caller->m_role);

	// Players
	printer.OpenElement("Players");
	for (PlayerSocket* player : m_players)
	{
		printer.OpenElement("Player");
		NewElementWithText(printer, "Role", player->m_role);
		NewElementWithText(printer, "Name", player->GetPUID());
		NewElementWithText(printer, "Type", "Human");
		printer.CloseElement("Player");
	}
	printer.CloseElement("Players");

	AppendToGameInitXML(printer, caller);

	printer.CloseElement("GameInit");
	return printer;
}

std::vector<std::string>
Match::ConstructGameStartMessagesXML(const PlayerSocket&) const
{
	return {};
}


bool
Match::IsValidChatNudgeMessage(const std::string& msg) const
{
	return msg == "1400_12345" || // Nudge (player 1)
		msg == "1400_12346"; // Nudge (player 2)
}
