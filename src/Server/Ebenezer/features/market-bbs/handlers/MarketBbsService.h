#ifndef SERVER_EBENEZER_FEATURES_MARKET_BBS_HANDLERS_MARKETBBSSERVICE_H
#define SERVER_EBENEZER_FEATURES_MARKET_BBS_HANDLERS_MARKETBBSSERVICE_H

#pragma once

namespace Ebenezer
{

class CUser;

namespace Features::MarketBbs
{

// Stateless service for the buy / sell market bulletin board. Owned
// as a single instance by EbenezerApp; bound to the PacketRouter via
// MarketBbsModule::Register at startup.
//
// Dispatches the WIZ_MARKET_BBS sub-opcode and forwards to existing
// CUser methods (MarketBBSRegister / MarketBBSDelete / MarketBBSReport
// / MarketBBSRemotePurchase / MarketBBSMessage). Those stay on CUser
// because they touch the static market post arrays + Aujard
// persistence not yet sliced.
class MarketBbsService
{
public:
	MarketBbsService() = default;

	// WIZ_MARKET_BBS handler — body starts with a one-byte sub-opcode.
	void HandleMarketBbs(CUser* user, const char* pBuf, int len);
};

} // namespace Features::MarketBbs

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_MARKET_BBS_HANDLERS_MARKETBBSSERVICE_H
