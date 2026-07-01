# Player (client) — the char's identity Com

Engine object: **Player** (`Class/Main/Player.lua`; a Com of `TPlayer`). Server counterpart:
`server/src/char/`. Carries the char's account identity + the char-select data handshake.

## Client-invoked
| Lua | file:line | does |
|---|---|---|
| `LuaOnClientLoad(self)` | `Player.lua:101` | `self:Community_LuaOnClientLoad()`; then `self:ClientGetSRQL()` (`:104`) — **first char only**, gated `DataUI_SelectedRandomQuestListIsNotFirst` |
| `OnClientGetSRQLResult` (RPC, `RPCType.Client`) | `:222` (`SetLuaNetFunc`) | receives the selected-random-quest-list = NORMAL char-select data (host stub `answer_srql_request`) |
| identity fields | — | `m_AccountId` (set by `onLoadPlayersCacheResult` cb), `m_name` (→ the ROW name) |
| `ClientWorld:FindObject(...)` | `:518/1004/1029` | client-side resolves |

`ClientGetSRQL` is NORMAL char-select data, **not** an in-game boot (`LUA_QUICKREF.md §2`).

## ROW contribution
name = `Player.m_name`; `m_AccountId` from the cache cb. The rest of the row (level/job) = `cstatusplayer.md`.

## Edges
`player ─▶ cstatusplayer (row columns)` · `─▶ csharedata` · relates `user (dropped for row)` ·
`LuaOnClientLoad ─▶ OnClientGetSRQLResult (SRQL round-trip)`.
