# loyalty

National Points (NP) and Manner Points. Awarded on PvP kill, deducted on grief, distributed to "chicken" party members in range.

Same shape as `features/gold/` — service-only, no inbound packet. The outbound `WIZ_LOYALTY_CHANGE` notification is built inside the service.

## Layers

| Folder        | Lives here                                                                                           |
|---------------|------------------------------------------------------------------------------------------------------|
| `handlers/`   | `LoyaltyService.{h,cpp}` — `Grant`, `Adjust`, `CheckManner`, `AdjustManner`. Stateless.              |

## Public surface

```cpp
namespace Ebenezer::Features::Loyalty {
    class LoyaltyService {
    public:
        // PvP-kill grant: source wins, victim loses. Encodes the
        // legacy level-difference and same-nation rules verbatim.
        void Grant(CUser& source, int targetSocketId);

        // Direct adjustment (used by zone events, GM commands, etc.).
        void Adjust(CUser& user, int delta, bool excludeMonthly);

        bool CheckManner(const CUser& user, int32_t min, int32_t max);

        // Manner adjustment with chicken-party distribution.
        void AdjustManner(CUser& user, int loyaltyAmount);
    };
}
```

`EbenezerApp` owns one `LoyaltyService` instance. `CUser::LoyaltyChange`, `CUser::ChangeLoyalty`, `CUser::CheckManner`, and `CUser::ChangeMannerPoint` are kept as forwarders to preserve every call site.

## Why no router binding

Loyalty has no client-initiated packet. It's a server-side service called from Combat (PvP kill), zone events, GM commands. The outbound `WIZ_LOYALTY_CHANGE` is built directly inside the service.

## Migration status

- ✅ Logic extracted from `CUser::LoyaltyChange`, `CUser::ChangeLoyalty`, `CUser::CheckManner`, `CUser::ChangeMannerPoint`
- ✅ `CUser::*` methods reduced to one-line forwarders
- ✅ Battle-event kill counters (`m_byBattleOpen`, `m_sKarusDead`, `m_sElmoradDead`) preserved verbatim
- ✅ Manner-point chicken-party distribution preserved verbatim (level bands, distance check)
