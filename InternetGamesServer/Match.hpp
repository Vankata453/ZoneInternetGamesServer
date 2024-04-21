#pragma once

#include <ctime>
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

	enum class Level {
		INVALID = 0,
		BEGINNER,
		INTERMEDIATE,
		EXPERT
	};
	static Level LevelFromPublicELO(const std::string& str);

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

	/** Event handling */
	void EventSend(const PlayerSocket* caller, const std::string& xml);
	void Chat(const StateChatTag tag);

	inline State GetState() const { return m_state; }
	inline REFGUID GetGUID() const { return m_guid; }
	inline Level GetLevel() const { return m_level; }

	/** Construct XML messages */
	std::string ConstructReadyXML() const;
	std::string ConstructStateXML(const std::vector<const StateTag*> tags) const;

	/** Construct "Tag"s to be used in "StateMessage"s */
	StateSTag ConstructGameInitSTag(PlayerSocket* caller) const;
	StateSTag ConstructGameStartSTag() const;
	StateSTag ConstructEventReceiveSTag(const std::string& xml) const;

protected:
	struct QueuedEvent final
	{
		QueuedEvent(const std::string& xml_, bool includeSender_ = false) :
			xml(xml_),
			includeSender(includeSender_)
		{}

		std::string xml; // The XML data string for the event.
		bool includeSender; // Should the event also be sent back to the original sender?
	};

protected:
	virtual size_t GetRequiredPlayerCount() const { return 2; }

	/** Game-specific matches can use this function to modify "EventSend" messages for "EventReceive". */
	virtual QueuedEvent ProcessEvent(const std::string& xml);

	virtual std::string ConstructGameInitXML(PlayerSocket* caller) const = 0;

protected:
	State m_state;

	GUID m_guid;
	const Level m_level;
	std::vector<PlayerSocket*> m_players;

	const std::time_t m_creationTime;
};
