# TPlayerCache (client) — the roster FETCH (by relation, not push)

Engine object: **TPlayerCache** (`Class/Main/Cache/TPlayerCache.lua`). Server counterpart:
`server/src/charactermanager/` (the fetch is account→CharacterManager relation).

## Client flow
| Lua | file:line | does |
|---|---|---|
| `LoadPlayers(from,accountId,session)` | `TPlayerCache.lua:2` | → `getByRelation(from,accountId,"TAccount","CharacterManager", cb)` |
| `cb(result, objectList)` | `:4` | `for v in objectList: v.Player.m_AccountId = accountId` → `session:onLoadPlayersCacheResult(from,objectList)` (`:12`) |

## The key native fact
`getByRelation → getByRelationImpl` = **native `0x01109fa0`, RESOLVE-ONLY** (dead-lane #8): it returns
chars that ALREADY exist in the client cache/GWorld as **havings of the account's CharacterManager**.
It does **NOT construct** them. So the char must already be resident (built via replication) BEFORE
this fetch runs, or each `objectList` element resolves to nil → the Lua table deletes the slot →
empty roster → `count=0` (`loadlist-empty-nil-element-deletion`, `LUA_QUICKREF.md §2`).

## Edges
`tplayercache ─▶ charactermanager (relation resolve)` · `─▶ session (onLoadPlayersCacheResult)` ·
depends on `gworld` residency (the char must be in the OID table + CharacterManager relation first).

## OPEN
The residency that makes `getByRelation` succeed = the char-replication trigger (`gworld.md §5`).
