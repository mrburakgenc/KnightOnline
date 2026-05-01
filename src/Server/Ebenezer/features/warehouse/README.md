# warehouse

Per-account warehouse (bank) — `WIZ_WAREHOUSE`. The packet body's first byte is a sub-command (`WAREHOUSE_OPEN` / `WAREHOUSE_INPUT` / `WAREHOUSE_OUTPUT` / `WAREHOUSE_GOLD_GAIN` / `WAREHOUSE_GOLD_LOSE` / page navigation) totalling ~280 LOC of inventory + persistence interaction.

The dispatcher and all sub-bodies stay in `CUser::WarehouseProcess` for now — they touch the account-level container, weight checks, and the Aujard data agent. The slice owns the **packet boundary** only; future work can extract the sub-handlers once the inventory + Aujard-RPC slices are cut.

## Layers

| Folder         | Lives here                                                                                  |
|----------------|---------------------------------------------------------------------------------------------|
| `handlers/`    | `WarehouseService.{h,cpp}` — forwards `WIZ_WAREHOUSE` to `CUser::WarehouseProcess`.         |
| `WarehouseModule.{h,cpp}` (slice root) | Registers the opcode with the router during `EbenezerApp` startup.|

## Public surface

```cpp
namespace Ebenezer::Features::Warehouse {
    class WarehouseService {
    public:
        void HandleWarehouse(CUser* user, const char* pBuf, int len);  // WIZ_WAREHOUSE
    };

    class WarehouseModule {
    public:
        static void Register(Shared::Network::PacketRouter& router, WarehouseService& service);
    };
}
```

## Behavior preserved verbatim

- All warehouse logic — open / deposit / withdraw / gold-gain / gold-lose / pagination, exchange-with-other-user lockout, dead-user lockout — stays in `CUser::WarehouseProcess`.
- Service is a thin forwarder; no validation is added or removed.

## Migration status

- ✅ `WIZ_WAREHOUSE` bound on `PacketRouter` via `WarehouseModule::Register`.
- ✅ `case WIZ_WAREHOUSE:` removed from `CUser::Parsing`.
- ⏸ `CUser::WarehouseProcess` retained — moves once inventory + Aujard-RPC slices land.
- ⏳ Smoke-test in-game: open warehouse NPC; deposit + withdraw an item; deposit + withdraw gold; verify failure when in a trade.
