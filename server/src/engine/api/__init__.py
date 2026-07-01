"""M-API — the bounded goEngine native API, implemented host-side over the live Lua objects.

The shipped shared Lua (run by M-LUA) calls a small, enumerable set of native globals/methods
the C++ engine used to provide: GetEnvironment/GetNObject (named singletons), getByRelationImpl
(the DB relation query behind ObjectCache:getByRelation), GWorld:FindObject/CreateObject,
BackupReplicate, etc. This module supplies host implementations of that surface over a registry
of live Lua-table objects (the Lua table IS the object; see runtime.py).
"""
from .host_context import HostContext

__all__ = ["HostContext"]
