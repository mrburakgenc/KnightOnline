# Ebenezer Migration — Phase 1 Inventory

**Status:** Phase 1 audit complete (this commit). Reference document for the rest of the migration on `arch/ebenezer-vsa-migration`.

## Headline numbers

- **28 gameplay features** + 5 infrastructure systems still living in monolithic files
- **User.cpp:** ~14,943 LOC (god class)
- **EbenezerApp.cpp:** ~3,548 LOC (god class)
- **Standalone feature files:** ~9,252 LOC across MagicProcess, KnightsManager, OperationMessage, EVENT, Map, Npc, etc.
- **Dead code:** 8 `#if 0` blocks, 5 `// TODO` markers, 1 fully disabled feature (Friend system), 3 stub files (Knights.cpp, KnightsSiegeWar.cpp, GameEvent.cpp)

## Feature isolation rating cheatsheet

5/5 = self-contained, almost-zero CUser/EbenezerApp coupling.
1/5 = deeply wired into the god classes; every line touches CUser internals.

The migration order below sorts by ascending difficulty.

## Migration order

### Phase 2 — Shared infrastructure (prereq for everything else)

These must exist in `shared/infrastructure/` before any feature with packet handlers or timed behavior migrates:

| # | Component                                         | Why                                                                                |
|---|---------------------------------------------------|------------------------------------------------------------------------------------|
| 1 | `PacketRouter` (`network/PacketRouter.{h,cpp}`)   | Replaces `User::Process` giant switch. Every feature with packets needs this.      |
| 2 | `TickScheduler` (`tick/TickScheduler.{h,cpp}`)    | Replaces ad-hoc tick blocks in `EbenezerApp::Tick`. Needed by King, Siege, Market. |
| 3 | `BaseRepository` interface                        | Contract every feature's repository conforms to (already partially exists).        |
| 4 | DI composition root in `EbenezerApp`              | Move from implicit globals to constructor injection.                               |
| 5 | (Defer) `EventBus` for cross-feature events       | Add only when first cross-feature need surfaces (Party→Chat invites, etc.).        |

### Phase 3A — Low-risk slices (high isolation, small size)

| # | Feature             | LOC  | Isolation | Notes                                                |
|---|---------------------|------|-----------|------------------------------------------------------|
| 1 | **Gold**            | 30   | 5/5       | First slice. Stateless, pure mutation + broadcast.   |
| 2 | **Warp / Portal**   | 30   | 5/5       | `SelectWarpList` + ZoneChange.                       |
| 3 | **Home / Resurrect**| 280  | 4/5       | `WIZ_HOME`, `WIZ_REGENE`, bind point logic.          |
| 4 | **Item Repair**     | 70   | 4/5       | NPC-driven durability mutation.                      |
| 5 | **Promotion**       | 80   | 4/5       | Class change at lvl 60.                              |
| 6 | **Chat**            | 110  | 4/5       | Stateless message broadcast.                         |
| 7 | **Loyalty / NP**    | 100  | 4/5       | National point state.                                |
| 8 | **Party BBS**       | 100  | 4/5       | Posting/search list ops.                             |

### Phase 3B — Medium-risk slices (100-600 LOC, some coupling)

| #  | Feature              | LOC  | Isolation | Notes                                                |
|----|----------------------|------|-----------|------------------------------------------------------|
| 9  | **Leveling / Exp**   | 100  | 3/5       | Triggers Promotion at lvl 60.                        |
| 10 | **Stats / Skill pt** | 150  | 3/5       | Stat alloc + skill point alloc.                      |
| 11 | **Party**            | 150  | 3/5       | Runtime state in EbenezerApp::m_PartyMap.            |
| 12 | **Object Events**    | 100  | 3/5       | Trap / lever / gate / bind state machines.           |
| 13 | **Item Upgrade**     | 600  | 3/5       | RNG + durability mutation.                           |
| 14 | **Warehouse**        | 290  | 3/5       | Per-account container.                               |
| 15 | **Market BBS**       | 300  | 3/5       | Async list management.                               |
| 16 | **NPC Interaction**  | 450  | 2/5       | NpcEvent is a big dispatcher — split by NPC kind.    |

### Phase 3C — High-coupling slices (the god-class-heavy ones)

