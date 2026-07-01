# CharacterManager — the char roster registry (havable Com of TAccount)

Engine object: **CharacterManager** (`Class/Main/CharacterManager.lua`; a **havable** Com — chars are
its havings). Server counterpart: `server/src/charactermanager/`. This object spans BOTH tiers.

## Client-invoked (this tree) — resolve + paint
| Lua / native | where | does |
|---|---|---|
| `getAllPlayers` | CharacterManager.lua | reads `CharacterManager+0x28` = `std::map<int,Object*>` (RB tree; node value @+0x10) |
| native `LoadList` `0x00a620c0` | — | parses arg3 `std::list` of bare char `Object*`; per-elem `a60f00` reads `Player+0x4c` = OID identity |
| `AddCharacter` | UIIntro consumes | native `bc6020` = `std::map::operator[]` insert → registers the row |
| resolve gate | — | `getAllPlayers` → 2× `bd3660` gate → `bc5860` resolve → `AddCharacter`; each objectList elem → `ba9420` unbox |

Empty `+0x28` ⇒ `HasNoCharacter` ⇒ no row. A resolved char with the right comps traverses the whole
path (`server/docs/derisk/D2-constructed-char-row.md`).

## Server-invoked (→ server tree) — residency
| Lua | file:line | does |
|---|---|---|
| `serverCreateCharacterResult(self,_,oid,result,info)` | `CharacterManager.lua:80` | `local object = GWorld:FindObject(oid)` (`:85`) → **`GWorld:FindObject(oid):BackupReplicate(true)` (`:89`)** ← the char-residency TRIGGER (server→client push) |
| `LuaCheckAccountValidation` | `:128` (`GWorld:FindObject(playeroid)`) | server-side validation with `ServerCom` |

## Edges
`serverCreateCharacterResult ─▶ gworld (server FindObject) ─▶ object:BackupReplicate ─▶ [client builds char locally] ─▶ tplayercache (getByRelation resolves) ─▶ getAllPlayers ─▶ AddCharacter ─▶ uiintro ROW`

## OPEN
The wire ENVELOPE that carries `BackupReplicate(true)` to the client's native receiver = `gworld.md §5`
(the E3 blocker). Host has server-side registration + serialization; only the envelope is missing.
