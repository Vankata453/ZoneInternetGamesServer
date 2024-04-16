#include "PlayerSocket.hpp"

#include <stdexcept>
#include <sstream>

#include <tinyxml2.h>

#include "MatchManager.hpp"
#include "Socket.hpp"
#include "Util.hpp"

PlayerSocket::PlayerSocket(SOCKET socket) :
	m_socket(socket),
	m_state(STATE_NOT_INITIALIZED),
	m_guid(),
	m_game(),
	m_match()
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

				m_state = STATE_JOINED;
				m_guid = StringSplit(receivedData[0], "=")[1];
				return { ConstructJoinContextMessage() };
			}
			break;

		case STATE_JOINED:
			if (StartsWith(receivedData[0], "PLAY match"))
			{
				return { ConstructReadyMessage(), ConstructStateMessage(ConstructReadyXML()) };
			}
			break;
	}
	return {};
}


void
PlayerSocket::OnGameStart()
{
	// Indicate the server is ready to the client
	//Socket::SendData(m_socket, {  });
}

void
PlayerSocket::OnDisconnected()
{
	if (m_match)
		m_match->DisconnectedPlayer(*this);
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
PlayerSocket::ConstructJoinContextMessage()
{
	// Construct JoinContext message
	std::stringstream out;
	out << "JoinContext " << m_match->GetGUID() << ' ' << m_guid << " 38&38&38&\r\n";
	return out.str();
}

std::string
PlayerSocket::ConstructReadyMessage()
{
	// Construct READY message
	std::stringstream out;
	out << "READY " << m_match->GetGUID() << "\r\n";
	return out.str();
}

std::string
PlayerSocket::ConstructStateMessage(const std::string& xml)
{
	// Get a hex number of XML string size
	std::stringstream hexsize;
	hexsize << std::hex << xml.length();

	// Construct STATE message
	std::stringstream out;
	out << "STATE " << m_match->GetGUID() << "\r\nLength: " << hexsize.str() << "\r\n\r\n" << xml << "\r\n";
	return out.str();
}


std::string
PlayerSocket::ConstructReadyXML()
{
	tinyxml2::XMLDocument doc;

	tinyxml2::XMLElement* elReadyMessage = doc.NewElement("ReadyMessage");
	doc.InsertFirstChild(elReadyMessage);

	tinyxml2::XMLElement* elEStatus = doc.NewElement("eStatus");
	elEStatus->SetText("Ready");
	elReadyMessage->InsertEndChild(elEStatus);

	tinyxml2::XMLElement* elSMode = doc.NewElement("sMode");
	elEStatus->SetText("normal");
	elReadyMessage->InsertEndChild(elSMode);

	tinyxml2::XMLPrinter printer;
	doc.Print(&printer);
	return printer.CStr();
}
