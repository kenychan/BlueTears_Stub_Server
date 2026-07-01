# Decision records (ADRs)

Short, dated, append-only. Newest first.

## 2026-06-28 — D-docs: architecture = quickref + detail, kept in sync
The host plan is two living tiers: `ARCHITECTURE_QUICKREF.md` (one-screen pointer/index) +
`architecture.md` (authoritative detail). The brainstorm spec stays the frozen rationale/prior-art;
live verdict/blocker stays in `CODEX_LATEST_STATUS.md`. Update the quickref + detail together on any
module/keystone/blocker change. Folded the D1 finding into both: M-REPL's real prerequisite is the
**flag1 ServerComputer attach (`c33210`)**, since `Connect`/`bfe230` (flag0) never sets up the tag-1
`c35140` lane.

## 2026-06-28 — D0: M0 verdict = HEAVY (Option-2 host)
The scripted ClientWorld stub cannot write the char's `obj+0x18` (reaches tag-2 `bff990`
needing an `owner` the char lacks; never the tag-1 `c35140` master lane — proven on primary
log this run, LATE-29). Template realization is necessary but moot without a lane reaching
`CreateDeferedTemplate`. ⇒ build the host that drives master/tag-1 replication.
**Supersedes** the LATE-28 "LIGHT via FUser+0x84 sequencing" framing (that was downstream of
the unexamined construction wall).

## 2026-06-28 — D-seq: de-risk before full build
Per user: run BOTH de-risks (D1 keystone replication, D2 constructed-char-row); commit to the
full host build only if neither shortcuts the work. Keeps the big commitment gated on evidence.

## 2026-06-28 — D-layout: dedicated `server/` folder
Host work lives in `server/` inside the main repo (version-controlled there). `qqxj_server`
stays the milestone mirror (PROJECT_RULES §19), not a dev checkout. `src/` modules scaffolded
per-module when the build starts (YAGNI until D1/D2 resolve).

## OPEN — D1-runtime: Python vs LuaJIT vs native (re-confirm after de-risks)
Heavy host is stateful. Candidate split: Python (transport/orchestration, reuse stub) +
embedded Lua (LuaJIT) for shipped server Lua + native/C hot path for codec/GWorld if perf
demands. Deferred until D1/D2 narrow the required surface.
