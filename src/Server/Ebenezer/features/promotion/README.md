# promotion

Class promotion + stat / skill respec entry — `WIZ_CLASS_CHANGE`. The opcode is multiplexed: the first byte after the opcode is an `e_ClassChangeOpcode` selector that routes to one of four sub-flows.

| Sub-opcode                | Meaning                                | Logic lives in                              |
|---------------------------|----------------------------------------|---------------------------------------------|
| `CLASS_CHANGE_STATUS_REQ` | Lvl 60 promotion request               | `CUser::NovicePromotionStatusRequest`       |
| `CLASS_RESET_STAT_REQ`    | Buy a stat-point respec                | `CUser::StatPointResetRequest`              |
| `CLASS_RESET_SKILL_REQ`   | Buy a skill-point respec               | `CUser::SkillPointResetRequest`             |
| `CLASS_RESET_COST_REQ`    | Quote the gold price for a respec      | inline in `PromotionService::HandleClassChange` |

The three request sub-opcodes still call into `CUser` because their flows touch a wide blast radius (DB request, level-up cap reset, packet broadcast). They will migrate piecemeal in later phases — the slice boundary today is the **packet** boundary.

## Layers

| Folder         | Lives here                                                                                  |
|----------------|---------------------------------------------------------------------------------------------|
| `handlers/`    | `PromotionService.{h,cpp}` — handles `WIZ_CLASS_CHANGE`. Stateless.                         |
| `PromotionModule.{h,cpp}` (slice root) | Registers the opcode with the router during `EbenezerApp` startup.|

## Public surface

```cpp
namespace Ebenezer::Features::Promotion {
    class PromotionService {
    public:
        void HandleClassChange(CUser* user, const char* pBuf, int len);  // WIZ_CLASS_CHANGE
    };

    class PromotionModule {
    public:
        static void Register(Shared::Network::PacketRouter& router, PromotionService& service);
    };
}
```

## Behavior preserved verbatim

- Cost formula: `money = round((level * 2)^3.4, 100)`; `<30` lvl → ×0.4, `60..90` → ×1.5.
- Skill respec costs 1.5× stat respec before discounts.
- Discount 1 (event + winning nation) → ×0.5; discount 2 (general) → ×0.5; both stack with the level scaling.
- The legacy `#if 0` "level 30..59 → ×1.0" branch was already a no-op; it is dropped here.
- Unknown sub-types (`sub_type` other than 1 or 2) silently fall through with no reply, exactly like the legacy code.

## Migration status

- ✅ Logic extracted from `CUser::ClassChange`.
- ✅ `WIZ_CLASS_CHANGE` bound on `PacketRouter` via `PromotionModule::Register`.
- ✅ Old `case WIZ_CLASS_CHANGE:` removed from `CUser::Parsing`; `CUser::ClassChange` declaration + body deleted.
- ⏳ Smoke-test in-game: lvl-60 promotion + respec quote.
