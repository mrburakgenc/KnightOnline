# Ebenezer — VSA Migration Architecture

**Status:** Phase 0 (architecture spike). Definitive for the migration on `arch/ebenezer-vsa-migration`.

## 1. Vision

Convert Ebenezer (the main game server) from monolithic god-classes (`User.cpp` 14k lines, `EbenezerApp.cpp` 4k lines) into **Vertical Slice Architecture (VSA)** — every gameplay feature is a self-contained folder with its own domain, handlers, persistence, and presentation.

Goals:
- One feature → one folder → one mental model. New devs (and AI assistants) load *only* the relevant slice.
- Dead code dies as features migrate. The `#if 0 // TODO` graveyard gets read, decided, and either ported or deleted.
- Eventual divergence from upstream Open-KO becomes painless because we control the structure.

Non-goals:
- Rewriting the network layer, packet pipeline, zone manager, or AI tick loop. Those stay as **shared infrastructure** — features call into them, not the other way around.
- Forcing CQRS, DDD, or any other paradigm where it adds ceremony without benefit. VSA here means *folder layout + dependency rule*, not a heavyweight framework.

## 2. Folder layout

```
src/Server/Ebenezer/
├── ARCHITECTURE.md             ← this file
├── EbenezerApp.{cpp,h}         ← composition root (will shrink dramatically)
├── shared/                     ← cross-feature: anything ≥2 features need
│   ├── domain/                 ← shared aggregates (CUser, Item, Skill, Zone, Nation)
│   ├── infrastructure/
│   │   ├── network/            ← TcpServer, PacketRouter, packet codecs
│   │   ├── persistence/        ← DB connection pool, base repository
│   │   ├── ai/                 ← AIServer comm
│   │   └── tick/               ← TickScheduler
│   └── kernel/                 ← Result<T>, error types, common typedefs, packet utilities
└── features/                   ← one folder per gameplay feature (see §3)
    └── <feature-name>/
        ├── README.md           ← feature-specific docs
        ├── domain/             ← feature-only entities/value objects
        ├── handlers/           ← command/query handlers (the "use cases")
        ├── persistence/        ← feature-specific repositories
        ├── presentation/       ← packet handlers, GM commands
        └── <Feature>Module.{cpp,h}  ← feature registration entry point
```

**Conventions:**
- Folders: `kebab-case` (`king-system`, `quest-engine`).
- Files / classes: `PascalCase` (existing project style — keep `C` prefix where already present, drop it for new classes in this migration).
- Namespaces: `Ebenezer::Features::<FeatureName>` for feature code, `Ebenezer::Shared::<Layer>` for shared.

## 3. Dependency rule

```
features/* ──────► shared/* ──────► (no further internal deps)
features/* ──X──► features/*           cross-feature import is FORBIDDEN
shared/* ──X──► features/*             shared cannot reach into features
```

If two features need the same logic → push it down to `shared/`. If one feature needs to talk to another → publish a domain event in `shared/kernel/events` and let the other subscribe (next phase concern, not initial migration).

This rule is the entire point of VSA. Violating it means you've reinvented the spaghetti you're trying to leave.

## 4. Feature anatomy

Every feature folder follows the same internal layout. Empty subfolders are allowed if a feature genuinely has no presence at that layer.

| Subfolder         | Contains                                                                                  |
|-------------------|-------------------------------------------------------------------------------------------|
| `domain/`         | Entities, value objects, domain logic with **zero** IO. Pure functions, testable in isolation. |
| `handlers/`       | One file per command/query. Orchestrates domain + persistence. Receives a request, returns a result. |
| `persistence/`    | Concrete repository implementations. Each repo has an interface in `domain/` (or in `handlers/` if not shared). |
| `presentation/`   | Packet handlers, GM command handlers, UI-facing translators. Calls into `handlers/`.       |
| `<Feature>Module` | Registration: `void Register(PacketRouter&, TickScheduler&, ...)`. Wires handlers to packet opcodes. |

Each feature is **buildable, testable, and reasonable about in isolation**. If you can't understand a feature without opening another feature's files, the slicing is wrong.

## 5. Packet routing

Today: giant `switch (opcode)` in `User::ParsePacket` and `EbenezerApp::Run`.

Target:
- `shared/infrastructure/network/PacketRouter` owns a `std::unordered_map<uint8_t, PacketHandler>`.
- Each feature's `Module::Register(router)` calls `router.Bind(WIZ_KING, &KingPacketHandler::Handle)`.
- `User::ParsePacket` becomes a one-liner: `m_pMain->Router().Dispatch(opcode, this, buffer)`.

The router is the *only* place that knows the full opcode → handler map. The big switch goes away by deletion as features migrate.

## 6. Tick scheduler

Today: ad-hoc `if (now > m_LastFooTick + interval)` blocks scattered across `EbenezerApp::Tick`.

Target:
- `shared/infrastructure/tick/TickScheduler` accepts `Subscribe(intervalMs, std::function<void()>)`.
- Features register via `module.Register(scheduler)`.
- Existing tick blocks migrate one at a time. `EbenezerApp::Tick` shrinks to `m_TickScheduler.Run(now)`.

