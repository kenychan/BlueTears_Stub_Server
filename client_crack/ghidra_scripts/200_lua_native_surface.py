# -*- coding: utf-8 -*-
# 200_lua_native_surface.py — recover the native Lua API surface WITHOUT needing
# to identify luaL_register: scan initialized data blocks for luaL_Reg[] arrays,
# i.e. runs of {name_ptr -> C-identifier string, fn_ptr -> .text} 8-byte pairs
# (Lua 5.1 `luaL_Reg { const char*; lua_CFunction; }`), terminated by {NULL,*}.
# Each array = one luaL_register library table => name -> function VA to seat.
#
# Output: ghidraRE/output/lua_native_surface.{json,md}
#
# Run (project already analysed):
#   analyzeHeadless <proj_dir> qqxj_tw -process NClient_unpacked.exe -noanalysis \
#     -scriptPath Res\ghidra_scripts -postScript 200_lua_native_surface.py -okToDelete

import os, jarray

try:
    import java.lang as java_lang
    JT = java_lang.Throwable
except Exception:
    JT = Exception

OUT_DIR = r"C:\Users\keny-\Downloads\qqxj\ghidraRE\output"

cp = currentProgram
af = cp.getAddressFactory()
fm = cp.getFunctionManager()
mem = cp.getMemory()

IMG_LO, IMG_HI = 0x00400000, 0x01900000     # any in-image pointer
CODE_LO, CODE_HI = 0x00401000, 0x01400000   # .text (fn_ptr must land here)
MIN_RUN = 3                                  # >=3 consecutive entries = a real table


def addr(va):
    return af.getAddress(hex(va & 0xffffffff).rstrip("L"))


def read_ident(va, maxlen=64):
    """C identifier at va via random access (infrequent: only for candidates)."""
    if va is None or not (IMG_LO <= va < IMG_HI):
        return None
    try:
        out = []
        for i in range(maxlen):
            b = mem.getByte(addr(va + i)) & 0xFF
            if b == 0:
                break
            c = chr(b)
            if i == 0 and not (c.isalpha() or c == '_'):
                return None
            if not (c.isalnum() or c == '_'):
                return None
            out.append(c)
        return "".join(out) if out else None
    except JT:
        return None
    except Exception:
        return None


def is_code(va):
    if va is None or not (CODE_LO <= va < CODE_HI):
        return False
    try:
        blk = mem.getBlock(addr(va))
        return blk is not None and blk.isExecute()
    except JT:
        return False
    except Exception:
        return False


def func_name(va):
    f = fm.getFunctionAt(addr(va))
    return f.getName() if f else None


arrays = []
for blk in mem.getBlocks():
    if not blk.isInitialized() or blk.isExecute():
        continue
    size = int(blk.getSize())
    if size <= 8 or size > 0x2000000:
        continue
    buf = jarray.zeros(size, 'b')
    try:
        mem.getBytes(blk.getStart(), buf)
    except JT:
        continue
    base = blk.getStart().getOffset()

    def u32(off):
        if off < 0 or off + 4 > size:
            return None
        return ((buf[off] & 0xFF) | ((buf[off + 1] & 0xFF) << 8) |
                ((buf[off + 2] & 0xFF) << 16) | ((buf[off + 3] & 0xFF) << 24))

    def try_entry(off):
        p1 = u32(off)
        p2 = u32(off + 4)
        if not p1 or p2 is None:
            return None
        if not (IMG_LO <= p1 < IMG_HI) or not (CODE_LO <= p2 < CODE_HI):
            return None
        nm = read_ident(p1)
        if not nm or not is_code(p2):
            return None
        return (nm, p2)

    i = 0
    lim = size - 8
    while i < lim:
        e = try_entry(i)
        if e:
            run = [e]
            j = i + 8
            while j < lim:
                e2 = try_entry(j)
                if e2:
                    run.append(e2)
                    j += 8
                else:
                    break
            if len(run) >= MIN_RUN:
                arrays.append((base + i, run))
                i = j + 8
            else:
                i += 4
        else:
            i += 4

total = sum(len(r) for _, r in arrays)

data = {"binary": "NClient_unpacked.exe", "arrays_count": len(arrays), "entries_count": total, "arrays": []}
for a_va, run in arrays:
    data["arrays"].append({
        "array_va": "0x%08x" % a_va,
        "count": len(run),
        "entries": [{"name": nm, "va": "0x%08x" % fv, "func": func_name(fv)} for (nm, fv) in run],
    })

with open(os.path.join(OUT_DIR, "lua_native_surface.json"), "w") as f:
    import json
    json.dump(data, f, indent=1)

lines = ["# Native Lua API surface -- luaL_Reg[] arrays (name -> VA)\n\n",
         "Auto-swept from NClient_unpacked.exe by `Res/ghidra_scripts/200_lua_native_surface.py`.\n",
         "Each array = one `luaL_register` library table. **%d entries across %d arrays.**\n" % (total, len(arrays)),
         "Sorted by array VA. Cross-reference the wire RPC surface in `flowmodels/api/rpc_surface.md`.\n"]
for a_va, run in sorted(arrays):
    lines.append("\n## array @ 0x%08x  (%d entries)\n" % (a_va, len(run)))
    for nm, fv in run:
        fn = func_name(fv)
        lines.append("- `%s` = `0x%08x`%s\n" % (nm, fv, ("  (" + fn + ")" if fn else "")))
with open(os.path.join(OUT_DIR, "lua_native_surface.md"), "w") as f:
    f.write("".join(lines))

print("LUANATIVE_DONE arrays=%d entries=%d" % (len(arrays), total))
