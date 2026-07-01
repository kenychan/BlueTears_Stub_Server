# TAccount (client) — the account object (4 Coms), built by-name on login

Engine object: **TAccount** (`Object/Main/TAccount.lua`). Server counterpart: `server/src/account/`.
Built LOCALLY by-name (`bb2580`) during login (LATE-62); **proven FULLY FAITHFUL** — the E1-log
`SELFDUMP` shows all 4 Coms materialize with non-null values + self-register in obj+0x14 AND obj+0x18
(`taccount-fully-faithful-late140`).

## The 4 Coms (per-Com client handshake — LATE-140/141)
| Com | kind | client `LuaOnClientLoad`? | file |
|---|---|---|---|
| **Session** | login/sign-in RPCs | none (`LuaOnLogin`/`LuaOnLogout` only) | `session.md` |
| **CharacterManager** | char roster (**havable**) | NONE | `charactermanager.md` |
| **CharacterHolder** | holds char havings (NotReplicate) | NONE | (here) |
| **CSharedData** | game data (OwnerOnly) | `LuaOnClientLoad` — GWorld-sourced, client-local | `csharedata.md` |

⇒ **No account-tier handshake we can skip blocks char load** (3 Coms have none; the 1 that does is
GWorld-sourced in-game data). CharacterHolder (`Class/Main/CharacterHolder.lua`) = NotReplicate holder,
no client handshake.

## Edges
`taccount ─▶ {session, charactermanager, characterholder, csharedata}` (its 4 Coms) · built by
`template`/`bb2580` on `logInClientResult`.

## Status
Account tier = E2, DONE (fully faithful). The remaining work is E3 (char residency into
CharacterManager), not the account tier. Host: `server/src/host.py` `setup_account`.
