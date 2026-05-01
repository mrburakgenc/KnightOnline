#ifndef SERVER_EBENEZER_FEATURES_STATS_STATSMODULE_H
#define SERVER_EBENEZER_FEATURES_STATS_STATSMODULE_H

#pragma once

namespace Ebenezer::Shared::Network
{
class PacketRouter;
}

namespace Ebenezer::Features::Stats
{

class StatsService;

// Registration entry point for the stats slice. Called once during
// EbenezerApp composition; binds WIZ_POINT_CHANGE + WIZ_SKILLPT_CHANGE
// on the router so packets dispatch into StatsService.
class StatsModule
{
public:
	static void Register(Shared::Network::PacketRouter& router, StatsService& service);
};

} // namespace Ebenezer::Features::Stats

#endif // SERVER_EBENEZER_FEATURES_STATS_STATSMODULE_H
