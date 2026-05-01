#include "pch.h"
#include "ItemUpgradeModule.h"

#include "handlers/ItemUpgradeService.h"

#include <Ebenezer/shared/infrastructure/network/PacketRouter.h>

#include <shared/packets.h>

namespace Ebenezer::Features::ItemUpgrade
{

void ItemUpgradeModule::Register(Shared::Network::PacketRouter& router, ItemUpgradeService& service)
{
	router.Bind(
		WIZ_ITEM_UPGRADE,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandleItemUpgrade(user, buf, len); },
		"item-upgrade");
}

} // namespace Ebenezer::Features::ItemUpgrade
