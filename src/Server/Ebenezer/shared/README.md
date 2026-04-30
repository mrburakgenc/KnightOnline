# shared/

Cross-feature code. Anything two or more features need lives here.

| Folder            | Purpose                                                                                     |
|-------------------|---------------------------------------------------------------------------------------------|
| `domain/`         | Shared aggregates and value objects (CUser, Item, Skill, Zone, Nation). Pure, no IO.        |
| `infrastructure/` | IO concerns: network, persistence, AI server comm, tick scheduler.                          |
| `kernel/`         | Cross-cutting primitives: `Result<T>`, error types, packet codec utilities, common typedefs. |

**Rule:** `shared/` cannot `#include` anything under `features/`. Inversion-of-dependency applies — features depend on shared, never the other way.

See `../ARCHITECTURE.md` for the full dependency model.
