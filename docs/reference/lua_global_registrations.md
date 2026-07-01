# goEngine global natives -- name -> VA (individual lua_setglobal registrations)

Resolved by `Res/ghidra_scripts/201_lua_global_registrations.py` (string-xref + fn-ptr window).
`resolved` = single unambiguous fn entry near the name's code xref; `ambiguous` = multiple
candidates (pick by inspection); complements the exact array scan in `lua_native_surface.md`.

- `GetEnvironment` = `0x00952610`  **resolved**
- `GetNObject` = `0x0094ff00`  **resolved**
- `GetUIResourceManager` = ambiguous: 0x00697ea0, 0x00697f70
- `GetUIControlManager` = `0x00697ea0`  **resolved**
- `PushUIEvent` = ambiguous: 0x006a3860, 0x006a3a40
- `PopUIEvent` = `0x006a3860`  **resolved**
- `GetLocale` = string @ 0x0155baec but no fn entry near xref
- `D3DCOLOR` = ambiguous: 0x00697f70, 0x00698040
- `RunLogLn` = string @ 0x0155c430 but no fn entry near xref
- `include` = string @ 0x014beadc but no fn entry near xref
- `Random` = string @ 0x01535508 but no fn entry near xref
- `IsClient` = string @ 0x01556cf0 but no fn entry near xref
- `goLua_CreateObject` = `0x00c48010`  **resolved**
- `goLua_RegistObject` = ambiguous: 0x00c483d0, 0x00c48520
- `goLua_Pack` = `0x00bb7630`  **resolved**
- `goLua_PackFixed` = `0x00bb7e70`  **resolved**
- `goLua_UnpackFixed` = `0x00bb8220`  **resolved**
- `goLua_setmetatable` = string @ 0x015555d4 but no fn entry near xref
- `goLua_loadfile` = ambiguous: 0x00c6ab10, 0x00c6b780
- `goLua_AddObjectFunction` = (no string in binary)
- `reload_file` = ambiguous: 0x00c6bc30, 0x00c6beb0
- `CreateDeferedTemplate` = string @ 0x01558824 but no fn entry near xref
- `Template` = (no string in binary)
- `Class` = string @ 0x014894fc, 0x01555710 but no fn entry near xref
- `Com` = (no string in binary)
- `GetClass` = (no string in binary)
- `CreateObject` = string @ 0x014c6825, 0x014c798f, 0x014c7a87, 0x014c7b2b, 0x014c7bbf, 0x014c849f, 0x014d1e59, 0x014d2106, 0x014d2468, 0x014d2749, 0x014d3669, 0x014d47da, 0x014d4ec5 but no fn entry near xref
- `FindObject` = string @ 0x015575b8 but no fn entry near xref
- `SetLuaNetFunc` = string @ 0x01556d80 but no fn entry near xref
- `BackupReplicate` = `0x00bcf870`  **resolved**
- `GetGWorld` = (no string in binary)
- `GetServerWorld` = (no string in binary)
- `GetClientWorld` = (no string in binary)
