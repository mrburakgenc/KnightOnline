# shared/infrastructure/

IO concerns shared across features.

| Subfolder        | Purpose                                                                              |
|------------------|--------------------------------------------------------------------------------------|
| `network/`       | TcpServer, PacketRouter, packet encoders/decoders, connection lifecycle.             |
| `persistence/`   | Database connection pool, base repository, transaction helpers.                      |
| `ai/`            | Communication with AIServer (UDP socket, NPC info packets).                          |
| `tick/`          | TickScheduler — features register periodic callbacks here instead of polling time.   |

These are *services* that features consume by constructor injection. No global state, no singletons — `EbenezerApp` constructs them once at startup and hands references down.
