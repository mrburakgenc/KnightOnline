# gold

Money. Adds and removes coins from a user's balance, clamps to currency bounds, and notifies the client.

This is the **first migrated slice after the King System PoC** — kept deliberately small (30-line domain) to validate the per-feature pattern before larger features arrive.

## Layers

| Folder        | Lives here                                                                                       |
|---------------|--------------------------------------------------------------------------------------------------|
| `domain/`     | `GoldBalance.h` — value-semantic helpers around `int32_t` balance: clamp, can-afford predicate.  |
| `handlers/`   | `GoldService.{h,cpp}` — `Gain(user, amount)` and `Lose(user, amount)`. Sends `WIZ_GOLD_CHANGE`.  |
| `presentation/` | (none — this feature has no client-initiated packet; outbound notification is built inside the handler.) |

## Public surface

```cpp
namespace Ebenezer::Features::Gold {
    class GoldService {
    public:
        void Gain(CUser& user, int32_t amount);
        bool Lose(CUser& user, int32_t amount);
    };
}
```

`EbenezerApp` owns one `GoldService` instance. `CUser::GoldGain` / `CUser::GoldLose` are kept as one-line forwarding methods so existing call sites (~13 of them across User.cpp / EbenezerApp.cpp / events / item upgrade) compile unchanged.

## Why no router binding

Gold has no inbound `WIZ_*` opcode — it's a server-side service called by other systems (NPC sale, drop, exp/quest reward, item upgrade cost, respec fee, royal reward, etc.). The outbound `WIZ_GOLD_CHANGE` packet is built directly by the service when balance changes.

When/if a feature gains a client-initiated packet later, the slice will gain a `presentation/` folder and a `Bind()` call in its module's registration step. See `king-system/` (full slice plan) and `ARCHITECTURE.md` §5.

## Migration status

- ✅ Domain extracted (`GoldBalance` value object)
- ✅ Handler extracted (`GoldService`)
- ✅ `CUser::GoldGain` / `CUser::GoldLose` reduced to one-line forwarders
- ✅ Smoke-tested against existing call sites (NPC trade, item upgrade, respec, royal reward)

This slice is the **template** for: Warp, Home/Resurrect, Item Repair, Promotion, Chat, Loyalty, PartyBBS — the next 7 low-risk migrations.
