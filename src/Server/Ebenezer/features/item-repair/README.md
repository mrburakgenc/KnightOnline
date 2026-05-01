# item-repair

NPC blacksmith repair flow — `WIZ_ITEM_REPAIR`. Validates a slot/inventory item id, charges a price scaled by the missing durability, and writes the slot's `sDuration` back to the item table's max.

## Layers

| Folder         | Lives here                                                                                  |
|----------------|---------------------------------------------------------------------------------------------|
| `handlers/`    | `ItemRepairService.{h,cpp}` — handles `WIZ_ITEM_REPAIR`. Stateless.                         |
| `ItemRepairModule.{h,cpp}` (slice root) | Registers the opcode with the router during `EbenezerApp` startup.|

## Public surface

```cpp
namespace Ebenezer::Features::ItemRepair {
    class ItemRepairService {
    public:
        void HandleItemRepair(CUser* user, const char* pBuf, int len);  // WIZ_ITEM_REPAIR
    };

    class ItemRepairModule {
    public:
        static void Register(Shared::Network::PacketRouter& router, ItemRepairService& service);
    };
}
```

## Behavior preserved verbatim

- `pos == 1` → equipped slot (`m_sItemArray[slot]`); `pos == 2` → backpack (`m_sItemArray[SLOT_MAX + slot]`).
- Item id mismatch, missing item table, or `Durability == 1` (non-repairable) → fail packet, no charge.
- Price formula: `((BuyPrice - 10) / 10000 + BuyPrice^0.75) * missingDurability / maxDurability`.
- Insufficient gold → fail packet.
- On success: `CurrencyChange(-money)`, slot `sDuration` reset to `Durability`, success packet (`0x01`) with new gold balance.

## Migration status

- ✅ Logic extracted from `CUser::ItemRepair`.
- ✅ `WIZ_ITEM_REPAIR` bound on `PacketRouter` via `ItemRepairModule::Register`.
- ✅ Old `case WIZ_ITEM_REPAIR:` removed from `CUser::Parsing`.
- ⏳ Smoke-test in-game at a blacksmith NPC.
