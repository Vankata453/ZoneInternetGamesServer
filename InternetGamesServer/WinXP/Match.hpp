#pragma once

#include "../Match.hpp"

#include "Protocol/Game.hpp"
#include "../Util.hpp"

namespace WinXP {

class PlayerSocket;

class Match : public ::Match<PlayerSocket>
{
public:
	enum class Game {
		INVALID = 0,
		BACKGAMMON,
		CHECKERS,
		SPADES,
		HEARTS,
		REVERSI
	};
	static Game GameFromString(const std::string& str);
	static std::string GameToNameString(Game game);

	enum class SkillLevel {
		INVALID = -1,
		BEGINNER,
		INTERMEDIATE,
		EXPERT
	};

	enum State {
		STATE_WAITINGFORPLAYERS,
		STATE_PENDINGSTART,
		STATE_PLAYING,
		STATE_GAMEOVER,
		STATE_ENDED
	};

public:
	Match(PlayerSocket& player);
	~Match() override;

	void JoinPlayer(PlayerSocket& player);
	void DisconnectedPlayer(PlayerSocket& player);

	/** Update match logic */
	virtual void Update() override;

	virtual Game GetGame() const = 0;
	inline State GetState() const { return m_state; }
	inline SkillLevel GetSkillLevel() const { return m_skillLevel; }
	inline uint32 GetGameID() const { return m_guid.Data1; }

	/** Processing messages */
	void ProcessMessage(const MsgChatSwitch& msg);
	void ProcessIncomingGameMessage(PlayerSocket& player, uint32 type);

protected:
	/** Processing messages */
	virtual void ProcessIncomingGameMessageImpl(PlayerSocket& player, uint32 type) = 0;

	// Returns message ID, 0 on failure
	static uint8_t ValidateChatMessage(const std::wstring& chatMsg, uint8_t customRangeStart, uint8_t customRangeEnd);

protected:
	/** Sending utilities */
	template<uint32 Type, typename T>
	void BroadcastGenericMessage(const T& msgApp, int excludePlayerSeat = -1, int len = sizeof(T))
	{
		if (WaitForSingleObject(m_broadcastMutex, 5000) == WAIT_ABANDONED) // Acquired ownership of an abandoned broadcast mutex
			throw std::runtime_error("WinXP::Match::BroadcastGenericMessage(): Got ownership of an abandoned broadcast mutex: " + std::to_string(GetLastError()));

		try
		{
			for (PlayerSocket* player : m_players)
			{
				if (player->m_seat != excludePlayerSeat)
					player->OnMatchGenericMessage<Type>(msgApp, len);
			}
		}
		catch (...)
		{
			ReleaseMutex(m_broadcastMutex);
			throw;
		}

		if (!ReleaseMutex(m_broadcastMutex))
			throw std::runtime_error("WinXP::Match::BroadcastGenericMessage(): Couldn't release broadcast mutex: " + std::to_string(GetLastError()));
	}
	template<uint32 Type, typename T>
	void BroadcastGameMessage(const T& msgGame, int excludePlayerSeat = -1, int len = sizeof(T))
	{
		if (WaitForSingleObject(m_broadcastMutex, 5000) == WAIT_ABANDONED) // Acquired ownership of an abandoned broadcast mutex
			throw std::runtime_error("WinXP::Match::BroadcastGameMessage(): Got ownership of an abandoned broadcast mutex: " + std::to_string(GetLastError()));

		try
		{
			for (PlayerSocket* player : m_players)
			{
				if (player->m_seat != excludePlayerSeat)
					player->OnMatchGameMessage<Type>(msgGame, len);
			}
		}
		catch (...)
		{
			ReleaseMutex(m_broadcastMutex);
			throw;
		}

		if (!ReleaseMutex(m_broadcastMutex))
			throw std::runtime_error("WinXP::Match::BroadcastGameMessage(): Couldn't release broadcast mutex: " + std::to_string(GetLastError()));
	}
	template<uint32 Type, typename T, typename M, uint16 MessageLen> // Trailing data array after T
	void BroadcastGameMessage(const T& msgGame, const Array<M, MessageLen>& msgGameSecond, int excludePlayerSeat = -1)
	{
		if (WaitForSingleObject(m_broadcastMutex, 5000) == WAIT_ABANDONED) // Acquired ownership of an abandoned broadcast mutex
			throw std::runtime_error("WinXP::Match::BroadcastGameMessage(): Got ownership of an abandoned broadcast mutex: " + std::to_string(GetLastError()));

		try
		{
			for (PlayerSocket* player : m_players)
			{
				if (player->m_seat != excludePlayerSeat)
					player->OnMatchGameMessage<Type>(msgGame, msgGameSecond);
			}
		}
		catch (...)
		{
			ReleaseMutex(m_broadcastMutex);
			throw;
		}

		if (!ReleaseMutex(m_broadcastMutex))
			throw std::runtime_error("WinXP::Match::BroadcastGameMessage(): Couldn't release broadcast mutex: " + std::to_string(GetLastError()));
	}

protected:
	State m_state;

	const SkillLevel m_skillLevel;

private:
	HANDLE m_processMessageMutex; // Mutex to prevent simultaneous processing of multiple messages from several players
	HANDLE m_broadcastMutex; // Mutex to prevent simultaneous broadcasting of multiple messages to players (otherwise they could get mashed up)

	std::time_t m_endTime;

private:
	Match(const Match&) = delete;
	Match operator=(const Match&) = delete;
};

}
