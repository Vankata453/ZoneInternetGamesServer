#define _WINSOCK_DEPRECATED_NO_WARNINGS // Allows usage of inet_ntoa()

#include "Socket.hpp"

#include <cassert>
#include <sstream>
#include <iostream>
#include <fstream>

#include "Util.hpp"
#include "Win7/PlayerSocket.hpp"
#include "WinXP/PlayerSocket.hpp"
#include "WinXP/Protocol/Init.hpp"
#include "WinXP/Security.hpp"

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
		std::unique_ptr<PlayerSocket> player;
		{
			// Determine socket type, create PlayerSocket object and parse initial message
			char receivedBuf[DEFAULT_BUFLEN];
			const int receivedLen = socket.ReceiveData(receivedBuf, DEFAULT_BUFLEN);
			if (!strncmp(receivedBuf, SOCKET_WIN7_HI_RESPONSE, receivedLen)) // WIN7
			{
				player = std::make_unique<Win7::PlayerSocket>(socket);

				socket.SendData(SOCKET_WIN7_HI_RESPONSE, static_cast<int>(strlen(SOCKET_WIN7_HI_RESPONSE)));
			}
			else if (receivedLen == sizeof(WinXP::MsgConnectionHi)) // WINXP
			{
				WinXP::MsgConnectionHi hiMessage;
				memcpy(&hiMessage, receivedBuf, receivedLen);
				WinXP::DecryptMessage(&hiMessage, sizeof(hiMessage), WinXP::DefaultSecurityKey);

				if (hiMessage.signature == WinXP::InternalProtocolSignature &&
					hiMessage.protocolVersion == WinXP::InternalProtocolVersion &&
					hiMessage.messageType == WinXP::MessageConnectionHi)
				{
					*logStream << "[INITIAL MESSAGE]: " << hiMessage << '\n' << std::endl;

					auto xpPlayer = std::make_unique<WinXP::PlayerSocket>(socket, hiMessage);

					socket.SendData(xpPlayer->ConstructHelloMessage(), WinXP::EncryptMessage, WinXP::DefaultSecurityKey);

					player = std::move(xpPlayer);
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
		}

		assert(player);
		player->ProcessMessages();
	}
	catch (const DisconnectionRequest&)
	{
		// Will be logged after this statement
	}
	catch (const ClientDisconnected&)
	{
		std::cout << "[SOCKET] Error communicating with socket " << socket.GetAddressString()
			<< ": Client has been disconnected." << std::endl;
		return 0;
	}
	catch (const std::exception& err)
	{
		std::cout << "[SOCKET] Error communicating with socket " << socket.GetAddressString()
			<< ": " << err.what() << std::endl;;
		return 0;
	}
	std::cout << "[SOCKET] Disconnecting from " << socket.GetAddressString() << '.' << std::endl;
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

	m_log << "[RECEIVED]: ";
	m_log.write(data, receivedLen);
	m_log << '\n' << std::endl;

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

	m_log << "[SENT]: ";
	m_log.write(data, sentLen);
	m_log << "\n(BYTES SENT=" << sentLen << ")\n\n" << std::endl;

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
