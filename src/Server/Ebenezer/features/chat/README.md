# chat

Chat broadcast — general, private, party, knights, public, command, shout. Plus the WIZ_CHAT_TARGET handshake that picks a private-chat partner.

This is the **first slice that registers handlers with PacketRouter** — proves the Phase-2 dispatch infrastructure end-to-end with a high-traffic real packet.

## Layers

| Folder         | Lives here                                                                                    |
|----------------|-----------------------------------------------------------------------------------------------|
| `handlers/`    | `ChatService.{h,cpp}` — handles `WIZ_CHAT` and `WIZ_CHAT_TARGET`. Stateless.                  |
| `ChatModule.{h,cpp}` (slice root) | Registers both opcodes with the router during `EbenezerApp` startup.       |

## Public surface

```cpp
namespace Ebenezer::Features::Chat {
    class ChatService {
    public:
        void HandleChat(CUser* user, const char* pBuf, int len);        // WIZ_CHAT
        void HandleChatTarget(CUser* user, const char* pBuf, int len);  // WIZ_CHAT_TARGET
    };

    class ChatModule {
    public:
        static void Register(Shared::Network::PacketRouter& router, ChatService& service);
    };
}
```

## Behavior preserved verbatim

- AUTHORITY_NOCHAT silenced (returns early).
- GM `+` commands routed to `OperationMessage::Process` (admin command surface — its own concern).
- `PUBLIC_CHAT` / `ANNOUNCEMENT_CHAT` only allowed from `AUTHORITY_MANAGER`.
- Chat-type-specific dispatch unchanged:
  - `GENERAL_CHAT` → near-region.
  - `PRIVATE_CHAT` → both endpoints; requires prior `WIZ_CHAT_TARGET`.
  - `PARTY_CHAT` → party members.
  - `SHOUT_CHAT` → region; costs 20% max MP via `CUser::MSpChange`.
  - `KNIGHTS_CHAT` → clan members in same zone.
  - `PUBLIC_CHAT` → `Send_All`.
  - `COMMAND_CHAT` → captain-only commander chat.
- WIZ_CHAT_TARGET: linear scan over connected users for a name match (legacy O(n)).

## Migration status

- ✅ Logic extracted from `CUser::Chat` and `CUser::ChatTargetSelect`
- ✅ `WIZ_CHAT` and `WIZ_CHAT_TARGET` bound on `PacketRouter` via `ChatModule::Register`
- ✅ Old `case WIZ_CHAT:` / `case WIZ_CHAT_TARGET:` blocks removed from `CUser::Parsing` (router takes them first)
- ✅ Smoke-tested against in-game general chat, shout, party chat, GM `+king_status`

This is the **template for any router-bound feature**. Subsequent slices with `WIZ_*` opcodes follow the same pattern.
