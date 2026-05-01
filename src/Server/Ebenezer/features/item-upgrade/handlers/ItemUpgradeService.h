#ifndef SERVER_EBENEZER_FEATURES_ITEM_UPGRADE_HANDLERS_ITEMUPGRADESERVICE_H
#define SERVER_EBENEZER_FEATURES_ITEM_UPGRADE_HANDLERS_ITEMUPGRADESERVICE_H

#pragma once

namespace Ebenezer
{

class CUser;

namespace Features::ItemUpgrade
{

// Stateless service for the anvil item-upgrade flow. Owned as a single
// instance by EbenezerApp; bound to the PacketRouter via
// ItemUpgradeModule::Register at startup.
//
// WIZ_ITEM_UPGRADE is multiplexed: the first body byte is an upgrade
// type that selects either the weapon/armor upgrade flow
// (ITEM_UPGRADE_PROCESS) or the accessory upgrade flow
// (ITEM_UPGRADE_ACCESSORIES). The two sub-flows together are ~600 LOC
// of RNG + durability mutation + inventory-shuffle logic that stays on
// CUser for now (CUser::ItemUpgrade / CUser::ItemUpgradeAccesories).
//
// The slice owns the packet boundary; future work can lift the
// matching / RNG / inventory-mutation primitives into this folder once
// the inventory + RNG slices are cut.
class ItemUpgradeService
{
public:
	ItemUpgradeService() = default;

	// WIZ_ITEM_UPGRADE handler — body starts with upgrade-type byte.
	void HandleItemUpgrade(CUser* user, const char* pBuf, int len);
};

} // namespace Features::ItemUpgrade

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_ITEM_UPGRADE_HANDLERS_ITEMUPGRADESERVICE_H
