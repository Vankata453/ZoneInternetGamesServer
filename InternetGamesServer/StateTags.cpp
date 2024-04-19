#include "StateTags.hpp"

#include "tinyxml2.h"

#include "Util.hpp"

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
