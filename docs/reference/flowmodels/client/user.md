# User (client) — controlled-player identity Com (DROPPED for the ROW)

Engine object: **User** (`Class/Main/User.lua`; a Com of `TPlayer`). Server counterpart:
`server/src/char/`.

## Status: DROP for char-SELECT (LATE-134)
`User` = the CONTROLLED-player identity. Including it on a char-select char drives the client into the
in-game world boot → `World_Disconnected` (the LATE-134 disconnect). **The ROW does NOT need User** —
row comps = `Player` + `CStatusPlayer` (+ `CBody`/`Physics` for the preview). `User` is only needed
AFTER "Enter Game" (`User:Start`), i.e. the world tier (`clientworld`/`logInServerResult` path).

## Edges
`user ─▶ (enter-game only)`; excluded from the char-SELECT roster path.
