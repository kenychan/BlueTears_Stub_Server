# UIIntro (client) — the char-select / job-class UI (CLIENT-ONLY)

Engine object: **UIIntro** (`UI/UIIntro.lua`). Client-only (UI); no server counterpart.

## Client flow
| Lua | file:line | does |
|---|---|---|
| `ShowJobClassSelection` | — | the CREATION screen (count==0; E1 end, reached ✅) |
| `AddCharacter(event)` | `UIIntro.lua:56` | paints a roster ROW — ONLY if `CharacterSelectionDialog` is loaded (`:94`) |
| row reads | `:56-72` | `event.m_param3`=char → `CStatus` Level/ClassLevel/JobClass; `character.CBody`+`Physics:SetPhysics`+`body:SetScale` (3D preview); name = `event.m_param1` |
| 3D preview | `:220-253` | `CreateTUIControlObject → ClientWorld:CreateLocalObject('TPlayerDummy')` |

## Edges
`session:Client_FinishLoadingCharacter(count) ─▶ uiintro (ShowJobClassSelection | roster)` ·
`charactermanager:AddCharacter ─▶ uiintro row` · `uiintro preview ─▶ clientworld:CreateLocalObject`.

## Note
The 3D preview (`TPlayerDummy` via `CreateLocalObject`) is a DIFFERENT render path from the SELECT
portrait (which needs char residency + CBody body-resource; the `+0x198` orphan wall, `LUA_QUICKREF.md §2`).
