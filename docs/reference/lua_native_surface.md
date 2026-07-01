# Native Lua API surface -- luaL_Reg[] arrays (name -> VA)

Auto-swept from NClient_unpacked.exe by `Res/ghidra_scripts/200_lua_native_surface.py`.
Each array = one `luaL_register` library table. **227 entries across 23 arrays.**
Sorted by array VA. Cross-reference the wire RPC surface in `flowmodels/api/rpc_surface.md`.

## array @ 0x014ce28c  (4 entries)
- `__gc` = `0x00ccf4b0`
- `srand` = `0x00ccf500`  (FUN_00ccf500)
- `rand` = `0x00ccf590`  (FUN_00ccf590)
- `rand53` = `0x00ccf730`  (FUN_00ccf730)

## array @ 0x014ce2b4  (4 entries)
- `__gc` = `0x00ccf7f0`
- `srand` = `0x00ccf840`  (FUN_00ccf840)
- `rand` = `0x00ccf8c0`  (FUN_00ccf8c0)
- `rand53` = `0x00ccfa10`  (FUN_00ccfa10)

## array @ 0x014ce2dc  (5 entries)
- `Random` = `0x00ccf190`  (FUN_00ccf190)
- `LCGSRand` = `0x00ccf220`  (FUN_00ccf220)
- `LCGRand` = `0x00ccf270`  (FUN_00ccf270)
- `LCG` = `0x00ccf4a0`  (FUN_00ccf4a0)
- `MersenneTwister` = `0x00ccf7e0`  (FUN_00ccf7e0)

## array @ 0x014ce310  (7 entries)
- `RunLog` = `0x00cc92f0`  (FUN_00cc92f0)
- `RunLogLn` = `0x00cc9390`  (FUN_00cc9390)
- `RunLog_Service` = `0x00cc92f0`  (FUN_00cc92f0)
- `RunLogLn_Service` = `0x00cc9390`  (FUN_00cc9390)
- `RunLogSetOutput` = `0x00cc9440`  (FUN_00cc9440)
- `RunLogSetOutputAll` = `0x00cc9510`
- `RunLogSetCategoryOption` = `0x00cc9550`  (FUN_00cc9550)

## array @ 0x014ce350  (7 entries)
- `RunLog` = `0x00cbddf0`
- `RunLogLn` = `0x00cbddf0`
- `RunLog_Service` = `0x00cbddf0`
- `RunLogLn_Service` = `0x00cbddf0`
- `RunLogSetOutput` = `0x00cbddf0`
- `RunLogSetOutputAll` = `0x00cc9510`
- `RunLogSetCategoryOption` = `0x00cbddf0`

## array @ 0x014ce390  (7 entries)
- `RunLog` = `0x00cbddf0`
- `RunLogLn` = `0x00cbddf0`
- `RunLog_Service` = `0x00cc92f0`  (FUN_00cc92f0)
- `RunLogLn_Service` = `0x00cc9390`  (FUN_00cc9390)
- `RunLogSetOutput` = `0x00cc9440`  (FUN_00cc9440)
- `RunLogSetOutputAll` = `0x00cc9510`
- `RunLogSetCategoryOption` = `0x00cc9550`  (FUN_00cc9550)

## array @ 0x014ce690  (15 entries)
- `byte` = `0x00c98590`  (FUN_00c98590)
- `char` = `0x00c98790`  (FUN_00c98790)
- `dump` = `0x00c989d0`  (FUN_00c989d0)
- `find` = `0x00c99870`  (FUN_00c99870)
- `format` = `0x00c9a530`  (FUN_00c9a530)
- `gfind` = `0x00c99b90`
- `gmatch` = `0x00c99a50`  (FUN_00c99a50)
- `gsub` = `0x00c99e20`  (FUN_00c99e20)
- `len` = `0x00c97e40`  (FUN_00c97e40)
- `lower` = `0x00c98140`  (FUN_00c98140)
- `match` = `0x00c99890`  (FUN_00c99890)
- `rep` = `0x00c98480`  (FUN_00c98480)
- `reverse` = `0x00c97fe0`  (FUN_00c97fe0)
- `sub` = `0x00c97eb0`  (FUN_00c97eb0)
- `upper` = `0x00c982e0`  (FUN_00c982e0)

