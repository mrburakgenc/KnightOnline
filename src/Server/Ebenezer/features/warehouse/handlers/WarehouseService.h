#ifndef SERVER_EBENEZER_FEATURES_WAREHOUSE_HANDLERS_WAREHOUSESERVICE_H
#define SERVER_EBENEZER_FEATURES_WAREHOUSE_HANDLERS_WAREHOUSESERVICE_H

#pragma once

namespace Ebenezer
{

class CUser;

namespace Features::Warehouse
{

// Stateless service for the per-account warehouse (bank). Owned as a
// single instance by EbenezerApp; bound to the PacketRouter via
// WarehouseModule::Register at startup.
//
// WIZ_WAREHOUSE has many sub-commands (open / deposit / withdraw /
// gold-gain / gold-lose / pagination) totalling ~280 LOC of inventory
// + persistence interaction. The dispatcher and sub-bodies stay on
// CUser (CUser::WarehouseProcess) for now because they touch the
// account-level container, weight checks, and the Aujard data agent.
// The slice owns the packet boundary only.
class WarehouseService
{
public:
	WarehouseService() = default;

	// WIZ_WAREHOUSE handler — forwards to CUser::WarehouseProcess.
	void HandleWarehouse(CUser* user, const char* pBuf, int len);
};

} // namespace Features::Warehouse

} // namespace Ebenezer

#endif // SERVER_EBENEZER_FEATURES_WAREHOUSE_HANDLERS_WAREHOUSESERVICE_H
