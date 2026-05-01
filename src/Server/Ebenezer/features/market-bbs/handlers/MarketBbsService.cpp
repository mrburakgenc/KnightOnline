#include "pch.h"
#include "MarketBbsService.h"

#include <Ebenezer/User.h>

#include <shared/packets.h>
#include <shared-server/utilities.h>

#include <spdlog/spdlog.h>

namespace Ebenezer::Features::MarketBbs
{

void MarketBbsService::HandleMarketBbs(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr)
		return;

	int   index = 0;
	char* pData = const_cast<char*>(pBuf); // legacy GetByte takes char*

	const uint8_t subcommand = GetByte(pData, index);

	// Legacy: prune expired/empty buy and sell posts before each dispatch.
	user->MarketBBSBuyPostFilter();
	user->MarketBBSSellPostFilter();

	switch (subcommand)
	{
		case MARKET_BBS_REGISTER:
			user->MarketBBSRegister(pData + index);
			break;

		case MARKET_BBS_DELETE:
			user->MarketBBSDelete(pData + index);
			break;

		case MARKET_BBS_REPORT:
			user->MarketBBSReport(pData + index, MARKET_BBS_REPORT);
			break;

		case MARKET_BBS_OPEN:
			user->MarketBBSReport(pData + index, MARKET_BBS_OPEN);
			break;

		case MARKET_BBS_REMOTE_PURCHASE:
			user->MarketBBSRemotePurchase(pData + index);
			break;

		case MARKET_BBS_MESSAGE:
			user->MarketBBSMessage(pData + index);
			break;

		default:
			spdlog::error("MarketBbsService::HandleMarketBbs: unhandled sub-opcode {:02X} "
						  "[characterName={}]",
				subcommand, user->m_pUserData->m_id);
#ifndef _DEBUG
			user->Close();
#endif
			break;
	}
}

} // namespace Ebenezer::Features::MarketBbs