## array @ 0x014ce710  (28 entries)
- `abs` = `0x00c972a0`  (FUN_00c972a0)
- `acos` = `0x00c974a0`  (FUN_00c974a0)
- `asin` = `0x00c97460`  (FUN_00c97460)
- `atan2` = `0x00c97520`  (FUN_00c97520)
- `atan` = `0x00c974e0`  (FUN_00c974e0)
- `ceil` = `0x00c97570`  (FUN_00c97570)
- `cosh` = `0x00c973a0`  (FUN_00c973a0)
- `cos` = `0x00c97360`  (FUN_00c97360)
- `deg` = `0x00c977f0`  (FUN_00c977f0)
- `exp` = `0x00c977b0`  (FUN_00c977b0)
- `floor` = `0x00c975b0`  (FUN_00c975b0)
- `fmod` = `0x00c975f0`  (FUN_00c975f0)
- `frexp` = `0x00c97870`  (FUN_00c97870)
- `ldexp` = `0x00c978d0`  (FUN_00c978d0)
- `log10` = `0x00c97770`  (FUN_00c97770)
- `log` = `0x00c97730`  (FUN_00c97730)
- `max` = `0x00c97aa0`  (FUN_00c97aa0)
- `min` = `0x00c97950`  (FUN_00c97950)
- `modf` = `0x00c97640`  (FUN_00c97640)
- `pow` = `0x00c976e0`  (FUN_00c976e0)
- `rad` = `0x00c97830`  (FUN_00c97830)
- `random` = `0x00c97bf0`  (FUN_00c97bf0)
- `randomseed` = `0x00c97d40`  (FUN_00c97d40)
- `sinh` = `0x00c97320`  (FUN_00c97320)
- `sin` = `0x00c972e0`  (FUN_00c972e0)
- `sqrt` = `0x00c976a0`  (FUN_00c976a0)
- `tanh` = `0x00c97420`  (FUN_00c97420)
- `tan` = `0x00c973e0`  (FUN_00c973e0)

## array @ 0x014ce7f8  (14 entries)
- `debug` = `0x00c96d00`  (FUN_00c96d00)
- `getfenv` = `0x00c95ec0`  (FUN_00c95ec0)
- `gethook` = `0x00c96ba0`  (FUN_00c96ba0)
- `getinfo` = `0x00c960a0`  (FUN_00c960a0)
- `getlocal` = `0x00c96420`  (FUN_00c96420)
- `getregistry` = `0x00c95db0`
- `getmetatable` = `0x00c95de0`  (FUN_00c95de0)
- `getupvalue` = `0x00c96730`  (FUN_00c96730)
- `setfenv` = `0x00c95ee0`  (FUN_00c95ee0)
- `sethook` = `0x00c969b0`  (FUN_00c969b0)
- `setlocal` = `0x00c96530`  (FUN_00c96530)
- `setmetatable` = `0x00c95e30`  (FUN_00c95e30)
- `setupvalue` = `0x00c96750`  (FUN_00c96750)
- `traceback` = `0x00c96f10`  (FUN_00c96f10)

## array @ 0x014ce898  (5 entries)
- `on` = `0x00c94530`
- `off` = `0x00c94550`
- `debug` = `0x00c94560`  (FUN_00c94560)
- `compile` = `0x00c945f0`  (FUN_00c945f0)
- `compilesub` = `0x00c94810`

## array @ 0x014ce8c8  (8 entries)
- `stats` = `0x00c95070`  (FUN_00c95070)
- `bytecode` = `0x00c95290`  (FUN_00c95290)
- `const` = `0x00c954e0`  (FUN_00c954e0)
- `upvalue` = `0x00c955b0`  (FUN_00c955b0)
- `closurenup` = `0x00c95680`  (FUN_00c95680)
- `mcode` = `0x00c95870`  (FUN_00c95870)
- `jsubmcode` = `0x00c959a0`  (FUN_00c959a0)
- `stackptr` = `0x00c95a60`  (FUN_00c95a60)

