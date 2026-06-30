#!/usr/bin/env python3
"""Resolve NpSlayer STACKCHAIN VAs into a named call tree (innermost first).

NpSlayer's exp_stack_scan emits lines like:
    <TAG> STACKCHAIN esp=0x001aece0: bec99f a7ff9b a80123 ...
(continuation lines: <TAG> STACKCHAIN+: ...)

This maps each VA to the nearest Ghidra function start and, for E8-rel32 call
sites, VALIDATES that the call target is itself a known function (kills false
positives). Resolved frames are the live call tree at the crash/teardown -
the FPO-robust replacement for the 2-deep ebp_chain.

Usage:
    py Res/stubs/resolve_stackscan.py [logfile] [--tag TRANSITION_EXCEPTION] [--last]
    (defaults: logfile=TW/Bin/NpSlayer_exp.log, all tags, every chain)
"""
import sys, csv, re, bisect

BIN = 'TW/Bin/NClient_standalone.exe'
FUNCS = 'ghidraRE/output/functions_ghidra.csv'

def load_funcs():
    addrs, names = [], {}
    with open(FUNCS) as f:
        for r in csv.DictReader(f):
            try:
                a = int(r['address'], 16)
            except Exception:
                continue
            addrs.append(a); names[a] = r['name']
    addrs.sort()
    return addrs, names, set(addrs)

def nearest(addrs, names, va):
    i = bisect.bisect_right(addrs, va) - 1
    if i < 0:
        return None, 0
    fa = addrs[i]
    return names[fa], va - fa

def load_image():
    try:
        import pefile
        pe = pefile.PE(BIN)
        return pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image()
    except Exception:
        return None, None

def validate_e8(img, ib, fstarts, v):
    """If v is preceded by E8 rel32, return the resolved call target if it is a
    known function start; else None. Non-E8 (FF-form) returns 'FF' (accepted)."""
    if img is None:
        return '?'
    off = v - ib
    if off < 5 or off >= len(img):
        return '?'
    if img[off-5] == 0xE8:
        rel = int.from_bytes(img[off-4:off], 'little', signed=True)
        tgt = (v + rel) & 0xffffffff
        return tgt if tgt in fstarts else None
    return 'FF'

def main():
    pos = [a for a in sys.argv[1:] if not a.startswith('--')]
    log = pos[0] if pos else 'TW/Bin/NpSlayer_exp.log'
    tag = None
    last = False
    for a in sys.argv[1:]:
        if a.startswith('--tag='): tag = a.split('=', 1)[1]
        elif a == '--tag' : pass
        elif a == '--last': last = True

    addrs, names, fstarts = load_funcs()
    ib, img = load_image()

    chains = []  # list of (tag, esp, [vas])
    with open(log, errors='replace') as f:
        for line in f:
            m = re.search(r'(\S+) STACKCHAIN(\+)?:?\s*(.*)', line)
            if not m:
                continue
            t, cont, rest = m.group(1), m.group(2), m.group(3)
            if tag and t != tag and not cont:
                continue
            esp_m = re.search(r'esp=0x([0-9a-f]+)', rest)
            esp = esp_m.group(1) if esp_m else '?'
            vas = [int(x, 16) for x in re.findall(r'\b([0-9a-f]{8})\b', rest) if x != esp]
            if cont and chains:
                chains[-1][2].extend(vas)
            else:
                chains.append([t, esp, vas])

    if not chains:
        print("no STACKCHAIN lines found in", log); return
    if last:
        chains = chains[-1:]

    for t, esp, vas in chains:
        print("=== %s call tree (innermost first) esp=0x%s, %d candidate frames ===" % (t, esp, len(vas)))
        shown = 0
        for v in vas:
            val = validate_e8(img, ib, fstarts, v)
            if val is None:
                continue  # E8 site whose target isn't a real function -> false positive
            nm, off = nearest(addrs, names, v)
            kind = '' if val in ('FF', '?') else ('  -> %s' % names.get(val, '0x%08x' % val))
            print("   0x%08x  %s+0x%x%s" % (v, nm, off, kind))
            shown += 1
        if shown == 0:
            print("   (no validated frames)")
        print()

if __name__ == '__main__':
    main()
