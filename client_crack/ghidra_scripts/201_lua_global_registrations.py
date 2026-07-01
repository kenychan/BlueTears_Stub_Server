# -*- coding: utf-8 -*-
# 201_lua_global_registrations.py — resolve name->VA for the goEngine globals that
# are registered INDIVIDUALLY (lua_pushcclosure + lua_setglobal), which the
# luaL_Reg[] array scan (200_...) does NOT catch. Name-driven + validated:
#   for each target name: find its C-string, take code xrefs to it, and within a
#   tight instruction window find the function-entry pointer pushed alongside it
#   (the lua_CFunction). Emit only when the window yields a single function entry
#   (unambiguous); otherwise report the candidates so nothing is silently wrong.
#
# Output: ghidraRE/output/lua_global_registrations.{json,md}

import os, json

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
listing = cp.getListing()
refmgr = cp.getReferenceManager()

CODE_LO, CODE_HI = 0x00401000, 0x01400000
WINDOW = 8   # instructions each side of the string-xref to search for the fn ptr

# The goEngine globals worth seating: everything the live GAP recorder witnessed +
# the LUA-first construct/codec/registration natives. (Methods like World:CreateObject
# / SetLuaNetFunc live on metatables, not globals -- not resolvable this way.)
TARGETS = [
    "GetEnvironment", "GetNObject", "GetUIResourceManager", "GetUIControlManager",
    "PushUIEvent", "PopUIEvent", "GetLocale", "D3DCOLOR", "RunLogLn", "include",
    "Random", "IsClient",
    "goLua_CreateObject", "goLua_RegistObject", "goLua_Pack", "goLua_PackFixed",
    "goLua_UnpackFixed", "goLua_setmetatable", "goLua_loadfile", "goLua_AddObjectFunction",
    "reload_file", "CreateDeferedTemplate", "Template", "Class", "Com", "GetClass",
    "CreateObject", "FindObject", "SetLuaNetFunc", "BackupReplicate", "GetGWorld",
    "GetServerWorld", "GetClientWorld",
]


def addr(va):
    return af.getAddress(hex(va & 0xffffffff).rstrip("L"))


def find_string_addrs(name):
    """All addresses whose C-string == name exactly (NUL-terminated, non-ident before)."""
    pat = bytearray(name.encode("ascii")) + b"\x00"
    import jarray
    barr = jarray.array([ (x if x < 128 else x - 256) for x in pat ], 'b')
    hits = []
    start = cp.getMinAddress()
    while True:
        try:
            found = mem.findBytes(start, barr, None, True, monitor)
        except JT:
            break
        if found is None:
            break
        va = found.getOffset()
        # reject if preceding byte is an identifier char (substring match)
        try:
            pb = mem.getByte(addr(va - 1)) & 0xFF
            c = chr(pb)
            ok = not (c.isalnum() or c == '_')
        except Exception:
            ok = True
        if ok:
            hits.append(va)
        start = found.add(1)
        if len(hits) > 12:
            break
    return hits


def func_entry_refs_near(str_addr):
    """Function-entry VAs referenced within +-WINDOW instructions of any code xref to str_addr."""
    cands = set()
    for ref in refmgr.getReferencesTo(addr(str_addr)):
        frm = ref.getFromAddress()
        if frm is None or not (CODE_LO <= frm.getOffset() < CODE_HI):
            continue
        ins = listing.getInstructionAt(frm)
        if ins is None:
            ins = listing.getInstructionContaining(frm)
        if ins is None:
            continue
        seq = [ins]
        p = ins
        for _ in range(WINDOW):
            p = p.getNext()
            if p is None:
                break
            seq.append(p)
        p = ins
        for _ in range(WINDOW):
            p = p.getPrevious()
            if p is None:
                break
            seq.insert(0, p)
        for it in seq:
            for r2 in it.getReferencesFrom():
                # the registered lua_CFunction is PUSHed as a pointer (data ref);
                # the pushcclosure/setglobal helpers are CALLed -- skip those.
                try:
                    if r2.getReferenceType().isCall():
                        continue
                except Exception:
                    pass
                ta = r2.getToAddress()
                if ta is None:
                    continue
                tv = ta.getOffset()
                if CODE_LO <= tv < CODE_HI and fm.getFunctionAt(ta) is not None:
                    cands.add(tv)
    return sorted(cands)


results = []
for name in TARGETS:
    saddrs = find_string_addrs(name)
    if not saddrs:
        results.append({"name": name, "status": "no_string"})
        continue
    all_cands = set()
    for sa in saddrs:
        for fv in func_entry_refs_near(sa):
            all_cands.add(fv)
    all_cands = sorted(all_cands)
    entry = {"name": name,
             "string_addrs": ["0x%08x" % s for s in saddrs],
             "fn_candidates": ["0x%08x" % v for v in all_cands]}
    if len(all_cands) == 1:
        entry["status"] = "resolved"
        entry["va"] = "0x%08x" % all_cands[0]
    elif len(all_cands) == 0:
        entry["status"] = "no_fn_near"
    else:
        entry["status"] = "ambiguous"
    results.append(entry)

with open(os.path.join(OUT_DIR, "lua_global_registrations.json"), "w") as f:
    json.dump({"binary": "NClient_unpacked.exe", "targets": results}, f, indent=1)

lines = ["# goEngine global natives -- name -> VA (individual lua_setglobal registrations)\n\n",
         "Resolved by `Res/ghidra_scripts/201_lua_global_registrations.py` (string-xref + fn-ptr window).\n",
         "`resolved` = single unambiguous fn entry near the name's code xref; `ambiguous` = multiple\n",
         "candidates (pick by inspection); complements the exact array scan in `lua_native_surface.md`.\n\n"]
for r in results:
    if r["status"] == "resolved":
        lines.append("- `%s` = `%s`  **resolved**\n" % (r["name"], r["va"]))
    elif r["status"] == "ambiguous":
        lines.append("- `%s` = ambiguous: %s\n" % (r["name"], ", ".join(r["fn_candidates"])))
    elif r["status"] == "no_fn_near":
        lines.append("- `%s` = string @ %s but no fn entry near xref\n" % (r["name"], ", ".join(r["string_addrs"])))
    else:
        lines.append("- `%s` = (no string in binary)\n" % r["name"])
with open(os.path.join(OUT_DIR, "lua_global_registrations.md"), "w") as f:
    f.write("".join(lines))

nres = len([r for r in results if r["status"] == "resolved"])
print("LUAGLOBAL_DONE targets=%d resolved=%d" % (len(results), nres))
