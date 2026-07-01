"""M-LUA — the shipped-goEngine-Lua runtime, run server-side (IsServer()=true).

See runtime.py. De-risk verdict (2026-06-28): the shipped OO core + class/instance/
field/method machinery runs unmodified in a standalone Lua 5.1 VM over plain-table
objects; the host owes only a bounded native shim. This is the keystone of the HEAVY host.
"""
from .runtime import LuaHost, PLUMBING_NATIVES

__all__ = ["LuaHost", "PLUMBING_NATIVES"]
