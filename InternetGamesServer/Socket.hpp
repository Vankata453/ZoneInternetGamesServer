#pragma once

#include <winsock2.h>

#include <cassert>
#include <string>
#include <sstream>
#include <vector>

/** SOCKET wrapper featuring general socket handling functions */
class Socket final
{
public:
	static std::string s_logsDirectory;
	static bool s_logPingMessages; // DEBUG: Log empty ping messages from sockets

	enum Type
	{
		UNKNOWN,
		WIN7,
		WINXP
	};

public:
	// Handler for the thread of a socket
	static DWORD WINAPI SocketHandler(void* socket);

	// Client disconnected exception
	class ClientDisconnected final : public std::exception
	{
	public:
		ClientDisconnected() throw() {}
	};

	static char* GetAddressString(IN_ADDR address);
	static std::string GetAddressString(SOCKET socket, const char portSeparator = ':');

public:
	Socket(SOCKET socket, std::ostream& log);
	~Socket();

	void Disconnect();

	/** Receive data */
	int ReceiveData(char* data, int len);
	std::vector<std::vector<std::string>> ReceiveData();
	template<class T>
	int ReceiveData(T& data, int len = sizeof(T))
	{
		assert(len <= sizeof(T));
		if (len == 0)
			return 0;

		const int receivedLen = recv(m_socket, reinterpret_cast<char*>(&data), len, 0);
		if (receivedLen == 0)
			throw ClientDisconnected();
		else if (receivedLen < 0)
			throw std::runtime_error("\"recv\" failed: " + WSAGetLastError());

		m_log << "[RECEIVED]: " << data << "\n\n" << std::endl;

		return receivedLen;
	}
	template<class T, typename Key>
	int ReceiveData(T& data, void(*decryptor)(void*, int, Key), Key decryptKey, int len = sizeof(T))
	{
		assert(len <= sizeof(T));
		if (len == 0)
			return 0;

		const int receivedLen = recv(m_socket, reinterpret_cast<char*>(&data), len, 0);
		if (receivedLen == 0)
			throw ClientDisconnected();
		else if (receivedLen < 0)
			throw std::runtime_error("\"recv\" failed: " + WSAGetLastError());

		decryptor(&data, len, decryptKey);

		m_log << "[RECEIVED]: " << data << "\n\n" << std::endl;

		return receivedLen;
	}
	template<class T, typename Key>
	int ReceiveData(T& data, void(T::*converter)(), void(*decryptor)(void*, int, Key), Key decryptKey,
		char* dataRaw = nullptr, int len = sizeof(T))
	{
		assert(len <= sizeof(T));
		if (len == 0)
			return 0;

		const int receivedLen = recv(m_socket, reinterpret_cast<char*>(&data), len, 0);
		if (receivedLen == 0)
			throw ClientDisconnected();
		else if (receivedLen < 0)
			throw std::runtime_error("\"recv\" failed: " + WSAGetLastError());

		decryptor(&data, len, decryptKey);

		if (dataRaw)
			std::memcpy(dataRaw, &data, len);
		(data.*converter)();

		m_log << "[RECEIVED]: " << data << "\n\n" << std::endl;

		return receivedLen;
	}

	/** Send data */
	int SendData(const char* data, int len);
	void SendData(std::vector<std::string> data);
	template<class T>
	int SendData(const T& data, int len = sizeof(T))
	{
		assert(len <= sizeof(T));

		const int sentLen = send(m_socket, reinterpret_cast<const char*>(&data), len, 0);
		if (sentLen == SOCKET_ERROR)
			throw std::runtime_error("\"send\" failed: " + WSAGetLastError());

		m_log << "[SENT]: " << data << "\n(BYTES SENT=" << len << ")\n\n" << std::endl;

		return sentLen;
	}
	template<class T, typename Key>
	int SendData(T data, void(*encryptor)(void*, int, Key), Key encryptKey, int len = sizeof(T))
	{
		assert(len <= sizeof(T));

		std::ostringstream logBuf;
		logBuf << "[SENT]: " << data;

		encryptor(&data, len, encryptKey);

		const int sentLen = send(m_socket, reinterpret_cast<const char*>(&data), len, 0);
		if (sentLen == SOCKET_ERROR)
			throw std::runtime_error("\"send\" failed: " + WSAGetLastError());

		m_log << logBuf.str() << "\n(BYTES SENT=" << len << ")\n\n" << std::endl;

		return sentLen;
	}
	template<class T, typename Key>
	int SendData(T data, void(T::*converter)(), void(*encryptor)(void*, int, Key), Key encryptKey, int len = sizeof(T))
	{
		assert(len <= sizeof(T));

		std::ostringstream logBuf;
		logBuf << "[SENT]: " << data;

		(data.*converter)();
		encryptor(&data, len, encryptKey);

		const int sentLen = send(m_socket, reinterpret_cast<const char*>(&data), len, 0);
		if (sentLen == SOCKET_ERROR)
			throw std::runtime_error("\"send\" failed: " + WSAGetLastError());

		m_log << logBuf.str() << "\n(BYTES SENT=" << len << ")\n\n" << std::endl;

		return sentLen;
	}

	inline SOCKET GetRaw() const { return m_socket; }
	inline std::string GetAddressString() const { return GetAddressString(m_socket); }
	inline bool IsDisconnected() const { return m_disconnected; }

private:
	const SOCKET m_socket;
	std::ostream& m_log;

	bool m_disconnected;

private:
	Socket(const Socket&) = delete;
	Socket operator=(const Socket&) = delete;
};
