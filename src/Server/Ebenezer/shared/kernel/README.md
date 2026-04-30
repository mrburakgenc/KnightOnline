# shared/kernel/

Cross-cutting primitives. The "stdlib of this server."

Planned residents:
- `Result<T>` / `Result<T, E>` — explicit success/error returns instead of exceptions in hot paths.
- Error types — common error categories (NotFound, InvalidArgument, NationMismatch, etc.).
- Packet codec utilities — `PacketReader` / `PacketWriter` thin wrappers over the existing `Get*` / `Set*` free functions.
- Common typedefs — `CharacterId`, `AccountId`, `ZoneId`, etc., to make function signatures unambiguous.

If a piece of code is generic enough that *every* feature might use it, it lives here. If only one or two do, it lives in those features (or in `shared/domain` if it's domain-shaped).
