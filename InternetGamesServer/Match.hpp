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
	static std::string GameToNameString(Game game);

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
		STATE_PLAYING,
		STATE_GAMEOVER,
		STATE_ENDED
	};

public:
	Match(PlayerSocket& player);

	void JoinPlayer(PlayerSocket& player);
	void DisconnectedPlayer(PlayerSocket& player);

	/** Update match logic */
	virtual void Update();

	/** Event handling */
	void EventSend(const PlayerSocket& caller, const std::string& xml);
	void Chat(const StateChatTag tag);

	virtual Game GetGame() const = 0;
	inline State GetState() const { return m_state; }
	inline REFGUID GetGUID() const { return m_guid; }
	inline Level GetLevel() const { return m_level; }

	/** Construct XML messages */
	std::string ConstructReadyXML() const;
	std::string ConstructStateXML(const std::vector<const StateTag*> tags) const;

	/** Construct XML data for STag messages */
	std::string ConstructGameInitXML(PlayerSocket* caller) const;

protected:
	struct QueuedEvent final
	{
		QueuedEvent(const std::string& xml, bool includeSender = false);
		QueuedEvent(const std::string& xml, const std::string& xmlSender);

		const std::string xml; // The XML data string for the event.
		const std::string xmlSender;  // The XML data string for the event, to be sent only to the original sender.
		const bool includeSender; // Should the event also be sent back to the original sender? (Ignored if xmlSender is set.)
	};

protected:
	virtual size_t GetRequiredPlayerCount() const { return 2; }

	/** Append additional XML data to STag messages */
	virtual void AppendToGameInitXML(XMLPrinter& printer, PlayerSocket* caller) const {}

	/** Process event and return a custom response. */
	virtual std::vector<QueuedEvent> ProcessEvent(const tinyxml2::XMLElement& elEvent, const PlayerSocket& caller);

protected:
	State m_state;

	GUID m_guid;
	const Level m_level;
	std::vector<PlayerSocket*> m_players;
	HANDLE m_event_mutex; // Mutex to prevent simultaneous event processing from multiple clients

	const std::time_t m_creationTime;

private:
	Match(const Match&) = delete;
	Match operator=(const Match&) = delete;
};
