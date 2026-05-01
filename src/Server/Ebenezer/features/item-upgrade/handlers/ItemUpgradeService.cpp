#include "pch.h"
#include "ItemUpgradeService.h"

#include <Ebenezer/User.h>

#include <shared/packets.h>
#include <shared-server/utilities.h>

namespace Ebenezer::Features::ItemUpgrade
{

void ItemUpgradeService::HandleItemUpgrade(CUser* user, const char* pBuf, int /*len*/)
{
	if (user == nullptr || user->m_pUserData == nullptr)
		return;

	int   index = 0;
	char* pData = const_cast<char*>(pBuf); // legacy GetByte takes char*

	const uint8_t upgradeType = GetByte(pData, index);
	if (upgradeType == ITEM_UPGRADE_PROCESS)
		user->ItemUpgrade(pData + index);
	else if (upgradeType == ITEM_UPGRADE_ACCESSORIES)
		user->ItemUpgradeAccesories(pData + index);
}

} // namespace Ebenezer::Features::ItemUpgrade
