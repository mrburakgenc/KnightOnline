# shared/domain/

Aggregates and value objects shared by 2+ features.

Currently planned residents:
- `user/CUser` — the god class, here as Phase A of the migration. Features access it by reference; new behavior goes in feature handlers, not here.
- `item/` — Item entity + ITEM_TABLE bindings.
- `skill/` — Magic/Skill entity + MAGIC_TABLE bindings.
- `zone/` — Zone/map metadata.
- `nation/` — Nation enum + helpers.

**No IO.** No DB, no network, no file system. Pure data + pure functions. Anything that does IO goes in `shared/infrastructure/` or a feature's `persistence/` or `presentation/`.
