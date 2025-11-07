#pragma once

#include <string>
#include <set>

typedef unsigned short USHORT;

class Config final
{
public:
	Config();

	void Load(const std::string& file);
	void Save();

	static const std::initializer_list<std::pair<std::string, std::string>> s_optionKeys;

	std::string GetValue(const std::string& key);
	void SetValue(const std::string& key, const std::string& value);

private:
	std::string m_file;

public:
	USHORT port;

	std::string logsDirectory;
	bool logPingMessages; // DEBUG: Log empty ping messages from sockets

	bool skipLevelMatching; // When searching for a lobby, don't take the match level into account
	bool disableXPAdBanner;

	std::set<std::string> bannedIPs;
};

extern Config g_config;