| #  | Feature                | LOC   | Isolation | Notes                                              |
|----|------------------------|-------|-----------|----------------------------------------------------|
| 17 | **Exchange / Trading** | 600   | 2/5       | Cross-player critical section. Needs Item migrated.|
| 18 | **Movement**           | 550   | 3/5       | Region-coupled. Needs Region/Map first.            |
| 19 | **Item / Inventory**   | 1,200 | 3/5       | Fragmented; ItemMove/ItemGet first, full later.    |
| 20 | **Quest / EVENT**      | 400   | 2/5       | EXEC system intertwined with EVENT_DATA mutations. |
| 21 | **Combat**             | 600   | 2/5       | Attack() + damage calc tightly entangled.          |
| 22 | **Magic**              | 3,200 | 4/5       | Self-contained engine BUT couples to CUser cast state. |
| 23 | **Knights**            | 1,650 | 2/5       | Alliance + ranking state. KnightsManager partial.  |
| 24 | **Siege War**          | ~100  | 1/5       | Time-driven, deeply wired into EbenezerApp::Tick.  |
| 25 | **Friend (disabled)**  | 50    | 5/5       | Decide: re-enable + migrate, or delete.            |

### Already migrated

- **King System** ✅ Phase 0 PoC (`features/king-system/`).
  - Domain + persistence layers extracted.
  - Handlers + presentation deferred (CKingSystem still in legacy location, GM commands in `OperationMessage.cpp`, packet handler in `KingSystem::PacketProcess`).

## Dead code map

### `#if 0` blocks in User.cpp
- Line 706 — `WIZ_FRIEND_PROCESS` dispatch — `// outdated`
- Line 4977 — `// not typically available`
- Line 7154 — unmarked
- Line 9319 — unmarked
- Line 9424 — unmarked
- Line 10361 — `// outdated`
- Line 14547 — `// TODO`
- Line 14577 — `// TODO:`

### `// TODO` markers in User.cpp
- Line 1902 (GameStart context)
- Line 14384 — "Possible bug, should likely send `m_pMain->m_nServerGroup`"
- Line 14530
- Line 14547 (also in #if 0)
- Line 14577 (also in #if 0)

### Stub / orphaned files
- `Knights.cpp` — 55 LOC, all logic is in `KnightsManager.cpp`.
- `KnightsSiegeWar.cpp` — 13 LOC, orchestration is in `EbenezerApp::Tick`.
- `GameEvent.cpp` — 46 LOC, mostly EVENT_DATA-driven.

### Disabled features
- **Friend system** (User.cpp:705-710 + methods at 441-443). Methods exist; packet dispatch is `#if 0`. Decide whether to restore or delete during Phase 3.

## Cross-feature dependency graph

```
            ┌── Chat
            │
Leveling ───┤── Stats ──┬── Combat ──┬── Magic
            │            │            │
            │            └── Loyalty  │
            │                         │
Movement ───┼─────────────────────────┴── NPC
            │
            └── Item ──┬── Warehouse
                       ├── Exchange
                       ├── Repair
                       ├── Upgrade
                       └── Object Events

Party ──── Party BBS
Market BBS (independent)

Knights ──┬── Loyalty
          ├── Combat
          └── Siege War

King ─────┬── Loyalty
          └── Chat

Promotion ─── Leveling + Stats
```

## Phase-by-phase effort estimate

| Phase    | Scope                                  | Estimate (passion-project hours) |
|----------|----------------------------------------|----------------------------------|
| 0        | Architecture + King PoC                | ✅ Done                           |
| 1        | This inventory                         | ✅ Done                           |
| 2        | Shared infra (Router, Tick, DI, Repo)  | 6-10h                             |
| 3A       | 8 low-risk features                    | 12-20h (1.5-2.5h each)            |
| 3B       | 8 medium-risk features                 | 30-60h (3-7h each)                |
| 3C       | 9 high-coupling features (incl. Magic) | 60-120h                           |
| 4        | Final cleanup, monolith deletion       | 4-8h                              |
| **Total**|                                        | **~110-220h**                     |

This is hobby-project pacing — no deadline, ships when it ships.

## First slice after Phase 2

**Gold System.** 30 LOC, isolation 5/5, zero cross-feature dependencies. Establishes the per-feature pattern (domain value object + handler + packet adapter + module registration) that the next 7 low-risk slices reuse mechanically.

Last updated: Phase 1 inventory commit.
