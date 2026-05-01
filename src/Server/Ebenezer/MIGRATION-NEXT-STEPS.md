# Ebenezer VSA Migration — Next-Steps Handoff

**Branch:** `arch/ebenezer-vsa-migration`
**Last commit checked in:** `2c881e6a` (Phase 3.15 — NPC). Phase 3B (medium-risk) complete.
**Remote:** `origin/arch/ebenezer-vsa-migration` is up to date.

This document is the resume point for future sessions (any agent, any human). Read this **before** `ARCHITECTURE.md` and `MIGRATION-INVENTORY.md`; those are the contract and the catalog, this is the bookmark.

---

## State of play

### What's already migrated

| # | Slice                          | Pattern        | Commit       | Notes                                                       |
|---|--------------------------------|----------------|--------------|-------------------------------------------------------------|
| 0 | `features/king-system/`        | Full slice (PoC) | `9e4cf1a8`   | Domain + persistence extracted; handlers/presentation deferred. |
| 1 | `features/gold/`               | Service-only   | `0083301c`   | `CUser::GoldGain` / `GoldLose` are 2-line forwarders.        |
| 2 | `features/chat/`               | Router-bound   | `60404a42`   | Binds `WIZ_CHAT` (0x10) and `WIZ_CHAT_TARGET` (0x11).        |
| 3 | `features/loyalty/`            | Service-only   | `43e1692c`   | NP + Manner + chicken-party distribution preserved verbatim. |
| 4 | `features/item-repair/`        | Router-bound   | `ec7b64d2`   | Binds `WIZ_ITEM_REPAIR` (NPC blacksmith durability repair). |
| 5 | `features/promotion/`          | Router-bound   | `adff83df`   | Binds `WIZ_CLASS_CHANGE` (lvl-60 promotion + stat/skill respec quote). |
| 6 | `features/home/`               | Router-bound   | `d036a09f`   | Binds `WIZ_HOME` + `WIZ_REGENE` (bind point + resurrect; CUser::Regene retained for clerical magic). |
| 7 | `features/party-bbs/`          | Router-bound   | `d8753164`   | Binds `WIZ_PARTY_BBS` (LFG bulletin board: register / delete / needed). |
| 8 | `features/leveling/`           | Service-only   | `103b1ee5`   | Owns `ExpChange` + `LevelChange`; CUser methods are forwarders. |
| 9 | `features/stats/`              | Router-bound   | `fa4362fe`   | Binds `WIZ_POINT_CHANGE` + `WIZ_SKILLPT_CHANGE` (stat / skill alloc). |
| 10 | `features/party/`             | Router-bound   | `7d69ee45`   | Binds `WIZ_PARTY` (create/permit/insert/remove/delete dispatcher). |
| 11 | `features/object-events/`     | Router-bound   | `2c1795c2`   | Binds `WIZ_OBJECT_EVENT` (bind altar / gate-lever / flag / warp / anvil). |
| 12 | `features/item-upgrade/`      | Router-bound   | `f676442a`   | Binds `WIZ_ITEM_UPGRADE` (anvil weapon / accessory upgrade dispatcher). |
| 13 | `features/warehouse/`         | Router-bound   | `9fd8afe8`   | Binds `WIZ_WAREHOUSE` (per-account bank — body remains on CUser::WarehouseProcess). |
| 14 | `features/market-bbs/`        | Router-bound   | `87bd3271`   | Binds `WIZ_MARKET_BBS` (buy/sell board — register / delete / open / report / remote-purchase / message). |
| 15 | `features/npc/`               | Router-bound   | `2c881e6a`   | Binds `WIZ_REQ_NPCIN` + `WIZ_NPC_EVENT` (region NPC list + interaction dispatcher). |

### Shared infrastructure (Phase 2, ready for use)

- `shared/infrastructure/network/PacketRouter` — opcode → handler map. Currently 16 opcodes bound (chat × 2, item-repair, promotion, home × 2, party-bbs, stats × 2, party, object-events, item-upgrade, warehouse, market-bbs, npc × 2).
- `shared/infrastructure/tick/TickScheduler` — periodic-task subscriber. Currently 0 subscriptions.
- `shared/infrastructure/persistence/SqlHelpers` — `ExecuteSql`, `SqlEscape`. Used by the King System repo.

