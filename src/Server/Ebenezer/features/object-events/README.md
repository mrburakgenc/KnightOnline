# object-events

World-object interaction — `WIZ_OBJECT_EVENT`. The client sends an `(objectIndex, npcId)` pair; the server resolves the object's `_OBJECT_EVENT` record on the active map and routes by `pEvent->sType` into the matching CUser sub-handler.

| Object type                              | Sub-handler                          |
|------------------------------------------|--------------------------------------|
| `OBJECT_TYPE_BIND`, `OBJECT_TYPE_REMOVE_BIND` | `CUser::BindObjectEvent`        |
| `OBJECT_TYPE_GATE`, `OBJECT_TYPE_DOOR_TOPDOWN` | (no-op — gate triggered via lever) |
| `OBJECT_TYPE_GATE_LEVER`                 | `CUser::GateLeverObjectEvent`        |
| `OBJECT_TYPE_FLAG`                       | `CUser::FlagObjectEvent`             |
| `OBJECT_TYPE_WARP_GATE`                  | `CUser::WarpListObjectEvent`         |
| `OBJECT_TYPE_ANVIL`                      | `CUser::SendItemUpgradeRequest`      |

Sub-handlers stay on `CUser` because they touch zone state, inventory, and the warp / item-upgrade subsystems not yet sliced. The slice owns the **packet boundary** only.

## Layers

| Folder         | Lives here                                                                                  |
|----------------|---------------------------------------------------------------------------------------------|
| `handlers/`    | `ObjectEventsService.{h,cpp}` — handles `WIZ_OBJECT_EVENT`. Stateless.                      |
| `ObjectEventsModule.{h,cpp}` (slice root) | Registers the opcode with the router during `EbenezerApp` startup.|

## Public surface

```cpp
namespace Ebenezer::Features::ObjectEvents {
    class ObjectEventsService {
    public:
        void HandleObjectEvent(CUser* user, const char* pBuf, int len);  // WIZ_OBJECT_EVENT
    };

    class ObjectEventsModule {
    public:
        static void Register(Shared::Network::PacketRouter& router, ObjectEventsService& service);
    };
}
```

## Behavior preserved verbatim

- Object lookup mismatch sends `WIZ_OBJECT_EVENT` with `objectType=0, errorCode=0` (matches legacy initial-zero state).
- `OBJECT_TYPE_GATE` / `OBJECT_TYPE_DOOR_TOPDOWN` are silently consumed — the legacy code commented out the direct gate handler because gates are triggered through their lever event.
- Failed sub-handlers reply with `WIZ_OBJECT_EVENT` failure packet via `SendObjectEventFailed`.
- Anvil dispatches into `SendItemUpgradeRequest` (item-upgrade quote flow).
- Unknown object type logs an error and replies with a failure packet — same hostile-client policy as the legacy switch.

## Migration status

- ✅ `WIZ_OBJECT_EVENT` bound on `PacketRouter` via `ObjectEventsModule::Register`.
- ✅ `case WIZ_OBJECT_EVENT:` removed from `CUser::Parsing`; `CUser::ObjectEvent` (decl + body) deleted.
- ⏸ `BindObjectEvent` / `GateLeverObjectEvent` / `FlagObjectEvent` / `WarpListObjectEvent` / `SendItemUpgradeRequest` / `SendObjectEventFailed` stay on user pending the warp + item-upgrade slices.
- ⏳ Smoke-test in-game: bind altar, gate lever, warp gate, anvil item upgrade.
