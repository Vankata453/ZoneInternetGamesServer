#pragma once

#include <string>
#include <functional>

class XMLPrinter;

namespace tinyxml2 {
	class XMLElement;
}

/** A tag, sent as a part of "arTags" in a "StateMessage" */
class StateTag
{
public:
	virtual void AppendToTags(XMLPrinter& printer) const = 0;
};

/** A tag, containing a message, sent as a part of "arTags" in a "StateMessage" */
class StateSTag final : public StateTag
{
public:
	static StateSTag ConstructGameInit(const std::string& xml);
	static StateSTag ConstructGameStart();
	static StateSTag ConstructEventReceive(const std::string& xml);

	static std::string ConstructMethodMessage(const char* managementModule, const std::string& method, const std::string& param = "",
		bool xmlParams = false);
	static std::string ConstructMethodMessage(const char* managementModule, const std::string& method,
		const std::function<void(XMLPrinter&)>& elParamsProcessor, bool xmlParams = false);
	static void PrintMethodMessage(XMLPrinter& printer, const char* managementModule, const std::string& method, const std::string& param = "",
		bool xmlParams = false);
	static void PrintMethodMessage(XMLPrinter& printer, const char* managementModule, const std::string& method,
		const std::function<void(XMLPrinter&)>& elParamsProcessor, bool xmlParams = false);

public:
	void AppendToTags(XMLPrinter& printer) const override;

public:
	std::string msgID;
	std::string msgIDSbky;
	std::string msgD;
};

/** A tag, containing details about a sent chat message */
class StateChatTag final : public StateTag
{
public:
	void AppendToTags(XMLPrinter& printer) const override;

public:
	std::string userID;
	std::string nickname;
	std::string text;
	std::string fontFace;
	std::string fontFlags;
	std::string fontColor;
	std::string fontCharSet;
};
