# CStatusPlayer (client) — char status Com (the ROW columns + appearance)

Engine object: **CStatusPlayer** (`Class/Main/CStatusPlayer.lua`; a Com of `TPlayer`). Server
counterpart: `server/src/char/`.

## Client reads (ROW paint + render) — `UIIntro.lua:56-72`
| field / method | used for |
|---|---|
| `GetCom(CStatus):GetAttributeValue("Level")` | Level column |
| `status.ClassLevel` | class-level column |
| `status:GetJobClass()` | job column + appearance |
| `status.m_nGender` (`m_nGender`) | gender → appearance (`CEquipment:GetCharacterDescription`) |

## ROW / render minimum (LATE-141)
**ROW = Player (name/m_AccountId) + CStatusPlayer (Level/ClassLevel/JobClass).** For the SELECT
portrait render also need valid `m_nGender`+JobClass (feeds `CEquipment:GetBaseDescription`;
`LUA_QUICKREF.md §2`). The DB-mage record supplies gender/job/appearance.

## Edges
`cstatusplayer ─▶ uiintro (row columns)` · `─▶ cbody/cequipment (appearance)`.
