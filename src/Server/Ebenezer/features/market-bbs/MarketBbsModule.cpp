#include "pch.h"
#include "MarketBbsModule.h"

#include "handlers/MarketBbsService.h"

#include <Ebenezer/shared/infrastructure/network/PacketRouter.h>

#include <shared/packets.h>

namespace Ebenezer::Features::MarketBbs
{

void MarketBbsModule::Register(Shared::Network::PacketRouter& router, MarketBbsService& service)
{
	router.Bind(
		WIZ_MARKET_BBS,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandleMarketBbs(user, buf, len); },
		"market-bbs");
}

} // namespace Ebenezer::Features::MarketBbs
