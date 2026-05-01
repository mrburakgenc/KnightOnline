#ifndef SERVER_EBENEZER_FEATURES_LEVELING_HANDLERS_LEVELINGSERVICE_H
#define SERVER_EBENEZER_FEATURES_LEVELING_HANDLERS_LEVELINGSERVICE_H

#pragma once

#include <cstdint>

namespace Ebenezer
{

class CUser;

namespace Features::Leveling
{

// Stateless service that owns experience-point accounting and level-up
// transitions. There is no inbound packet for this slice — it is
// invoked by combat resolution (kill rewards, death penalties, etc.)
// and by clerical resurrection. Owned as a single instance by
// EbenezerApp.
//
// Behavior is preserved verbatim from the legacy CUser::ExpChange /
// CUser::LevelChange. Those methods on CUser remain as one-line
// forwarders so existing call sites in AISocket, MagicProcess, and the
// rest of the codebase compile unchanged while the slice owns the
// actual logic.
class LevelingService
{
public:
	LevelingService() = default;

	// Apply an experience delta. Negative deltas may cause a de-level;
	// positive deltas may cause one or more level-ups via LevelChange.
	void ExpChange(CUser& user, int iExp);

	// Apply a level transition. Level-up grants 3 stat points (capped),
	// 2 skill points (level >= 10, capped), refills HP/MP, recomputes
	// max-stats, broadcasts WIZ_LEVEL_CHANGE to the region, and notifies
	// party members.
	void LevelChange(CUser& user, int16_t level, bool bLevelUp = true);
};

} // namespace Features::Leveling

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_LEVELING_HANDLERS_LEVELINGSERVICE_H
