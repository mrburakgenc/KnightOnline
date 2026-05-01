#include "pch.h"
#include "WarehouseModule.h"

#include "handlers/WarehouseService.h"

#include <Ebenezer/shared/infrastructure/network/PacketRouter.h>

#include <shared/packets.h>

namespace Ebenezer::Features::Warehouse
{

void WarehouseModule::Register(Shared::Network::PacketRouter& router, WarehouseService& service)
{
	router.Bind(
		WIZ_WAREHOUSE,
		[&service](CUser* user, const char* buf, int len)
		{ service.HandleWarehouse(user, buf, len); },
		"warehouse");
}

} // namespace Ebenezer::Features::Warehouse
