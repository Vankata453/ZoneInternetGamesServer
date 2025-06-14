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
		STATE_PLAYING
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

	/** Processing messages */
	void ProcessMessage(const MsgChatSwitch& msg);
	virtual void ProcessIncomingGameMessage(PlayerSocket& player, uint32 type) = 0;

protected:
	/** Sending utilities */
	template<uint32 Type, typename T>
	void BroadcastGenericMessage(const T& msgApp, int excludePlayerID = -1, int len = sizeof(T))
	{
		for (PlayerSocket* player : m_players)
		{
			if (player->m_ID != excludePlayerID)
				player->OnMatchGenericMessage<Type>(msgApp, len);
		}
	}
	template<uint32 Type, typename T>
	void BroadcastGameMessage(const T& msgGame, int excludePlayerID = -1, int len = sizeof(T))
	{
		for (PlayerSocket* player : m_players)
		{
			if (player->m_ID != excludePlayerID)
				player->OnMatchGameMessage<Type>(msgGame, len);
		}
	}
	template<uint32 Type, typename T, uint16 MessageLen> // Another arbitrary message right after T
	void BroadcastGameMessage(const T& msgGame, const CharArray<MessageLen>& msgGameSecond, int excludePlayerID = -1)
	{
		for (PlayerSocket* player : m_players)
		{
			if (player->m_ID != excludePlayerID)
				player->OnMatchGameMessage<Type>(msgGame, msgGameSecond);
		}
	}

	/** Other utilities */
	static bool ValidateCommonChatMessage(const std::wstring& chatMsg);

protected:
	State m_state;

	const SkillLevel m_skillLevel;

private:
	Match(const Match&) = delete;
	Match operator=(const Match&) = delete;
};

}
