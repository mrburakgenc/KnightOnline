#ifndef SERVER_EBENEZER_FEATURES_ITEM_REPAIR_ITEMREPAIRMODULE_H
#define SERVER_EBENEZER_FEATURES_ITEM_REPAIR_ITEMREPAIRMODULE_H

#pragma once

namespace Ebenezer::Shared::Network
{
class PacketRouter;
}

namespace Ebenezer::Features::ItemRepair
{

class ItemRepairService;

// Registration entry point for the item-repair slice. Called once during
// EbenezerApp composition; binds WIZ_ITEM_REPAIR on the router so packets
// dispatch into ItemRepairService::HandleItemRepair.
class ItemRepairModule
{
public:
	static void Register(Shared::Network::PacketRouter& router, ItemRepairService& service);
};

} // namespace Ebenezer::Features::ItemRepair

#endif // SERVER_EBENEZER_FEATURES_ITEM_REPAIR_ITEMREPAIRMODULE_H
