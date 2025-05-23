#define _WINSOCK_DEPRECATED_NO_WARNINGS // Allows usage of inet_ntoa()

#include "Socket.hpp"

#include <cassert>
#include <sstream>
#include <iostream>
#include <fstream>
#include <random>

#include "PlayerSocket.hpp"
#include "Util.hpp"
#include "XP/Protocol/Init.hpp"
#include "XP/Security.hpp"

#define DEFAULT_BUFLEN 2048

static const char* SOCKET_WIN7_HI_RESPONSE = "STADIUM/2.0\r\n";

std::string Socket::s_logsDirectory = "";
bool Socket::s_logPingMessages = false;


// Handler for the thread of a socket
DWORD WINAPI
Socket::SocketHandler(void* socket_)
{
	const SOCKET rawSocket = reinterpret_cast<SOCKET>(socket_);

	// Open a stream to log Socket events to
	std::unique_ptr<std::ostream> logStream;
	if (s_logsDirectory.empty())
	{
		logStream = std::make_unique<NullStream>();
	}
	else
	{
		std::ostringstream logFileName;
		logFileName << s_logsDirectory << "\\SOCKET_" << GetAddressString(rawSocket, '_')
			<< "_" << std::time(nullptr) << ".txt";

		logStream = std::make_unique<std::ofstream>(logFileName.str());
		if (!static_cast<std::ofstream*>(logStream.get())->is_open())
			throw std::runtime_error("Failed to open log file \"" + logFileName.str() + "\"!");
	}

	Socket socket(rawSocket, *logStream);
	try
	{
		std::unique_ptr<PlayerSocket> player; // TODO: PlayerSocket base class

		// Determine socket type, create PlayerSocket object and parse initial message
		char receivedBuf[DEFAULT_BUFLEN];
		const int receivedLen = socket.ReceiveData(receivedBuf, DEFAULT_BUFLEN);
		if (!strcmp(receivedBuf, SOCKET_WIN7_HI_RESPONSE)) // SOCKET_WIN7
		{
			player = std::make_unique<PlayerSocket>(socket);

			socket.SendData(SOCKET_WIN7_HI_RESPONSE, static_cast<int>(strlen(SOCKET_WIN7_HI_RESPONSE)));
		}
		else if (receivedLen == sizeof(XP::MsgConnectionHi)) // SOCKET_WINXP
		{
			XP::MsgConnectionHi hiMessage;
			memcpy(&hiMessage, receivedBuf, receivedLen);
			XP::DecryptMessage(&hiMessage, sizeof(hiMessage), XP::DefaultSecurityKey);

			if (hiMessage.signature == XP::InternalProtocolSignature &&
				hiMessage.protocolVersion == XP::InternalProtocolVersion &&
				hiMessage.messageType == XP::MessageConnectionHi)
			{
#if LOG_DEBUG
				std::cout << "WinXP Socket Hi Message:" << std::endl;
				std::cout << "  Header:" << std::endl;
				std::cout << "    signature      = 0x" << std::hex << hiMessage.signature << std::dec << std::endl;
				std::cout << "    totalLength    = " << hiMessage.totalLength << std::endl;
				std::cout << "    messageType    = " << hiMessage.messageType << std::endl;
				std::cout << "    intLength      = " << hiMessage.intLength << std::endl;
				std::cout << "  protocolVersion  = " << hiMessage.protocolVersion << std::endl;
				std::cout << "  productSignature = 0x" << std::hex << hiMessage.productSignature << std::dec << std::endl;
				std::cout << "  optionFlagsMask  = 0x" << std::hex << hiMessage.optionFlagsMask << std::dec << std::endl;
				std::cout << "  optionFlags      = 0x" << std::hex << hiMessage.optionFlags << std::dec << std::endl;
				std::cout << "  clientKey        = 0x" << std::hex << hiMessage.clientKey << std::dec << std::endl;
				std::cout << "  machineGUID      = " << hiMessage.machineGUID << std::endl;
#endif
				player = std::make_unique<PlayerSocket>(socket); // TODO: Derived WinXP PlayerSocket

				XP::MsgConnectionHello helloMessage;
				helloMessage.signature = XP::InternalProtocolSignature;
				helloMessage.totalLength = sizeof(helloMessage);
				helloMessage.intLength = sizeof(helloMessage);
				helloMessage.messageType = XP::MessageConnectionHello;

				std::mt19937 keyRng(std::random_device{}());
				helloMessage.key = std::uniform_int_distribution<uint32>{}(keyRng);
				helloMessage.machineGUID = hiMessage.machineGUID;

				XP::EncryptMessage(&helloMessage, sizeof(helloMessage), XP::DefaultSecurityKey);
				socket.SendData(reinterpret_cast<char*>(&helloMessage), sizeof(helloMessage));
			}
			else
			{
				throw std::runtime_error("Invalid initial message!");
			}
		}
		else
		{
			throw std::runtime_error("Invalid initial message!");
		}

		assert(player);
		while (true)
		{
			// Receive and send back data
			const std::vector<std::vector<std::string>> receivedData = socket.ReceiveData();
			assert(!receivedData.empty());

			for (const std::vector<std::string>& receivedLineData : receivedData)
				socket.SendData(player->GetResponse(receivedLineData));
		}
	}
	catch (const DisconnectionRequest&)
	{
		std::cout << "[SOCKET] Disconnecting from " << socket.GetAddressString() << '.' << std::endl;
	}
	catch (const ClientDisconnected&)
	{
		std::cout << "[SOCKET] Error communicating with socket " << socket.GetAddressString()
			<< ": Client has been disconnected." << std::endl;
	}
	catch (const std::exception& err)
	{
		std::cout << "[SOCKET] Error communicating with socket " << socket.GetAddressString()
			<< ": " << err.what() << std::endl;
	}
	return 0;
}