## array @ 0x014ce948  (13 entries)
- `attributes` = `0x00c940b0`
- `chdir` = `0x00c92d00`  (FUN_00c92d00)
- `currentdir` = `0x00c92dd0`
- `dir` = `0x00c93840`  (FUN_00c93840)
- `link` = `0x00c93520`
- `lock` = `0x00c93290`  (FUN_00c93290)
- `mkdir` = `0x00c93530`  (FUN_00c93530)
- `rmdir` = `0x00c93600`  (FUN_00c93600)
- `symlinkattributes` = `0x00c940d0`
- `setmode` = `0x00c93240`  (FUN_00c93240)
- `touch` = `0x00c93b00`  (FUN_00c93b00)
- `unlock` = `0x00c93410`
- `lock_dir` = `0x00c92f40`

## array @ 0x014ce9b8  (9 entries)
- `concat` = `0x00c92430`  (FUN_00c92430)
- `foreach` = `0x00c91e90`
- `foreachi` = `0x00c91d40`  (FUN_00c91d40)
- `getn` = `0x00c920c0`  (FUN_00c920c0)
- `maxn` = `0x00c91fc0`  (FUN_00c91fc0)
- `insert` = `0x00c92190`  (FUN_00c92190)
- `remove` = `0x00c92290`
- `setn` = `0x00c92120`  (FUN_00c92120)
- `sort` = `0x00c92b30`

## array @ 0x014cea80  (12 entries)
- `clock` = `0x00c8fb20`
- `date` = `0x00c8fd60`  (FUN_00c8fd60)
- `difftime` = `0x00c90320`  (FUN_00c90320)
- `execute` = `0x00c8f7b0`
- `exit` = `0x00c90450`
- `getenv` = `0x00c8fa60`  (FUN_00c8fa60)
- `remove` = `0x00c8f830`  (FUN_00c8f830)
- `rename` = `0x00c8f8d0`  (FUN_00c8f8d0)
- `setlocale` = `0x00c903a0`  (FUN_00c903a0)
- `time` = `0x00c90110`  (FUN_00c90110)
- `tmpname` = `0x00c8f9e0`  (FUN_00c8f9e0)
- `fullpath` = `0x00c8fb50`  (FUN_00c8fb50)

## array @ 0x014ceb28  (11 entries)
- `close` = `0x00c8df00`  (FUN_00c8df00)
- `flush` = `0x00c8f280`
- `input` = `0x00c8e4b0`
- `lines` = `0x00c8e5c0`  (FUN_00c8e5c0)
- `open` = `0x00c8e010`  (FUN_00c8e010)
- `output` = `0x00c8e4d0`
- `popen` = `0x00c8e120`  (FUN_00c8e120)
- `read` = `0x00c8ed30`  (FUN_00c8ed30)
- `tmpfile` = `0x00c8e230`
- `type` = `0x00c8dba0`  (FUN_00c8dba0)
- `write` = `0x00c8f020`  (FUN_00c8f020)

## array @ 0x014ceb88  (9 entries)
- `close` = `0x00c8df00`  (FUN_00c8df00)
- `flush` = `0x00c8f310`
- `lines` = `0x00c8e540`  (FUN_00c8e540)
- `read` = `0x00c8ed50`
- `seek` = `0x00c8f080`
- `setvbuf` = `0x00c8f180`
- `write` = `0x00c8f040`
- `__gc` = `0x00c8df90`  (FUN_00c8df90)
- `__tostring` = `0x00c8dfc0`  (FUN_00c8dfc0)

