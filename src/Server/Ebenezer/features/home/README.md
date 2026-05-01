# home

Bind point + resurrect — `WIZ_HOME` (`/town` command) and `WIZ_REGENE` (post-death regen, including the Stone-of-Resurrection path). The slice is the packet boundary; the heavy CUser methods (`Home`, `Regene`, `InitType3/4`) stay where they are because `Regene` is also called from `MagicProcess` for clerical resurrection.

## Layers

| Folder         | Lives here                                                                                  |
|----------------|---------------------------------------------------------------------------------------------|
| `handlers/`    | `HomeService.{h,cpp}` — handles `WIZ_HOME` + `WIZ_REGENE`. Stateless.                       |
| `HomeModule.{h,cpp}` (slice root) | Registers both opcodes with the router during `EbenezerApp` startup.     |

## Public surface

```cpp
namespace Ebenezer::Features::Home {
    class HomeService {
    public:
        void HandleHome(CUser* user, const char* pBuf, int len);    // WIZ_HOME
        void HandleRegene(CUser* user, const char* pBuf, int len);  // WIZ_REGENE
    };

    class HomeModule {
    public:
        static void Register(Shared::Network::PacketRouter& router, HomeService& service);
    };
}
```

## Behavior preserved verbatim

- `WIZ_HOME` → `CUser::Home()` → `GetStartPosition` + `Warp(x, z)`. No body.
- `WIZ_REGENE` payload: one byte `regene_type` (1 = death regen, 2 = resurrection-stone regen). Other values coerced to 1.
- The legacy `User::Parsing` called `InitType3(); InitType4(); Regene(...)`. `Regene` itself ALSO calls `InitType3/InitType4` at the top — the double-init is preserved for verbatim parity.
- Regene-type 2 consumes 3×level resurrection stones (item id 379006000) and refuses below level 5.
- Bind-point lookup uses `m_pUserData->m_sBind` against the current map's object events; falls back to nation/zone-class spawn tables in `model::Home` when no bind is available.
- Clerical resurrection (`magicid > 0`) restores HP/MP, applies blink abnormal, refunds lost EXP × `pType->ExpRecover/100` if killed by no-one and `regene_type == 1`.
- Sends `AG_USER_REGENE` to AI Server when the regene was not a magic-blink path.
- Notifies party members via `WIZ_PARTY` `PARTY_STATUSCHANGE` for type-3 / type-4 buff state.

## Migration status

- ✅ Both opcodes bound on `PacketRouter` via `HomeModule::Register`.
- ✅ `case WIZ_HOME:` and `case WIZ_REGENE:` removed from `CUser::Parsing`.
- ⏸ `CUser::Home` / `CUser::Regene` retained as public methods — moving them is deferred until the combat / magic / movement slices land (`Regene` is reused by clerical-resurrection magic).
- ⏳ Smoke-test: `/town` warp; die in PvE → resurrect at bind point; cleric Resurrection cast on a corpse.
