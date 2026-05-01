#ifndef SERVER_EBENEZER_FEATURES_STATS_HANDLERS_STATSSERVICE_H
#define SERVER_EBENEZER_FEATURES_STATS_HANDLERS_STATSSERVICE_H

#pragma once

namespace Ebenezer
{

class CUser;

namespace Features::Stats
{

// Stateless service for stat-point and skill-point allocation. Owned
// as a single instance by EbenezerApp; bound to the PacketRouter via
// StatsModule::Register at startup.
//
// Handles WIZ_POINT_CHANGE (stat allocation: STR/STA/DEX/INT/CHA) and
// WIZ_SKILLPT_CHANGE (skill-tree point allocation). Both are
// rate-limited to one point per packet and reject hostile input by
// closing the connection in release builds.
class StatsService
{
public:
	StatsService() = default;

	// WIZ_POINT_CHANGE — buy one stat point of a given type.
	void HandlePointChange(CUser* user, const char* pBuf, int len);

	// WIZ_SKILLPT_CHANGE — move one skill point from the master pool
	// (index 0) into one of the eight specialty trees (index 1..8).
	void HandleSkillPointChange(CUser* user, const char* pBuf, int len);
};

} // namespace Features::Stats

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_STATS_HANDLERS_STATSSERVICE_H