std::string
Socket::GetAddressString(SOCKET socket, const char portSeparator)
{
	sockaddr_in socketInfo;
	int socketInfoSize = sizeof(socketInfo);

	getpeername(socket, reinterpret_cast<sockaddr*>(&socketInfo), &socketInfoSize);

	std::stringstream stream;
	stream << inet_ntoa(socketInfo.sin_addr) << portSeparator << ntohs(socketInfo.sin_port);
	return stream.str();
}


Socket::Socket(SOCKET socket, std::ostream& log) :
	m_socket(socket),
	m_log(log)
{}

Socket::~Socket()
{
	// Clean up
	Disconnect();
	closesocket(m_socket);
}


void
Socket::Disconnect()
{
	// Shut down the connection
	HRESULT shutdownResult = shutdown(m_socket, SD_BOTH);
	if (shutdownResult == SOCKET_ERROR)
		std::cout << "[SOCKET] \"shutdown\" failed: " << WSAGetLastError() << std::endl;
}


int
Socket::ReceiveData(char* data, int len)
{
	const int receivedLen = recv(m_socket, data, len, 0);
	if (receivedLen == 0)
		throw ClientDisconnected();
	else if (receivedLen < 0)
		throw std::runtime_error("\"recv\" failed: " + WSAGetLastError());

	m_log << "[RECEIVED]: " << data << std::endl;

	return receivedLen;
}

std::vector<std::vector<std::string>>
Socket::ReceiveData()
{
	char receivedBuf[DEFAULT_BUFLEN];

	const int receivedLen = recv(m_socket, receivedBuf, DEFAULT_BUFLEN, 0);
	if (receivedLen > 0)
	{
		const std::string received(receivedBuf, receivedLen);
		std::istringstream receivedStream(received);

		std::vector<std::vector<std::string>> receivedEntries;
		std::string receivedLine;
		while (std::getline(receivedStream, receivedLine))
		{
			if (receivedLine.empty())
				continue;

			if (receivedLine.back() == '\r')
				receivedLine.pop_back(); // Remove carriage return

			receivedEntries.push_back(StringSplit(std::move(receivedLine), "&")); // Split data by "&" for easier parsing in certain cases
		}

		if (!s_logPingMessages && receivedEntries.size() == 1 && receivedEntries[0].size() == 1 && receivedEntries[0][0].empty())
			return receivedEntries;

		m_log << "[RECEIVED]: " << std::endl;
		for (const std::vector<std::string>& receivedLineEntries : receivedEntries)
		{
			for (const std::string& entry : receivedLineEntries)
			{
				m_log << entry << std::endl;
			}
			m_log << std::endl;
		}
		m_log << '\n' << std::endl;

		return receivedEntries;
	}
	else if (receivedLen == 0)
	{
		throw ClientDisconnected();
	}
	throw std::runtime_error("\"recv\" failed: " + WSAGetLastError());
}


int
Socket::SendData(const char* data, int len)
{
	const int sentLen = send(m_socket, data, len, 0);
	if (sentLen == SOCKET_ERROR)
		throw std::runtime_error("\"send\" failed: " + WSAGetLastError());

	m_log << "[SENT]: " << data << "\n(BYTES SENT=" << sentLen << ")\n\n" << std::endl;

	return sentLen;
}

void
Socket::SendData(std::vector<std::string> data)
{
	for (const std::string& message : data)
	{
		if (!message.empty())
			SendData(message.c_str(), static_cast<int>(message.length()));
	}
}
