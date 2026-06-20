# Project Rules

1. This repository is the clean milestone mirror, not the scratch RE lab.
2. Do not copy original game client files or full game assets into this repo.
3. Do not copy temporary dumps, cache folders, failed-attempt DLLs, stale logs, or exploratory traces.
4. Only commit artifacts needed to reproduce major wall-moving breakthroughs from a plain original client.
5. A real breakthrough means the game wall visibly moves: examples are reaching character creation UI, successful character creation/list refresh, and entering the game world.
6. Every milestone must include a concise recipe, exact files used, hashes for patched/hook DLLs, and curated evidence logs.
7. Use the original lab `C:\Users\keny-\Downloads\qqxj` for messy reverse engineering. Mirror only clean, proven results here.
8. If a future milestone needs a client-side hook or patched hook DLL, document the target, purpose, rollback file, and why it is safe enough to use.
9. NpSlayer UI logging should be improved and kept on for char-select-and-beyond runs: log visible UI lifecycle, text labels, identifiers, and useful coordinates so progress can be understood from logs.
10. Start history at the 2026-06-20 character-creation-UI milestone. Older history can be referenced, but do not backfill noisy scratch artifacts unless they become necessary for reproducing a milestone.

Preservation context: this is a local/offline recovery project for the long-discontinued online game Punch Monster / Blue Tears / QQXJ. The goal is personal childhood nostalgia and preservation through a local private server or faithful offline emulation.

