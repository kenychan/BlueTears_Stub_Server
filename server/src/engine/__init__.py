"""server.src.engine — the emulator ENGINE SUBSTRATE (not per-Lua-object).

The VM (lua), the goEngine native surface (api), the wire codec (codec), replication
(repl), the object store (obj), and persistence (db). The per-Lua-object HOST OPERATIONS
live in server/src/<object>/ and compose these. Transport is server/src/net/.
"""
