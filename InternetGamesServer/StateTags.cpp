#include "StateTags.hpp"

#include "tinyxml2.h"

#include "Util.hpp"

std::string
StateSTag::ConstructGameManagementMessage(const std::string& method, const std::string& param)
{
	tinyxml2::XMLDocument doc;

	tinyxml2::XMLElement* elMessage = doc.NewElement("Message");
	doc.InsertFirstChild(elMessage);

	tinyxml2::XMLElement* elGameManagement = doc.NewElement("GameManagement");
	elMessage->InsertFirstChild(elGameManagement);

	NewElementWithText(elGameManagement, "Method", method);
	if (!param.empty())
		NewElementWithText(elGameManagement, "Params", param);

	return PrintXML(doc);
}

void
StateSTag::AppendToTags(tinyxml2::XMLElement& arTags) const
{
	tinyxml2::XMLDocument& doc = *arTags.GetDocument();

	tinyxml2::XMLElement* elTag = doc.NewElement("Tag");
	arTags.InsertFirstChild(elTag);

	NewElementWithText(elTag, "id", "STag");

	// "STag" value
	tinyxml2::XMLElement* elOValue = doc.NewElement("oValue");
	NewElementWithText(elOValue, "MsgID", msgID);
	NewElementWithText(elOValue, "MsgIDSbky", msgIDSbky);
	NewElementWithText(elOValue, "MsgD", msgD.empty() ? "0" : msgD);
	elTag->InsertEndChild(elOValue);
}


void
StateChatTag::AppendToTags(tinyxml2::XMLElement& arTags) const
{
	tinyxml2::XMLDocument& doc = *arTags.GetDocument();

	tinyxml2::XMLElement* elTag = doc.NewElement("Tag");
	arTags.InsertFirstChild(elTag);

	NewElementWithText(elTag, "id", "chat");

	// "STag" value
	tinyxml2::XMLElement* elOValue = doc.NewElement("oValue");
	NewElementWithText(elOValue, "UserID", userID);
	NewElementWithText(elOValue, "Nickname", nickname);
	NewElementWithText(elOValue, "Text", text);
	NewElementWithText(elOValue, "FontFace", fontFace);
	NewElementWithText(elOValue, "FontFlags", fontFlags);
	NewElementWithText(elOValue, "FontColor", fontColor);
	NewElementWithText(elOValue, "FontCharSet", fontCharSet);
	NewElementWithText(elOValue, "MessageFlags", "2"); // TODO: Figure out what "MessageFlags" is for. Currently it doesn't seem to matter
	elTag->InsertEndChild(elOValue);
}
