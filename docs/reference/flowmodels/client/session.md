# Session (client) — the login/sign-in RPC handlers

Engine object: **Session** — a Com of `TAccount` (`Class/Main/Session.lua`; template component
`obj18-session-is-template-component`). Server counterpart: `server/src/session/`.

## Client-invoked handlers (this tree) — StoC RPCs the host drives
| Lua handler | file:line | does |
|---|---|---|
| `signInClientResult(from,result,master,channel)` | `Session.lua:10` | → `user:onSignInResult` — master sign-in OK |
| `logInClientResult(from,result,master,channel,accountId,plrId,type)` | `Session.lua:117` | → `user:onLogInResult` — account login OK |
| `Client_FinishLoadingCharacter(from,result,count)` | `Session.lua:129` | → `user:onFinishLoadingCharacter`; **count==0 → `UIIntro:ShowJobClassSelection`** (E1 end, reached ✅); count>=1 → char-SELECT roster |
| `LuaOnLogin` / `LuaOnLogout` | `Session.lua:178/185` | session lifecycle (no client handshake) |

All log to `TW/Bin/client_internal.log` (the Lua channel). The host emits these as ClientWorld
RemoteCall/DownCall packets — see `server/src/net/packets.py` `build_loginclientresult_downcall`,
and `build_clientworld_remotecall(..., "signInClientResult"/"Client_FinishLoadingCharacter")`.

## Server-invoked (→ server tree, NOT here)
`logIn`(handler `a6d6b0`), `logInMaster`(`a6e1e0`), `logInServerResult`(`a6ec40`) DownCalls — the
client→server login state machine (`claude_loginserverresult_wire_spec.md`). Addressed to the Session
object; `logInServerResult` sets `Session+0x8c=1`.

## Edges
`session ─▶ user (onSignInResult/onLogInResult/onFinishLoadingCharacter)` · `Client_FinishLoadingCharacter ─▶ uiintro (ShowJobClassSelection | roster)` · driven by `net/login.py` clean-lane sequence.

## Notes / status
Clean E1/E2 login (this whole handler chain) is **live-verified through `server/run_server.py`**
(2026-07-01) → `ShowJobClassSelection`. See `LUA_QUICKREF.md §2.0`.
