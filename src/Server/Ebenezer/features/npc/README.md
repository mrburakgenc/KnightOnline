# npc

Inbound NPC packets — `WIZ_REQ_NPCIN` (region NPC list) and `WIZ_NPC_EVENT` (NPC click / interaction). The interaction dispatcher (`CUser::NpcEvent`) fans out into many NPC kinds — shop, warp, trainer, dialog quest, repair, anvil, BBS, etc. — so this slice intentionally only owns the **packet boundary**; per-kind bodies will move into their own slices (some already shipped: item-repair, item-upgrade, warehouse, market-bbs, party-bbs, object-events).

## Layers

| Folder         | Lives here                                                                                  |
|----------------|---------------------------------------------------------------------------------------------|
| `handlers/`    | `NpcService.{h,cpp}` — forwards both opcodes to existing CUser methods.                     |
| `NpcModule.{h,cpp}` (slice root) | Registers both opcodes with the router during `EbenezerApp` startup.      |

## Public surface

```cpp
namespace Ebenezer::Features::Npc {
    class NpcService {
    public:
        void HandleRequestNpcIn(CUser* user, const char* pBuf, int len);  // WIZ_REQ_NPCIN
        void HandleNpcEvent(CUser* user, const char* pBuf, int len);      // WIZ_NPC_EVENT
    };

    class NpcModule {
    public:
        static void Register(Shared::Network::PacketRouter& router, NpcService& service);
    };
}
```

## Behavior preserved verbatim

- `WIZ_REQ_NPCIN` → `CUser::RequestNpcIn` (region NPC visibility).
- `WIZ_NPC_EVENT` → `CUser::NpcEvent` (per-kind interaction dispatch).
- All existing zone state, NPC manager, and per-kind logic stays on CUser unchanged.

## Migration status

- ✅ Both opcodes bound on `PacketRouter` via `NpcModule::Register`.
- ✅ `case WIZ_REQ_NPCIN:` and `case WIZ_NPC_EVENT:` removed from `CUser::Parsing`.
- ⏸ `CUser::RequestNpcIn` / `CUser::NpcEvent` retained — bodies will migrate per NPC kind in later phases.
- ⏳ Smoke-test in-game: zone in (NPCs visible); right-click each NPC kind covered by existing slices to confirm dispatch still works.
