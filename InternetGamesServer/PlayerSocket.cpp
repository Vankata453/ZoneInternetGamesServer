#include "PlayerSocket.hpp"

#include <stdexcept>
#include <sstream>

#include "tinyxml2.h"

#include "MatchManager.hpp"
#include "Socket.hpp"
#include "Util.hpp"

PlayerSocket::PlayerSocket(SOCKET socket) :
	m_socket(socket),
	m_state(STATE_NOT_INITIALIZED),
	m_guid(),
	m_game(),
	m_match(),
	m_name(RandomString(8) + "      01"),
	m_role(-1)
{
}


std::vector<std::string>
PlayerSocket::GetResponse(const std::vector<std::string>& receivedData)
{
	switch (m_state)
	{
		case STATE_NOT_INITIALIZED:
			if (receivedData[0] == "STADIUM/2.0\r\n")
			{
				m_state = STATE_INITIALIZED;
				return { "STADIUM/2.0\r\n" };
			}
			break;

		case STATE_INITIALIZED:
			if (receivedData.size() >= 6 && StartsWith(receivedData[0], "JOIN Session="))
			{
				ParseGasTicket(receivedData[3]);

				// Find/Create a lobby (pending match), based on the game to be played
				m_match = MatchManager::Get().FindLobby(*this, m_game); // TODO: Find lobbies, based on skill level

				m_state = STATE_JOINING;
				m_guid = StringSplit(receivedData[0], "=")[1];
				return { ConstructJoinContextMessage() };
			}
			break;

		case STATE_JOINING:
			if (StartsWith(receivedData[0], "PLAY match"))
			{
				m_state = STATE_WAITINGFOROPPONENTS;
				return { ConstructReadyMessage(), ConstructStateMessage(m_match->ConstructReadyXML()) };
			}
			break;

		case STATE_PLAYING:
			if (StartsWith(receivedData[0], "CALL GameReady")) // Game is ready, start it
			{
				// Send a game start message
				std::vector<std::unique_ptr<StateTag>> tags;
				tags.push_back(m_match->ConstructGameStartSTag());
				return { ConstructStateMessage(m_match->ConstructStateXML(tags)) };
			}
			else if (receivedData[0] == "CALL EventSend messageID=EventSend" && receivedData.size() > 1 &&
				StartsWith(receivedData[1], "XMLDataString=")) // An event is being sent, let the Match send it to all players
			{
				m_match->EventSend(this, receivedData[1].substr(14)); // Remove "XMLDataString=" from the beginning
				return {};
			}
			break;
	}
	return {};
}


void
PlayerSocket::OnGameStart()
{
	if (m_state != STATE_WAITINGFOROPPONENTS)
		return;

	m_state = STATE_PLAYING;

	// Send a game initialization message
	std::vector<std::unique_ptr<StateTag>> tags;
	tags.push_back(m_match->ConstructGameInitSTag(this));
	Socket::SendData(m_socket, { ConstructStateMessage(m_match->ConstructStateXML(tags)) });
}

void
PlayerSocket::OnDisconnected()
{
	if (m_match)
		m_match->DisconnectedPlayer(*this);
}

void
PlayerSocket::OnEventReceive(const std::string& XMLDataString) const
{
	// Send an event receive message
	std::vector<std::unique_ptr<StateTag>> tags;
	tags.push_back(m_match->ConstructEventReceiveSTag(DecodeURL(XMLDataString)));
	Socket::SendData(m_socket, { ConstructStateMessage(m_match->ConstructStateXML(tags)) });
}


void
PlayerSocket::ParseGasTicket(const std::string& xml)
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError status = doc.Parse(xml.substr(10).c_str()); // Remove "GasTicket=" from the beginning
	if (status != tinyxml2::XML_SUCCESS)
		throw std::runtime_error("Corrupted data received: Error parsing GasTicket: " + status);

	tinyxml2::XMLElement* elAnon = doc.RootElement();
	if (!elAnon)
		throw std::runtime_error("Corrupted data received: No root element in GasTicket.");

	tinyxml2::XMLElement* elPub = elAnon->FirstChildElement("pub");
	if (!elPub)
		throw std::runtime_error("Corrupted data received: No \"<pub>...</pub>\" in GasTicket.");

	// "Game"
	tinyxml2::XMLElement* elGame = elPub->FirstChildElement("Game");
	if (!elGame || !elGame->GetText())
		throw std::runtime_error("Corrupted data received: No \"<Game>...</Game>\" in GasTicket.");
	m_game = Match::GameFromString(elGame->GetText());
}


std::string
PlayerSocket::ConstructJoinContextMessage() const
{
	// Construct JoinContext message
	std::stringstream out;
	out << "JoinContext " << m_match->GetGUID() << ' ' << m_guid << " 38&38&38&\r\n";
	return out.str();
}

std::string
PlayerSocket::ConstructReadyMessage() const
{
	// Construct READY message
	std::stringstream out;
	out << "READY " << m_match->GetGUID() << "\r\n";
	return out.str();
}

std::string
PlayerSocket::ConstructStateMessage(const std::string& xml) const
{
	// Get a hex number of XML string size
	std::stringstream hexsize;
	hexsize << std::hex << xml.length();

	// Construct STATE message
	std::stringstream out;
	out << "STATE " << m_match->GetGUID() << "\r\nLength: " << hexsize.str() << "\r\n\r\n" << xml << "\r\n";
	return out.str();
}