### Still in legacy locations

- `User.cpp` — was 14,943 LOC, now ~14,500. Most gameplay still here.
- `EbenezerApp.cpp` — was 3,548 LOC, basically untouched apart from infra wiring.
- `OperationMessage.cpp` — admin command surface (~1,500 LOC). Not yet a slice.
- All standalone files (Knights, Magic, Map, Npc, AISocket, UdpSocket, EVENT, EXEC) — untouched.

---

## The two slice patterns

When picking up the next migration, identify which pattern fits:

### Pattern A — Service-only

For features that are **server-side-called** with no inbound packet (Gold, Loyalty are templates).

**Steps:**

1. Create `features/<name>/handlers/<Name>Service.{h,cpp}`.
2. Move logic into the service. Methods take `CUser&` (and other deps) as parameters.
3. Add `Features::<Name>::<Name>Service m_<Name>Service;` member to `EbenezerApp`.
4. Add include in `EbenezerApp.h`: `#include <Ebenezer/features/<name>/handlers/<Name>Service.h>`.
5. Reduce the corresponding `CUser::*` methods to one-line forwarders: `m_pMain->m_<Name>Service.Method(*this, args);`.
6. Register the new files in `Ebenezer.Core.vcxproj` and `.filters` (mirror Gold's entries).
7. Build, smoke-test, commit.

**Look at `features/gold/` for the canonical layout.**

### Pattern B — Router-bound

For features that handle an **inbound `WIZ_*` packet** from the client (Chat is the template).

**Steps:**

1. Create `features/<name>/handlers/<Name>Service.{h,cpp}` with handler methods of signature
   `void Handle<X>(CUser* user, const char* pBuf, int len);`
2. Create `features/<name>/<Name>Module.{h,cpp}` with `static void Register(PacketRouter&, <Name>Service&);`
3. Add `m_<Name>Service` member to `EbenezerApp`.
4. In `EbenezerApp::OnStart`, after the existing `Features::Chat::ChatModule::Register(...)` call, add yours.
5. Delete the corresponding `case WIZ_*:` from the `User::Parsing` switch.
6. Delete the `CUser::<Method>` declaration in `User.h` and body in `User.cpp` — router takes the packet now.
7. Register everything in `Ebenezer.Core.vcxproj` and `.filters`.
8. Build, smoke-test (verify log shows `PacketRouter has N bound opcode(s)` with N going up by one).
9. Commit.

**Look at `features/chat/` for the canonical layout.**

---

## Recommended next slices (in order)

Each line is a 30-60 minute migration once you have the patterns memorized.

### Phase 3A complete

The "low-risk" pass (item-repair, promotion, home, party-bbs) is now shipped. Pivot to Phase 3B medium-risk slices below.

### Phase 3B complete

Leveling, Stats, Party, Object Events, Item Upgrade, Warehouse, Market BBS, and NPC have all shipped. Pivot to Phase 3C (high-coupling) below.

### Phase 3C (high-coupling — last)

`Exchange (Trading)`, `Movement`, `Item / Inventory`, `Quest / EVENT`, `Combat`, `Magic`, `Knights`, `Siege War`. Each needs careful planning; do **not** attempt without re-reading `MIGRATION-INVENTORY.md` for current footprint and dependencies. `Combat` and `Magic` are the final boss — defer until at least 3-4 medium-risk slices have shipped to validate cross-feature collaboration patterns.

### Special case

- **Friend system** (`User.cpp:705-710`) is `#if 0`-disabled. Decide before Phase 3C closes: re-enable and migrate, or delete the dead code (`FriendAccept`, `FriendRequest`, `FriendReport` declarations at `User.cpp:441-443`).

---

## Phase 4 (final cleanup, not yet started)

1. Delete dead `#if 0` blocks in `User.cpp` at lines 4977, 7154, 9319, 9424, 10361, 14547, 14577 — verify each isn't load-bearing first.
2. Delete stub files: `Knights.cpp` (55 LOC), `KnightsSiegeWar.cpp` (13 LOC), `GameEvent.cpp` (46 LOC) — once their content moves into slices.
3. Move `KingSystem.h`/`KingSystem.cpp` to `features/king-system/handlers/` (currently still in legacy location for compat).
4. Migrate `OperationMessage.cpp` admin commands into per-feature `presentation/` folders (King's `+royalorder` → `features/king-system/presentation/`, etc.).
5. Verify `User::Parsing` switch is empty (or close to it). Delete remaining boilerplate.
6. Squash + merge `arch/ebenezer-vsa-migration` → `master` in a single atomic commit. PR title: `refactor(ebenezer): VSA migration complete`.

---

## Build & smoke commands

PowerShell, every time:

```pwsh
# Stop running server, build core lib, then wrapper exe
$pid_eben = (Get-Process -Name Ebenezer -ErrorAction SilentlyContinue).Id
if ($pid_eben) { Stop-Process -Id $pid_eben -Force; Start-Sleep -Seconds 1 }

& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "C:\Users\Unkn0wn\source\repos\KnightOnline\src\Server\Ebenezer\Ebenezer.Core.vcxproj" `
  /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo

& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  "C:\Users\Unkn0wn\source\repos\KnightOnline\src\Server\Ebenezer\Ebenezer.vcxproj" `
  /p:Configuration=Debug /p:Platform=x64 /m /v:minimal /nologo

# Restart
$bin = "C:\Users\Unkn0wn\source\repos\KnightOnline\bin\Debug-x64"
$map = "C:\Users\Unkn0wn\source\repos\KnightOnline\assets\Server\MAP"
$quests = "C:\Users\Unkn0wn\source\repos\KnightOnline\assets\Server\QUESTS"
Start-Process -FilePath "$bin\Ebenezer.exe" `
  -ArgumentList "--map-dir=`"$map`"","--quests-dir=`"$quests`"" `
  -WorkingDirectory $bin
```

**Critical:** `Ebenezer.Core.vcxproj` MUST be built first. Building only `Ebenezer.vcxproj` relinks against a stale `.lib` — the change goes nowhere visible. (This caused 40 minutes of confusion in the King System debug round; saved as a memory entry: `feedback_ebenezer_build.md`.)

### Smoke-test signals

Log file: `bin\Debug-x64\logs\Ebenezer.log`. Look for:

- `KingSystemRepository::LoadAll: nation=1 byType=7 king='LeqenDRapToR'` — King System persistence still works.
- `EbenezerApp::OnStart: PacketRouter has N bound opcode(s)` — N goes up by 1 each time you migrate a router-bound slice.
- No `OperationMessage::Process: UNHANDLED command='+...'` warnings on basic GM commands.

Per-slice gameplay smoke tests (do these in-game with `LeqenDRapToR`):

| Slice          | Test                                                                                   |
|----------------|----------------------------------------------------------------------------------------|
| Gold           | NPC sale gives money; gold counter updates.                                            |
| Chat           | Type general chat; type `+king_status` and check log + chat banner.                    |
| Loyalty        | Kill an enemy nation player → NP increases; check `m_iLoyalty` packet.                 |
| Item Repair    | Visit blacksmith NPC, repair item.                                                     |
| Promotion      | Lvl 60 char triggers promotion quest. Respec NPC: ask cost (sub_type 1/2 → quote).      |
| Home           | `/town` warps to nation start; die → resurrect at bind; cleric Resurrection on a corpse. |
| Party BBS      | Open the LFG board: register a note, request the needed list, delete your note.        |
| Leveling       | Kill mob → exp bar moves; cross threshold → WIZ_LEVEL_CHANGE; die in non-battle zone above lvl 6 → exp drops. |
| Stats          | Spend a stat point in each of STR/STA/DEX/INT/CHA; spend a skill point into one tree.   |
| Party          | Invite a partner → accept/decline → kick → disband; verify party HP/level broadcasts.   |
| Object Events  | Bind altar, gate lever (opens linked gate), warp gate, anvil item upgrade quote.        |
| Item Upgrade   | Anvil weapon upgrade success/fail; accessory upgrade; verify durability mutation.       |
| Warehouse      | Open warehouse NPC; deposit + withdraw an item and gold; verify trade-lockout fail.     |
| Market BBS     | Open market board; post sell note; filtered listing; remote purchase; GM broadcast.     |
| NPC            | Zone in (NPCs visible); right-click each NPC kind to confirm interaction dispatch.       |
| Home           | Die → respawn at bind; type `/home` (or whatever the actual binding is).               |

---

## Pitfalls / gotchas observed so far

1. **`#include` paths.** When the file lives in a subfolder under `Ebenezer/`, use angle-include with the `Ebenezer/` prefix: `#include <Ebenezer/features/foo/bar.h>`. The build's include path adds `src/Server` so `<Ebenezer/...>` resolves correctly. Quoted relative includes break when files later move.
2. **`strnicmp` is `_strnicmp` on MSVC** with `/W4` + `/WX`. Use the underscored name in new code.
3. **PCH:** `#include "pch.h"` works in subfolder cpp files thanks to MSVC's PCH treatment — don't worry about the relative path mismatch, it's a special-case directive.
4. **Two pattern types are not mutually exclusive.** A feature can be both router-bound AND service-only (e.g., Magic has packet handlers AND is called by Combat). Use both: the service is the logic; the module's `Register` binds packets that call into it.
5. **`OperationMessage` GM commands** are a parallel surface to packets. When a feature has GM commands (King has 14+), they currently live in `OperationMessage.cpp`. Either accept that for now and migrate them in Phase 4, or move them into the slice's `presentation/` immediately. Decision: moving them with the slice produces the cleanest VSA boundary, but `OperationMessage.cpp`'s djb2 dispatch table is a single switch — splitting it requires either (a) feature modules each registering their commands with a similar router, or (b) leaving the `OperationMessage::King*()` methods as forwarders to slice-presentation handlers. Pick (a) when you build a `CommandRouter` to mirror `PacketRouter`. Defer until at least 3 slices have GM commands to migrate.
6. **`CUser` is a god class.** During Phase A migration: features take `CUser&` as a param, do not add methods to `CUser`. New behavior goes in the slice. The forwarder methods (`GoldGain`, `LoyaltyChange`, etc.) are *temporary* — they exist so existing call sites compile. Phase 4 is when we decide whether to delete them or keep them as a thin compat layer.
7. **Build cache.** If a change "doesn't seem to apply", check `bin\Debug-x64\Ebenezer.exe` `LastWriteTime`. If it didn't update, MSBuild's incremental check decided nothing changed — usually because `Ebenezer.Core.vcxproj` wasn't rebuilt. Rebuild Core explicitly.

---

## When you resume

1. Read this file (you are here).
2. Glance at `ARCHITECTURE.md` for the contract.
3. Glance at `MIGRATION-INVENTORY.md` for the catalog.
4. `git checkout arch/ebenezer-vsa-migration && git pull`.
5. Pick the next slice from "Recommended next slices" above.
6. Decide its pattern (A or B) — usually obvious from "is there an inbound `WIZ_*` opcode".
7. Copy the layout from `features/gold/` (Pattern A) or `features/chat/` (Pattern B).
8. Migrate, build, smoke-test, commit, push.
9. Update this file's "Already migrated" table to add the new entry.

---

## Out of scope for this migration

- AIServer module (`src/Server/AIServer/`) — separate VSA pass when Ebenezer is done.
- Aujard, VersionManager, ItemManager, LoginServer — same.
- Client (`src/Client/`) — not in scope.
- Network protocol changes — only structural refactor; packet wire format stays identical.
- Performance optimization — keep behavior, don't tune.
- New gameplay features (proc engine, set bonuses, lvl 83 cap) — these are blocked on the migration completing. After Phase 4, add a `features/proc-engine/` and `features/set-bonus/` from a clean base. See `MIGRATION-INVENTORY.md` "scope recommendation" for that follow-on.

---

Last updated: Phase 3.15 commit (NPC) — Phase 3B complete.
