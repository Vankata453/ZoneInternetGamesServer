#pragma once

#include <winsock2.h>

#include <string>
#include <vector>

/** SOCKET wrapper featuring general socket handling functions */
class Socket final
{
public:
	static std::string s_logsDirectory;
	static bool s_showPingMessages; // DEBUG: Print empty ping messages from clients on the console

public:
	// Handler for the thread of a socket
	static DWORD WINAPI SocketHandler(void* socket);

	// Server disconnection request
	class DisconnectionRequest final : public std::exception
	{
	public:
		DisconnectionRequest() throw() {}
	};
	// Client disconnected exception
	class ClientDisconnected final : public std::exception
	{
	public:
		ClientDisconnected() throw() {}
	};

	static std::string GetAddressString(SOCKET socket, const char portSeparator = ':');

public:
	Socket(SOCKET socket, std::ostream& log);
	~Socket();

	void Disconnect();

	std::vector<std::vector<std::string>> ReceiveData();
	void SendData(std::vector<std::string> data);

	inline SOCKET GetRaw() const { return m_socket; }
	inline std::string GetAddressString() const { return GetAddressString(m_socket); }

private:
	const SOCKET m_socket;
	std::ostream& m_log;
};
