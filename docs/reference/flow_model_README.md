# flow_model.json — the lean RE knowledge graph

## Why this exists
Ghidra gives the static call graph + decompiles. It does NOT give a curated
*protocol-flow* model ("function X reads A/B/C, requires gate G, emits packet Z,
and here is what we still don't know"). This file is that overlay, in a form that
composes and queries — so we stop re-walking functions we already understand and
we always know the live frontier.

It is also the **server spec in progress**: when every stage is SOLVED and
`unknowns` is empty, this file is enough to build the server.

## Shape
- `stages[]` — ordered login->ingame pipeline, each with a `status`
  (SOLVED / SOLVED-MANUAL / CURRENT-WALL / UNVALIDATED / STATIC-RECIPE-ONLY).
- `packets{}` / `lower_head` — wire formats + builder names + proof level.
- `functions{}` — recovered functions: role, field reads, **gates** (the
  conditions that accept/reject/branch), calls, emits, and `evidence`
  (static vs runtime).
- `objects{}` — NESL objects and their components.
- `unknowns[]` — the frontier. Each has `kind`:
  `static-recoverable` (just decompile deeper), `static-from-lua`,
  `static-or-runtime`, or `runtime-only` (genuinely needs a live observation,
  e.g. protector relocation).

## How to use it
- Before decompiling something, check `functions{}` — we may already have it.
- Before a live test, check `unknowns[]` — only `runtime-only` items truly need
  a run; the rest should be closed statically first (cheaper, no clicking).
- When you recover something, add/extend the node and move the matching
  `unknowns[]` entry out. Keep `stages[].status` honest.

## Method (the point of the strategic pivot)
Static-first: recover the complete requirement set per stage, then do ONE
confirmation run. Runtime is for *confirming a complete hypothesis*, not for
*discovering requirements* one round-trip at a time. The only inherently
runtime-only facts here are server-assigned state and protector behavior.

## Not built (deliberately deferred)
The maximal "executable harness that calls recovered functions in isolation +
side-effect logging" is over-engineering on a protected binary — it fights the
anti-cheat for less payoff than finishing the protocol. Revisit only if we
stall on a `runtime-only` unknown that static cannot close.
