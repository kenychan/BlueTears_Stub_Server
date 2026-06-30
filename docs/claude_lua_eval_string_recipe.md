# Claude — Run an arbitrary Lua string inside the client's Lua VM (native recipe)

Bounded RE lookup. All addresses are in the client image (NClient_unpacked.exe,
goEngine; base 0x00400000, image as analyzed). The engine ships its OWN Lua VM
(goEngine fork of Lua 5.1 — same stack model, custom function numbers), so these
are engine functions, NOT stock lua5.1.dll exports.

Source decompiles:
- `claude_lua_load_primitives.{c,md}`
- `claude_lua_load_followup.{c}` + `claude_lua_load_followup_asm.md`
- `claude_lua_state_global.{c}` + `claude_lua_state_global_asm.md`
- `claude_lua_load_core.c` + `claude_lua_load_core_asm.md`

------------------------------------------------------------------------
## 1. Global lua_State*  =  0x01825da8

`FUN_00bc11a0` is the Lua-subsystem init. At 0x00bc11f8 it calls
`FUN_00c7ff50` (lua_open / newstate; allocs 0x1b0 bytes, returns L in EAX) and
stores the result:

    DAT_01825da8 = FUN_00c7ff50();      // 0x00bc120d: MOV [0x01825da8], EAX

Every subsequent registration reloads it: `MOV ESI,[0x01825da8]` then pushes a
name and a CFunction. So **`L = *(lua_State**)0x01825da8`** (a single deref;
the slot lives in writable .data even though Ghidra flags it "read-only").

NOTE: this is NOT 0x01825dc4. 0x01825dc4 is a lazily-installed callback hook
(GC/stack-grow), zero at rest, only CALLed if non-zero at the top of c7d120/cbda80.

Lua "registers"/stack are reached off L:  L+0x8 = stack top pointer,
L+0x10 = global-state substruct (global table hash at +0x0, sizemask at +0x8,
GC limits at +0x40/+0x44). You normally don't touch these directly — use the
engine's push/call helpers below with L as the first arg.

------------------------------------------------------------------------
## 2. The load primitives (compile source -> Lua function on stack)

### lua_load core: FUN_00c825b0  (wrapper FUN_00c7d990 = clean cdecl entry)
`FUN_00c6b780` (the goLua_loadfile CFunction impl) compiles a chunk via:

    iVar11 = FUN_00c7d990(L, FUN_00c6b750 /*reader*/, reader_ctx, chunkname);
    // returns 0 on success; the compiled function is now on top of L's stack
    if (iVar11 == 0 && FUN_00cbda80(L, 0, 0) /*pcall*/ ) { ... }   // run it

- `FUN_00c7d990(L, reader_fn, reader_data, chunkname)`  __cdecl, 4 args.
  Thin shim: builds a ZIO and calls `FUN_00c825b0` (the real lua_load:
  protected-parse via FUN_00c82270 with handler 0xc82370, then FUN_00c86be0).
  Returns 0 = OK. This is the **lua_load(L, reader, data, chunkname)** analog.
- There is NO obvious luaL_loadbuffer/luaL_loadstring C export reached directly;
  the engine loads everything through this reader-callback path (lua_load), OR
  through the Lua-level `loadstring` global (see §3, simplest).

`chunkname`: if NULL the shim substitutes `0x015596ec`. Pass your own "=inject".

### Loading a STRING via the reader path
You'd need a reader callback that yields your buffer once then NULL — doable but
fiddly. The far simpler route is §3 (use the engine's own string-eval helper).

------------------------------------------------------------------------
## 3. SIMPLEST: the engine's own "eval a string" helpers (USE THESE)

The binary already contains two functions that compile+run a Lua expression
string by going through the Lua global `loadstring`:

- `FUN_00be82a0`  and  `FUN_00ccb850`  (identical logic; ccb850 takes L in *ECX).

Their body (the reusable recipe), with L = *0x01825da8:

    FUN_00c7d380(L, -0x2712 /*LUA_GLOBALSINDEX, =0xffffd8ee*/, "loadstring");
        // push _G["loadstring"]  (the Lua base-lib CFunction) onto the stack
    FUN_00c7d1f0(L /*ESI*/, "return %s", your_cstr);
        // push a formatted Lua string  -> "return <your_cstr>"
    FUN_00cbda80(L, 1, 1);   // pcall loadstring("return <expr>")  -> compiled fn
    FUN_00cbda80(L, 0, 0|1); // pcall that compiled fn

So loadstring's C impl is the Lua base-library `loadstring` reached as a global;
it is bound by the base-lib open (NOT by the goLua_ registrar c6a720). The
string 0x01556b6c "loadstring" is referenced statically ONLY by these two
helper callers and a base-lib name->func table slot at 0x014cec60 — i.e. the
engine never re-registers it; it's a stock base-lib function in _G.

### Helper primitive signatures (cdecl unless noted)
- `FUN_00c7d380(L, index, const char* name)` — push table[name] by C-string
  (index 0xffffd8ee = the globals pseudo-index). Internally: push global table
  (FUN_00c7c870) + luaV_gettable (FUN_00c848a0) over the interned name (c843e0).
- `FUN_00c7d1f0(L=ECX/ESI, const char* fmt, ...)` — push a Lua string built by
  the engine's lua_pushvfstring core `FUN_00c83cf0` (note: ECX=L, EDX=fmt — it is
  __fastcall-ish; in be82a0 it is called with ESI already = L). Only `%s/%d/%c/
  %f/%%` style conversions (goEngine pushfstring), enough for "return %s".