## 7. DI / composition

No DI framework. **Manual constructor injection** at the composition root.

- `EbenezerApp` constructor builds shared infrastructure (DB, network, tick, router).
- Then constructs each feature's module: `m_KingSystemModule = std::make_unique<KingSystemModule>(m_DbPool, m_Router, m_TickScheduler)`.
- Each module owns its handlers, repos, etc.
- Lifetime: shared infra outlives features; features outlive their internal handlers.

Service location is forbidden (no `GetGlobalThing()` calls inside handlers). Everything a handler needs is constructor-injected.

## 8. Domain modeling — the CUser problem

`CUser` is 14k lines and touches *everything*. Splitting it perfectly into per-feature aggregates is a massive task. Migration strategy:

**Phase A (now):** `CUser` lives in `shared/domain/user/`. Every feature gets `CUser&` as needed. Features add behavior via free functions in their own `handlers/`, not by adding methods to `CUser`. This freezes `CUser`'s public surface while letting features migrate.

**Phase B (later):** Identify cohesive slices of `CUser` state (e.g., trade state, party state, magic cooldowns) and extract them into per-feature aggregates that `CUser` *holds* by composition. `CUser` shrinks naturally.

**Phase C (much later):** `CUser` becomes a thin coordinator over per-feature aggregates. Maybe.

We do not block Phase A on Phases B/C.

## 9. Handler pattern

Lightweight, no MediatR-style bus. Each handler is just:

```cpp
namespace Ebenezer::Features::KingSystem {

class StartElectionHandler {
public:
    StartElectionHandler(IKingRepository& repo, ITickScheduler& tick, IBroadcaster& bcast);

    Result<void> Handle(const StartElectionCommand& cmd);

private:
    IKingRepository& _repo;
    ITickScheduler& _tick;
    IBroadcaster& _bcast;
};

} // namespace
```

Commands and queries are plain structs in the same folder. No reflection, no codegen, no global registration.

## 10. Tests

This codebase has gtest available. Every feature should ship with at least:
- One unit test per non-trivial domain entity.
- One handler test using fakes for repositories.
- (Optional) Integration test using a test DB.

Tests live at `features/<feature>/tests/`. They run as part of the existing test target (added to `Ebenezer.Core.vcxproj` filters).

If a feature is migrated *without* any tests because the source had none, that's acceptable for the migration — but **don't add new behavior** during migration. Pure move + test the move with a smoke test.

## 11. Migration order

1. **Phase 0 (this branch's first commits):** ARCHITECTURE.md + folder skeleton + King System as PoC slice.
2. **Phase 1:** Inventory pass — list every feature in Ebenezer, mark dead code, set migration order based on size & risk.
3. **Phase 2:** Build out shared infrastructure (PacketRouter, TickScheduler, repository base, DI composition).
4. **Phase 3:** Migrate features one at a time. Order: smallest/most-isolated first (Inn, Warp), god-class slices last (combat, movement).
5. **Phase 4:** Delete emptied monolithic files. Verify nothing is left behind. Single atomic merge to `master`.

`master` does NOT receive new gameplay features during this migration. Bug fixes only, and only if urgent.

## 12. Anti-patterns

**Banned during migration:**
- Adding new code to `User.cpp` / `EbenezerApp.cpp`. New code goes in a feature slice or shared infrastructure.
- Cross-feature `#include`. If you need it, you've put logic in the wrong place.
- "I'll add this method to `CUser` for now and refactor later." Later never comes.
- New global singletons. Use the composition root.
- Service locator (`GetGlobalKingSystem()`). Inject it.

**Allowed but watched:**
- Free functions in `shared/kernel/` that operate on `CUser&`. OK as long as they're stateless and have one job.
- `friend` declarations across feature boundaries. Smell. Justify in PR.

## 13. AI-assistance notes

This file is the contract for any AI agent (Claude Code, Copilot, Cursor) helping with Ebenezer work. When asked to add a feature or modify behavior:

- Locate or create the feature folder under `features/`.
- Never modify `User.cpp` or `EbenezerApp.cpp` to add behavior — they are migration targets, not destinations.
- Read the relevant feature's `README.md` first.
- If the change touches infrastructure (network, DB, tick), update the shared layer, not a feature.
- If the change requires cross-feature collaboration, *stop and ask* — that's an architectural decision, not a routine change.

Small slices, clear boundaries, predictable patterns = AI doesn't have to guess where things go. Hallucinations come from ambiguity. We are removing the ambiguity.

## 14. Open questions (deferred)

- Cross-feature events: do we need a pub/sub bus, or is direct call through `shared/kernel` interfaces enough? Decide after first 3 feature migrations.
- AIServer/Aujard slicing: same migration, separate branch later.
- Feature-as-static-lib: each feature module compiled as its own `.lib`? Build complexity vs. enforcement of dependency rule. Defer.
- `CUser` decomposition (Phase B/C): plan after enough features have migrated to see the cleavage planes.

Last updated: Phase 0 commit (initial spike).
