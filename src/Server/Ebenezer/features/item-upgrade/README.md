# item-upgrade

Anvil item upgrade flow — `WIZ_ITEM_UPGRADE`. The first body byte is an `e_ItemUpgradeOpcode` that selects either the weapon / armor upgrade or the accessory upgrade flow.

| Sub-opcode                    | Meaning                                       | Logic lives in              |
|-------------------------------|-----------------------------------------------|-----------------------------|
| `ITEM_UPGRADE_PROCESS`        | Weapon / armor upgrade (RNG + durability)     | `CUser::ItemUpgrade`        |
| `ITEM_UPGRADE_ACCESSORIES`    | Accessory upgrade (different scrolls + table) | `CUser::ItemUpgradeAccesories` |

The two sub-handlers together are ~600 LOC of RNG + durability mutation + inventory shuffle. They stay on `CUser` because they touch private inventory + the item-upgrade table + RNG state not yet sliced. The slice owns the **packet boundary** only; once the inventory and RNG slices are cut, the bodies can move into this folder.

## Layers

| Folder         | Lives here                                                                                  |
|----------------|---------------------------------------------------------------------------------------------|
| `handlers/`    | `ItemUpgradeService.{h,cpp}` — handles `WIZ_ITEM_UPGRADE`. Stateless.                       |
| `ItemUpgradeModule.{h,cpp}` (slice root) | Registers the opcode with the router during `EbenezerApp` startup.|

## Public surface

```cpp
namespace Ebenezer::Features::ItemUpgrade {
    class ItemUpgradeService {
    public:
        void HandleItemUpgrade(CUser* user, const char* pBuf, int len);  // WIZ_ITEM_UPGRADE
    };

    class ItemUpgradeModule {
    public:
        static void Register(Shared::Network::PacketRouter& router, ItemUpgradeService& service);
    };
}
```

## Behavior preserved verbatim

- Sub-opcode dispatch matches legacy `CUser::ItemUpgradeProcess` exactly.
- Unknown sub-opcodes are silently dropped (legacy code falls through with no else / no Close).
- All RNG, durability mutation, inventory consumption, and result packet emission remain in `CUser::ItemUpgrade` / `CUser::ItemUpgradeAccesories` unchanged.

## Migration status

- ✅ `WIZ_ITEM_UPGRADE` bound on `PacketRouter` via `ItemUpgradeModule::Register`.
- ✅ `case WIZ_ITEM_UPGRADE:` removed from `CUser::Parsing`; `CUser::ItemUpgradeProcess` (decl + body) deleted.
- ⏸ `ItemUpgrade` / `ItemUpgradeAccesories` / `MatchingItemUpgrade` / `SendItemUpgradeFailed` / `SendItemUpgradeRequest` stay on user pending the inventory + RNG slices.
- ⏳ Smoke-test in-game: anvil weapon upgrade success path; intentional fail path (durability decrement); accessory upgrade.
