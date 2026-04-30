# features/

One folder per gameplay feature. Each feature is a vertical slice — domain, handlers, persistence, presentation all co-located.

## Adding a new feature

```
features/<feature-name>/
├── README.md
├── domain/                  # entities, value objects (no IO)
├── handlers/                # command/query handlers (use cases)
├── persistence/             # repository implementations
├── presentation/            # packet handlers, GM commands
└── <FeatureName>Module.{cpp,h}   # registers handlers with router/scheduler
```

## Rules

- A feature **cannot** `#include` from another feature. If you need to, push the shared piece down to `shared/`.
- A feature **can** depend on anything in `shared/`.
- A feature's `Module` is the single composition entry — `EbenezerApp` constructs it once.
- New gameplay code goes here. Never in `User.cpp` or `EbenezerApp.cpp` (those are migration targets, shrinking).

See `../ARCHITECTURE.md` §3-4 for the dependency rule and feature anatomy.
