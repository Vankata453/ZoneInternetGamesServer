#include "PlayerSocket.hpp"

#include "Util.hpp"

PlayerSocket::PlayerSocket(SOCKET socket) :
	m_socket(socket),
	m_state(STATE_NOT_INITIALIZED),
	m_guid()
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
				m_state = STATE_JOINED;
				m_guid = StringSplit(receivedData[0], "=")[1];
				return { "JoinContext {372EB895-6783-45DB-84ED-D4EE0B27B42B} " + m_guid + " 38&38&38&\r\n" };
			}
			break;
	}
	return {};
}

void
PlayerSocket::OnDisconnected()
{
}
