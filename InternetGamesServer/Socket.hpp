#pragma once

#include <winsock2.h>

#include <cassert>
#include <string>
#include <sstream>
#include <vector>

class PlayerSocket;

void LoadXPAdBannerImage();

/** SOCKET wrapper featuring general socket handling functions */
class Socket final
{
public:
	static std::string s_logsDirectory;
	static bool s_logPingMessages; // DEBUG: Log empty ping messages from sockets
	static bool s_disableXPAdBanner;

	enum Type
	{
		UNKNOWN,
		WIN7,
		WINXP,
		WINXP_BANNER_AD_REQUEST,
		WINXP_BANNER_AD_IMAGE_REQUEST,
	};
	static std::string TypeToString(Type type);

private:
	static std::vector<Socket*> s_socketList;

public:
	static inline const std::vector<Socket*>& GetList() { return s_socketList; }

	// Handler for the thread of a socket
	static DWORD WINAPI SocketHandler(void* socket);

	// Used to request disconnection without an actual error having occured
	class DisconnectSocket final : public std::exception
	{
	public:
		DisconnectSocket(const std::string& err) :
			m_err(err)
		{}

		const char* what() const override { return m_err.c_str(); }

	private:
		const std::string m_err;
	};

	static char* GetAddressString(IN_ADDR address);
	static std::string GetAddressString(SOCKET socket, const char portSeparator = ':');

private:
	// Client disconnected exception
	class ClientDisconnected final : public std::exception
	{
	public:
		ClientDisconnected() throw() {}
	};

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
			throw std::runtime_error("\"recv\" failed: " + std::to_string(WSAGetLastError()));

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
			throw std::runtime_error("\"recv\" failed: " + std::to_string(WSAGetLastError()));

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
			throw std::runtime_error("\"recv\" failed: " + std::to_string(WSAGetLastError()));

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
			throw std::runtime_error("\"send\" failed: " + std::to_string(WSAGetLastError()));

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
			throw std::runtime_error("\"send\" failed: " + std::to_string(WSAGetLastError()));

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
			throw std::runtime_error("\"send\" failed: " + std::to_string(WSAGetLastError()));

		m_log << logBuf.str() << "\n(BYTES SENT=" << len << ")\n\n" << std::endl;

		return sentLen;
	}

	inline SOCKET GetRaw() const { return m_socket; }
	inline std::string GetAddressString() const { return GetAddressString(m_socket); }
	inline bool IsDisconnected() const { return m_disconnected; }
	inline std::time_t GetConnectionTime() const { return m_connectionTime; }
	inline Type GetType() const { return m_type; }
	inline PlayerSocket* GetPlayerSocket() const { return m_playerSocket; }

private:
	const SOCKET m_socket;
	std::ostream& m_log;
	const std::time_t m_connectionTime;

	bool m_disconnected;

	Type m_type;
	PlayerSocket* m_playerSocket;

private:
	Socket(const Socket&) = delete;
	Socket operator=(const Socket&) = delete;
};
