"""Regenerate the API flow model from the decompiled client Lua.

Self-contained: walks Res/tw_lua_decompiled for every `SetLuaNetFunc("name", RPCType.*)`
and writes:
  ghidraRE/output/flowmodels/api/rpc_surface.md   (the wire RPC surface, grouped by class)
  ghidraRE/output/api_surface_index.json          (machine-readable class -> rpcs)

native_api.md and _MASTER.md are hand-curated (they carry pinned VAs); this only rebuilds the
grep-derived RPC surface. Run:  py ghidraRE/output/gen_api_surface.py
"""
import os, re, json, collections

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LUA = os.path.join(REPO, "Res", "tw_lua_decompiled")
API_DIR = os.path.join(REPO, "ghidraRE", "output", "flowmodels", "api")
JSON_OUT = os.path.join(REPO, "ghidraRE", "output", "api_surface_index.json")

rx = re.compile(r'SetLuaNetFunc\(\s*"(?P<name>[^"]+)"\s*,?\s*(?P<types>[^)]*)\)')


def walk():
    per_file = collections.OrderedDict()
    for root, _dirs, files in os.walk(LUA):
        for fn in files:
            if not fn.endswith(".lua"):
                continue
            p = os.path.join(root, fn)
            rel = os.path.relpath(p, LUA).replace("\\", "/")
            try:
                text = open(p, encoding="utf-8", errors="replace").read()
            except OSError:
                continue
            for i, line in enumerate(text.splitlines(), 1):
                m = rx.search(line)
                if not m:
                    continue
                types = [t[0].upper() + t[1:].lower() for t in re.findall(r'RPCType\.(\w+)', m.group("types"))]
                per_file.setdefault(rel, []).append((m.group("name"), "+".join(types) or "?", i))
    return per_file


def cls(f):
    return os.path.splitext(os.path.basename(f))[0]


def main():
    os.makedirs(API_DIR, exist_ok=True)
    per_file = walk()
    files_sorted = sorted(per_file.items(), key=lambda kv: -len(kv[1]))
    total = sum(len(v) for v in per_file.values())
    cnt = collections.Counter(t for v in per_file.values() for (_n, ts, _l) in v for t in ts.split("+") if t != "?")
    srvmap = {"CStatusPlayer": "server/src/char/", "Player": "server/src/char/", "CStatus": "server/src/char/",
              "Session": "server/src/net/ (login)", "User": "server/src/char/",
              "CharacterManager": "server/src/charactermanager/", "TPlayerCache": "server/src/charactermanager/"}

    md = ["# API flow model — the wire RPC surface (`SetLuaNetFunc`)\n",
          "Auto-generated from `Res/tw_lua_decompiled` (regenerate: `ghidraRE/output/gen_api_surface.py`). "
          "Every replicated method a Lua object exposes on the wire is registered via "
          "`self:SetLuaNetFunc(name, RPCType.*)`. **Client** = client-invokable (server->client downcall), "
          "**Server** = server-invokable (client->server upcall), **Master** = master-channel. Native surface "
          "the Lua rides on = `native_api.md`; the login->roster->char call graph = `../client/_MASTER.md`.\n",
          f"- **{total}** registrations across **{len(per_file)}** owning classes\n",
          "- RPCType split: " + ", ".join(f"{k}={v}" for k, v in cnt.most_common()) + "\n",
          "\n## Owning classes (by RPC count)\n",
          "| class | file | RPCs | server counterpart |\n|---|---|---:|---|\n"]
    for f, methods in files_sorted:
        md.append(f"| `{cls(f)}` | `{f}` | {len(methods)} | {srvmap.get(cls(f), '—')} |\n")
    md.append("\n## Per-class RPC listing\n")
    for f, methods in files_sorted:
        md.append(f"\n### `{cls(f)}`  ({len(methods)} RPCs) — `{f}`\n")
        by_t = collections.defaultdict(list)
        for name, tset, _line in sorted(methods):
            by_t[tset].append(name)
        for tset in sorted(by_t):
            md.append(f"- **{tset}** ({len(by_t[tset])}): " + ", ".join(f"`{n}`" for n in by_t[tset]) + "\n")
    open(os.path.join(API_DIR, "rpc_surface.md"), "w", encoding="utf-8").write("".join(md))

    idx = {"generated_from": "Res/tw_lua_decompiled", "total": total,
           "classes": {cls(f): {"file": f, "rpcs": [{"method": n, "types": t.split("+"), "line": l}
                                                     for (n, t, l) in sorted(v)]}
                       for f, v in per_file.items()}}
    json.dump(idx, open(JSON_OUT, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    print(f"wrote rpc_surface.md rows={total} classes={len(per_file)}")
    print("wrote api_surface_index.json")


if __name__ == "__main__":
    main()
