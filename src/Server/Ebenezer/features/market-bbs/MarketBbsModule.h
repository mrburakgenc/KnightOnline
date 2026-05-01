#ifndef SERVER_EBENEZER_FEATURES_MARKET_BBS_MARKETBBSMODULE_H
#define SERVER_EBENEZER_FEATURES_MARKET_BBS_MARKETBBSMODULE_H

#pragma once

namespace Ebenezer::Shared::Network
{
class PacketRouter;
}

namespace Ebenezer::Features::MarketBbs
{

class MarketBbsService;

// Registration entry point for the market-bbs slice. Called once
// during EbenezerApp composition; binds WIZ_MARKET_BBS on the router
// so packets dispatch into MarketBbsService::HandleMarketBbs.
class MarketBbsModule
{
public:
	static void Register(Shared::Network::PacketRouter& router, MarketBbsService& service);
};

} // namespace Ebenezer::Features::MarketBbs

#endif // SERVER_EBENEZER_FEATURES_MARKET_BBS_MARKETBBSMODULE_H
