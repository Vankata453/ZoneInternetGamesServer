#pragma once

#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include <winsock2.h>

#include "StateTags.hpp"

class PlayerSocket;

class Match
{
public:
	enum class Game {
		INVALID = 0,
		BACKGAMMON,
		CHECKERS,
		SPADES
	};
	static Game GameFromString(const std::string& str);

	enum State {
		STATE_WAITINGFORPLAYERS,
		STATE_PENDINGSTART,
		STATE_PLAYING
	};

public:
	Match(PlayerSocket& player);

	void JoinPlayer(PlayerSocket& player);
	void DisconnectedPlayer(PlayerSocket& player);

	/** Update match logic */
	virtual void Update();

	inline State GetState() const { return m_state; }
	inline REFGUID GetGUID() const { return m_guid; }

	/** Construct XML messages */
	std::string ConstructReadyXML() const;
	std::string ConstructStateXML(const std::vector<std::unique_ptr<StateTag>>& tags) const;

	/** Construct "Tag"s to be used in "StateMessage"s */
	std::unique_ptr<StateTag> ConstructGameInitSTag(PlayerSocket* caller) const;
	std::unique_ptr<StateTag> ConstructGameStartSTag() const;

protected:
	virtual size_t GetRequiredPlayerCount() const { return 2; }

	virtual std::string ConstructGameInitXML(PlayerSocket* caller) const = 0;

protected:
	State m_state;

	GUID m_guid;
	std::vector<PlayerSocket*> m_players;

	const std::time_t m_creationTime;
};
