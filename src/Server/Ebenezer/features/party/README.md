# party

Party formation — `WIZ_PARTY`. The packet body's first byte is a sub-opcode that selects create / accept-or-decline / leader-add / kick / disband.

| Sub-opcode       | Meaning                                  | Logic lives in              |
|------------------|------------------------------------------|-----------------------------|
| `PARTY_CREATE`   | Invite by name; create the party         | `CUser::PartyRequest(.., true)` |
| `PARTY_PERMIT`   | Accept (≠0) → `PartyInsert`; decline (0) → `PartyCancel` | `CUser::PartyInsert` / `CUser::PartyCancel` |
| `PARTY_INSERT`   | Leader-only invite of an additional name | `CUser::PartyRequest(.., false)` |
| `PARTY_REMOVE`   | Kick by socket id                        | `CUser::PartyRemove`        |
| `PARTY_DELETE`   | Disband / leave                          | `CUser::PartyDelete`        |

The five sub-handlers stay on `CUser` because they mutate runtime party state on `EbenezerApp::m_PartyMap` and the user's session helpers. The slice owns the **packet boundary** only.

## Layers

| Folder         | Lives here                                                                                  |
|----------------|---------------------------------------------------------------------------------------------|
| `handlers/`    | `PartyService.{h,cpp}` — handles `WIZ_PARTY`. Stateless.                                    |
| `PartyModule.{h,cpp}` (slice root) | Registers the opcode with the router during `EbenezerApp` startup.      |

## Public surface

```cpp
namespace Ebenezer::Features::Party {
    class PartyService {
    public:
        void HandleParty(CUser* user, const char* pBuf, int len);  // WIZ_PARTY
    };

    class PartyModule {
    public:
        static void Register(Shared::Network::PacketRouter& router, PartyService& service);
    };
}
```

## Behavior preserved verbatim

- Sub-opcode dispatch matches legacy `CUser::PartyProcess` exactly.
- `PARTY_CREATE` / `PARTY_INSERT`: read 16-bit string length + character name; reject if length ≤ 0 or > `MAX_ID_SIZE`; resolve to a connected user; resolve to socket id; call `PartyRequest`.
- `PARTY_PERMIT`: 1 byte result; non-zero → `PartyInsert`, zero → `PartyCancel`.
- `PARTY_REMOVE`: 16-bit socket id, forwarded to `PartyRemove`.
- `PARTY_DELETE`: no payload, calls `PartyDelete`.
- Unknown sub-opcode logs an error and (release-only) closes the connection — same hostile-client policy as the legacy switch.

## Migration status

- ✅ `WIZ_PARTY` bound on `PacketRouter` via `PartyModule::Register`.
- ✅ `case WIZ_PARTY:` removed from `CUser::Parsing`; `CUser::PartyProcess` (decl + body) deleted.
- ⏸ `CUser::PartyRequest` / `Insert` / `Cancel` / `Remove` / `Delete` stay on user — they will move once party-state ownership migrates into this slice (will need a `PartySession` value object owning `m_PartyMap` entries).
- ⏳ Smoke-test in-game: invite a partner, accept, level-up broadcast, kick, disband.
