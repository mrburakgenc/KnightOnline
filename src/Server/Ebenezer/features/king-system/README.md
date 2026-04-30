# king-system

Per-nation monarchy: kings, election, impeachment, tax, NOAH/EXP events, royal commands (RoyalOrder, Reward, Weather), election scheduler, and candidate notice board.

## Layers

| Folder            | Lives here                                                                                              |
|-------------------|---------------------------------------------------------------------------------------------------------|
| `domain/`         | `KingConstants.h` (nation IDs, phase enum), `KingNationState.h` (per-nation state struct).              |
| `handlers/`       | `KingSystem.{h,cpp}` — orchestrator class. Mutates state, drives transitions, applies royal commands.   |
| `persistence/`    | `KingSystemRepository.{h,cpp}` — `KING_SYSTEM` row read/write, `KING_CANDIDACY_NOTICE_BOARD` CRUD.      |
| `presentation/`   | (Phase 0 PoC: deferred. WIZ_KING packet handler + `+king_*` GM commands still in `OperationMessage.cpp` and `KingSystem::PacketProcess`. Will migrate in a follow-up commit.) |

## Public surface

`EbenezerApp` constructs one `CKingSystem` and calls:
- `LoadFromDb()` at startup
- `Tick()` per game-time tick
- `PacketProcess(user, buf)` on `WIZ_KING` (until presentation layer split)

`OperationMessage` calls into `CKingSystem` for `+king_*` GM commands (until presentation layer split).

## Migration status

- ✅ Domain extracted (constants + state struct)
- ✅ Persistence extracted (DB layer behind a class)
- ⏳ Handlers — stays as `CKingSystem` for now, relies on injected repository
- ⏳ Presentation — packet handler + GM commands still in legacy locations
- ⏳ Module registration — minimal; full registration after PacketRouter exists in `shared/infrastructure/network/`

This slice is intentionally **incomplete** as a PoC. The folder layout, layering, and dependency rule are demonstrated; the remaining splits are mechanical and will be done in follow-up commits as the migration progresses.
