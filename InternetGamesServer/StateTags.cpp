#include "StateTags.hpp"

#include "Util.hpp"

StateSTag
StateSTag::ConstructGameInit(const std::string& xml)
{
	StateSTag sTag;
	sTag.msgID = "GameInit";
	sTag.msgIDSbky = "GameInit";
	sTag.msgD = xml;
	return sTag;
}

StateSTag
StateSTag::ConstructGameStart()
{
	StateSTag sTag;
	sTag.msgID = "GameStart";
	sTag.msgIDSbky = "GameStart";
	return sTag;
}

StateSTag
StateSTag::ConstructEventReceive(const std::string& xml)
{
	StateSTag sTag;
	sTag.msgID = "EventReceive";
	sTag.msgIDSbky = "EventReceive";
	sTag.msgD = xml;
	return sTag;
}


std::string
StateSTag::ConstructMethodMessage(const char* managementModule, const std::string& method, const std::string& param,
	bool xmlParams)
{
	XMLPrinter printer;
	PrintMethodMessage(printer, managementModule, method, param, xmlParams);
	return printer;
}

std::string
StateSTag::ConstructMethodMessage(const char* managementModule, const std::string& method,
	const std::function<void(XMLPrinter&)>& elParamsProcessor, bool xmlParams)
{
	XMLPrinter printer;
	PrintMethodMessage(printer, managementModule, method, elParamsProcessor, xmlParams);
	return printer;
}

void
StateSTag::PrintMethodMessage(XMLPrinter& printer, const char* managementModule, const std::string& method, const std::string& param,
	bool xmlParams)
{
	printer.OpenElement("Message");

	if (xmlParams)
	{
		printer.OpenElement("XMLParams");
		NewElementWithText(printer, "Object", managementModule);
	}
	else
	{
		printer.OpenElement(managementModule);
	}
	NewElementWithText(printer, "Method", method);
	if (!param.empty())
		NewElementWithText(printer, "Params", param);
	printer.CloseElement(xmlParams ? "XMLParams" : managementModule);

	printer.CloseElement("Message");
}

void
StateSTag::PrintMethodMessage(XMLPrinter& printer, const char* managementModule, const std::string& method,
	const std::function<void(XMLPrinter&)>& elParamsProcessor, bool xmlParams)
{
	printer.OpenElement("Message");

	if (xmlParams)
	{
		printer.OpenElement("XMLParams");
		NewElementWithText(printer, "Object", managementModule);
	}
	else
	{
		printer.OpenElement(managementModule);
	}
	NewElementWithText(printer, "Method", method);

	printer.OpenElement("Params");
	elParamsProcessor(printer);
	printer.CloseElement("Params");

	printer.CloseElement(xmlParams ? "XMLParams" : managementModule);

	printer.CloseElement("Message");
}


void
StateSTag::AppendToTags(XMLPrinter& printer) const
{
	printer.OpenElement("Tag");

	NewElementWithText(printer, "id", "STag");

	// "STag" value
	printer.OpenElement("oValue");
	NewElementWithText(printer, "MsgID", msgID);
	NewElementWithText(printer, "MsgIDSbky", msgIDSbky);
	NewElementWithText(printer, "MsgD", msgD.empty() ? "0" : msgD);
	printer.CloseElement("oValue");

	printer.CloseElement("Tag");
}


void
StateChatTag::AppendToTags(XMLPrinter& printer) const
{
	printer.OpenElement("Tag");

	NewElementWithText(printer, "id", "chat");

	// "STag" value
	printer.OpenElement("oValue");
	NewElementWithText(printer, "UserID", userID);
	NewElementWithText(printer, "Nickname", nickname);
	NewElementWithText(printer, "Text", text);
	NewElementWithText(printer, "FontFace", fontFace);
	NewElementWithText(printer, "FontFlags", fontFlags);
	NewElementWithText(printer, "FontColor", fontColor);
	NewElementWithText(printer, "FontCharSet", fontCharSet);
	NewElementWithText(printer, "MessageFlags", "2"); // TODO: Figure out what "MessageFlags" is for. Currently it doesn't seem to matter
	printer.CloseElement("oValue");

	printer.CloseElement("Tag");
}
