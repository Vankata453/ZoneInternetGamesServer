#pragma once

#include <string>

namespace tinyxml2 {
	class XMLElement;
}

/** A tag, sent as a part of "arTags" in a "StateMessage" */
class StateTag
{
public:
	virtual void AppendToTags(tinyxml2::XMLElement& arTags) const = 0;
};

/** A tag, containing a message, sent as a part of "arTags" in a "StateMessage" */
class StateSTag final : public StateTag
{
public:
	static std::string ConstructGameManagementMessage(const std::string& method, const std::string& param = {});

public:
	void AppendToTags(tinyxml2::XMLElement& arTags) const override;

public:
	std::string msgID;
	std::string msgIDSbky;
	std::string msgD;
};

/** A tag, containing details about a sent chat message */
class StateChatTag final : public StateTag
{
public:
	void AppendToTags(tinyxml2::XMLElement& arTags) const override;

public:
	std::string userID;
	std::string nickname;
	std::string text;
	std::string fontFace;
	std::string fontFlags;
	std::string fontColor;
	std::string fontCharSet;
};
