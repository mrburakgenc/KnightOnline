#ifndef SERVER_EBENEZER_FEATURES_ITEM_REPAIR_HANDLERS_ITEMREPAIRSERVICE_H
#define SERVER_EBENEZER_FEATURES_ITEM_REPAIR_HANDLERS_ITEMREPAIRSERVICE_H

#pragma once

namespace Ebenezer
{

class CUser;

namespace Features::ItemRepair
{

// Stateless service that handles WIZ_ITEM_REPAIR. Owned as a single
// instance by EbenezerApp; bound to the PacketRouter via
// ItemRepairModule::Register at startup.
//
// Behavior is preserved verbatim from the legacy CUser::ItemRepair —
// validate slot/inventory position, look up the item table for the
// max durability, charge the user a price scaled by the missing
// durability, and reply with a success/failure WIZ_ITEM_REPAIR packet
// carrying the new gold balance.
class ItemRepairService
{
public:
	ItemRepairService() = default;

	// WIZ_ITEM_REPAIR handler.
	//   pBuf : packet body after the WIZ_ITEM_REPAIR opcode byte.
	//   len  : remaining length (validation hook for future hardening).
	void HandleItemRepair(CUser* user, const char* pBuf, int len);
};

} // namespace Features::ItemRepair

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_ITEM_REPAIR_HANDLERS_ITEMREPAIRSERVICE_H
