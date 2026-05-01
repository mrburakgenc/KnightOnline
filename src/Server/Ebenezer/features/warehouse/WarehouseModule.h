#ifndef SERVER_EBENEZER_FEATURES_WAREHOUSE_WAREHOUSEMODULE_H
#define SERVER_EBENEZER_FEATURES_WAREHOUSE_WAREHOUSEMODULE_H

#pragma once

namespace Ebenezer::Shared::Network
{
class PacketRouter;
}

namespace Ebenezer::Features::Warehouse
{

class WarehouseService;

// Registration entry point for the warehouse slice. Called once during
// EbenezerApp composition; binds WIZ_WAREHOUSE on the router so packets
// dispatch into WarehouseService.
class WarehouseModule
{
public:
	static void Register(Shared::Network::PacketRouter& router, WarehouseService& service);
};

} // namespace Ebenezer::Features::Warehouse

#endif // SERVER_EBENEZER_FEATURES_WAREHOUSE_WAREHOUSEMODULE_H
