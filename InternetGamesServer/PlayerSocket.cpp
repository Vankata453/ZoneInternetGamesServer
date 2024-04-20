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
	m_name(RandomString(8)),
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
				const StateSTag tag = m_match->ConstructGameStartSTag();
				return { ConstructStateMessage(m_match->ConstructStateXML({ &tag })) };
			}
			else if (receivedData[0] == "CALL EventSend messageID=EventSend" && receivedData.size() > 1 &&
				StartsWith(receivedData[1], "XMLDataString=")) // An event is being sent, let the Match send it to all players
			{
				m_match->EventSend(this, receivedData[1].substr(14)); // Remove "XMLDataString=" from the beginning
				return {};
			}
			else if (StartsWith(receivedData[0], "CALL Chat") && receivedData.size() > 1) // A chat message was sent, let the Match send it to all players
			{
				StateChatTag tag;
				tag.userID = m_guid.substr(1, m_guid.size() - 2); // Remove braces from beginning and end
				tag.nickname = m_name;
				tag.text = receivedData[0].substr(20); // Remove "CALL Chat sChatText=" from the beginning
				tag.fontFace = DecodeURL(receivedData[1].substr(10)); // Remove "sFontFace=" from the beginning
				tag.fontFlags = receivedData[2].substr(13); // Remove "arfFontFlags=" from the beginning
				tag.fontColor = receivedData[3].substr(11); // Remove "eFontColor=" from the beginning
				tag.fontCharSet = receivedData[4].substr(11); // Remove "eFontCharSet=" from the beginning

				m_match->ChatByID(this, std::move(tag));
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
	const StateSTag tag = m_match->ConstructGameInitSTag(this);
	Socket::SendData(m_socket, { ConstructStateMessage(m_match->ConstructStateXML({ &tag })) });
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
	const StateSTag tag = m_match->ConstructEventReceiveSTag(DecodeURL(XMLDataString));
	Socket::SendData(m_socket, { ConstructStateMessage(m_match->ConstructStateXML({ &tag })) });
}

void
PlayerSocket::OnChatByID(const StateChatTag* tag)
{
	// Send the "chatbyid" tag
	Socket::SendData(m_socket, { ConstructStateMessage(m_match->ConstructStateXML({ tag })) });
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
