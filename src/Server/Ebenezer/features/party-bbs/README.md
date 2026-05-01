# party-bbs

Group-finder bulletin board — `WIZ_PARTY_BBS`. The packet body's first byte is a sub-opcode that selects register / delete / needed-list flows.

| Sub-opcode             | Meaning                                       | Logic lives in              |
|------------------------|-----------------------------------------------|-----------------------------|
| `PARTY_BBS_REGISTER`   | Post / overwrite this user's 'LFG' note       | `CUser::PartyBBSRegister`   |
| `PARTY_BBS_DELETE`     | Remove this user's 'LFG' note                 | `CUser::PartyBBSDelete`     |
| `PARTY_BBS_NEEDED`     | Request the current open-needs list           | `CUser::PartyBBSNeeded`     |

The three sub-handlers stay on `CUser` because they read private session/inventory state. The slice owns the **packet boundary**, not the implementation.

## Layers

| Folder         | Lives here                                                                                  |
|----------------|---------------------------------------------------------------------------------------------|
| `handlers/`    | `PartyBbsService.{h,cpp}` — handles `WIZ_PARTY_BBS`. Stateless.                             |
| `PartyBbsModule.{h,cpp}` (slice root) | Registers the opcode with the router during `EbenezerApp` startup.|

## Public surface

```cpp
namespace Ebenezer::Features::PartyBbs {
    class PartyBbsService {
    public:
        void HandlePartyBbs(CUser* user, const char* pBuf, int len);  // WIZ_PARTY_BBS
    };

    class PartyBbsModule {
    public:
        static void Register(Shared::Network::PacketRouter& router, PartyBbsService& service);
    };
}
```

## Behavior preserved verbatim

- Sub-opcode dispatch matches legacy `CUser::PartyBBS` exactly.
- `PARTY_BBS_NEEDED` forwards `PARTY_BBS_NEEDED` as the response-type byte (the legacy code reused the constant for both inbound + outbound).
- Unknown sub-opcode logs an error and (release-only) closes the connection — same hostile-client policy as the legacy switch.

## Migration status

- ✅ `WIZ_PARTY_BBS` bound on `PacketRouter` via `PartyBbsModule::Register`.
- ✅ `case WIZ_PARTY_BBS:` removed from `CUser::Parsing`; `CUser::PartyBBS` dispatcher (decl + body) deleted.
- ⏸ `CUser::PartyBBSRegister` / `Delete` / `Needed` stay on user — they will move once the inventory / party state slices land.
- ⏳ Smoke-test in-game: open the party BBS UI, post a note, request the needed list.
