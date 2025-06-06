#pragma once

#include "../Match.hpp"

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

protected:
	State m_state;

	const SkillLevel m_skillLevel;

private:
	Match(const Match&) = delete;
	Match operator=(const Match&) = delete;
};

}
