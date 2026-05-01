# leveling

Experience-point accounting and level transitions. There is **no inbound packet** for this slice — `LevelingService` is invoked by:

- Combat resolution (kill rewards) — `AISocket.cpp` (`pUser->ExpChange(...)`).
- Death penalty — same (`-m_iMaxExp / 100` or `/20`).
- Magic processing — `MagicProcess.cpp` (debuffs that drain exp, clerical resurrection that restores it).

Layered as a Service-only slice (Gold / Loyalty are templates). `CUser::ExpChange` and `CUser::LevelChange` are kept as one-line forwarders so existing callers compile unchanged.

## Layers

| Folder         | Lives here                                                                                  |
|----------------|---------------------------------------------------------------------------------------------|
| `handlers/`    | `LevelingService.{h,cpp}` — exp/level math + WIZ_EXP_CHANGE / WIZ_LEVEL_CHANGE packets.     |

## Public surface

```cpp
namespace Ebenezer::Features::Leveling {
    class LevelingService {
    public:
        void ExpChange(CUser& user, int iExp);
        void LevelChange(CUser& user, int16_t level, bool bLevelUp = true);
    };
}
```

## Behavior preserved verbatim

- `ExpChange`:
  - Below level 6, all negative deltas are ignored.
  - In `ZONE_BATTLE`, all negative deltas are ignored.
  - Negative delta crossing 0 → de-level (down to a floor of 5), refund the now-current-level's required exp.
  - Positive delta crossing `m_iMaxExp` → level-up via `LevelChange(level + 1)`.
  - At `MAX_LEVEL` exp is clamped to `m_iMaxExp`.
  - Stores `m_iLostExp` for resurrection-magic exp restore (used by clerical Resurrection).
  - Sends `WIZ_EXP_CHANGE` only on the no-level-cross path — level-up cases skip it because `LevelChange` broadcasts a richer packet.

- `LevelChange`:
  - Level-up adds 3 stat points if total stat allocation is below `300 + 3*(level-1)`.
  - Level-up adds 2 skill points (only level ≥ 10) if total skill allocation is below `2*(level-9)`.
  - Recomputes `m_iMaxExp` from the LevelUp table.
  - Calls `SetSlotItemValue` + `SetUserAbility` to refresh derived stats.
  - Refills MP to `m_iMaxMp`, HP to `m_iMaxHp` via `HpChange`.
  - Notifies AI Server (`Send2AI_UserUpdateInfo`).
  - Broadcasts `WIZ_LEVEL_CHANGE` to the region (carries the full level/stat/HP/MP/weight bundle).
  - If in a party, broadcasts `WIZ_PARTY PARTY_LEVELCHANGE` to party members.
  - The legacy `Send2AI_UserUpdateInfo` ordering is preserved: AI before region broadcast, party last.

## Migration status

- ✅ Logic extracted from `CUser::ExpChange` / `CUser::LevelChange`.
- ✅ `EbenezerApp::m_LevelingService` instantiated.
- ✅ `CUser::ExpChange(int)` and `CUser::LevelChange(int16_t, bool)` are now thin forwarders.
- ⏳ Smoke-test: kill a mob → exp gain bar moves; gain enough to level → WIZ_LEVEL_CHANGE; die in a non-battle zone above level 6 → exp drops; cleric Resurrection restores lost exp.
