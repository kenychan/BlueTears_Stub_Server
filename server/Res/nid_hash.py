#!/usr/bin/env python3
"""Native Blue Tears NID hash.

Recovered statically from NClient_unpacked.exe:
- table initializer: 0x00ccfc90
- string hash:       0x00ccfdb0

This matches the client's native GetNID backing function used by BaseLib.lua.
"""

from __future__ import annotations

import argparse


def _build_crypt_rows() -> list[list[int]]:
    seed = 0x100001
    rows = [[0] * 256 for _ in range(5)]
    for i in range(256):
        for row in range(5):
            seed = (seed * 0x7D + 3) % 0x2AAAAB
            hi = seed
            seed = (seed * 0x7D + 3) % 0x2AAAAB
            lo = seed
            rows[row][i] = ((hi << 16) | (lo & 0xFFFF)) & 0xFFFFFFFF
    return rows


_CRYPT_ROWS = _build_crypt_rows()


def native_nid(name: str) -> int:
    data = name.encode("ascii")
    h = 0x7FED7FED
    h2 = 0xEEEFEEEF
    table = _CRYPT_ROWS[2]
    for b in data:
        h = ((h + h2) ^ table[b]) & 0xFFFFFFFF
        h2 = (h + 3 + ((h2 * 0x21) & 0xFFFFFFFF) + b) & 0xFFFFFFFF
    return h


def main() -> int:
    ap = argparse.ArgumentParser(description="compute native GetNID(name)")
    ap.add_argument("name", nargs="+")
    args = ap.parse_args()
    for name in args.name:
        value = native_nid(name)
        print(f"{name}\t0x{value:08x}\t{value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