- `FUN_00cbda80(L, nargs, nresults)` = **lua_pcall** (the documented anchor).
  Returns 0/false on success (it returns `result & 0xffffff00`, low byte = status
  byte; non-zero low byte = error). On error it can pop up a "lua error" MsgBox
  if the dev flag DAT_01859b3c/38 path is enabled.

------------------------------------------------------------------------
## 4. Minimal native recipe (what NpSlayer should call)

Given a C string `src` ("print('hi from native')"):

    lua_State* L = *(lua_State**)0x01825da8;       // global L (single deref)

    // push _G.loadstring, then "return <src>" formatted, then pcall twice:
    ((void(__cdecl*)(void*,int,const char*))0x00c7d380)(L, 0xffffd8ee, "loadstring");
    // c7d1f0 expects L in ECX (fastcall-ish); call with a tiny thunk or set ECX:
    //   ECX=L, EDX="return %s", push src as the vararg, then CALL 0x00c7d1f0
    ((char(__cdecl*)(void*,int,int))0x00cbda80)(L, 1, 1);  // loadstring("return src")
    ((char(__cdecl*)(void*,int,int))0x00cbda80)(L, 0, 0);  // run it

CAVEAT — "return %s": this compiles `return <src>` (an EXPRESSION). To run a
STATEMENT BLOCK (e.g. multiple statements), either:
  (a) format without "return " — call FUN_00c7d1f0(L, "%s", src) instead so the
      chunk is `<src>` verbatim (loadstring compiles a full chunk, not just an
      expr); the "return %s" wrapper is only the engine's convenience for exprs.
  (b) or use the lua_load path §2 with your own reader.
Recommend (a): push "loadstring" global, push src verbatim via FUN_00c7d1f0(L,
"%s",src) [or pushstring], pcall(1,1) to get the chunk, pcall(0, n) to run it.

------------------------------------------------------------------------
## 5. Calling-convention notes (verified from asm)

- `FUN_00c7d120(nameptr, namelen)` and `FUN_00c7d230(L, cfunc, nupvals)` — at
  the call sites L is loaded into ESI/EAX first (`MOV ESI,[0x01825da8]`) then
  args PUSHed; c7d230 is cdecl with L as 1st pushed arg (see 0x00bc1403:
  PUSH 0; PUSH cfunc; PUSH L; CALL c7d230).
- `FUN_00cbda80(L, nargs, nresults)` — cdecl, L is 1st arg (`[ESP+0x24]`/param_1).
- `FUN_00c7d990(L, reader, data, chunkname)` — cdecl, 4 stack args.
- `FUN_00c825b0` / `FUN_00c7d1f0` use register passing (ESI=L, ECX/EDX) — call
  via the cdecl wrappers (c7d990 / via be82a0's pattern) rather than directly,
  or set ECX/ESI=L in a thunk.

------------------------------------------------------------------------
## 6. Could NOT determine statically
- The exact `loadstring` base-lib CFunction ADDRESS (it's installed at runtime
  into _G by the base-lib opener; statically only its name string + a name->func
  table slot at 0x014cec60 are visible, and the slot is 0 in the static image).
  Not needed for the recipe — §3/§4 reach it by name through _G, which is how the
  engine itself does it.
- Whether the dev "lua error MsgBox" flag is on in the live client (it gates a
  blocking MessageBoxA on pcall error). If injecting, prefer checking pcall's
  return status yourself and not relying on visible errors.
- FUN_00c7d1f0's full format-spec set (only confirmed it routes to the
  pushvfstring core 0x00c83cf0 which dispatches on %-codes incl. %s/%d/%f/%c).