## array @ 0x014cec18  (28 entries)
- `assert` = `0x00c8c810`  (FUN_00c8c810)
- `collectgarbage` = `0x00c8bfb0`  (FUN_00c8bfb0)
- `dofile` = `0x00c8c770`
- `error` = `0x00c8b880`  (FUN_00c8b880)
- `gcinfo` = `0x00c8bf80`
- `getfenv` = `0x00c8bbd0`  (FUN_00c8bbd0)
- `getmetatable` = `0x00c8b980`  (FUN_00c8b980)
- `loadfile` = `0x00c8c4d0`
- `load` = `0x00c8c660`  (FUN_00c8c660)
- `loadstring` = `0x00c8c410`
- `next` = `0x00c8c110`
- `pcall` = `0x00c8cad0`  (FUN_00c8cad0)
- `print` = `0x00c8b570`  (FUN_00c8b570)
- `rawequal` = `0x00c8bd40`  (FUN_00c8bd40)
- `rawget` = `0x00c8bdf0`  (FUN_00c8bdf0)
- `rawset` = `0x00c8beb0`  (FUN_00c8beb0)
- `select` = `0x00c8c9f0`  (FUN_00c8c9f0)
- `setfenv` = `0x00c8bc20`  (FUN_00c8bc20)
- `setmetatable` = `0x00c8b9f0`
- `tonumber` = `0x00c8b6b0`  (FUN_00c8b6b0)
- `tostring` = `0x00c8ce70`  (FUN_00c8ce70)
- `type` = `0x00c8c080`  (FUN_00c8c080)
- `unpack` = `0x00c8c8c0`
- `xpcall` = `0x00c8cb80`  (FUN_00c8cb80)
- `getreturntype` = `0x00c8d0e0`  (FUN_00c8d0e0)
- `getparamtypes` = `0x00c8d150`  (FUN_00c8d150)
- `getsyntaxtype` = `0x00c8d1a0`  (FUN_00c8d1a0)
- `trycall` = `0x00c8cc90`  (FUN_00c8cc90)

## array @ 0x014ced10  (7 entries)
- `create` = `0x00c8d6d0`  (FUN_00c8d6d0)
- `resume` = `0x00c8d500`  (FUN_00c8d500)
- `running` = `0x00c8d7f0`
- `status` = `0x00c8d3a0`
- `wrap` = `0x00c8d7a0`  (FUN_00c8d7a0)
- `yield` = `0x00c8d7d0`  (FUN_00c8d7d0)
- `cstacksize` = `0x00c8d660`  (FUN_00c8d660)

## array @ 0x014d02f8  (10 entries)
- `package` = `0x00c91a30`  (FUN_00c91a30)
- `table` = `0x00c92c60`  (FUN_00c92c60)
- `io` = `0x00c8f550`  (FUN_00c8f550)
- `os` = `0x00c90480`  (FUN_00c90480)
- `string` = `0x00c9aa50`  (FUN_00c9aa50)
- `math` = `0x00c97da0`  (FUN_00c97da0)
- `debug` = `0x00c97270`  (FUN_00c97270)
- `jit` = `0x00c95c40`  (FUN_00c95c40)
- `luca` = `0x00c9ad10`
- `lfs` = `0x00c942b0`  (FUN_00c942b0)

## array @ 0x016f43a8  (3 entries)
- `getaddrinfo` = `0x0114d7b0`  (FUN_0114d7b0)
- `getnameinfo` = `0x0114dac0`  (FUN_0114dac0)
- `freeaddrinfo` = `0x0114d760`  (FUN_0114d760)

## array @ 0x016f80e8  (3 entries)
- `getaddrinfo` = `0x0114d7b0`  (FUN_0114d7b0)
- `getnameinfo` = `0x0114dac0`  (FUN_0114dac0)
- `freeaddrinfo` = `0x0114d760`  (FUN_0114d760)

## array @ 0x0180f618  (7 entries)
- `SetBuild` = `0x00cb4390`
- `SetBuildName` = `0x00cb4440`
- `SetLocale` = `0x00cb4500`
- `SetLocaleName` = `0x00cb45b0`
- `SetCodePage` = `0x00cb4670`
- `SetCodePageName` = `0x00cb4720`
- `SetCharSet` = `0x00cb47e0`

## array @ 0x0180f6a0  (11 entries)
- `mode` = `0x00c93c70`  (FUN_00c93c70)
- `dev` = `0x00c93cd0`  (FUN_00c93cd0)
- `ino` = `0x00c93d00`  (FUN_00c93d00)
- `nlink` = `0x00c93d30`  (FUN_00c93d30)
- `uid` = `0x00c93d60`
- `gid` = `0x00c93d90`
- `rdev` = `0x00c93dc0`
- `access` = `0x00c93df0`
- `modification` = `0x00c93e10`
- `change` = `0x00c93e30`
- `size` = `0x00c93e50`
