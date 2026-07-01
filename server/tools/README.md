# tools/ — de-risk harnesses & probe configs

Test scaffolding for the de-risks (NpSlayer probe configs, stub emitter snippets, log
parsers). The live RE/test stub itself stays at `Res/gss_stub_server_v4.py` and NpSlayer at
`Res/stubs/NpSlayer_exp.c` (logging/test only) — reference them; don't fork them here until
the host build replaces them.

RUN-OP reminder: kill lingering stub (port 5004) before each launch; stall-kill only
NClient/PunchMonster, keep the worker; use `py`.
