# market-bbs

Buy / sell bulletin board — `WIZ_MARKET_BBS`. The packet body's first byte is a sub-opcode that selects register / delete / report / open / remote-purchase / GM-broadcast.

| Sub-opcode                  | Meaning                                       | Logic lives in                  |
|-----------------------------|-----------------------------------------------|---------------------------------|
| `MARKET_BBS_REGISTER`       | Post a buy / sell note                        | `CUser::MarketBBSRegister`      |
| `MARKET_BBS_DELETE`         | Remove this user's note                       | `CUser::MarketBBSDelete`        |
| `MARKET_BBS_REPORT`         | Filtered listing                              | `CUser::MarketBBSReport`        |
| `MARKET_BBS_OPEN`           | Initial listing on UI open                    | `CUser::MarketBBSReport`        |
| `MARKET_BBS_REMOTE_PURCHASE`| Confirm a remote barter                       | `CUser::MarketBBSRemotePurchase`|
| `MARKET_BBS_MESSAGE`        | GM emergency broadcast                        | `CUser::MarketBBSMessage`       |

The six sub-handlers stay on `CUser` because they touch the static market post arrays and Aujard persistence. Pre-dispatch the slice runs `MarketBBSBuyPostFilter` + `MarketBBSSellPostFilter` to prune expired or empty posts — same as the legacy dispatcher.

## Layers

| Folder         | Lives here                                                                                  |
|----------------|---------------------------------------------------------------------------------------------|
| `handlers/`    | `MarketBbsService.{h,cpp}` — handles `WIZ_MARKET_BBS`. Stateless.                           |
| `MarketBbsModule.{h,cpp}` (slice root) | Registers the opcode with the router during `EbenezerApp` startup.|

## Public surface

```cpp
namespace Ebenezer::Features::MarketBbs {
    class MarketBbsService {
    public:
        void HandleMarketBbs(CUser* user, const char* pBuf, int len);  // WIZ_MARKET_BBS
    };

    class MarketBbsModule {
    public:
        static void Register(Shared::Network::PacketRouter& router, MarketBbsService& service);
    };
}
```

## Behavior preserved verbatim

- Pre-dispatch buy/sell post pruning matches legacy `CUser::MarketBBS`.
- Sub-opcode dispatch order and forwarding match exactly.
- `MARKET_BBS_REPORT` and `MARKET_BBS_OPEN` both forward to `MarketBBSReport`, with the original sub-opcode passed as the response-type byte.
- Unknown sub-opcode logs an error and (release-only) closes the connection — same hostile-client policy.

## Migration status

- ✅ `WIZ_MARKET_BBS` bound on `PacketRouter` via `MarketBbsModule::Register`.
- ✅ `case WIZ_MARKET_BBS:` removed from `CUser::Parsing`; `CUser::MarketBBS` dispatcher (decl + body) deleted.
- ⏸ All `MarketBBS*` sub-handlers stay on user pending the market-state slice.
- ⏳ Smoke-test in-game: open the market BBS, post a sell note, search by filter, remote purchase a partner's note, GM `MARKET_BBS_MESSAGE` broadcast.
