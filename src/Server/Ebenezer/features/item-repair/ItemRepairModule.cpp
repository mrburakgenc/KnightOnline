#include "pch.h"
#include "ItemRepairModule.h"

#include "handlers/ItemRepairService.h"

#include <Ebenezer/shared/infrastructure/network/PacketRouter.h>

#include <shared/packets.h>

namespace Ebenezer::Features::ItemRepair
{

void ItemRepairModule::Register(Shared::Network::PacketRouter& router, ItemRepairService& service)
{
	router.Bind(
		WIZ_ITEM_REPAIR,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandleItemRepair(user, buf, len); },
		"item-repair");
}

} // namespace Ebenezer::Features::ItemRepair
