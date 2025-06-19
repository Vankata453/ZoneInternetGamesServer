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
	virtual void ProcessIncomingGameMessage(PlayerSocket& player, uint32 type) = 0;

protected:
	/** Sending utilities */
	template<uint32 Type, typename T>
	void BroadcastGenericMessage(const T& msgApp, int excludePlayerSeat = -1, int len = sizeof(T))
	{
		switch (WaitForSingleObject(m_broadcastMutex, 5000))
		{
			case WAIT_OBJECT_0: // Acquired ownership of the broadcast mutex
			{
				for (PlayerSocket* player : m_players)
				{
					if (player->m_seat != excludePlayerSeat)
						player->OnMatchGenericMessage<Type>(msgApp, len);
				}

				if (!ReleaseMutex(m_broadcastMutex))
					throw std::runtime_error("Match::BroadcastGenericMessage(): Couldn't release broadcast mutex: " + GetLastError());
				break;
			}

			case WAIT_ABANDONED: // Acquired ownership of an abandoned broadcast mutex
				throw std::runtime_error("Match::BroadcastGenericMessage(): Got ownership of an abandoned broadcast mutex: " + GetLastError());
		}
	}
	template<uint32 Type, typename T>
	void BroadcastGameMessage(const T& msgGame, int excludePlayerSeat = -1, int len = sizeof(T))
	{
		switch (WaitForSingleObject(m_broadcastMutex, 5000))
		{
			case WAIT_OBJECT_0: // Acquired ownership of the broadcast mutex
			{
				for (PlayerSocket* player : m_players)
				{
					if (player->m_seat != excludePlayerSeat)
						player->OnMatchGameMessage<Type>(msgGame, len);
				}

				if (!ReleaseMutex(m_broadcastMutex))
					throw std::runtime_error("Match::BroadcastGameMessage(): Couldn't release broadcast mutex: " + GetLastError());
				break;
			}

			case WAIT_ABANDONED: // Acquired ownership of an abandoned broadcast mutex
				throw std::runtime_error("Match::BroadcastGameMessage(): Got ownership of an abandoned broadcast mutex: " + GetLastError());
		}
	}
	template<uint32 Type, typename T, typename M, uint16 MessageLen> // Trailing data array after T
	void BroadcastGameMessage(const T& msgGame, const Array<M, MessageLen>& msgGameSecond, int excludePlayerSeat = -1)
	{
		switch (WaitForSingleObject(m_broadcastMutex, 5000))
		{
			case WAIT_OBJECT_0: // Acquired ownership of the broadcast mutex
			{
				for (PlayerSocket* player : m_players)
				{
					if (player->m_seat != excludePlayerSeat)
						player->OnMatchGameMessage<Type>(msgGame, msgGameSecond);
				}

				if (!ReleaseMutex(m_broadcastMutex))
					throw std::runtime_error("Match::BroadcastGameMessage(): Couldn't release broadcast mutex: " + GetLastError());
				break;
			}

			case WAIT_ABANDONED: // Acquired ownership of an abandoned broadcast mutex
				throw std::runtime_error("Match::BroadcastGameMessage(): Got ownership of an abandoned broadcast mutex: " + GetLastError());
		}
	}

	/** Other static utilities */
	static bool ValidateCommonChatMessage(const std::wstring& chatMsg);

protected:
	State m_state;

	const SkillLevel m_skillLevel;

private:
	HANDLE m_broadcastMutex;

	std::time_t m_endTime;

private:
	Match(const Match&) = delete;
	Match operator=(const Match&) = delete;
};

}
