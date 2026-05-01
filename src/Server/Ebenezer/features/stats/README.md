# stats

Stat-point and skill-tree allocation — `WIZ_POINT_CHANGE` (STR/STA/DEX/INT/CHA) and `WIZ_SKILLPT_CHANGE` (skill master pool → one of 8 trees). Both opcodes spend one point per packet.

## Layers

| Folder         | Lives here                                                                                  |
|----------------|---------------------------------------------------------------------------------------------|
| `handlers/`    | `StatsService.{h,cpp}` — stateless handler for both opcodes.                                |
| `StatsModule.{h,cpp}` (slice root) | Registers both opcodes with the router during `EbenezerApp` startup.    |

## Public surface

```cpp
namespace Ebenezer::Features::Stats {
    class StatsService {
    public:
        void HandlePointChange(CUser* user, const char* pBuf, int len);       // WIZ_POINT_CHANGE
        void HandleSkillPointChange(CUser* user, const char* pBuf, int len);  // WIZ_SKILLPT_CHANGE
    };

    class StatsModule {
    public:
        static void Register(Shared::Network::PacketRouter& router, StatsService& service);
    };
}
```

## Behavior preserved verbatim

### WIZ_POINT_CHANGE

- Reject if `type > 5` or `|value| > 1` (one point per packet).
- Reject if the user has no available points.
- Reject if the target stat is already at the byte cap (255).
- Apply the increment, run side-effects:
  - STR / DEX → `SetUserAbility()` (recompute hit / weapon stats).
  - STA → `SetMaxHp()` + `SetMaxMp()`.
  - INT → `SetMaxMp()`.
  - CHA → no recompute.
- Reply with the new stat value, max HP, max MP, total-hit, current weight.
- Unhandled stat type in either switch logs an error and (release-only) closes the connection.

### WIZ_SKILLPT_CHANGE

- Reject if `type > 8` (silent drop, matches legacy fall-through).
- Reject if master pool (`m_bstrSkill[0]`) is empty — replies with current value.
- Reject if `m_bstrSkill[type] + 1 > level` (cap by level) — replies with current value.
- Otherwise: decrement master pool, increment selected tree. No success packet — the legacy code returns silently and relies on the client predicting the change.

## Migration status

- ✅ Both opcodes bound on `PacketRouter` via `StatsModule::Register`.
- ✅ Old switch cases + `CUser::PointChange` / `CUser::SkillPointChange` (decl + body) removed.
- ⏳ Smoke-test in-game: spend one stat point of each type; verify HP/MP refresh; spend a skill point into one tree.
