#ifndef SERVER_EBENEZER_FEATURES_ITEM_UPGRADE_ITEMUPGRADEMODULE_H
#define SERVER_EBENEZER_FEATURES_ITEM_UPGRADE_ITEMUPGRADEMODULE_H

#pragma once

namespace Ebenezer::Shared::Network
{
class PacketRouter;
}

namespace Ebenezer::Features::ItemUpgrade
{

class ItemUpgradeService;

// Registration entry point for the item-upgrade slice. Called once
// during EbenezerApp composition; binds WIZ_ITEM_UPGRADE on the router
// so packets dispatch into ItemUpgradeService.
class ItemUpgradeModule
{
public:
	static void Register(Shared::Network::PacketRouter& router, ItemUpgradeService& service);
};

} // namespace Ebenezer::Features::ItemUpgrade

#endif // SERVER_EBENEZER_FEATURES_ITEM_UPGRADE_ITEMUPGRADEMODULE_H
