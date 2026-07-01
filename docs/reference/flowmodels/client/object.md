# Object methods — component + replication primitives (any engine object)

The native methods every engine object exposes. Server counterparts: `server/src/obj/` (object model) +
`server/src/repl/` (replication bytes).

## Component methods
| method | native | does |
|---|---|---|
| `obj:AddComponent(comp)` | handler `bcf510` → `FUN_00bd2cd0` | insert component into `obj+0x18` keyed by descriptor ordinal (Player=1/CStatusPlayer=2/User=4) |
| `obj:RemoveComponent` / `GetBaseComponent` | native | detach / fetch a base component |
| `obj:GetCom(name)` | native | read a component off the object (used by UIIntro/CBody) |

**`obj+0x18`** = the self/RPC-routing map (this-object's RPC-routable components), written ONLY by full
construction / `bd2cd0` (`gworld.md §2.4`, `obj18-session-is-template-component`). **`obj+0x14`** = the
component container (template-clone + wire tail). The key03 NewObject tail does NOT fill `obj+0x18` —
`AddComponent` does (`claude_loginserverresult_wire_spec.md:52`).

## Replication methods (server→client)
| method | where | does |
|---|---|---|
| `obj:BackupReplicate(bReliable)` | `CharacterManager.lua:89` | full-state server→client push — the char-residency trigger |
| `obj:ForceReplicate(...)` | `Player.lua:457` | force a NetVar replication |

Host side = M-REPL (`server/src/repl/replicate.py`): serializes an object to the tag-05 array of tag-07
BackupReplicate bytes. The wire ENVELOPE to deliver them is the open unknown (`gworld.md §5`).

## Edges
`object:AddComponent ─▶ bd2cd0 ─▶ obj+0x18` · `object:BackupReplicate ─▶ [wire] ─▶ client bff990 build` ·
`gworld` registers the built object in the OID table.
