/*
 * NpSlayer_exp.c -- EXPERIMENTAL NpSlayer (do NOT overwrite the known-good
 * NpSlayer_stub.c). Base = the working GameGuard-fake stub + a runtime
 * experiment harness driven from a worker thread.
 *
 * Build (x86, same as the known-good):
 *   "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\
 *      Auxiliary\Build\vcvarsall.bat" x86
 *   cl /nologo /O2 /MT /LD /Fe:Res\stubs\build\NpSlayer.dll \
 *      Res\stubs\NpSlayer_exp.c /link /DEF:Res\stubs\NpSlayer_exp.def /NOLOGO
 *   copy /Y Res\stubs\build\NpSlayer.dll TW\Bin\NpSlayer.dll
 * Rollback: copy /Y Res\stubs\build\NpSlayer_known_good.dll TW\Bin\NpSlayer.dll
 *   (or: git checkout npslayer-known-good -- Res/stubs/build/NpSlayer.dll)
 *
 * EXP_MODE selects the experiment:
 *   1 = TEST 1 (aggressive): int3+VEH one-shot reach hooks on the key6 / FUser
 *       chain. This WRITES NClient .text (0xCC), so it is the protector test:
 *       if "Protection error 15" fires, .text patching from the NpSlayer seat
 *       is dead too (fall back to mode 2). If it survives, the log tells us --
 *       for the first time -- whether the key6 callable is actually reached.
 *   2 = TEST 2 (mild): read-only poll of the FUser statechart. No .text write.
 *       Always safe; offsets are TODO (see EXP2_* below).
 *
 * EXP_TARGET_PROFILE selects mode-1 int3 targets:
 *   1 = late success path: key6 executor, SetControlObject, FUser finish.
 *   2 = ClientWorld/key6 bisect: key02, key03, key6 thunk/receiver/executor.
 *   3 = delayed lower-pipeline bisect: zlib, validity, queue, session dispatch.
 *   4 = force key6 receiver past the executor-skip JZ, then log late path.
 *   5 = key6 receiver branch-state logger: target/self/lookup/resolver gates.
 *   6 = method-map dump: at the 0x00c00a48 null lookup, walk the std::map at
 *       manager+0x1c and report size + whether method key 0xeae76408
 *       (native_nid SetControlObject) is present. Answers s31's wrong-map vs
 *       not-ready-yet question directly (empty map => registration/timing;
 *       non-empty without the key => wrong receiver/namespace).
 *   7 = self/component dump (after field2=object-id fix): at the 0x00c00a85
 *       executor-skip JZ, log the self resolver result ESI and dump the
 *       std::map at *(object+0x18) that FUN_00bd3660 searches by self_key.
 *       Reveals what self/component keys exist so we learn the right field3
 *       (current wire self_key=2). Empty => component not built; present-but-
 *       not-2 => wrong field3 value.
 *   8 = component-population x-ray (s6 frontier): the self-map is EMPTY because
 *       NewObject's component builder FUN_00bd4b20 either iterates an empty
 *       component-list or 0x00bb2580 returns a null component. Hooks the builder
 *       entry (obj + component-list), the post-materialize TEST ESI,ESI at
 *       0x00bd4c40 (component null?), and the self-map write at 0x00bd4cf4
 *       (did it fire?). One run answers: empty list vs null materialize.
 *   9 = obj+0x18 writer census (s11 frontier): logging-only re-arming hooks on
 *       the three known self/owner-map writers plus the TClient slot-23
 *       populator and the key6 self-gate. Answers whether any object gets
 *       key=2 / any entries, and whether replicated TClient is the anomaly.
 *   10 = Connector handshake reach logger (s20/s37 frontier): logging-only
 *       re-arming hooks on Connector open/connect/challenge/response handlers
 *       and the obj+0x18 writer. Answers whether Connector::open initializes
 *       +54/+58/+5c before connect, whether connect emits challenge, and
 *       whether a challenge/response handler ever runs.
 *   11 = obj+0x18 injection spec Stage 1: DUMP ONLY. Hooks key6 dispatch/
 *       self-gate and downstream SetControl/connect markers, scans obj+0x14
 *       for Connector and obj+0x18 for key=2/Connector. No injection.
 *   12 = obj+0x18 injection spec Stage 2: inject m_ControlObj key=2 only
 *       for live TClient objects at the pre-selfgate key6 point, then log
 *       executor/SetControl/OnSigSetControlObject/connect reach markers.
 *   13 = obj+0x18 direct key/value diagnostic: use the smaller
 *       FUN_00bd3ae0(object, 2, DAT_0183ef78) helper after profile 12
 *       proved full FUN_00bd2cd0 registration needs extra context.
 *   14 = logging-only Channel owner/source probe: profile 11 markers plus
 *       Channel::open and Channel::ChangeChannelFinish, with no obj+18 repair.
 *   15 = logging-only dispatcher-owner discriminator: ClientWorld key6 vs
 *       tag1 Computer key6/key2 plus shared key6 executor. No protocol/client
 *       behavior repair; this is only a reach marker profile.
 *   16 = logging-only obj+0x18 writer census with class/pid columns plus
 *       tag1 Computer register/insert markers. No protocol/client repair.
 *   17 = logging-only connect/attach reach probe: FUser::connect,
 *       FClientWorld::Connect, master attach, ClientComputer attach. No
 *       protocol/client repair.
 *   18 = logging-only OnSig/connector replication probe: FClientWorld
 *       OnSigSetControlObject, FUser::onConnectorReplicated, connect emit,
 *       and DownCall connect. No protocol/client repair.
 *   19 = logging-only AddComponent/loginresult probe: AddComponent entry,
 *       obj+0x18 writer, DownCall logInServerResult, inner 0x00a81020, and
 *       the first "No X" throw-site markers. No protocol/client repair.
 *   20 = logging-only login key6 gate probe: key6 field/lookup/self gates,
 *       executor/callable markers, logIn/logInMaster/logInServerResult entries,
 *       and login throw-sites. No protocol/client repair.
 *   21 = logging-only onLoadPlayers probe: key6 field/lookup/self gates,
 *       Session::onLoadPlayersCacheResult wrapper/inner logic, and obj+0x18
 *       writer. No protocol/client repair.
 *   22 = diagnostic TAccount Session self-entry oracle: before the Session
 *       key6 self resolver runs, if TAccount obj+0x14 already contains
 *       Session, promote that value into obj+0x18 with FUN_00bd3ae0, then log
 *       whether onLoadPlayers/login handlers become reachable. Debug-only.
 *   23 = logging-only AddComponent fork probe: profile21 key6/self/obj18
 *       markers plus AddComponent entry and the two executor rejection sites
 *       ("Invalid Network Call Format" and "CallNetFunction num args wrong").
 *       No protocol/client repair.
 *   24 = logging-only call-mode probe: profile23 plus executor stream
 *       vtable+0x3c result at 0x00bee4b1 (does AddComponent read mode 0/1?).
 *       No protocol/client repair.
 *   25 = force-patch P1 + logging: NOP only the bf0e30 scope-gate JZ at
 *       0x00bee5b0, then log AddComponent/obj18/login reach. Does not patch
 *       ba9640 or the key6 self-gate.
 *   26 = force-patch P1+P2 + logging: profile25 plus NOP the ba9640 JZ at
 *       0x00bee649 after profile25 proved ba9640 returned zero. Does not
 *       patch the key6 self-gate.
 *   27 = s45 goLua_AddComponent probe: profile25 P1 only, plus
 *       goLua_AddComponent 0x00c483d0 and its bd2cd0 callsite 0x00c484b1.
 *       Does not patch ba9640 or the key6 self-gate.
 *   28 = profile27 plus P2: force-NOP ba9640 JZ at 0x00bee649 after
 *       profile27 proved goLua_AddComponent still returns EAX=0 there.
 *       Does not patch the key6 self-gate.
 *   29 = s46 bare-name AddComponent probe: profile25 P1 only, plus
 *       post-bc5860 name-string logging at 0x00bee632, ba9640 argument/return,
 *       and goLua_AddComponent wrapper markers. No P2, no self-gate patch.
 *   30 = s47 natural-flow account/Lua census: logging-only hooks on all
 *       obj+0x18 writers, goLua_CreateObject/AddComponent/RegistObject, and
 *       the signInClientResult handler family. No NOP gates and no injections.
 *   31 = s51 B1 Session-tail proof: logging-only hooks on NewObject component
 *       tail materialization (0x00bb2580 / 0x00bd4b20 path), obj+0x18
 *       writers, and key6 self-gate. No NOP gates and no injections.
 *   35 = s54 narrowed Lua-call proof: profile33's real Session self-entry
 *       bd2cd0 injection plus only the key6 executor Lua callsite/return and
 *       error markers. Avoids profile34's global cbda80 boot-time log flood.
 *   38 = s11 OnLoadPlayerData middle-bridge proof: profile37 plus
 *       generated OnLoadPlayerData 0x00a76440, Session::OnLoadPlayerData
 *       0x00a80f30, and downstream logInServerResult emitter 0x00a6e740.
 *   39 = s13 OnLoadPlayerData gate frontier: profile38 plus
 *       logInServerResult handler/inner throw markers and the
 *       SetControlObject signal helper. Logging + Session obj18 seat only.
 *   40 = s13 OnLoadPlayerData outbound bridge frontier: profile39 plus
 *       0x00a87b90 early-outs and post-writer enqueue/queue markers.
 *   41 = s14 outbound drain/send-gate probe: profile40's proven
 *       OnLoadPlayerData sequence plus logging-only hooks on the drain worker
 *       call, FUN_009302b0 send gate, FUN_0115f970 send helper, and teardown.
 *   42 = s17 B4 descriptor dump: logging-only read of Player/CStatusPlayer/User
 *       runtime field-list descriptors so TPlayerDummy component tails can be
 *       framed byte-correctly before any B4 packet is built.
 *   43 = s18 B4 component/gate run: profile40+component materialization and
 *       the same bd2cd0 self-register bridge for Session/Player/CStatusPlayer/
 *       User as components land. Used for one B4 live validation run.
 *   45 = s22 B4 boundary probe: profile44 plus cursor stamps around
 *       bb0060's Lua Unpack call and bede50's cache-copy side-channel.
 *   46 = s26 User B4 field loop probe: profile45 plus per-field logging at
 *       bb0060's descriptor loop (field key/type/mode/offset + cursor).
 *   47 = s30 goLua_UnpackFixed key probe: profile46 plus logging-only hook
 *       at goLua_UnpackFixed entry (0x00bb8220) to dump Lua frame/reader args.
 *   48 = s31 goLua_UnpackFixed key-string probe: profile47 plus guarded
 *       decode/dump of the Lua TString key in argument slot 3.
 *   49 = s35 gate-knockdown probe: profile48 tail/key logging plus profile43
 *       logInServerResult/FUser/key6 gate ladder markers.
 *   50 = s41 B4 OnLoad early key6 guard probe: profile49 plus the missing
 *       FUN_00c009a0 target-guard/cleanup branch hooks at 0x00c00a1c/1f/24/2c,
 *       0x00c00ac8, and 0x00c00ae3. Logging only; no protocol byte changes.
 *   51 = s59 B4 simple-field unpack probe: profile49 plus FUN_00baaa80 entry,
 *       its type-vtable +0x14 indirect call at 0x00baab01, and return
 *       cursor delta at 0x00baab03. Logging only; no protocol byte changes.
 *   52 = s64 post-finish UI delivery probe: preserves the proven diagnostic
 *       Session/B4 obj+18 self-register seat, then logs whether
 *       Master_FinishLoadingCharacter reaches UIIntro/updateCharacter and
 *       HasNoCharacter/AddCharacter. No protocol byte changes.
 *   53 = s2 UIGMSEventDialog bridge probe: profile52 plus the actual
 *       FUser:SetUIControl setter (FUN_010b0700) and its FUser+0x74 write
 *       site, to prove whether UIGMSEventDialog is the finish event target.
 *       Logging only; no protocol byte changes.
 *   54 = s3 narrower UIGMSEventDialog bridge probe: profile53 without the
 *       always-on 0x006f1ec0 hook, which profile53 proved is too hot during
 *       boot/login. Keeps SetUIControl, finish, updateCharacter, and
 *       character-list markers. Logging only; no protocol byte changes.
 *   55 = s4 deferred event-helper probe: profile54 plus a one-shot deferred
 *       arm of 0x006f1ec0 only when finish_notify is reached, and the return
 *       site after the call. Logging only; no protocol byte changes.
 *   56 = s6 deferred HandleEvent handler-loop probe: profile55 plus deferred
 *       hooks inside the one post-finish 0x006f1ec0 call at 0x006f201a,
 *       0x006f201f, 0x006f2050, 0x006f2055, and 0x006f2057. Logging only;
 *       no protocol byte changes.
 *   57 = s7 profile56 plus UICommandControlMethod::Execute delegate call/ret
 *       hooks at 0x00754dff/0x00754e01 to identify handler+0x18. Logging
 *       only; no protocol byte changes.
 *   58 = s10 profile57 plus ItemFactory::OnLoadingEnd entry/capability/emit
 *       hooks to identify the B6 loading gate receiver and event support test.
 *       Logging only; no protocol byte changes.
 *   59 = s11 profile58 plus the actual UI delegate object fields `[+4]` and
 *       `[+8]` used by 0x006c3390. Profile58 proved the vtable-adjacent
 *       string was not the callable; the data-field callee must be logged.
 *       Logging only; no protocol byte changes.
 *   60 = s12 profile59 correction: log the actual UIEventUser pointer and
 *       fields after FUser::onFinishLoadingCharacter. Static 0x0071ae00 maps
 *       m_param1=event+0x1c and m_param2=event+0x38. Logging only; no protocol
 *       byte changes.
 *   61 = s13 profile60 narrowed: corrected live UIEventUser data offsets
 *       (m_param1=event+0x20, m_param2=event+0x3c) and arms event hooks only
 *       inside the finish handler window. Logging only; no protocol bytes.
 *   62 = s17 B6 intro-map probe: profile61 plus deferred
 *       FUser::onMapStartLoading/onMapCompleteLoading. The deferred window
 *       stays open after finish_after so delayed IntroMapStart prerequisites
 *       are visible. Logging only; no protocol bytes. RunLogLn is deliberately
 *       not trapped in this profile because the first profile62 run regressed
 *       before finish and we need the narrower map callback question answered.
 *   63 = s17 B6 UICommandLua executor probe: profile61 finish-window path
 *       plus concrete UICommandLuaFunction::Execute/LuaExecute hooks and
 *       map/update markers. Logging only; no protocol bytes.
 *   64 = s18 lean B6 UICommandLua probe: only required obj+18 planting
 *       seats plus finish/deferred UICommandLua/map markers. Logging only;
 *       no protocol bytes.
 *   65 = s18 minimal B6 Lua-command probe: profile61 known-good pre-finish
 *       behavior plus only UICommandLuaFunction::Execute/LuaExecute in the
 *       finish window. Logging only; no protocol bytes.
 *   66 = s20 force B6 updateCharacter probe: profile63-style finish window
 *       plus one direct FUser::updateCharacter(FUser+0x74) call after the
 *       finish event returns. This does not patch client .text beyond the
 *       normal NpSlayer int3 hooks; it tests whether the native char-list UI
 *       path can render once invoked.
 *   67 = s21 force B6 updateCharacter-at-notify probe: same as profile66,
 *       but invokes updateCharacter at finish_notify because profile66 proved
 *       finish_after_006f1ec0 is not reached before the socket/process closes.
 *   68 = s22 force B6 CharacterManager slot: capture the real
 *       bd2cd0 CharacterManager component and plant FUser+0x84 before the
 *       single updateCharacter-at-notify call.
 *   69 = s23 LoadList/finish-send probe: profile68 bridge plus logging at
 *       CharacterManager::LoadList, its onLoad return edge, and the
 *       Server_FinishLoadingCharacter sender. No protocol byte changes.
 *   70 = s24 transition-teardown probe: keep the proven B4/session planters,
 *       then log FClientApp finalize, drain teardown, NetComputer report,
 *       session shutdown, socket close helper, and all non-break exceptions.
 *       Base probe is ordered-b4-b5-empty-select; no protocol byte changes.
 *   profile61 rebuild note (2026-06-18): also logs the return edge after
 *       CharacterManager::getAllPlayers so the char-list vector state is
 *       visible without changing protocol bytes.
 *   73 = profile72 transition/component teardown probe plus BB0060
 *       component field/tail logging and SetUIControl/UI breadcrumbs.
 *   74 = profile73 plus low-noise UI lifecycle breadcrumbs
 *       (SetVisible/Attach/MoveFront) so a run can prove which UI control
 *       layer became visible without depending only on screenshots.
 *   75 = profile74 plus sharp onLoadPlayers transition branch markers:
 *       existing/create CharacterManager leg, LoadList call/return, native
 *       finish sender, and UI breadcrumbs armed only after onLoad starts.
 *   76 = profile75 plus profile69 CharacterManager::LoadList internals:
 *       empty/list branch, node payload, resolver, row-add, exception, and
 *       same post-onLoad UI lifecycle breadcrumbs. Logging only.
 *   77 = profile76 plus Session+0xe0 lifecycle breadcrumbs:
 *       Session::masterOpen, Session::masterReopen, and the a8d1d0 slot
 *       materializer. Logging only; no protocol or control-flow forcing.
 *   78 = profile77 plus one narrow LoadList guard: at the
 *       CharacterManager::LoadList -> Session::UpdateCharacterCount callsite
 *       (0x00a62d48), if Session+0xe0 is NULL, skip only that bookkeeping
 *       call by ESP+=4 and EIP=0x00a62d4d. This does not patch NClient.exe
 *       on disk and does not change protocol bytes. It also captures the real
 *       CharacterManager from LoadList's ESI and, at FUser::updateCharacter,
 *       fills only missing FUser+0x84 with that manager so getAllPlayers can
 *       test the next char-select gate.
 */

#include <windows.h>
#pragma comment(lib, "user32.lib")   /* wsprintfA */

#ifndef EXP_MODE
#define EXP_MODE 1
#endif

#ifndef EXP_TARGET_PROFILE
#define EXP_TARGET_PROFILE 78
#endif

/* ------------------------------------------------------------------ */
/* GameGuard fake (verbatim from the known-good stub -- keeps boot ok) */
/* ------------------------------------------------------------------ */

typedef BOOL (WINAPI *CreateProcessA_t)(
    LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD,
    LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION);
typedef BOOL (WINAPI *GetExitCodeProcess_t)(HANDLE, LPDWORD);

static CreateProcessA_t g_real_CreateProcessA = NULL;
static GetExitCodeProcess_t g_real_GetExitCodeProcess = NULL;
static HANDLE g_fake_process = NULL;
static HANDLE g_fake_thread = NULL;
static char g_module_dir[MAX_PATH];
static DWORD g_module_base = 0;
static DWORD g_module_limit = 0;

enum {
    NPS_GAMEGUARD_SUCCESS = 0x755,
    QQXJ_CREATE_PROCESS_SLOT = 0x0148012c,
    QQXJ_GET_EXIT_CODE_SLOT = 0x01480130,
};

static int contains(const char *s, const char *needle)
{
    const char *p;
    if (!s || !needle) return 0;
    for (; *s; ++s) {
        p = needle;
        while (*p && s[p - needle] == *p) ++p;
        if (!*p) return 1;
    }
    return 0;
}

static void log_to(const char *file, const char *msg)
{
    HANDLE h;
    DWORD written;
    char path[MAX_PATH];
    path[0] = 0;
    if (g_module_dir[0]) {
        lstrcpynA(path, g_module_dir, MAX_PATH);
        lstrcatA(path, "\\");
        lstrcatA(path, file);
        file = path;
    }
    h = CreateFileA(file, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, msg, lstrlenA(msg), &written, NULL);
    WriteFile(h, "\r\n", 2, &written, NULL);
    CloseHandle(h);
}

static void log_line(const char *msg) { log_to("NpSlayer_qqxj_hook.log", msg); }
static void exp_log(const char *msg)  { log_to("NpSlayer_exp.log", msg); }

static void exp_logf(const char *fmt, DWORD a, DWORD b, DWORD c)
{
    char buf[256];
    wsprintfA(buf, fmt, a, b, c);
    exp_log(buf);
}

static void exp_log_addr_region(const char *tag, DWORD va)
{
    MEMORY_BASIC_INFORMATION mbi;
    char path[MAX_PATH];
    char buf[1024];
    DWORD alloc, base, size, rva;
    path[0] = 0;
    if (!tag) tag = "ADDR_REGION";
    if (!va) {
        wsprintfA(buf, "%s addr=0x%08x null", tag, va);
        exp_log(buf);
        return;
    }
    if (VirtualQuery((void*)(DWORD_PTR)va, &mbi, sizeof(mbi)) == 0) {
        wsprintfA(buf, "%s addr=0x%08x VirtualQuery=FAIL", tag, va);
        exp_log(buf);
        return;
    }
    alloc = (DWORD)(DWORD_PTR)mbi.AllocationBase;
    base = (DWORD)(DWORD_PTR)mbi.BaseAddress;
    size = (DWORD)mbi.RegionSize;
    rva = (alloc && va >= alloc) ? (va - alloc) : 0;
    if (!GetModuleFileNameA((HMODULE)(DWORD_PTR)alloc, path, MAX_PATH))
        path[0] = 0;
    wsprintfA(buf,
        "%s addr=0x%08x alloc=0x%08x base=0x%08x size=0x%08x state=0x%08x protect=0x%08x type=0x%08x rva=0x%08x module=%s",
        tag, va, alloc, base, size, mbi.State, mbi.Protect, mbi.Type, rva, path);
    exp_log(buf);
}

/* tiny structured-exception-guarded 32-bit read, usable in any mode */
static DWORD exp2_read_safe(DWORD p){ DWORD v=0; __try{ v=*(DWORD*)(DWORD_PTR)p; }__except(1){} return v; }

static int exp_is_self_module_addr(DWORD va)
{
    return g_module_base && va >= g_module_base && va < g_module_limit;
}

static void exp_read_cstr_safe(DWORD p, char *buf, int cap)
{
    int i;
    char ch;
    if (!buf || cap <= 0)
        return;
    buf[0] = 0;
    if (!p)
        return;
    __try {
        for (i = 0; i < cap - 1; ++i) {
            ch = *(char*)(DWORD_PTR)(p + (DWORD)i);
            if (!ch)
                break;
            if ((unsigned char)ch < 0x20 || (unsigned char)ch > 0x7e)
                ch = '.';
            buf[i] = ch;
        }
        buf[i] = 0;
    } __except(1) {
        buf[0] = 0;
    }
}

static void exp_read_mem_string(DWORD p, DWORD n, char *buf, int cap)
{
    DWORD i, maxn;
    char ch;
    if (!buf || cap <= 0)
        return;
    buf[0] = 0;
    if (!p || !n)
        return;
    maxn = n;
    if (maxn > (DWORD)(cap - 1))
        maxn = (DWORD)(cap - 1);
    __try {
        for (i = 0; i < maxn; ++i) {
            ch = *(char*)(DWORD_PTR)(p + i);
            if (!ch)
                break;
            if ((unsigned char)ch < 0x20 || (unsigned char)ch > 0x7e)
                ch = '.';
            buf[i] = ch;
        }
        buf[i] = 0;
    } __except(1) {
        buf[0] = 0;
    }
}

static void exp_read_std_string_safe(DWORD s, char *buf, int cap)
{
    DWORD len, res, p;
    if (!buf || cap <= 0)
        return;
    buf[0] = 0;
    if (!s)
        return;
    len = exp2_read_safe(s + 0x10);
    res = exp2_read_safe(s + 0x14);
    if (len > 0x200 || res > 0x100000)
        return;
    p = (res < 0x10) ? s : exp2_read_safe(s);
    exp_read_mem_string(p, len, buf, cap);
}

static void init_module_dir(HINSTANCE hinst)
{
    DWORD n = GetModuleFileNameA(hinst, g_module_dir, MAX_PATH);
    char *p;
    g_module_base = (DWORD)(DWORD_PTR)hinst;
    g_module_limit = g_module_base + 0x40000u;
    __try {
        IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)(DWORD_PTR)g_module_base;
        IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(DWORD_PTR)(g_module_base + dos->e_lfanew);
        if (dos->e_magic == IMAGE_DOS_SIGNATURE && nt->Signature == IMAGE_NT_SIGNATURE)
            g_module_limit = g_module_base + nt->OptionalHeader.SizeOfImage;
    } __except(1) {
    }
    if (n == 0 || n >= MAX_PATH) {
        g_module_dir[0] = 0;
        return;
    }
    p = g_module_dir + lstrlenA(g_module_dir);
    while (p > g_module_dir && p[-1] != '\\' && p[-1] != '/') --p;
    if (p > g_module_dir) p[-1] = 0;
}

static BOOL WINAPI Hook_CreateProcessA(
    LPCSTR app, LPSTR cmd, LPSECURITY_ATTRIBUTES proc_attr,
    LPSECURITY_ATTRIBUTES thread_attr, BOOL inherit, DWORD flags, LPVOID env,
    LPCSTR cwd, LPSTARTUPINFOA startup, LPPROCESS_INFORMATION pi)
{
    if ((contains(app, "GameGuard.des") || contains(cmd, "GameGuard.des")) && pi) {
        if (!g_fake_process) g_fake_process = CreateEventA(NULL, TRUE, TRUE, NULL);
        if (!g_fake_thread)  g_fake_thread  = CreateEventA(NULL, TRUE, TRUE, NULL);
        pi->hProcess = g_fake_process;
        pi->hThread = g_fake_thread;
        pi->dwProcessId = 0x755;
        pi->dwThreadId = 0x756;
        log_line("faked CreateProcessA(GameGuard.des)");
        return TRUE;
    }
    if (g_real_CreateProcessA)
        return g_real_CreateProcessA(app, cmd, proc_attr, thread_attr, inherit,
                                     flags, env, cwd, startup, pi);
    return FALSE;
}

static BOOL WINAPI Hook_GetExitCodeProcess(HANDLE process, LPDWORD exit_code)
{
    if (process == g_fake_process) {
        if (exit_code) *exit_code = NPS_GAMEGUARD_SUCCESS;
        log_line("faked GetExitCodeProcess(GameGuard.des)=0x755");
        return TRUE;
    }
    if (g_real_GetExitCodeProcess)
        return g_real_GetExitCodeProcess(process, exit_code);
    return FALSE;
}

static void patch_slot(void **slot, void *hook, void **orig)
{
    DWORD old_protect;
    if (!slot || !hook) return;
    if (*slot == hook) return;
    if (!*orig && *slot) *orig = *slot;
    if (VirtualProtect(slot, sizeof(void *), PAGE_EXECUTE_READWRITE, &old_protect)) {
        *slot = hook;
        VirtualProtect(slot, sizeof(void *), old_protect, &old_protect);
        FlushInstructionCache(GetCurrentProcess(), slot, sizeof(void *));
    }
}

static void install_hooks(void)
{
    patch_slot((void **)QQXJ_CREATE_PROCESS_SLOT, Hook_CreateProcessA, (void **)&g_real_CreateProcessA);
    patch_slot((void **)QQXJ_GET_EXIT_CODE_SLOT, Hook_GetExitCodeProcess, (void **)&g_real_GetExitCodeProcess);
}

/* ------------------------------------------------------------------ */
/* Experiment targets (VAs from the unpacked image; live VAs match --  */
/* the IAT slots above are in the same 0x014xxxxx region and work).     */
/* ------------------------------------------------------------------ */

/* Reach/bisect points (findings s11/s25/s29 and Claude s30 baton). */
#define VA_KEY6_EXECUTOR        0x00bee460u  /* key6 executor/callable gate      */
#define VA_KEY6_TARGET_CMP      0x00c00a1cu  /* CMP target vs manager pid        */
#define VA_KEY6_TARGET_CUR_JZ   0x00c00a1fu  /* JZ if target == manager pid      */
#define VA_KEY6_TARGET_NEG1_JZ  0x00c00a24u  /* JZ if target == -1 wildcard      */
#define VA_KEY6_TARGET_REJECT   0x00c00a2cu  /* JNZ to cleanup if not 0x80000000 */
#define VA_KEY6_LOOKUP_NULL_JZ  0x00c00a48u  /* descriptor lookup null gate      */
#define VA_KEY6_RESOLVED_TEST   0x00c00a83u  /* self resolver result test        */
#define VA_KEY6_EXEC_CALLSITE   0x00c00a9fu  /* CALL 0x00bee460 site             */
#define VA_KEY6_CLEANUP_ENTRY   0x00c00ac8u  /* cleanup/exit block before helper */
#define VA_KEY6_CLEANUP_CALL    0x00c00ae3u  /* cleanup helper call              */
#define VA_SETCTRL_HANDLER      0x00c51950u  /* SetControlObject callable        */
#define VA_FUSER_FINISH         0x010b3d20u  /* FUser finish success emitter     */
#define VA_ZLIB_INFLATE         0x01168700u  /* type-4 zlib inflate entry        */
#define VA_LOWER_VALIDITY       0x0115bfd0u  /* lower packet validity helper     */
#define VA_QUEUE_PRODUCER       0x01158950u  /* decoded packet queue producer    */
#define VA_QUEUE_CONSUMER       0x0115b0b0u  /* per-tick receive queue drain     */
#define VA_SESSION_DISPATCH     0x00ca4540u  /* CL_NetSession validator/dispatch */
#define VA_KEY02_THUNK          0x00c10650u  /* ClientWorld key 0x02 thunk       */
#define VA_KEY03_NEWOBJECT      0x00bff990u  /* ClientWorld key 0x03 receiver    */
#define VA_NEWOBJ_PID_STORED    0x00bffafdu  /* after NewObject writes obj+0x3c  */
#define VA_KEY6_THUNK           0x00c10610u  /* ClientWorld key 0x06 thunk       */
#define VA_KEY6_RECEIVER        0x00c009a0u  /* ClientWorld key 0x06 receiver    */
#define VA_KEY6_SKIP_EXEC_JZ    0x00c00a85u  /* JZ skip before CALL 0x00bee460   */
#define VA_COMP_MATERIALIZE     0x00bb2580u  /* materialize component by desc/key */
#define VA_BB2580_BC5860_CALL   0x00bb25e5u  /* resolver calls 0x00bc5860        */
#define VA_BB2580_BC5860_RET    0x00bb25eau  /* after 0x00bc5860; EAX=result     */
#define VA_BB2580_BB5F40_CALL   0x00bb25f3u  /* resolver calls 0x00bb5f40        */
#define VA_BB2580_BB5F40_RET    0x00bb25f8u  /* after 0x00bb5f40; EAX=result     */
#define VA_OBJPTR_MAT_ENTRY     0x00c537d0u  /* ObjectPtr materializer entry     */
#define VA_OBJPTR_BB2580_CALL   0x00c5380du  /* ObjectPtr calls 0x00bb2580       */
#define VA_OBJPTR_BB2580_RET    0x00c53812u  /* after 0x00bb2580; EAX=object     */
#define VA_NEWOBJ_COMP_INIT     0x00bd4b20u  /* NewObject component builder       */
#define VA_COMP_NULL_TEST       0x00bd4c40u  /* TEST ESI,ESI after 0x00bb2580     */
#define VA_COMP_MAP_WRITE       0x00bd4cf4u  /* MOV [EBX],ECX self-map node write */
#define VA_COMP_LOOP_RECHECK    0x00bd4d75u  /* CMP EAX,ESI: post-slot7 cursor<end  */
#define VA_COMP_SLOT7_RET       0x00bd4d38u  /* insn AFTER slot-7 call returns      */
#define VA_COMP_KEY_READ_RET    0x00bd4bfeu  /* after reader +0x28 key read         */
#define VA_COMP_LOOKUP_RET      0x00bd4c19u  /* after bd38c0 existing-comp lookup   */
#define VA_COMP_EXISTING_TEST   0x00bd4c25u  /* TEST ECX,ECX existing component     */
#define VA_COMP_SLOT7_ENTRY     0x00bd4d2cu  /* before component vslot +0x1c reader */
#define VA_BB0060_UNPACK_BA9640 0x00bb036bu  /* before FUN_00ba9640("Unpack")       */
#define VA_BB0060_UNPACK_CBDA80 0x00bb03d1u  /* before Lua Unpack dispatch           */
#define VA_BB0060_UNPACK_RET    0x00bb03d6u  /* after Lua Unpack dispatch returns    */
#define VA_BB0060_FIELD_GATE    0x00bb012eu  /* before FUN_005fb3f0 field special gate */
#define VA_BB0060_FIELD_GATE_RET 0x00bb0136u /* after FUN_005fb3f0, AL=special?       */
#define VA_BB0060_ENTRY         0x00bb0060u  /* NObject wrapper vslot +0x1c entry    */
#define VA_BB0060_TYPE2_ENTRY   0x00bb0255u  /* descriptor type==2 Lua-tail branch    */
#define VA_BB0060_TYPE2_BLOB1   0x00bb0281u  /* first reader+0x6c blob call          */
#define VA_BB0060_TYPE2_U8_A    0x00bb032au  /* reader+0x44 byte after first blob    */
#define VA_BB0060_TYPE2_BLOB2   0x00bb0338u  /* second reader+0x6c blob call         */
#define VA_BB0060_TYPE2_U8_B    0x00bb0346u  /* reader+0x48 byte after second blob   */
#define VA_BB0060_TYPE2_F64     0x00bb0362u  /* reader+0x50 f64 before Unpack lookup */
#define VA_BB0060_TYPE2_EXIT    0x00bb0409u  /* after descriptor type==2 tail region  */
#define VA_SIMPLE_FIELD_READ    0x00baaa80u  /* FUN_00baaa80 simple goTypeDB unpack    */
#define VA_SIMPLE_UNPACK_CALL   0x00baab01u  /* indirect call type-object vtable +0x14  */
#define VA_SIMPLE_UNPACK_RET    0x00baab03u  /* after type-object +0x14 returns         */
#define VA_BEDE50_AFTER_BB0060  0x00bee0f0u  /* bede50 immediately after bb0060      */
#define VA_BEDE50_CURSOR_RET    0x00bee0f9u  /* after reader slot +0x04 cursor read  */
#define VA_BEDE50_CACHE_BE9840  0x00bee1edu  /* cache stat/update helper             */
#define VA_BEDE50_SEEK_START    0x00bee203u  /* reader slot +0x74 seek to start      */
#define VA_BEDE50_CACHE_RESET   0x00bee211u  /* side-cache reset call                */
#define VA_BEDE50_COPY_RAW      0x00bee241u  /* reader slot +0x58 raw copy           */
#define VA_BEDE50_SEEK_BACK     0x00bee257u  /* reader slot +0x74 seek back          */
#define VA_OBJ18_REG_BD2CD0     0x00bd2cd0u  /* FUN_00bd2cd0 entry: obj+18 register */
#define VA_OBJ18_WRITE_BD2D4D   0x00bd2d4du  /* FUN_00bd2cd0 map write              */
#define VA_OBJ18_REG_BD3AE0     0x00bd3ae0u  /* FUN_00bd3ae0 entry: key/value insert*/
#define VA_OBJ18_WRITE_BD3B15   0x00bd3b15u  /* FUN_00bd3ae0 map write              */
#define VA_OBJ18_REG_BD9C50     0x00bd9c50u  /* FUN_00bd9c50 entry: register variant*/
#define VA_OBJ18_WRITE_BD9D5F   0x00bd9d5fu  /* FUN_00bd9c50 map write              */
#define VA_TCLIENT_POP_C5A880   0x00c5a880u  /* TClient vtable slot 23 self setup   */
#define VA_CONN_CONNECT_BASE     0x00a8e3a0u  /* Connector connect base RPC handler   */
#define VA_CONN_OPEN_IMPL        0x00a90f00u  /* Connector::open field initializer     */
#define VA_CONN_CONNECT_IMPL     0x00a91210u  /* Connector::connect implementation    */
#define VA_CONN_CONNECT_EMIT     0x00a8ec20u  /* connect emit helper from a91210      */
#define VA_CONN_CHALLENGE_BASE   0x00a8efc0u  /* Connector challenge base handler     */
#define VA_CONN_RESPONSE_BASE    0x00a8f550u  /* Connector response base handler      */
#define VA_CONN_RESPONSE_IMPL    0x00a912f0u  /* Connector::response / hash gate      */
#define VA_CONN_ARG_READER       0x00bab510u  /* one-arg reader used by challenge     */
#define VA_PM_CREATEOBJECT       0x00b8f750u  /* PlayerManager::createObject helper   */
#define VA_EVAL_MASTERHASH_MARK  0x00a82340u  /* evalMasterHash trace marker only     */
#define VA_SETCTRL_SIGNAL_HELPER 0x00c51f30u  /* SetControlObject store/signal helper */
#define VA_ONSIG_SETCONTROL      0x01097270u  /* FClientWorld OnSigSetControlObject   */
#define VA_FUSER_ONCONNECTOR     0x010b3530u  /* FUser::onConnectorReplicated         */
#define VA_FUSER_ONSIGNIN        0x010b3760u  /* FUser::onSignInResult                */
#define VA_FUSER_ONLOGIN         0x010b3a60u  /* FUser::onLogInResult                 */
#define VA_FUSER_ONSESSION       0x010b68c0u  /* FUser::onSessionReplicated           */
#define VA_FUSER_STATE1_WRITE_8C 0x010b0c70u  /* state1 writes FUser+0x8c=1            */
#define VA_FUSER_STATE1_WRITE_74 0x010b0cbcu  /* state1 writes FUser+0x74 notifier     */
#define VA_FUSER_SIGNIN_74_CMP   0x010b37ffu  /* onSignInResult +0x74 gate             */
#define VA_FUSER_SIGNIN_SCS_OK   0x010b3822u  /* onSignInResult SCS_OK emit            */
#define VA_FUSER_SIGNIN_EVENT    0x010b3832u  /* onSignInResult Master_SignInResult    */
#define VA_FUSER_SIGNIN_NOTIFY   0x010b38b1u  /* onSignInResult notifier call          */
#define VA_FUSER_LOGIN_SCS_OK    0x010b3ae2u  /* onLogInResult SCS_OK emit             */
#define VA_FUSER_LOGIN_EVENT     0x010b3af2u  /* onLogInResult channel/login event      */
#define VA_FUSER_ONSESSION_RESOLVE 0x010b69d2u /* onSessionReplicated self resolve edge */
#define VA_FUSER_ONSESSION_STATE_CMP 0x010b6a74u /* reads FUser+0x8c                    */
#define VA_FUSER_ONSESSION_74_CMP 0x010b6ad8u /* onSessionReplicated +0x74 gate        */
#define VA_FUSER_ONSESSION_SCS_OK 0x010b6aedu /* onSessionReplicated SCS_OK emit       */
#define VA_FUSER_ONSESSION_EVENT 0x010b6b01u  /* onSessionReplicated Master_SignIn     */
#define VA_FUSER_ONSESSION_NOTIFY 0x010b6b90u /* onSessionReplicated notifier call     */
#define VA_FUSER_ONSESSION_STATE5_WRITE 0x010b6bddu /* onSession state-5 write        */
#define VA_FUSER_SETUICONTROL    0x010b0700u  /* actual SetUIControl setter         */
#define VA_FUSER_SETUICONTROL_WRITE_74 0x010b0770u /* writes [FUser+0x74]=UI control */
#define VA_FUSER_FINISH_RESULT_CMP 0x010b3d54u /* finish result gate                    */
#define VA_FUSER_FINISH_74_CMP   0x010b3d77u  /* finish +0x74 gate                     */
#define VA_FUSER_FINISH_SCS_OK   0x010b3d9eu  /* finish SCS_OK emit                    */
#define VA_FUSER_FINISH_EVENT    0x010b3db7u  /* finish Master_FinishLoadingCharacter  */
#define VA_FUSER_FINISH_NOTIFY   0x010b3e37u  /* finish notifier call                  */
#define VA_FUSER_FINISH_AFTER_EVENT 0x010b3e3fu /* after finish call to 0x006f1ec0    */
#define VA_EVENT_EMIT_B          0x0071bdd0u  /* state/event emitter used by finish    */
#define VA_FUSER_UI_UPDATE_HELPER 0x010b0490u /* FUser UI/state helper after event     */
#define VA_POST_STATE_EVENT_HELPER 0x006f1ec0u /* post-state event/update helper       */
#define VA_HE_HANDLER_LIST_CALL  0x006f201au /* HandleEvent: CALL FUN_006f4190       */
#define VA_HE_HANDLER_LIST_RET   0x006f201fu /* HandleEvent: after handler-list call */
#define VA_HE_HANDLER_TARGET_LOAD 0x006f2050u /* HandleEvent: EAX=[handler.vt+0x2c]  */
#define VA_HE_HANDLER_CALL       0x006f2055u /* HandleEvent: CALL handler+0x2c       */
#define VA_HE_HANDLER_RET        0x006f2057u /* HandleEvent: TEST AL after handler   */
#define VA_UIMANAGER_ATTACH      0x006b8e60u /* UIControlManager::AttachControl      */
#define VA_UIMANAGER_MOVEFRONT   0x006b9000u /* UIControlManager::MoveControlToFront */
#define VA_UIMANAGER_RENDER3DUI  0x006ba4e0u /* UIControlManager::Render3DUI         */
#define VA_UICONTROL_RENDER      0x006f07d0u /* UIControl::Render                    */
#define VA_UICONTROL_SETVISIBLE  0x006f2420u /* UIControl::SetVisible                */
#define VA_UICONTAINER_SETVISIBLE 0x006f8c80u /* UIContainer::SetVisible             */
#define VA_UICONTAINER_RENDER    0x006f91d0u /* UIContainer::Render                  */
#define VA_UICONTAINER_ATTACH    0x006f9710u /* UIContainer::AttachControl           */
#define VA_UICONTAINER_ATTACHBACK 0x006f98a0u /* UIContainer::AttachControlToBack    */
#define VA_UICONTAINER_RENDER3DUI 0x006fa580u /* UIContainer::Render3DUI             */
#define VA_UICONTAINER_RENDER3DLAYER 0x006fa7c0u /* UIContainer::Render3DLayer        */
#define VA_UICMD_EXEC_DELEGATE_CALL 0x00754dffu /* UICommandControlMethod delegate call */
#define VA_UICMD_EXEC_DELEGATE_RET  0x00754e01u /* after delegate call, AL=result       */
#define VA_UICMD_LUAFUNC_LUAEXECUTE 0x00e95340u /* UICommandLuaFunction::LuaExecute     */
#define VA_UICMD_LUAFUNC_EXECUTE    0x00e955e0u /* UICommandLuaFunction::Execute        */
#define VA_ITEM_ONLOAD_ENTRY     0x006c6410u  /* ItemFactory::OnLoadingEnd entry       */
#define VA_ITEM_ONLOAD_RECEIVER_RET 0x006c6462u /* after vtable+0x14 receiver call     */
#define VA_ITEM_ONLOAD_CAP_CALL  0x006c646fu  /* call receiver.vtable+0x34 OnLoadingEnd */
#define VA_ITEM_ONLOAD_CAP_RET   0x006c6471u  /* TEST AL after capability call         */
#define VA_ITEM_ONLOAD_EMIT_CALL 0x006c6489u  /* call FUN_00c7ca70 loading callback    */
#define VA_ITEM_ONLOAD_EMIT_RET  0x006c648eu  /* after loading callback call           */
#define VA_ITEM_ONLOAD_RETURN    0x006c64bcu  /* OnLoadingEnd return                   */
#define VA_FUSER_UPDATECHAR      0x010b2080u  /* FUser::updateCharacter                */
#define VA_CHARMGR_GETALL        0x00a5fdf0u  /* CharacterManager::getAllPlayers       */
#define VA_CHARMGR_LOADLIST      0x00a620c0u  /* CharacterManager::LoadList            */
#define VA_CHARMGR_CTOR          0x00a5e880u  /* CharacterManager::CharacterManager    */
#define VA_CHARMGR_CTOR_STORE28  0x00a5e917u  /* ctor writes native list to +0x28       */
#define VA_CHARMGR_DTOR          0x00a5e970u  /* CharacterManager::~CharacterManager   */
#define VA_CHARMGR_DTOR_ZERO28   0x00a5e9d4u  /* dtor zeros native list at +0x28        */
#define VA_ONLOAD_AFTER_LOADLIST 0x00a7ff9bu  /* after onLoadPlayers LoadList call     */
#define VA_ONLOAD_ARG3_CALL      0x00a7379cu  /* onLoadPlayers arg3 desc+0x24 CALL EDX */
#define VA_SERVER_FINISH_SEND    0x00a717a0u  /* Server_FinishLoadingCharacter sender  */
#define VA_ONLOAD_SELFLOOKUP_RET 0x00a7fe18u  /* after obj+0x18 reader in onLoad body   */
#define VA_ONLOAD_SELFVALUE_TEST 0x00a7fe2au  /* onLoad self-entry value test           */
#define VA_ONLOAD_CREATE_HASH    0x00a7ff1au  /* hash "CharacterManager" before create  */
#define VA_ONLOAD_CREATE_RET     0x00a7ff24u  /* after bb2580 CharacterManager create   */
#define VA_ONLOAD_BD2CD0_RET     0x00a7ff6du  /* after bd2cd0 CharacterManager promote  */
#define VA_ONLOAD_LISTCOPY_CALL  0x00a7ff90u  /* before copying objectList arg          */
#define VA_ONLOAD_LOADLIST_CALL  0x00a7ff96u  /* before CharacterManager::LoadList      */
#define VA_ONLOAD_FINISH_CALL    0x00a7ffb2u  /* before native finish sender            */
#define VA_LOADLIST_EMPTY_CHECK  0x00a62190u  /* LoadList: input list empty check      */
#define VA_LOADLIST_NODE_PAYLOAD 0x00a62196u  /* LoadList: node[8] object payload      */
#define VA_LOADLIST_REF_RET      0x00a621abu  /* LoadList: payload addref return       */
#define VA_LOADLIST_DUP_SCAN     0x00a621d0u  /* LoadList: existing-row duplicate scan */
#define VA_LOADLIST_DUP_RESULT   0x00a621fau  /* LoadList: duplicate scan result       */
#define VA_LOADLIST_PLAYER_DESC  0x00a6221eu  /* LoadList: Player descriptor           */
#define VA_LOADLIST_PLAYER_RESOLVE 0x00a62233u /* LoadList: resolve Player component    */
#define VA_LOADLIST_PLAYER_RET   0x00a62238u  /* LoadList: after Player resolve        */
#define VA_LOADLIST_PLAYER_TEST  0x00a6224du  /* LoadList: test resolved Player ptr    */
#define VA_LOADLIST_PLAYER_FLAGS 0x00a62320u  /* LoadList: Player deleted/active flags */
#define VA_LOADLIST_SCORE_RET    0x00a6235bu  /* LoadList: Player::IsDeleted score ret */
#define VA_LOADLIST_ACTIVE_PATH  0x00a623feu  /* LoadList: active/not-deleted path     */
#define VA_LOADLIST_OBJ_RESOLVE2 0x00a62420u  /* LoadList: second component resolve    */
#define VA_LOADLIST_OBJ_RET2     0x00a62425u  /* LoadList: after second resolve        */
#define VA_LOADLIST_TPLAYER_CALL 0x00a6266cu  /* LoadList: TPlayer lookup call ret area*/
#define VA_LOADLIST_ROW_ADD_CALL 0x00a6299au  /* LoadList: about to add row            */
#define VA_LOADLIST_ROW_ADD_RET  0x00a6299fu  /* LoadList: after row-add helper        */
#define VA_LOADLIST_LOOP_NEXT    0x00a629eau  /* LoadList: next list node / cleanup    */
#define VA_LOADLIST_EMPTY_BRANCH 0x00a62a33u  /* LoadList: empty-input cleanup branch  */
#define VA_LOADLIST_EMPTY_RELEASE_RET 0x00a62a62u /* empty branch stale row release   */
#define VA_LOADLIST_EMPTY_RELEASE_DTOR 0x00a62a70u /* empty branch release dtor      */
#define VA_LOADLIST_EMPTY_PARAM3_SCAN 0x00a62a9bu /* empty branch param3 scan        */
#define VA_LOADLIST_OLD_BEGIN    0x00a62b5cu  /* empty branch old-list cleanup begin   */
#define VA_LOADLIST_OLD_MARK_ROW 0x00a62b83u  /* old-list row mark/delete prep         */
#define VA_LOADLIST_OLD_HELPER   0x00a62ba0u  /* old-list helper before row vcall      */
#define VA_LOADLIST_OLD_FETCH_RET 0x00a62bacu /* old-list context fetch return         */
#define VA_LOADLIST_OLD_ROW_VCALL 0x00a62bd4u /* LoadList: old row virtual cleanup     */
#define VA_LOADLIST_OLD_AFTER_VCALL 0x00a62bf0u /* after old row virtual cleanup       */
#define VA_LOADLIST_EMPTY_CLEANUP_RET 0x00a62df8u /* LoadList: empty cleanup tail      */
#define VA_LOADLIST_UPDATECOUNT_CALL 0x00a62d48u /* LoadList calls UpdateCharacterCount */
#define VA_LOADLIST_UPDATECOUNT_AFTER 0x00a62d4du /* after UpdateCharacterCount call     */
#define VA_LOADLIST_BAD_HELPER_ENTRY 0x00b787a0u /* helper entered with bad param_1?   */
#define VA_LOADLIST_BAD_HELPER_DEREF 0x00b787d1u /* helper derefs [param_1+4]          */
#define VA_OBJLIST_A_PARAM_TEST  0x005f464eu  /* FUN_005f4640 after param2 load/test   */
#define VA_OBJLIST_A_OUTER_TAG_RET 0x005f4667u /* after first c7c870; EAX=outer slot  */
#define VA_OBJLIST_A_COUNT_RET   0x005f4683u  /* after c7cf60; EAX=luaH_getn count     */
#define VA_OBJLIST_A_TABLE_RET2  0x005f46b4u  /* after loop c7c870; EAX=table slot     */
#define VA_OBJLIST_A_INDEX_RET   0x005f46c1u  /* after c835d0; EAX=element Variant     */
#define VA_OBJLIST_A_NESTED_RET  0x005f470eu  /* after nested element +0x24 decode     */
#define VA_OBJLIST_A_NODE_ALLOC_RET 0x005f4722u /* after list node alloc/link helper  */
#define VA_OBJLIST_A_NODE_LINK_RET  0x005f472cu /* after list node final link helper   */
#define VA_OBJLIST_A_WRAPPER     0x005eefd0u  /* candidate-A descriptor +0x24 wrapper  */
#define VA_OBJLIST_B_WRAPPER     0x005ef990u  /* candidate-B descriptor +0x24 wrapper  */
#define VA_OBJLIST_B_ENTRY       0x005f4d40u  /* candidate-B list reader entry         */
#define VA_OBJLIST_B_PARAM_TEST  0x005f4d73u  /* B reader param2 test/JZ site          */
#define VA_VARIANT_TAG0B_ENTRY   0x00bef735u  /* Variant decoder: compare tag 0x0b     */
#define VA_VARIANT_TAG0B_DESC_RET 0x00bef753u /* after descriptor lookup; EAX=desc     */
#define VA_VARIANT_TAG0B_OBJ_RET 0x00bef77cu  /* after desc+0x2c materializer          */
#define VA_VARIANT_TAG0B_BA9200_RET 0x00bef790u /* after push-object helper           */
#define VA_VARIANT_TAG0B_NIL_OBJ 0x00bef7a4u  /* materializer produced NULL object     */
#define VA_UPDATE_AFTER_GETALL   0x010b211bu  /* after getAllPlayers vector return     */
#define VA_UPDATE_ADDCHAR        0x010b2435u  /* FUser::updateCharacter AddCharacter   */
#define VA_UPDATE_HASNOCHAR      0x010b25acu  /* FUser::updateCharacter HasNoCharacter */
#define VA_UPDATE_SELECTCHAR     0x010b26d0u  /* FUser::updateCharacter SelectCharacter*/
#define VA_FUSER_MAP_START       0x010b4860u  /* FUser::onMapStartLoading              */
#define VA_FUSER_MAP_COMPLETE    0x010b4a70u  /* FUser::onMapCompleteLoading           */
#define VA_RUNLOGLN              0x00cc9390u  /* Lua RunLogLn / RunLogLn_Service       */
#define VA_CONNECT_EMIT_A8E500   0x00a8e500u  /* Connector connect local emit wrapper  */
#define VA_CONNECT_DOWNCALL_A8E5B0 0x00a8e5b0u /* Connector DownCall connect path       */
#define VA_CONNECT_WRITER_63D2E0 0x0063d2e0u  /* generated connect upcall writer       */
#define VA_CHANNEL_OPEN          0x00b5bce0u  /* Channel::open native                  */
#define VA_CHANNEL_CHANGEFINISH  0x00b5e0e0u  /* Channel::ChangeChannelFinish native   */
#define VA_TAG1_KEY6_RECEIVER    0x00c363f0u  /* tag1 Computer key 0x06 receiver       */
#define VA_TAG1_KEY2_RECEIVER    0x00c35060u  /* tag1 Computer key 0x02 receiver       */
#define VA_TAG1_KEY6_CALLSITE    0x00c367d7u  /* tag1 CALL 0x00bee460 site             */
#define VA_TAG1_COMP_REGISTER    0x00c33cc0u  /* Computer construct+register marker    */
#define VA_TAG1_COMP_INSERT_CALL 0x00c33e06u  /* pre-CALL 00647860 object-id insert     */
#define VA_FUSER_CONNECT         0x010b0c20u  /* FUser::connect after GSS SCS_OK        */
#define VA_FCLIENTWORLD_CONNECT  0x01081e50u  /* FClientWorld::Connect master edge      */
#define VA_MASTER_ATTACH_BFE230  0x00bfe230u  /* ClientWorld master Computer attach     */
#define VA_CLIENTCOMPUTER_ATTACH 0x00c33720u  /* tag1 ClientComputer attach helper      */
#define VA_CONTROL_DESC_STORAGE  0x0183ef78u  /* DAT_0183ef78, m_ControlObj desc       */
#define VA_ADD_COMPONENT_BCF510  0x00bcf510u  /* base NObject AddComponent handler      */
#define VA_LOGINRESULT_DOWNCALL  0x00a6ec40u  /* Session DownCall logInServerResult     */
#define VA_LOGINRESULT_A81020    0x00a81020u  /* logInServerResult inner logic          */
#define VA_LOGIN_DOWNCALL        0x00a6d6b0u  /* Session DownCall logIn                  */
#define VA_LOGINMASTER_DOWNCALL  0x00a6e1e0u  /* Session DownCall logInMaster            */
#define VA_LOGIN_NO_WORLD        0x00a81106u  /* string site: No World                   */
#define VA_LOGIN_NO_OBJECT       0x00a8118cu  /* string site: No Object for OID          */
#define VA_LOGIN_NO_CLIENTCOMP   0x00a8121cu  /* string site: No ClientComputer          */
#define VA_LOGIN_NO_COM_PLAYER   0x00a8130bu  /* throw marker: No COM: Player            */
#define VA_LOGIN_NO_COM_CSTATUS  0x00a81399u  /* throw marker: No COM: CStatusPlayer     */
#define VA_LOGIN_NO_COM_USER     0x00a8142eu  /* throw marker: No COM: User              */
#define VA_ONLOADPLAYERS_WRAP    0x00a736e0u  /* Session onLoadPlayersCacheResult wrapper*/
#define VA_ONLOADPLAYERS_INNER   0x00a7fd80u  /* Session onLoadPlayersCacheResult logic  */
#define VA_ONLOADPLAYERDATA_RECV 0x00a76440u  /* generated OnLoadPlayerData receiver     */
#define VA_ONLOADPLAYERDATA_INNER 0x00a80f30u /* Session::OnLoadPlayerData               */
#define VA_ONLOADPLAYERDATA_EMIT 0x00a6e740u  /* builds logInServerResult emitter         */
#define VA_ONLOAD_BRIDGE_A87B90  0x00a87b90u  /* logInServerResult outbound writer        */
#define VA_ONLOAD_BRIDGE_NO_OWNER_JZ 0x00a87bf5u /* param1+0x14 null early-out            */
#define VA_ONLOAD_BRIDGE_NO_BUILDER_JZ 0x00a87c1eu /* packet builder null early-out        */
#define VA_ONLOAD_BRIDGE_ENQUEUE_CALL 0x00a87ce7u /* CALL 0x00bbfe80 emit packet          */
#define VA_POST_WRITER_ENQUEUE_A 0x00bbfe80u  /* common post-writer enqueue helper A       */
#define VA_POST_WRITER_QUEUE     0x00bc28c0u  /* common post-writer queue push             */
#define VA_DRAIN_POP_CALL        0x00bc0cddu  /* worker CALL 0x009302b0 with node args      */
#define VA_DRAIN_SEND_ENTRY      0x009302b0u  /* FUN_009302b0 per-node send gate entry      */
#define VA_DRAIN_TARGET_BOOL_RET 0x009302d2u  /* after target.vtable+4 bool call            */
#define VA_DRAIN_SENDHELPER_RET  0x009302deu  /* after FUN_0115f970 bool return             */
#define VA_SEND_LAYER_HELPER     0x0115f970u  /* FUN_0115f970 actual send-layer helper      */
#define VA_DRAIN_TEARDOWN        0x00bc15a0u  /* FUN_00bc15a0 sets drain shutdown flag      */
#define VA_DRAIN_FLAG            0x01825dacu  /* DAT_01825dac drain shutdown flag           */
#define VA_DRAIN_WAKE_HANDLE     0x018165c4u  /* DAT_018165c4 queue wake event handle       */
#define VA_NET_REPORT_HELPER     0x00ca4070u  /* NetComputer report helper -> shutdown      */
#define VA_SESSION_SHUTDOWN      0x0115de60u  /* session/layer shutdown helper              */
#define VA_SOCKET_CLOSE_HELPER   0x011672e0u  /* socket close helper -> ws2_32!closesocket  */
#define VA_FCLIENTAPP_FINALIZE   0x01036c50u  /* FClientApp::Terminate / OnFinalize         */
#define VA_PROACTOR_DISPATCH_CALL 0x0115b88fu /* Proactor packet handler CALL EAX           */
#define VA_PROACTOR_EXCEPTION_BUILD 0x0115ba48u /* packet-handler exception landing          */
#define VA_PROACTOR_EXCEPTION_TEXT_RET 0x0115ba77u /* EAX=exception text before report       */
#define VA_PROACTOR_REPORT_CALL  0x0115baa6u  /* CALL 0x00ca4070 from Proactor.cpp:0x3f7    */
#define VA_B4_PLAYER_DESC_BASE   0x01838820u  /* Player descriptor base from s17             */
#define VA_B4_PLAYER_DESC_KEY    0x01838838u  /* Player self-key descriptor/global           */
#define VA_B4_CSTATUS_DESC_BASE  0x0182d394u  /* CStatusPlayer descriptor base from s17      */
#define VA_B4_CSTATUS_DESC_KEY   0x0182d3a8u  /* CStatusPlayer self-key descriptor/global    */
#define VA_B4_USER_DESC_BASE     0x01838dc0u  /* User descriptor base from s17               */
#define VA_B4_USER_DESC_KEY      0x01838dd8u  /* User self-key descriptor/global             */
#define VA_SESSION_DESC_BASE     0x01838c50u  /* Session descriptor helper base (s20260619)  */
#define VA_SESSION_DESC_KEY      0x01838c68u  /* Session component descriptor key candidate  */
#define VA_SESSION_SELF_DESC     0x01838b90u  /* Session login self-desc consumed by RPCs    */
#define VA_SESSION_MASTEROPEN    0x00a7f120u  /* Session::masterOpen; initializes +0xe0      */
#define VA_SESSION_MASTERREOPEN  0x00a7f360u  /* Session::masterReopen; consumes +0xe0        */
#define VA_SESSION_E0_STORE_HELPER 0x00a8d1d0u /* slot materializer used by masterOpen       */
#define VA_CHANNELCTX_DESC_BASE  0x0183dcf8u  /* ChannelContext helper base (s20260619)      */
#define VA_CHANNELCTX_DESC_KEY   0x0183dd10u  /* ChannelContext descriptor key candidate     */
#define VA_KEY6_CALLMODE_RESULT  0x00bee4b1u  /* after stream vtable+0x3c call-mode read */
#define VA_RPC_SCOPE_RET         0x00bee5aeu  /* after CALL 0x00bf0e30, TEST AL,AL */
#define VA_RPC_SCOPE_JZ          0x00bee5b0u  /* JZ 0x00bee856, P1 target */
#define VA_RPC_BC5860_RET        0x00bee632u  /* after CALL 0x00bc5860; EAX=std::string */
#define VA_RPC_BA9640_CALL       0x00bee642u  /* CALL 0x00ba9640 */
#define VA_RPC_BA9640_JZ         0x00bee649u  /* JZ after ba9640 result */
#define VA_RPC_INVALID_FORMAT    0x00bee69fu  /* push "Invalid Network Call Format"      */
#define VA_RPC_ARGC_WRONG        0x00bee6d4u  /* push "CallNetFunction num args wrong"   */
#define VA_RPC_LUA_CALLSITE      0x00bee72eu  /* CALL 0x00cbda80 from key6 executor      */
#define VA_RPC_LUA_RET           0x00bee733u  /* after CALL 0x00cbda80                   */
#define VA_GOLUA_CREATEOBJECT    0x00c48010u  /* goLua_CreateObject native wrapper       */
#define VA_GOLUA_ADDCOMP         0x00c483d0u  /* goLua_AddComponent native wrapper       */
#define VA_GOLUA_ADDCOMP_BD2CD0  0x00c484b1u  /* callsite to FUN_00bd2cd0 in wrapper     */
#define VA_GOLUA_REGISTOBJECT    0x00c48520u  /* goLua_RegistObject native wrapper       */
#define VA_GOLUA_UNPACKFIXED     0x00bb8220u  /* goLua_UnpackFixed native wrapper         */
#define VA_LUA_DISPATCH_CBDA80   0x00cbda80u  /* generic Lua call-vector dispatcher      */
#define VA_LUA_PCALL_CBD750      0x00cbd750u  /* protected Lua pcall/status helper       */
#define VA_LUA_PCALL_RET_CBDAD2  0x00cbdad2u  /* cbda80 after cbd750, EBX=status         */
#define VA_LUA_ERROR_UNKNOWN     0x00cbdb49u  /* fallback "unknown" Lua error string     */
#define VA_LUA_ERROR_MSGBOX      0x00cbdb77u  /* _dev_lua_error_msgbox path              */
#define VA_SIGNINCLIENT_DIRECT   0x00a6b360u  /* signInClientResult direct handler       */
#define VA_SIGNINCLIENT_FORWARD  0x00a6b430u  /* signInClientResult native/Lua forwarder */
#define VA_SIGNINCLIENT_BASE     0x00a6b4f0u  /* signInClientResult base wrapper         */
#define VA_SIGNINCLIENT_UP       0x00a6b630u  /* UpCallsignInClientResult handler        */
#define VA_SIGNINCLIENT_JUMPUP   0x00a6b770u  /* JumpUpCallsignInClientResult handler    */
#define VA_SIGNINCLIENT_DOWN     0x00a6b8a0u  /* DownCallsignInClientResult handler      */
#define VA_SIGNINCLIENT_GHOST    0x00a6b9f0u  /* GhostCallsignInClientResult handler     */
#define VA_SIGNINCLIENT_JUMPGHOST 0x00a6bb20u /* JumpGhostCallsignInClientResult handler */
#define VA_SIGNINCLIENT_GENERATED 0x00a6bbb0u /* generated signInClientResult handler    */
#define VA_LOGINCLIENT_LOGIC     0x00a6ed50u  /* Session::logInClientResult logic        */
#define VA_LOGINCLIENT_HANDLER_A 0x00a6ee20u  /* caller/wrapper of logInClientResult      */
#define VA_LOGINCLIENT_HANDLER_B 0x00a6f6f0u  /* caller/wrapper of logInClientResult      */
#define KEY_CONNECTOR_NID        0x7d1fec2eu  /* native_nid("Connector")              */
#define KEY_TCLIENT_NID          0x61caee6du  /* observed TClient class id/NID         */
#define KEY_TACCOUNT_NID         0x5c68f407u  /* observed TAccount class id/NID        */
#define KEY_SESSION_NID          0xdb2c74e0u  /* native_nid("Session")                 */
#define KEY_PLAYER_NID           0x5d2c8910u  /* native_nid("Player")                  */
#define KEY_CSTATUSPLAYER_NID    0x5833cd58u  /* native_nid("CStatusPlayer")           */
#define KEY_USER_NID             0xcb16687bu  /* native_nid("User")                    */
#define KEY_CHARMGR_NID          0x6c12c692u  /* observed native_nid("CharacterManager") */

#if EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
static volatile DWORD g_last_charmgr = 0;
static volatile DWORD g_last_charmgr_owner = 0;
#if EXP_TARGET_PROFILE == 69 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
static volatile DWORD g_last_slot7_comp = 0;
static volatile DWORD g_last_slot7_key = 0;
static volatile LONG g_loadlist_exception_logged = 0;
static volatile LONG g_objptr_resolve_active = 0;
static volatile DWORD g_objptr_last_scalar = 0;
static volatile DWORD g_objptr_last_newobj = 0;
static volatile DWORD g_objptr_last_newobj_pid = 0;
#endif
#endif

#if EXP_TARGET_PROFILE == 1
static const DWORD g_targets[] = {
    VA_KEY6_EXECUTOR,
    VA_SETCTRL_HANDLER,
    VA_FUSER_FINISH,
};
#define VA_READY_GATE VA_KEY6_EXECUTOR
#define EXP_INSTALL_DELAY_MS 0
#elif EXP_TARGET_PROFILE == 2
static const DWORD g_targets[] = {
    VA_KEY02_THUNK,
    VA_KEY03_NEWOBJECT,
    VA_KEY6_THUNK,
    VA_KEY6_RECEIVER,
    VA_KEY6_EXECUTOR,
};
#define VA_READY_GATE VA_KEY6_EXECUTOR
#define EXP_INSTALL_DELAY_MS 0
#elif EXP_TARGET_PROFILE == 3
static const DWORD g_targets[] = {
    VA_ZLIB_INFLATE,
    VA_LOWER_VALIDITY,
    VA_QUEUE_PRODUCER,
    VA_QUEUE_CONSUMER,
    VA_SESSION_DISPATCH,
    VA_KEY02_THUNK,
};
#define VA_READY_GATE VA_ZLIB_INFLATE
#define EXP_INSTALL_DELAY_MS 13000
#elif EXP_TARGET_PROFILE == 4
static const DWORD g_targets[] = {
    VA_KEY6_EXECUTOR,
    VA_SETCTRL_HANDLER,
    VA_FUSER_FINISH,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#elif EXP_TARGET_PROFILE == 5
static const DWORD g_targets[] = {
    VA_KEY6_RECEIVER,
    VA_KEY6_TARGET_CMP,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#elif EXP_TARGET_PROFILE == 6
static const DWORD g_targets[] = {
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,   /* dump manager+0x1c method map here */
    VA_KEY6_EXECUTOR,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_METHOD_MAP 1
#elif EXP_TARGET_PROFILE == 7
static const DWORD g_targets[] = {
    VA_KEY6_LOOKUP_NULL_JZ,   /* re-confirm object resolves (EAX!=0)   */
    VA_KEY6_SKIP_EXEC_JZ,     /* 0x00c00a85: log ESI + dump self map   */
    VA_KEY6_EXECUTOR,         /* does it reach now?                    */
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#elif EXP_TARGET_PROFILE == 8
static const DWORD g_targets[] = {
    VA_NEWOBJ_COMP_INIT,   /* component builder entry: obj + component-list */
    VA_COMP_NULL_TEST,     /* 0x00bd4c40: is the materialized component null? */
    VA_COMP_MAP_WRITE,     /* 0x00bd4cf4: did the self-map write fire?         */
    VA_COMP_SLOT7_RET,     /* 0x00bd4d38: did ServerCom slot-7 return (no throw)? */
    VA_COMP_LOOP_RECHECK,  /* 0x00bd4d75: cursor after ServerCom slot-7 read   */
    VA_KEY6_SKIP_EXEC_JZ,  /* downstream: self gate still null?                */
    VA_KEY6_EXECUTOR,      /* downstream: executor reached?                    */
};
#define VA_READY_GATE VA_NEWOBJ_COMP_INIT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_COMPONENT_PATH 1
#elif EXP_TARGET_PROFILE == 9
static const DWORD g_targets[] = {
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_OBJ18_REG_BD3AE0,
    VA_OBJ18_WRITE_BD3B15,
    VA_OBJ18_REG_BD9C50,
    VA_OBJ18_WRITE_BD9D5F,
    VA_TCLIENT_POP_C5A880,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXECUTOR,
    VA_SETCTRL_HANDLER,
    VA_FUSER_FINISH,
};
#define VA_READY_GATE VA_OBJ18_REG_BD2CD0
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 320
#elif EXP_TARGET_PROFILE == 10
static const DWORD g_targets[] = {
    VA_CONN_CONNECT_BASE,
    VA_CONN_OPEN_IMPL,
    VA_CONN_CONNECT_IMPL,
    VA_CONN_CONNECT_EMIT,
    VA_CONN_CHALLENGE_BASE,
    VA_CONN_RESPONSE_BASE,
    VA_CONN_RESPONSE_IMPL,
    VA_CONN_ARG_READER,
    VA_EVAL_MASTERHASH_MARK,
    VA_PM_CREATEOBJECT,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXECUTOR,
};
#define VA_READY_GATE VA_CONN_CONNECT_BASE
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_CONNECTOR_HANDSHAKE 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 240
#elif EXP_TARGET_PROFILE == 11
static const DWORD g_targets[] = {
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_SETCTRL_HANDLER,
    VA_SETCTRL_SIGNAL_HELPER,
    VA_ONSIG_SETCONTROL,
    VA_FUSER_ONCONNECTOR,
    VA_CONNECT_EMIT_A8E500,
    VA_CONNECT_WRITER_63D2E0,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 260
#elif EXP_TARGET_PROFILE == 12
static const DWORD g_targets[] = {
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_SETCTRL_HANDLER,
    VA_SETCTRL_SIGNAL_HELPER,
    VA_ONSIG_SETCONTROL,
    VA_FUSER_ONCONNECTOR,
    VA_CONNECT_EMIT_A8E500,
    VA_CONNECT_WRITER_63D2E0,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_INJECT_CONTROL_OBJ18 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 320
#elif EXP_TARGET_PROFILE == 13
static const DWORD g_targets[] = {
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_SETCTRL_HANDLER,
    VA_SETCTRL_SIGNAL_HELPER,
    VA_ONSIG_SETCONTROL,
    VA_FUSER_ONCONNECTOR,
    VA_CONNECT_EMIT_A8E500,
    VA_CONNECT_WRITER_63D2E0,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_INJECT_CONTROL_OBJ18 1
#define EXP_INSERT_CONTROL_DIRECT 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 360
#elif EXP_TARGET_PROFILE == 14
static const DWORD g_targets[] = {
    VA_CHANNEL_OPEN,
    VA_CHANNEL_CHANGEFINISH,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_SETCTRL_HANDLER,
    VA_SETCTRL_SIGNAL_HELPER,
    VA_ONSIG_SETCONTROL,
    VA_FUSER_ONCONNECTOR,
    VA_CONNECT_EMIT_A8E500,
    VA_CONNECT_WRITER_63D2E0,
};
#define VA_READY_GATE VA_CHANNEL_OPEN
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 360
#elif EXP_TARGET_PROFILE == 15
static const DWORD g_targets[] = {
    VA_KEY6_RECEIVER,
    VA_TAG1_KEY6_RECEIVER,
    VA_TAG1_KEY2_RECEIVER,
    VA_KEY6_EXEC_CALLSITE,
    VA_TAG1_KEY6_CALLSITE,
    VA_KEY6_EXECUTOR,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 260
#elif EXP_TARGET_PROFILE == 16
static const DWORD g_targets[] = {
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_OBJ18_REG_BD3AE0,
    VA_OBJ18_WRITE_BD3B15,
    VA_OBJ18_REG_BD9C50,
    VA_OBJ18_WRITE_BD9D5F,
    VA_TAG1_COMP_REGISTER,
    VA_TAG1_COMP_INSERT_CALL,
    VA_KEY6_RECEIVER,
    VA_TAG1_KEY6_RECEIVER,
    VA_TAG1_KEY2_RECEIVER,
    VA_KEY6_EXECUTOR,
};
#define VA_READY_GATE VA_OBJ18_REG_BD2CD0
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_WRITER_CENSUS 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 900
#elif EXP_TARGET_PROFILE == 17
static const DWORD g_targets[] = {
    VA_FUSER_CONNECT,
    VA_FCLIENTWORLD_CONNECT,
    VA_MASTER_ATTACH_BFE230,
    VA_CLIENTCOMPUTER_ATTACH,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 120
#elif EXP_TARGET_PROFILE == 18
static const DWORD g_targets[] = {
    VA_ONSIG_SETCONTROL,
    VA_FUSER_ONCONNECTOR,
    VA_CONNECT_EMIT_A8E500,
    VA_CONNECT_DOWNCALL_A8E5B0,
};
#define VA_READY_GATE VA_ONSIG_SETCONTROL
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 120
#elif EXP_TARGET_PROFILE == 19
static const DWORD g_targets[] = {
    VA_ADD_COMPONENT_BCF510,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_LOGINRESULT_DOWNCALL,
    VA_LOGINRESULT_A81020,
    VA_LOGIN_NO_WORLD,
    VA_LOGIN_NO_OBJECT,
    VA_LOGIN_NO_CLIENTCOMP,
    VA_LOGIN_NO_COM_PLAYER,
    VA_LOGIN_NO_COM_CSTATUS,
    VA_LOGIN_NO_COM_USER,
    VA_KEY6_RECEIVER,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
};
#define VA_READY_GATE VA_OBJ18_REG_BD2CD0
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_WRITER_CENSUS 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 900
#elif EXP_TARGET_PROFILE == 20
static const DWORD g_targets[] = {
    VA_KEY6_RECEIVER,
    VA_KEY6_TARGET_CMP,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_LOGIN_DOWNCALL,
    VA_LOGINMASTER_DOWNCALL,
    VA_LOGINRESULT_DOWNCALL,
    VA_LOGINRESULT_A81020,
    VA_LOGIN_NO_WORLD,
    VA_LOGIN_NO_OBJECT,
    VA_LOGIN_NO_CLIENTCOMP,
    VA_LOGIN_NO_COM_PLAYER,
    VA_LOGIN_NO_COM_CSTATUS,
    VA_LOGIN_NO_COM_USER,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 1400
#elif EXP_TARGET_PROFILE == 21
static const DWORD g_targets[] = {
    VA_KEY6_RECEIVER,
    VA_KEY6_TARGET_CMP,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 900
#elif EXP_TARGET_PROFILE == 22
static const DWORD g_targets[] = {
    VA_KEY6_RECEIVER,
    VA_KEY6_TARGET_CMP,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
    VA_LOGIN_DOWNCALL,
    VA_LOGINMASTER_DOWNCALL,
    VA_LOGINRESULT_DOWNCALL,
    VA_LOGINRESULT_A81020,
    VA_LOGIN_NO_WORLD,
    VA_LOGIN_NO_OBJECT,
    VA_LOGIN_NO_CLIENTCOMP,
    VA_LOGIN_NO_COM_PLAYER,
    VA_LOGIN_NO_COM_CSTATUS,
    VA_LOGIN_NO_COM_USER,
    VA_OBJ18_REG_BD3AE0,
    VA_OBJ18_WRITE_BD3B15,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 1600
#elif EXP_TARGET_PROFILE == 23
static const DWORD g_targets[] = {
    VA_KEY6_RECEIVER,
    VA_KEY6_TARGET_CMP,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_RPC_INVALID_FORMAT,
    VA_RPC_ARGC_WRONG,
    VA_ADD_COMPONENT_BCF510,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 1400
#elif EXP_TARGET_PROFILE == 24
static const DWORD g_targets[] = {
    VA_KEY6_RECEIVER,
    VA_KEY6_TARGET_CMP,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
    VA_RPC_INVALID_FORMAT,
    VA_RPC_ARGC_WRONG,
    VA_ADD_COMPONENT_BCF510,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_DUMP_CALLMODE 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 1600
#elif EXP_TARGET_PROFILE == 25 || EXP_TARGET_PROFILE == 26 || EXP_TARGET_PROFILE == 27 || EXP_TARGET_PROFILE == 28 || EXP_TARGET_PROFILE == 29
static const DWORD g_targets[] = {
    VA_KEY6_RECEIVER,
    VA_KEY6_TARGET_CMP,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
    VA_RPC_SCOPE_RET,
#if EXP_TARGET_PROFILE == 29
    VA_RPC_BC5860_RET,
#endif
    VA_RPC_BA9640_CALL,
    VA_RPC_BA9640_JZ,
    VA_RPC_INVALID_FORMAT,
    VA_RPC_ARGC_WRONG,
#if EXP_TARGET_PROFILE == 27 || EXP_TARGET_PROFILE == 28 || EXP_TARGET_PROFILE == 29
    VA_GOLUA_ADDCOMP,
    VA_GOLUA_ADDCOMP_BD2CD0,
#endif
    VA_ADD_COMPONENT_BCF510,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_OBJ18_REG_BD3AE0,
    VA_OBJ18_WRITE_BD3B15,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
    VA_LOGIN_DOWNCALL,
    VA_LOGINMASTER_DOWNCALL,
    VA_LOGINRESULT_DOWNCALL,
    VA_LOGINRESULT_A81020,
    VA_LOGIN_NO_WORLD,
    VA_LOGIN_NO_OBJECT,
    VA_LOGIN_NO_CLIENTCOMP,
    VA_LOGIN_NO_COM_PLAYER,
    VA_LOGIN_NO_COM_CSTATUS,
    VA_LOGIN_NO_COM_USER,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_EXECUTOR_POSTMODE 1
#define EXP_FORCE_BF0E30_SCOPE_GATE 1
#if EXP_TARGET_PROFILE == 27 || EXP_TARGET_PROFILE == 28 || EXP_TARGET_PROFILE == 29
#define EXP_DUMP_GOLUA_ADDCOMP 1
#endif
#if EXP_TARGET_PROFILE == 29
#define EXP_DUMP_LUA_CALLABLE_NAME 1
#endif
#if EXP_TARGET_PROFILE == 26 || EXP_TARGET_PROFILE == 28
#define EXP_FORCE_BA9640_GATE 1
#endif
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 2600
#elif EXP_TARGET_PROFILE == 30
static const DWORD g_targets[] = {
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_OBJ18_REG_BD3AE0,
    VA_OBJ18_WRITE_BD3B15,
    VA_OBJ18_REG_BD9C50,
    VA_OBJ18_WRITE_BD9D5F,
    VA_GOLUA_CREATEOBJECT,
    VA_GOLUA_ADDCOMP,
    VA_GOLUA_ADDCOMP_BD2CD0,
    VA_GOLUA_REGISTOBJECT,
    VA_SIGNINCLIENT_DIRECT,
    VA_SIGNINCLIENT_FORWARD,
    VA_SIGNINCLIENT_BASE,
    VA_SIGNINCLIENT_UP,
    VA_SIGNINCLIENT_JUMPUP,
    VA_SIGNINCLIENT_DOWN,
    VA_SIGNINCLIENT_GHOST,
    VA_SIGNINCLIENT_JUMPGHOST,
    VA_SIGNINCLIENT_GENERATED,
    VA_KEY6_RECEIVER,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXECUTOR,
};
#define VA_READY_GATE VA_OBJ18_REG_BD2CD0
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_WRITER_CENSUS 1
#define EXP_DUMP_GOLUA_BUILDERS 1
#define EXP_DUMP_SIGNINCLIENTRESULT 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 3600
#elif EXP_TARGET_PROFILE == 31
static const DWORD g_targets[] = {
    VA_KEY03_NEWOBJECT,
    VA_NEWOBJ_COMP_INIT,
    VA_COMP_MATERIALIZE,
    VA_COMP_NULL_TEST,
    VA_COMP_MAP_WRITE,
    VA_COMP_SLOT7_RET,
    VA_COMP_LOOP_RECHECK,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_OBJ18_REG_BD3AE0,
    VA_OBJ18_WRITE_BD3B15,
    VA_OBJ18_REG_BD9C50,
    VA_OBJ18_WRITE_BD9D5F,
    VA_KEY6_RECEIVER,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXECUTOR,
};
#define VA_READY_GATE VA_KEY03_NEWOBJECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_COMPONENT_PATH 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_WRITER_CENSUS 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 4200
#elif EXP_TARGET_PROFILE == 32
static const DWORD g_targets[] = {
    VA_KEY03_NEWOBJECT,
    VA_NEWOBJ_COMP_INIT,
    VA_COMP_MATERIALIZE,
    VA_COMP_MAP_WRITE,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_OBJ18_REG_BD3AE0,
    VA_OBJ18_WRITE_BD3B15,
    VA_OBJ18_REG_BD9C50,
    VA_OBJ18_WRITE_BD9D5F,
    VA_TCLIENT_POP_C5A880,
    VA_SETCTRL_SIGNAL_HELPER,
    VA_ONSIG_SETCONTROL,
    VA_GOLUA_ADDCOMP,
    VA_GOLUA_ADDCOMP_BD2CD0,
    VA_SIGNINCLIENT_DIRECT,
    VA_SIGNINCLIENT_FORWARD,
    VA_SIGNINCLIENT_BASE,
    VA_SIGNINCLIENT_UP,
    VA_SIGNINCLIENT_JUMPUP,
    VA_SIGNINCLIENT_DOWN,
    VA_SIGNINCLIENT_GHOST,
    VA_SIGNINCLIENT_JUMPGHOST,
    VA_SIGNINCLIENT_GENERATED,
    VA_LOGINCLIENT_LOGIC,
    VA_LOGINCLIENT_HANDLER_A,
    VA_LOGINCLIENT_HANDLER_B,
    VA_KEY6_RECEIVER,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXECUTOR,
};
#define VA_READY_GATE VA_KEY03_NEWOBJECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_COMPONENT_PATH 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_WRITER_CENSUS 1
#define EXP_DUMP_GOLUA_BUILDERS 1
#define EXP_DUMP_SIGNINCLIENTRESULT 1
#define EXP_DUMP_LOGINCLIENTRESULT 1
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 5200
#elif EXP_TARGET_PROFILE == 33
static const DWORD g_targets[] = {
    VA_KEY03_NEWOBJECT,
    VA_NEWOBJ_COMP_INIT,
    VA_COMP_MATERIALIZE,
    VA_COMP_MAP_WRITE,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_OBJ18_REG_BD3AE0,
    VA_OBJ18_WRITE_BD3B15,
    VA_OBJ18_REG_BD9C50,
    VA_OBJ18_WRITE_BD9D5F,
    VA_GOLUA_ADDCOMP,
    VA_GOLUA_ADDCOMP_BD2CD0,
    VA_SIGNINCLIENT_DIRECT,
    VA_SIGNINCLIENT_FORWARD,
    VA_SIGNINCLIENT_BASE,
    VA_SIGNINCLIENT_UP,
    VA_SIGNINCLIENT_JUMPUP,
    VA_SIGNINCLIENT_DOWN,
    VA_SIGNINCLIENT_GHOST,
    VA_SIGNINCLIENT_JUMPGHOST,
    VA_SIGNINCLIENT_GENERATED,
    VA_LOGINCLIENT_LOGIC,
    VA_LOGINCLIENT_HANDLER_A,
    VA_LOGINCLIENT_HANDLER_B,
    VA_LOGIN_DOWNCALL,
    VA_LOGINMASTER_DOWNCALL,
    VA_LOGINRESULT_DOWNCALL,
    VA_LOGINRESULT_A81020,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
    VA_RPC_SCOPE_RET,
    VA_RPC_SCOPE_JZ,
    VA_RPC_BC5860_RET,
    VA_RPC_BA9640_CALL,
    VA_RPC_BA9640_JZ,
    VA_RPC_INVALID_FORMAT,
    VA_RPC_ARGC_WRONG,
};
#define VA_READY_GATE VA_KEY03_NEWOBJECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_COMPONENT_PATH 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_WRITER_CENSUS 1
#define EXP_DUMP_GOLUA_BUILDERS 1
#define EXP_DUMP_SIGNINCLIENTRESULT 1
#define EXP_DUMP_LOGINCLIENTRESULT 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_EXECUTOR_POSTMODE 1
#define EXP_DUMP_LUA_CALLABLE_NAME 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 7600
#elif EXP_TARGET_PROFILE == 34
static const DWORD g_targets[] = {
    VA_KEY03_NEWOBJECT,
    VA_NEWOBJ_COMP_INIT,
    VA_COMP_MATERIALIZE,
    VA_COMP_MAP_WRITE,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_OBJ18_REG_BD3AE0,
    VA_OBJ18_WRITE_BD3B15,
    VA_OBJ18_REG_BD9C50,
    VA_OBJ18_WRITE_BD9D5F,
    VA_SIGNINCLIENT_DIRECT,
    VA_SIGNINCLIENT_FORWARD,
    VA_SIGNINCLIENT_BASE,
    VA_SIGNINCLIENT_UP,
    VA_SIGNINCLIENT_JUMPUP,
    VA_SIGNINCLIENT_DOWN,
    VA_SIGNINCLIENT_GHOST,
    VA_SIGNINCLIENT_JUMPGHOST,
    VA_SIGNINCLIENT_GENERATED,
    VA_LOGINCLIENT_LOGIC,
    VA_LOGINCLIENT_HANDLER_A,
    VA_LOGINCLIENT_HANDLER_B,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
    VA_RPC_SCOPE_RET,
    VA_RPC_SCOPE_JZ,
    VA_RPC_BC5860_RET,
    VA_RPC_BA9640_CALL,
    VA_RPC_BA9640_JZ,
    VA_RPC_INVALID_FORMAT,
    VA_RPC_ARGC_WRONG,
    VA_RPC_LUA_CALLSITE,
    VA_RPC_LUA_RET,
    VA_LUA_DISPATCH_CBDA80,
    VA_LUA_PCALL_CBD750,
    VA_LUA_PCALL_RET_CBDAD2,
    VA_LUA_ERROR_UNKNOWN,
    VA_LUA_ERROR_MSGBOX,
};
#define VA_READY_GATE VA_KEY03_NEWOBJECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_COMPONENT_PATH 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_WRITER_CENSUS 1
#define EXP_DUMP_SIGNINCLIENTRESULT 1
#define EXP_DUMP_LOGINCLIENTRESULT 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_EXECUTOR_POSTMODE 1
#define EXP_DUMP_LUA_CALLABLE_NAME 1
#define EXP_DUMP_LUA_DISPATCH 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 9000
#elif EXP_TARGET_PROFILE == 35
static const DWORD g_targets[] = {
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_OBJ18_REG_BD3AE0,
    VA_OBJ18_WRITE_BD3B15,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
    VA_RPC_SCOPE_RET,
    VA_RPC_SCOPE_JZ,
    VA_RPC_BC5860_RET,
    VA_RPC_BA9640_CALL,
    VA_RPC_BA9640_JZ,
    VA_RPC_INVALID_FORMAT,
    VA_RPC_ARGC_WRONG,
    VA_RPC_LUA_CALLSITE,
    VA_RPC_LUA_RET,
    VA_LUA_ERROR_UNKNOWN,
    VA_LUA_ERROR_MSGBOX,
    VA_SIGNINCLIENT_DIRECT,
    VA_SIGNINCLIENT_FORWARD,
    VA_SIGNINCLIENT_BASE,
    VA_SIGNINCLIENT_UP,
    VA_SIGNINCLIENT_JUMPUP,
    VA_SIGNINCLIENT_DOWN,
    VA_SIGNINCLIENT_GHOST,
    VA_SIGNINCLIENT_JUMPGHOST,
    VA_SIGNINCLIENT_GENERATED,
    VA_LOGINCLIENT_LOGIC,
    VA_LOGINCLIENT_HANDLER_A,
    VA_LOGINCLIENT_HANDLER_B,
};
#define VA_READY_GATE VA_KEY6_RECEIVER
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_SIGNINCLIENTRESULT 1
#define EXP_DUMP_LOGINCLIENTRESULT 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_EXECUTOR_POSTMODE 1
#define EXP_DUMP_LUA_CALLABLE_NAME 1
#define EXP_DUMP_LUA_DISPATCH 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 3600
#elif EXP_TARGET_PROFILE == 36
static const DWORD g_targets[] = {
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
    VA_RPC_BC5860_RET,
    VA_RPC_BA9640_JZ,
    VA_RPC_LUA_CALLSITE,
    VA_RPC_LUA_RET,
    VA_LUA_ERROR_UNKNOWN,
    VA_LUA_ERROR_MSGBOX,
    VA_FUSER_CONNECT,
    VA_FUSER_STATE1_WRITE_8C,
    VA_FUSER_STATE1_WRITE_74,
    VA_FUSER_ONCONNECTOR,
    VA_FUSER_ONSIGNIN,
    VA_FUSER_SIGNIN_74_CMP,
    VA_FUSER_SIGNIN_SCS_OK,
    VA_FUSER_SIGNIN_EVENT,
    VA_FUSER_SIGNIN_NOTIFY,
    VA_FUSER_ONLOGIN,
    VA_FUSER_LOGIN_SCS_OK,
    VA_FUSER_LOGIN_EVENT,
    VA_FUSER_ONSESSION,
    VA_FUSER_ONSESSION_RESOLVE,
    VA_FUSER_ONSESSION_STATE_CMP,
    VA_FUSER_ONSESSION_74_CMP,
    VA_FUSER_ONSESSION_SCS_OK,
    VA_FUSER_ONSESSION_EVENT,
    VA_FUSER_ONSESSION_NOTIFY,
    VA_FUSER_ONSESSION_STATE5_WRITE,
    VA_FUSER_FINISH,
    VA_FUSER_FINISH_RESULT_CMP,
    VA_FUSER_FINISH_74_CMP,
    VA_FUSER_FINISH_SCS_OK,
    VA_FUSER_FINISH_EVENT,
    VA_FUSER_FINISH_NOTIFY,
    VA_SETCTRL_HANDLER,
    VA_ONSIG_SETCONTROL,
    VA_FCLIENTWORLD_CONNECT,
    VA_MASTER_ATTACH_BFE230,
    VA_CONNECT_EMIT_A8E500,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_EXECUTOR_POSTMODE 1
#define EXP_DUMP_LUA_CALLABLE_NAME 1
#define EXP_DUMP_LUA_DISPATCH 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_DUMP_FUSER_STATE 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 5200
#elif EXP_TARGET_PROFILE == 37
static const DWORD g_targets[] = {
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
    VA_RPC_BC5860_RET,
    VA_RPC_BA9640_JZ,
    VA_RPC_LUA_CALLSITE,
    VA_RPC_LUA_RET,
    VA_LUA_ERROR_UNKNOWN,
    VA_LUA_ERROR_MSGBOX,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
    VA_FUSER_CONNECT,
    VA_FUSER_STATE1_WRITE_8C,
    VA_FUSER_STATE1_WRITE_74,
    VA_FUSER_ONCONNECTOR,
    VA_FUSER_ONSIGNIN,
    VA_FUSER_SIGNIN_74_CMP,
    VA_FUSER_SIGNIN_SCS_OK,
    VA_FUSER_SIGNIN_EVENT,
    VA_FUSER_SIGNIN_NOTIFY,
    VA_FUSER_ONLOGIN,
    VA_FUSER_LOGIN_SCS_OK,
    VA_FUSER_LOGIN_EVENT,
    VA_FUSER_ONSESSION,
    VA_FUSER_ONSESSION_RESOLVE,
    VA_FUSER_ONSESSION_STATE_CMP,
    VA_FUSER_ONSESSION_74_CMP,
    VA_FUSER_ONSESSION_SCS_OK,
    VA_FUSER_ONSESSION_EVENT,
    VA_FUSER_ONSESSION_NOTIFY,
    VA_FUSER_ONSESSION_STATE5_WRITE,
    VA_FUSER_FINISH,
    VA_FUSER_FINISH_RESULT_CMP,
    VA_FUSER_FINISH_74_CMP,
    VA_FUSER_FINISH_SCS_OK,
    VA_FUSER_FINISH_EVENT,
    VA_FUSER_FINISH_NOTIFY,
    VA_SETCTRL_HANDLER,
    VA_ONSIG_SETCONTROL,
    VA_FCLIENTWORLD_CONNECT,
    VA_MASTER_ATTACH_BFE230,
    VA_CONNECT_EMIT_A8E500,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_EXECUTOR_POSTMODE 1
#define EXP_DUMP_LUA_CALLABLE_NAME 1
#define EXP_DUMP_LUA_DISPATCH 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_DUMP_FUSER_STATE 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 5600
#elif EXP_TARGET_PROFILE == 38
static const DWORD g_targets[] = {
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
    VA_RPC_BC5860_RET,
    VA_RPC_BA9640_JZ,
    VA_RPC_LUA_CALLSITE,
    VA_RPC_LUA_RET,
    VA_LUA_ERROR_UNKNOWN,
    VA_LUA_ERROR_MSGBOX,
    VA_ONLOADPLAYERDATA_RECV,
    VA_ONLOADPLAYERDATA_INNER,
    VA_ONLOADPLAYERDATA_EMIT,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
    VA_FUSER_CONNECT,
    VA_FUSER_STATE1_WRITE_8C,
    VA_FUSER_STATE1_WRITE_74,
    VA_FUSER_ONCONNECTOR,
    VA_FUSER_ONSIGNIN,
    VA_FUSER_SIGNIN_74_CMP,
    VA_FUSER_SIGNIN_SCS_OK,
    VA_FUSER_SIGNIN_EVENT,
    VA_FUSER_SIGNIN_NOTIFY,
    VA_FUSER_ONLOGIN,
    VA_FUSER_LOGIN_SCS_OK,
    VA_FUSER_LOGIN_EVENT,
    VA_FUSER_ONSESSION,
    VA_FUSER_ONSESSION_RESOLVE,
    VA_FUSER_ONSESSION_STATE_CMP,
    VA_FUSER_ONSESSION_74_CMP,
    VA_FUSER_ONSESSION_SCS_OK,
    VA_FUSER_ONSESSION_EVENT,
    VA_FUSER_ONSESSION_NOTIFY,
    VA_FUSER_ONSESSION_STATE5_WRITE,
    VA_FUSER_FINISH,
    VA_FUSER_FINISH_RESULT_CMP,
    VA_FUSER_FINISH_74_CMP,
    VA_FUSER_FINISH_SCS_OK,
    VA_FUSER_FINISH_EVENT,
    VA_FUSER_FINISH_NOTIFY,
    VA_SETCTRL_HANDLER,
    VA_ONSIG_SETCONTROL,
    VA_FCLIENTWORLD_CONNECT,
    VA_MASTER_ATTACH_BFE230,
    VA_CONNECT_EMIT_A8E500,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_EXECUTOR_POSTMODE 1
#define EXP_DUMP_LUA_CALLABLE_NAME 1
#define EXP_DUMP_LUA_DISPATCH 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_DUMP_FUSER_STATE 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_DUMP_ONLOADPLAYERDATA 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 6200
#elif EXP_TARGET_PROFILE == 39
static const DWORD g_targets[] = {
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
    VA_RPC_BC5860_RET,
    VA_RPC_BA9640_JZ,
    VA_RPC_LUA_CALLSITE,
    VA_RPC_LUA_RET,
    VA_LUA_ERROR_UNKNOWN,
    VA_LUA_ERROR_MSGBOX,
    VA_ONLOADPLAYERDATA_RECV,
    VA_ONLOADPLAYERDATA_INNER,
    VA_ONLOADPLAYERDATA_EMIT,
    VA_LOGINRESULT_DOWNCALL,
    VA_LOGINRESULT_A81020,
    VA_LOGIN_NO_WORLD,
    VA_LOGIN_NO_OBJECT,
    VA_LOGIN_NO_CLIENTCOMP,
    VA_LOGIN_NO_COM_PLAYER,
    VA_LOGIN_NO_COM_CSTATUS,
    VA_LOGIN_NO_COM_USER,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
    VA_FUSER_CONNECT,
    VA_FUSER_STATE1_WRITE_8C,
    VA_FUSER_STATE1_WRITE_74,
    VA_FUSER_ONCONNECTOR,
    VA_FUSER_ONSIGNIN,
    VA_FUSER_SIGNIN_74_CMP,
    VA_FUSER_SIGNIN_SCS_OK,
    VA_FUSER_SIGNIN_EVENT,
    VA_FUSER_SIGNIN_NOTIFY,
    VA_FUSER_ONLOGIN,
    VA_FUSER_LOGIN_SCS_OK,
    VA_FUSER_LOGIN_EVENT,
    VA_FUSER_ONSESSION,
    VA_FUSER_ONSESSION_RESOLVE,
    VA_FUSER_ONSESSION_STATE_CMP,
    VA_FUSER_ONSESSION_74_CMP,
    VA_FUSER_ONSESSION_SCS_OK,
    VA_FUSER_ONSESSION_EVENT,
    VA_FUSER_ONSESSION_NOTIFY,
    VA_FUSER_ONSESSION_STATE5_WRITE,
    VA_FUSER_FINISH,
    VA_FUSER_FINISH_RESULT_CMP,
    VA_FUSER_FINISH_74_CMP,
    VA_FUSER_FINISH_SCS_OK,
    VA_FUSER_FINISH_EVENT,
    VA_FUSER_FINISH_NOTIFY,
    VA_SETCTRL_HANDLER,
    VA_SETCTRL_SIGNAL_HELPER,
    VA_ONSIG_SETCONTROL,
    VA_FCLIENTWORLD_CONNECT,
    VA_MASTER_ATTACH_BFE230,
    VA_CONNECT_EMIT_A8E500,
    VA_CONNECT_DOWNCALL_A8E5B0,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_EXECUTOR_POSTMODE 1
#define EXP_DUMP_LUA_CALLABLE_NAME 1
#define EXP_DUMP_LUA_DISPATCH 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_DUMP_FUSER_STATE 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_DUMP_ONLOADPLAYERDATA 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 7600
#elif EXP_TARGET_PROFILE == 40
static const DWORD g_targets[] = {
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
    VA_RPC_BC5860_RET,
    VA_RPC_BA9640_JZ,
    VA_RPC_LUA_CALLSITE,
    VA_RPC_LUA_RET,
    VA_LUA_ERROR_UNKNOWN,
    VA_LUA_ERROR_MSGBOX,
    VA_ONLOADPLAYERDATA_RECV,
    VA_ONLOADPLAYERDATA_INNER,
    VA_ONLOADPLAYERDATA_EMIT,
    VA_ONLOAD_BRIDGE_A87B90,
    VA_ONLOAD_BRIDGE_NO_OWNER_JZ,
    VA_ONLOAD_BRIDGE_NO_BUILDER_JZ,
    VA_ONLOAD_BRIDGE_ENQUEUE_CALL,
    VA_POST_WRITER_ENQUEUE_A,
    VA_POST_WRITER_QUEUE,
    VA_LOGINRESULT_DOWNCALL,
    VA_LOGINRESULT_A81020,
    VA_LOGIN_NO_WORLD,
    VA_LOGIN_NO_OBJECT,
    VA_LOGIN_NO_CLIENTCOMP,
    VA_LOGIN_NO_COM_PLAYER,
    VA_LOGIN_NO_COM_CSTATUS,
    VA_LOGIN_NO_COM_USER,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
    VA_FUSER_CONNECT,
    VA_FUSER_STATE1_WRITE_8C,
    VA_FUSER_STATE1_WRITE_74,
    VA_FUSER_ONCONNECTOR,
    VA_FUSER_ONSIGNIN,
    VA_FUSER_SIGNIN_74_CMP,
    VA_FUSER_SIGNIN_SCS_OK,
    VA_FUSER_SIGNIN_EVENT,
    VA_FUSER_SIGNIN_NOTIFY,
    VA_FUSER_ONLOGIN,
    VA_FUSER_LOGIN_SCS_OK,
    VA_FUSER_LOGIN_EVENT,
    VA_FUSER_ONSESSION,
    VA_FUSER_ONSESSION_RESOLVE,
    VA_FUSER_ONSESSION_STATE_CMP,
    VA_FUSER_ONSESSION_74_CMP,
    VA_FUSER_ONSESSION_SCS_OK,
    VA_FUSER_ONSESSION_EVENT,
    VA_FUSER_ONSESSION_NOTIFY,
    VA_FUSER_ONSESSION_STATE5_WRITE,
    VA_FUSER_FINISH,
    VA_FUSER_FINISH_RESULT_CMP,
    VA_FUSER_FINISH_74_CMP,
    VA_FUSER_FINISH_SCS_OK,
    VA_FUSER_FINISH_EVENT,
    VA_FUSER_FINISH_NOTIFY,
    VA_SETCTRL_HANDLER,
    VA_SETCTRL_SIGNAL_HELPER,
    VA_ONSIG_SETCONTROL,
    VA_FCLIENTWORLD_CONNECT,
    VA_MASTER_ATTACH_BFE230,
    VA_CONNECT_EMIT_A8E500,
    VA_CONNECT_DOWNCALL_A8E5B0,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_EXECUTOR_POSTMODE 1
#define EXP_DUMP_LUA_CALLABLE_NAME 1
#define EXP_DUMP_LUA_DISPATCH 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_DUMP_FUSER_STATE 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_DUMP_ONLOADPLAYERDATA 1
#define EXP_DUMP_ONLOAD_EMIT_BRIDGE 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 8400
#elif EXP_TARGET_PROFILE == 41
static const DWORD g_targets[] = {
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_ONLOADPLAYERDATA_RECV,
    VA_ONLOADPLAYERDATA_INNER,
    VA_ONLOADPLAYERDATA_EMIT,
    VA_ONLOAD_BRIDGE_A87B90,
    VA_ONLOAD_BRIDGE_ENQUEUE_CALL,
    VA_POST_WRITER_ENQUEUE_A,
    VA_POST_WRITER_QUEUE,
    VA_DRAIN_POP_CALL,
    VA_DRAIN_SEND_ENTRY,
    VA_DRAIN_TARGET_BOOL_RET,
    VA_SEND_LAYER_HELPER,
    VA_DRAIN_SENDHELPER_RET,
    VA_DRAIN_TEARDOWN,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_ONLOADPLAYERDATA 1
#define EXP_DUMP_ONLOAD_EMIT_BRIDGE 1
#define EXP_DUMP_OUTBOUND_DRAIN 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 3200
#elif EXP_TARGET_PROFILE == 42
static const DWORD g_targets[] = {
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_FUSER_CONNECT,
    VA_ONLOADPLAYERDATA_RECV,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_B4_DESCRIPTORS 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 800
#elif EXP_TARGET_PROFILE == 43
static const DWORD g_targets[] = {
    VA_KEY03_NEWOBJECT,
    VA_NEWOBJ_COMP_INIT,
    VA_COMP_MATERIALIZE,
    VA_COMP_NULL_TEST,
    VA_COMP_MAP_WRITE,
    VA_COMP_SLOT7_RET,
    VA_COMP_LOOP_RECHECK,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
    VA_RPC_BC5860_RET,
    VA_RPC_BA9640_JZ,
    VA_RPC_LUA_CALLSITE,
    VA_RPC_LUA_RET,
    VA_LUA_ERROR_UNKNOWN,
    VA_LUA_ERROR_MSGBOX,
    VA_ONLOADPLAYERDATA_RECV,
    VA_ONLOADPLAYERDATA_INNER,
    VA_ONLOADPLAYERDATA_EMIT,
    VA_ONLOAD_BRIDGE_A87B90,
    VA_ONLOAD_BRIDGE_ENQUEUE_CALL,
    VA_POST_WRITER_QUEUE,
    VA_LOGINRESULT_DOWNCALL,
    VA_LOGINRESULT_A81020,
    VA_LOGIN_NO_WORLD,
    VA_LOGIN_NO_OBJECT,
    VA_LOGIN_NO_CLIENTCOMP,
    VA_LOGIN_NO_COM_PLAYER,
    VA_LOGIN_NO_COM_CSTATUS,
    VA_LOGIN_NO_COM_USER,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
    VA_FUSER_CONNECT,
    VA_FUSER_ONSIGNIN,
    VA_FUSER_ONLOGIN,
    VA_FUSER_ONSESSION,
    VA_FUSER_FINISH,
    VA_FUSER_FINISH_RESULT_CMP,
    VA_FUSER_FINISH_74_CMP,
    VA_DRAIN_TEARDOWN,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_COMPONENT_PATH 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_EXECUTOR_POSTMODE 1
#define EXP_DUMP_LUA_CALLABLE_NAME 1
#define EXP_DUMP_LUA_DISPATCH 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_DUMP_ONLOADPLAYERDATA 1
#define EXP_DUMP_ONLOAD_EMIT_BRIDGE 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_DUMP_FUSER_STATE 1
#define EXP_DUMP_OUTBOUND_DRAIN 1
#if EXP_TARGET_PROFILE != 77
#if EXP_TARGET_PROFILE != 77
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_INJECT_B4_COMPONENT_OBJ18 1
#endif
#endif
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 10000
#elif EXP_TARGET_PROFILE == 44
static const DWORD g_targets[] = {
    VA_KEY03_NEWOBJECT,
    VA_NEWOBJ_COMP_INIT,
    VA_COMP_KEY_READ_RET,
    VA_COMP_LOOKUP_RET,
    VA_COMP_EXISTING_TEST,
    VA_COMP_MATERIALIZE,
    VA_COMP_NULL_TEST,
    VA_COMP_MAP_WRITE,
    VA_COMP_SLOT7_ENTRY,
    VA_COMP_SLOT7_RET,
    VA_COMP_LOOP_RECHECK,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_DRAIN_TEARDOWN,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_COMPONENT_PATH 1
#define EXP_DUMP_COMPONENT_SHARP 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_OUTBOUND_DRAIN 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_INJECT_B4_COMPONENT_OBJ18 1
#define EXP_REARM_TARGETS 1
#if EXP_TARGET_PROFILE == 74
#define EXP_TARGET_REARM_BUDGET 12000
#else
#define EXP_TARGET_REARM_BUDGET 4000
#endif
#elif EXP_TARGET_PROFILE == 45
static const DWORD g_targets[] = {
    VA_KEY03_NEWOBJECT,
    VA_NEWOBJ_COMP_INIT,
    VA_COMP_KEY_READ_RET,
    VA_COMP_LOOKUP_RET,
    VA_COMP_EXISTING_TEST,
    VA_COMP_MATERIALIZE,
    VA_COMP_NULL_TEST,
    VA_COMP_MAP_WRITE,
    VA_COMP_SLOT7_ENTRY,
    VA_BB0060_UNPACK_BA9640,
    VA_BB0060_UNPACK_CBDA80,
    VA_BB0060_UNPACK_RET,
    VA_BEDE50_AFTER_BB0060,
    VA_BEDE50_CURSOR_RET,
    VA_BEDE50_CACHE_BE9840,
    VA_BEDE50_SEEK_START,
    VA_BEDE50_CACHE_RESET,
    VA_BEDE50_COPY_RAW,
    VA_BEDE50_SEEK_BACK,
    VA_COMP_SLOT7_RET,
    VA_COMP_LOOP_RECHECK,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_DRAIN_TEARDOWN,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_COMPONENT_PATH 1
#define EXP_DUMP_COMPONENT_SHARP 1
#define EXP_DUMP_B4_BOUNDARY 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_OUTBOUND_DRAIN 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_INJECT_B4_COMPONENT_OBJ18 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 6000
#elif EXP_TARGET_PROFILE == 46
static const DWORD g_targets[] = {
    VA_KEY03_NEWOBJECT,
    VA_NEWOBJ_COMP_INIT,
    VA_COMP_KEY_READ_RET,
    VA_COMP_LOOKUP_RET,
    VA_COMP_EXISTING_TEST,
    VA_COMP_MATERIALIZE,
    VA_COMP_NULL_TEST,
    VA_COMP_MAP_WRITE,
    VA_COMP_SLOT7_ENTRY,
    VA_BB0060_FIELD_GATE,
    VA_BB0060_FIELD_GATE_RET,
    VA_BB0060_UNPACK_BA9640,
    VA_BB0060_UNPACK_CBDA80,
    VA_BB0060_UNPACK_RET,
    VA_BEDE50_AFTER_BB0060,
    VA_BEDE50_CURSOR_RET,
    VA_BEDE50_CACHE_BE9840,
    VA_BEDE50_SEEK_START,
    VA_BEDE50_CACHE_RESET,
    VA_BEDE50_COPY_RAW,
    VA_BEDE50_SEEK_BACK,
    VA_COMP_SLOT7_RET,
    VA_COMP_LOOP_RECHECK,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_DRAIN_TEARDOWN,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_COMPONENT_PATH 1
#define EXP_DUMP_COMPONENT_SHARP 1
#define EXP_DUMP_B4_BOUNDARY 1
#define EXP_DUMP_BB0060_FIELDS 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_OUTBOUND_DRAIN 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_INJECT_B4_COMPONENT_OBJ18 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 7000
#elif EXP_TARGET_PROFILE == 47 || EXP_TARGET_PROFILE == 48 || EXP_TARGET_PROFILE == 49 || EXP_TARGET_PROFILE == 50
static const DWORD g_targets[] = {
    VA_KEY03_NEWOBJECT,
    VA_NEWOBJ_COMP_INIT,
    VA_COMP_KEY_READ_RET,
    VA_COMP_LOOKUP_RET,
    VA_COMP_EXISTING_TEST,
    VA_COMP_MATERIALIZE,
    VA_COMP_NULL_TEST,
    VA_COMP_MAP_WRITE,
    VA_COMP_SLOT7_ENTRY,
    VA_BB0060_FIELD_GATE,
    VA_BB0060_FIELD_GATE_RET,
    VA_BB0060_UNPACK_BA9640,
    VA_BB0060_UNPACK_CBDA80,
    VA_BB0060_UNPACK_RET,
    VA_GOLUA_UNPACKFIXED,
    VA_BEDE50_AFTER_BB0060,
    VA_BEDE50_CURSOR_RET,
    VA_BEDE50_CACHE_BE9840,
    VA_BEDE50_SEEK_START,
    VA_BEDE50_CACHE_RESET,
    VA_BEDE50_COPY_RAW,
    VA_BEDE50_SEEK_BACK,
    VA_COMP_SLOT7_RET,
    VA_COMP_LOOP_RECHECK,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
#if EXP_TARGET_PROFILE == 49 || EXP_TARGET_PROFILE == 50
    VA_KEY6_RECEIVER,
#if EXP_TARGET_PROFILE == 50
    VA_KEY6_TARGET_CMP,
    VA_KEY6_TARGET_CUR_JZ,
    VA_KEY6_TARGET_NEG1_JZ,
    VA_KEY6_TARGET_REJECT,
#endif
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
#if EXP_TARGET_PROFILE == 50
    VA_KEY6_CLEANUP_ENTRY,
    VA_KEY6_CLEANUP_CALL,
#endif
    VA_RPC_BC5860_RET,
    VA_RPC_BA9640_JZ,
    VA_RPC_LUA_CALLSITE,
    VA_RPC_LUA_RET,
    VA_LUA_ERROR_UNKNOWN,
    VA_LUA_ERROR_MSGBOX,
    VA_ONLOADPLAYERDATA_RECV,
    VA_ONLOADPLAYERDATA_INNER,
    VA_ONLOADPLAYERDATA_EMIT,
    VA_ONLOAD_BRIDGE_A87B90,
    VA_ONLOAD_BRIDGE_ENQUEUE_CALL,
    VA_POST_WRITER_QUEUE,
    VA_LOGINRESULT_DOWNCALL,
    VA_LOGINRESULT_A81020,
    VA_LOGIN_NO_WORLD,
    VA_LOGIN_NO_OBJECT,
    VA_LOGIN_NO_CLIENTCOMP,
    VA_LOGIN_NO_COM_PLAYER,
    VA_LOGIN_NO_COM_CSTATUS,
    VA_LOGIN_NO_COM_USER,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
    VA_FUSER_CONNECT,
    VA_FUSER_ONSIGNIN,
    VA_FUSER_ONLOGIN,
    VA_FUSER_ONSESSION,
    VA_FUSER_FINISH,
    VA_FUSER_FINISH_RESULT_CMP,
    VA_FUSER_FINISH_74_CMP,
#endif
    VA_DRAIN_TEARDOWN,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_COMPONENT_PATH 1
#define EXP_DUMP_COMPONENT_SHARP 1
#define EXP_DUMP_B4_BOUNDARY 1
#define EXP_DUMP_BB0060_FIELDS 1
#define EXP_DUMP_GOLUA_UNPACKFIXED 1
#if EXP_TARGET_PROFILE == 48 || EXP_TARGET_PROFILE == 49
#define EXP_DUMP_GOLUA_KEYSTR 1
#endif
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_OUTBOUND_DRAIN 1
#if EXP_TARGET_PROFILE == 49 || EXP_TARGET_PROFILE == 50
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_EXECUTOR_POSTMODE 1
#define EXP_DUMP_LUA_CALLABLE_NAME 1
#define EXP_DUMP_LUA_DISPATCH 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_DUMP_ONLOADPLAYERDATA 1
#define EXP_DUMP_ONLOAD_EMIT_BRIDGE 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_DUMP_FUSER_STATE 1
#if EXP_TARGET_PROFILE == 50
#define EXP_DUMP_KEY6_EARLY_GUARD 1
#endif
#endif
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_INJECT_B4_COMPONENT_OBJ18 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 9000
#elif EXP_TARGET_PROFILE == 51
static const DWORD g_targets[] = {
    VA_KEY03_NEWOBJECT,
    VA_NEWOBJ_COMP_INIT,
    VA_COMP_KEY_READ_RET,
    VA_COMP_LOOKUP_RET,
    VA_COMP_EXISTING_TEST,
    VA_COMP_MATERIALIZE,
    VA_COMP_NULL_TEST,
    VA_COMP_MAP_WRITE,
    VA_COMP_SLOT7_ENTRY,
    VA_BB0060_FIELD_GATE,
    VA_BB0060_FIELD_GATE_RET,
    VA_SIMPLE_FIELD_READ,
    VA_SIMPLE_UNPACK_CALL,
    VA_SIMPLE_UNPACK_RET,
    VA_BB0060_UNPACK_BA9640,
    VA_BB0060_UNPACK_CBDA80,
    VA_BB0060_UNPACK_RET,
    VA_GOLUA_UNPACKFIXED,
    VA_BEDE50_AFTER_BB0060,
    VA_BEDE50_CURSOR_RET,
    VA_BEDE50_CACHE_BE9840,
    VA_BEDE50_SEEK_START,
    VA_BEDE50_CACHE_RESET,
    VA_BEDE50_COPY_RAW,
    VA_BEDE50_SEEK_BACK,
    VA_COMP_SLOT7_RET,
    VA_COMP_LOOP_RECHECK,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_KEY6_CALLMODE_RESULT,
    VA_RPC_BC5860_RET,
    VA_RPC_BA9640_JZ,
    VA_RPC_LUA_CALLSITE,
    VA_RPC_LUA_RET,
    VA_LUA_ERROR_UNKNOWN,
    VA_LUA_ERROR_MSGBOX,
    VA_ONLOADPLAYERDATA_RECV,
    VA_ONLOADPLAYERDATA_INNER,
    VA_ONLOADPLAYERDATA_EMIT,
    VA_ONLOAD_BRIDGE_A87B90,
    VA_ONLOAD_BRIDGE_ENQUEUE_CALL,
    VA_POST_WRITER_QUEUE,
    VA_LOGINRESULT_DOWNCALL,
    VA_LOGINRESULT_A81020,
    VA_LOGIN_NO_WORLD,
    VA_LOGIN_NO_OBJECT,
    VA_LOGIN_NO_CLIENTCOMP,
    VA_LOGIN_NO_COM_PLAYER,
    VA_LOGIN_NO_COM_CSTATUS,
    VA_LOGIN_NO_COM_USER,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
    VA_FUSER_CONNECT,
    VA_FUSER_ONSIGNIN,
    VA_FUSER_ONLOGIN,
    VA_FUSER_ONSESSION,
    VA_FUSER_FINISH,
    VA_FUSER_FINISH_RESULT_CMP,
    VA_FUSER_FINISH_74_CMP,
    VA_DRAIN_TEARDOWN,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_COMPONENT_PATH 1
#define EXP_DUMP_COMPONENT_SHARP 1
#define EXP_DUMP_B4_BOUNDARY 1
#define EXP_DUMP_BB0060_FIELDS 1
#define EXP_DUMP_SIMPLE_FIELD_READ 1
#define EXP_DUMP_GOLUA_UNPACKFIXED 1
#define EXP_DUMP_GOLUA_KEYSTR 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_OUTBOUND_DRAIN 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_EXECUTOR_POSTMODE 1
#define EXP_DUMP_LUA_CALLABLE_NAME 1
#define EXP_DUMP_LUA_DISPATCH 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_DUMP_ONLOADPLAYERDATA 1
#define EXP_DUMP_ONLOAD_EMIT_BRIDGE 1
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_DUMP_FUSER_STATE 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_INJECT_B4_COMPONENT_OBJ18 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 11000
#elif EXP_TARGET_PROFILE == 52 || EXP_TARGET_PROFILE == 53 || EXP_TARGET_PROFILE == 54 || EXP_TARGET_PROFILE == 55 || EXP_TARGET_PROFILE == 56 || EXP_TARGET_PROFILE == 57 || EXP_TARGET_PROFILE == 58 || EXP_TARGET_PROFILE == 59 || EXP_TARGET_PROFILE == 60 || EXP_TARGET_PROFILE == 61 || EXP_TARGET_PROFILE == 62 || EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 65 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
static const DWORD g_targets[] = {
    /* Keep the proven profile51 diagnostic self-register path alive. */
    VA_KEY03_NEWOBJECT,
    VA_NEWOBJ_COMP_INIT,
    VA_COMP_MAP_WRITE,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_RPC_LUA_CALLSITE,
    VA_RPC_LUA_RET,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
#if EXP_TARGET_PROFILE == 53 || EXP_TARGET_PROFILE == 54 || EXP_TARGET_PROFILE == 55 || EXP_TARGET_PROFILE == 56 || EXP_TARGET_PROFILE == 57 || EXP_TARGET_PROFILE == 58 || EXP_TARGET_PROFILE == 59 || EXP_TARGET_PROFILE == 60 || EXP_TARGET_PROFILE == 61 || EXP_TARGET_PROFILE == 62 || EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 65 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
    /* s2 UI-control bridge markers. */
    VA_FUSER_SETUICONTROL,
    VA_FUSER_SETUICONTROL_WRITE_74,
#endif
    /* B6: legal CharacterManager planter path for FUser+0x84. */
    VA_FUSER_ONSESSION,
    VA_FUSER_ONSESSION_RESOLVE,
    VA_FUSER_ONSESSION_STATE_CMP,
    VA_FUSER_ONSESSION_74_CMP,
    VA_FUSER_ONSESSION_SCS_OK,
    VA_FUSER_ONSESSION_EVENT,
    VA_FUSER_ONSESSION_NOTIFY,
    VA_FUSER_ONSESSION_STATE5_WRITE,
    VA_FUSER_FINISH,
    VA_FUSER_FINISH_RESULT_CMP,
    VA_FUSER_FINISH_74_CMP,
    VA_FUSER_FINISH_SCS_OK,
    VA_FUSER_FINISH_EVENT,
    VA_FUSER_FINISH_NOTIFY,
#if EXP_TARGET_PROFILE == 55 || EXP_TARGET_PROFILE == 56 || EXP_TARGET_PROFILE == 57 || EXP_TARGET_PROFILE == 58 || EXP_TARGET_PROFILE == 59 || EXP_TARGET_PROFILE == 60 || EXP_TARGET_PROFILE == 61 || EXP_TARGET_PROFILE == 62 || EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 65 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
    VA_FUSER_FINISH_AFTER_EVENT,
#endif
    /* s64 post-finish UI/list delivery markers. */
 #if EXP_TARGET_PROFILE != 61 && EXP_TARGET_PROFILE != 62 && EXP_TARGET_PROFILE != 63 && EXP_TARGET_PROFILE != 65 && EXP_TARGET_PROFILE != 66 && EXP_TARGET_PROFILE != 67 && EXP_TARGET_PROFILE != 68 && EXP_TARGET_PROFILE != 69
    VA_EVENT_EMIT_B,
    VA_FUSER_UI_UPDATE_HELPER,
 #endif
#if EXP_TARGET_PROFILE != 54 && EXP_TARGET_PROFILE != 55 && EXP_TARGET_PROFILE != 56 && EXP_TARGET_PROFILE != 57 && EXP_TARGET_PROFILE != 58 && EXP_TARGET_PROFILE != 59 && EXP_TARGET_PROFILE != 60 && EXP_TARGET_PROFILE != 61 && EXP_TARGET_PROFILE != 62 && EXP_TARGET_PROFILE != 63 && EXP_TARGET_PROFILE != 65 && EXP_TARGET_PROFILE != 66 && EXP_TARGET_PROFILE != 67 && EXP_TARGET_PROFILE != 68 && EXP_TARGET_PROFILE != 69
    VA_POST_STATE_EVENT_HELPER,
#endif
#if EXP_TARGET_PROFILE != 66 && EXP_TARGET_PROFILE != 67 && EXP_TARGET_PROFILE != 68 && EXP_TARGET_PROFILE != 69
    VA_FUSER_UPDATECHAR,
#endif
    VA_CHARMGR_GETALL,
#if EXP_TARGET_PROFILE == 69
    VA_BB0060_ENTRY,
    VA_BB0060_FIELD_GATE,
    VA_BB0060_FIELD_GATE_RET,
    VA_BB0060_TYPE2_ENTRY,
    VA_BB0060_TYPE2_BLOB1,
    VA_BB0060_TYPE2_U8_A,
    VA_BB0060_TYPE2_BLOB2,
    VA_BB0060_TYPE2_U8_B,
    VA_BB0060_TYPE2_F64,
    VA_BB0060_UNPACK_BA9640,
    VA_BB0060_UNPACK_CBDA80,
    VA_BB0060_UNPACK_RET,
    VA_BB0060_TYPE2_EXIT,
    VA_NEWOBJ_PID_STORED,
    VA_OBJPTR_MAT_ENTRY,
    VA_OBJPTR_BB2580_CALL,
    VA_OBJPTR_BB2580_RET,
    VA_BB2580_BC5860_CALL,
    VA_BB2580_BC5860_RET,
    VA_BB2580_BB5F40_CALL,
    VA_BB2580_BB5F40_RET,
    VA_ONLOAD_ARG3_CALL,
    VA_OBJLIST_A_WRAPPER,
    VA_OBJLIST_A_PARAM_TEST,
    VA_OBJLIST_A_OUTER_TAG_RET,
    VA_OBJLIST_A_COUNT_RET,
    VA_OBJLIST_A_TABLE_RET2,
    VA_OBJLIST_A_INDEX_RET,
    VA_OBJLIST_A_NESTED_RET,
    VA_OBJLIST_A_NODE_ALLOC_RET,
    VA_OBJLIST_A_NODE_LINK_RET,
    VA_OBJLIST_B_WRAPPER,
    VA_OBJLIST_B_ENTRY,
    VA_OBJLIST_B_PARAM_TEST,
    VA_VARIANT_TAG0B_ENTRY,
    VA_VARIANT_TAG0B_DESC_RET,
    VA_VARIANT_TAG0B_OBJ_RET,
    VA_VARIANT_TAG0B_BA9200_RET,
    VA_VARIANT_TAG0B_NIL_OBJ,
    VA_CHARMGR_LOADLIST,
    VA_LOADLIST_EMPTY_CHECK,
    VA_LOADLIST_NODE_PAYLOAD,
    VA_LOADLIST_REF_RET,
    VA_LOADLIST_DUP_SCAN,
    VA_LOADLIST_DUP_RESULT,
    VA_LOADLIST_PLAYER_DESC,
    VA_LOADLIST_PLAYER_RESOLVE,
    VA_LOADLIST_PLAYER_RET,
    VA_LOADLIST_PLAYER_TEST,
    VA_LOADLIST_PLAYER_FLAGS,
    VA_LOADLIST_SCORE_RET,
    VA_LOADLIST_ACTIVE_PATH,
    VA_LOADLIST_OBJ_RESOLVE2,
    VA_LOADLIST_OBJ_RET2,
    VA_LOADLIST_TPLAYER_CALL,
    VA_LOADLIST_ROW_ADD_CALL,
    VA_LOADLIST_ROW_ADD_RET,
    VA_LOADLIST_LOOP_NEXT,
    VA_ONLOAD_AFTER_LOADLIST,
    VA_SERVER_FINISH_SEND,
#endif
    VA_UPDATE_AFTER_GETALL,
    VA_UPDATE_ADDCHAR,
    VA_UPDATE_HASNOCHAR,
    VA_UPDATE_SELECTCHAR,
    VA_DRAIN_TEARDOWN,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_COMPONENT_PATH 1
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_LUA_DISPATCH 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_DUMP_FUSER_STATE 1
#define EXP_DUMP_POST_FINISH_UI 1
#if EXP_TARGET_PROFILE == 69
#define EXP_DUMP_BB0060_FIELDS 1
#define EXP_DUMP_BB0060_TAIL 1
#endif
#if EXP_TARGET_PROFILE == 60 || EXP_TARGET_PROFILE == 61 || EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 65 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
#define EXP_DUMP_UIEVENT_FIELDS 1
#endif
#if EXP_TARGET_PROFILE == 66
#define EXP_FORCE_UPDATECHAR_AFTER_FINISH 1
#endif
#if EXP_TARGET_PROFILE == 67
#define EXP_FORCE_UPDATECHAR_AT_NOTIFY 1
#endif
#if EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69 || EXP_TARGET_PROFILE == 76
#define EXP_FORCE_UPDATECHAR_AT_NOTIFY 1
#define EXP_FORCE_UPDATECHAR_WITH_CHARMGR 1
#endif
#if EXP_TARGET_PROFILE == 62
#define EXP_DUMP_UIEVENT_FIELDS 1
#define EXP_DUMP_MAP_AND_RUNLOG 1
#endif
#define EXP_DUMP_OUTBOUND_DRAIN 1
#if EXP_TARGET_PROFILE != 77
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_INJECT_B4_COMPONENT_OBJ18 1
#endif
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 6000
#elif EXP_TARGET_PROFILE == 70 || EXP_TARGET_PROFILE == 71 || EXP_TARGET_PROFILE == 72 || EXP_TARGET_PROFILE == 73 || EXP_TARGET_PROFILE == 74 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
static const DWORD g_targets[] = {
    /* Required planters for the proven B4/B5 login path. */
    VA_KEY03_NEWOBJECT,
    VA_NEWOBJ_COMP_INIT,
#if EXP_TARGET_PROFILE == 72 || EXP_TARGET_PROFILE == 73 || EXP_TARGET_PROFILE == 74 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
    VA_COMP_KEY_READ_RET,
    VA_COMP_LOOKUP_RET,
    VA_COMP_EXISTING_TEST,
    VA_COMP_MATERIALIZE,
    VA_COMP_NULL_TEST,
    VA_COMP_SLOT7_ENTRY,
    VA_COMP_SLOT7_RET,
    VA_COMP_LOOP_RECHECK,
#endif
#if EXP_TARGET_PROFILE == 73 || EXP_TARGET_PROFILE == 74 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
    VA_BB0060_ENTRY,
    VA_BB0060_FIELD_GATE,
    VA_BB0060_FIELD_GATE_RET,
    VA_BB0060_TYPE2_ENTRY,
    VA_BB0060_TYPE2_BLOB1,
    VA_BB0060_TYPE2_U8_A,
    VA_BB0060_TYPE2_BLOB2,
    VA_BB0060_TYPE2_U8_B,
    VA_BB0060_TYPE2_F64,
    VA_BB0060_UNPACK_BA9640,
    VA_BB0060_UNPACK_CBDA80,
    VA_BB0060_UNPACK_RET,
    VA_BB0060_TYPE2_EXIT,
    VA_FUSER_SETUICONTROL,
    VA_FUSER_SETUICONTROL_WRITE_74,
    VA_FUSER_UI_UPDATE_HELPER,
    VA_FUSER_UPDATECHAR,
#endif
#if EXP_TARGET_PROFILE == 74 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
    VA_UIMANAGER_ATTACH,
    VA_UIMANAGER_MOVEFRONT,
    VA_UIMANAGER_RENDER3DUI,
    VA_UICONTROL_RENDER,
    VA_UICONTROL_SETVISIBLE,
    VA_UICONTAINER_SETVISIBLE,
    VA_UICONTAINER_RENDER,
    VA_UICONTAINER_ATTACH,
    VA_UICONTAINER_ATTACHBACK,
    VA_UICONTAINER_RENDER3DUI,
    VA_UICONTAINER_RENDER3DLAYER,
#endif
#if EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
    VA_ONLOAD_SELFLOOKUP_RET,
    VA_ONLOAD_SELFVALUE_TEST,
    VA_ONLOAD_CREATE_HASH,
    VA_ONLOAD_CREATE_RET,
    VA_ONLOAD_BD2CD0_RET,
    VA_ONLOAD_LISTCOPY_CALL,
    VA_ONLOAD_LOADLIST_CALL,
    VA_CHARMGR_LOADLIST,
    VA_ONLOAD_AFTER_LOADLIST,
    VA_ONLOAD_FINISH_CALL,
    VA_SERVER_FINISH_SEND,
#endif
#if EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
    VA_CHARMGR_CTOR,
    VA_CHARMGR_CTOR_STORE28,
    VA_CHARMGR_DTOR,
    VA_CHARMGR_DTOR_ZERO28,
#endif
#if EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
    VA_LOADLIST_EMPTY_CHECK,
    VA_LOADLIST_EMPTY_BRANCH,
    VA_LOADLIST_EMPTY_RELEASE_RET,
    VA_LOADLIST_EMPTY_RELEASE_DTOR,
    VA_LOADLIST_EMPTY_PARAM3_SCAN,
    VA_LOADLIST_OLD_BEGIN,
    VA_LOADLIST_OLD_MARK_ROW,
    VA_LOADLIST_OLD_HELPER,
    VA_LOADLIST_OLD_FETCH_RET,
    VA_LOADLIST_NODE_PAYLOAD,
    VA_LOADLIST_REF_RET,
    VA_LOADLIST_DUP_SCAN,
    VA_LOADLIST_DUP_RESULT,
    VA_LOADLIST_PLAYER_DESC,
    VA_LOADLIST_PLAYER_RESOLVE,
    VA_LOADLIST_PLAYER_RET,
    VA_LOADLIST_PLAYER_TEST,
    VA_LOADLIST_PLAYER_FLAGS,
    VA_LOADLIST_SCORE_RET,
    VA_LOADLIST_ACTIVE_PATH,
    VA_LOADLIST_OBJ_RESOLVE2,
    VA_LOADLIST_OBJ_RET2,
    VA_LOADLIST_TPLAYER_CALL,
    VA_LOADLIST_ROW_ADD_CALL,
    VA_LOADLIST_ROW_ADD_RET,
    VA_LOADLIST_LOOP_NEXT,
    VA_LOADLIST_OLD_ROW_VCALL,
    VA_LOADLIST_OLD_AFTER_VCALL,
    VA_LOADLIST_EMPTY_CLEANUP_RET,
#if EXP_TARGET_PROFILE == 78
    VA_LOADLIST_UPDATECOUNT_CALL,
#endif
    VA_LOADLIST_BAD_HELPER_ENTRY,
    VA_LOADLIST_BAD_HELPER_DEREF,
#endif
    VA_COMP_MAP_WRITE,
    VA_OBJ18_REG_BD2CD0,
    VA_OBJ18_WRITE_BD2D4D,
    VA_KEY6_RECEIVER,
    VA_KEY6_LOOKUP_NULL_JZ,
    VA_KEY6_RESOLVED_TEST,
    VA_KEY6_SKIP_EXEC_JZ,
    VA_KEY6_EXEC_CALLSITE,
    VA_KEY6_EXECUTOR,
    VA_RPC_INVALID_FORMAT,
    VA_RPC_ARGC_WRONG,
    VA_RPC_LUA_CALLSITE,
    VA_RPC_LUA_RET,
    VA_ONLOADPLAYERS_WRAP,
    VA_ONLOADPLAYERS_INNER,
#if EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
    VA_LOGIN_DOWNCALL,
    VA_LOGINMASTER_DOWNCALL,
    VA_LOGINRESULT_DOWNCALL,
    VA_LOGINRESULT_A81020,
    VA_LOGIN_NO_WORLD,
    VA_LOGIN_NO_OBJECT,
    VA_LOGIN_NO_CLIENTCOMP,
    VA_LOGIN_NO_COM_PLAYER,
    VA_LOGIN_NO_COM_CSTATUS,
    VA_LOGIN_NO_COM_USER,
    VA_SETCTRL_HANDLER,
    VA_SETCTRL_SIGNAL_HELPER,
    VA_ONSIG_SETCONTROL,
    VA_FUSER_ONCONNECTOR,
    VA_CONNECT_EMIT_A8E500,
    VA_CONNECT_DOWNCALL_A8E5B0,
    VA_CONNECT_WRITER_63D2E0,
    VA_FUSER_CONNECT,
    VA_FCLIENTWORLD_CONNECT,
    VA_MASTER_ATTACH_BFE230,
    VA_CLIENTCOMPUTER_ATTACH,
    VA_CONN_CONNECT_BASE,
    VA_CONN_OPEN_IMPL,
    VA_CONN_CONNECT_IMPL,
    VA_CONN_CONNECT_EMIT,
    VA_CONN_CHALLENGE_BASE,
    VA_CONN_RESPONSE_BASE,
    VA_CONN_RESPONSE_IMPL,
    VA_CONN_ARG_READER,
    VA_EVAL_MASTERHASH_MARK,
    VA_PM_CREATEOBJECT,
#endif
    VA_FUSER_FINISH,
    VA_FUSER_FINISH_SCS_OK,
    VA_FUSER_FINISH_EVENT,
    VA_FUSER_FINISH_NOTIFY,
    VA_UPDATE_AFTER_GETALL,
    VA_UPDATE_HASNOCHAR,

    /* Transition-disconnect decision points. */
    VA_FCLIENTAPP_FINALIZE,
    VA_DRAIN_TEARDOWN,
    VA_NET_REPORT_HELPER,
    VA_SESSION_SHUTDOWN,
    VA_SOCKET_CLOSE_HELPER,
#if EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
    VA_SESSION_MASTEROPEN,
    VA_SESSION_MASTERREOPEN,
    VA_SESSION_E0_STORE_HELPER,
#endif
#if EXP_TARGET_PROFILE == 71 || EXP_TARGET_PROFILE == 72 || EXP_TARGET_PROFILE == 73 || EXP_TARGET_PROFILE == 74 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
    VA_PROACTOR_DISPATCH_CALL,
    VA_PROACTOR_EXCEPTION_BUILD,
    VA_PROACTOR_EXCEPTION_TEXT_RET,
    VA_PROACTOR_REPORT_CALL,
#endif
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_COMPONENT_PATH 1
#if EXP_TARGET_PROFILE == 72 || EXP_TARGET_PROFILE == 73 || EXP_TARGET_PROFILE == 74 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
#define EXP_DUMP_COMPONENT_SHARP 1
#endif
#define EXP_DUMP_OBJ18_WRITERS 1
#define EXP_DUMP_CALLMODE 1
#define EXP_DUMP_LUA_DISPATCH 1
#define EXP_DUMP_METHOD_MAP 1
#define EXP_DUMP_SELF_MAP 1
#define EXP_DUMP_ONLOADPLAYERS 1
#define EXP_DUMP_FUSER_STATE 1
#define EXP_DUMP_POST_FINISH_UI 1
#if EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
#define EXP_DUMP_LOGINRESULT_GATES 1
#define EXP_DUMP_PRECONNECT_STAGE 1
#define EXP_DUMP_CONNECTOR_HANDSHAKE 1
#endif
#define EXP_DUMP_OUTBOUND_DRAIN 1
#define EXP_DUMP_TRANSITION_TEARDOWN 1
#define EXP_LOG_ALL_EXCEPTIONS 1
#if EXP_TARGET_PROFILE == 71 || EXP_TARGET_PROFILE == 72 || EXP_TARGET_PROFILE == 73 || EXP_TARGET_PROFILE == 74 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
#define EXP_DUMP_PROACTOR_EXCEPTION 1
#endif
#if EXP_TARGET_PROFILE == 73 || EXP_TARGET_PROFILE == 74 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
#define EXP_DUMP_BB0060_FIELDS 1
#define EXP_DUMP_BB0060_TAIL 1
#define EXP_DUMP_B4_DESCRIPTORS 1
#endif
#if EXP_TARGET_PROFILE == 74 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
#define EXP_DUMP_UI_LIFECYCLE 1
#endif
#if EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
#define EXP_DUMP_ONLOAD_TRANSITION 1
#endif
#if EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
#define EXP_DUMP_SESSION_E0_LIFECYCLE 1
#endif
#if EXP_TARGET_PROFILE == 78
#define EXP_GUARD_LOADLIST_UPDATECOUNT 1
#define EXP_CAPTURE_LOADLIST_ESI_CHARMGR 1
#define EXP_GUARD_FUSER84_CHARMGR 1
#endif
#if EXP_TARGET_PROFILE == 71
#define EXP_IGNORE_CXX_EH_SPAM 1
#endif
#if EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
#define EXP_IGNORE_CXX_EH_SPAM 1
#endif
#if EXP_TARGET_PROFILE != 77 && EXP_TARGET_PROFILE != 78
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_INJECT_B4_COMPONENT_OBJ18 1
#endif
#define EXP_REARM_TARGETS 1
#if EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
#define EXP_TARGET_REARM_BUDGET 12000
#else
#define EXP_TARGET_REARM_BUDGET 4000
#endif
#elif EXP_TARGET_PROFILE == 64
static const DWORD g_targets[] = {
    /* Required silent planters for the known-good B4/B5 path. */
    VA_COMP_MAP_WRITE,
    VA_KEY6_LOOKUP_NULL_JZ,

    /* B6 finish and terminal character-selection markers. */
    VA_FUSER_FINISH,
    VA_FUSER_FINISH_SCS_OK,
    VA_FUSER_FINISH_EVENT,
    VA_FUSER_FINISH_NOTIFY,
    VA_FUSER_FINISH_AFTER_EVENT,
    VA_FUSER_UPDATECHAR,
    VA_CHARMGR_GETALL,
    VA_UPDATE_ADDCHAR,
    VA_UPDATE_HASNOCHAR,
    VA_UPDATE_SELECTCHAR,
    VA_DRAIN_TEARDOWN,
};
#define VA_READY_GATE VA_FUSER_CONNECT
#define EXP_INSTALL_DELAY_MS 0
#define EXP_DUMP_FUSER_STATE 1
#define EXP_DUMP_POST_FINISH_UI 1
#define EXP_DUMP_UIEVENT_FIELDS 1
#define EXP_INJECT_SESSION_OBJ18 1
#define EXP_INJECT_SESSION_OBJ18_BD2CD0 1
#define EXP_INJECT_B4_COMPONENT_OBJ18 1
#define EXP_REARM_TARGETS 1
#define EXP_TARGET_REARM_BUDGET 1200
#else
#error Unsupported EXP_TARGET_PROFILE
#endif
#define N_TARGETS (sizeof(g_targets)/sizeof(g_targets[0]))

/* ------------------------------------------------------------------ */
/* TEST 1 -- int3 + VEH one-shot reach hook (aggressive: writes .text) */
/* ------------------------------------------------------------------ */
#if EXP_MODE == 1

static BYTE  g_orig_byte[N_TARGETS];
static BOOL  g_armed[N_TARGETS];
#ifdef EXP_REARM_TARGETS
static volatile LONG g_target_budget = EXP_TARGET_REARM_BUDGET;
#endif

/* s6 re-arming read-primitive tracer: catch EVERY typed read during ServerCom
 * slot-7 (not just the first) so the last cursor before the throw names the
 * mis-shaped field. Armed lazily at COMPWRITE (immediately before slot-7) and
 * re-armed after each hit via single-step (trap flag); a budget confines logging
 * to the component window so we don't spam the whole login. ecx = reader at each
 * primitive entry; cursor (body-relative) = *(*(ecx+0x18)+8). */
/* RawRead (vtable+0x58 = 0x005fa9d0) is the single choke point every typed read
 * funnels through: ecx=reader, [esp+8]=byte count, cursor=*(*(reader+0x18)+8)
 * advanced by bytes read. Tracing it alone gives the complete field map (widths
 * + positions) regardless of which typed wrapper called it. */
#define N_TRACE 1
static const DWORD g_trace_va[N_TRACE] = { 0x005fa9d0u };
static BYTE  g_trace_orig[N_TRACE];
static BOOL  g_trace_armed[N_TRACE];
static volatile LONG  g_trace_budget = 0;
static volatile DWORD g_rearm_va = 0;

static void exp_log_key6_context(DWORD addr, PCONTEXT ctx)
{
    char buf[384];
    DWORD ebp, manager, manager_pid, field0_from, field1_target;
    DWORD field2_object, field3_self, field4_callable;
    DWORD lookup_ptr, resolver_result;
    if (!ctx) return;
    ebp = ctx->Ebp;
    manager = exp2_read_safe(ebp - 0x20);
    manager_pid = manager ? exp2_read_safe(manager + 0x7c) : 0;
    field0_from = exp2_read_safe(ebp - 0x28);
    field1_target = ctx->Edi;
    field2_object = ctx->Ebx;
    field3_self = exp2_read_safe(ebp - 0x1c);
    field4_callable = exp2_read_safe(ebp - 0x18);
    lookup_ptr = exp2_read_safe(ebp - 0x14);
    resolver_result = ctx->Esi;
    wsprintfA(buf,
        "KEY6CTX addr=0x%08x EAX=0x%08x manager=0x%08x manager_pid=0x%08x field0_from=0x%08x field1_target=0x%08x field2_object=0x%08x field2_lookup=0x%08x field3_self=0x%08x field3_resolver_ESI=0x%08x field4_callable=0x%08x EFLAGS=0x%08x",
        addr, ctx->Eax, manager, manager_pid, field0_from, field1_target,
        field2_object, lookup_ptr, field3_self, resolver_result,
        field4_callable, ctx->EFlags);
    exp_log(buf);
}

static void exp_log_key6_early_guard(DWORD addr, PCONTEXT ctx)
{
    char buf[512];
    const char *label = "unknown";
    DWORD ebp, manager, manager_pid, field0_from, field1_target;
    DWORD field2_object, field3_self, field4_callable, lookup_ptr;
    DWORD zf;
    if (!ctx) return;
    if (addr == VA_KEY6_TARGET_CMP) label = "cmp target manager_pid";
    else if (addr == VA_KEY6_TARGET_CUR_JZ) label = "jz target manager_pid";
    else if (addr == VA_KEY6_TARGET_NEG1_JZ) label = "jz target -1";
    else if (addr == VA_KEY6_TARGET_REJECT) label = "jnz reject unless 0x80000000";
    else if (addr == VA_KEY6_CLEANUP_ENTRY) label = "cleanup entry";
    else if (addr == VA_KEY6_CLEANUP_CALL) label = "cleanup helper call";
    ebp = ctx->Ebp;
    manager = exp2_read_safe(ebp - 0x20);
    manager_pid = manager ? exp2_read_safe(manager + 0x7c) : 0;
    field0_from = exp2_read_safe(ebp - 0x28);
    field1_target = ctx->Edi;
    field2_object = ctx->Ebx;
    field3_self = exp2_read_safe(ebp - 0x1c);
    field4_callable = exp2_read_safe(ebp - 0x18);
    lookup_ptr = exp2_read_safe(ebp - 0x14);
    zf = (ctx->EFlags >> 6) & 1u;
    wsprintfA(buf,
        "KEY6EARLY addr=0x%08x label=\"%s\" zf=%u eax=0x%08x edi_target=0x%08x manager=0x%08x manager_pid=0x%08x",
        addr, label, zf, ctx->Eax, field1_target, manager, manager_pid);
    exp_log(buf);
    wsprintfA(buf,
        "KEY6EARLY fields from=0x%08x target=0x%08x object=0x%08x self=0x%08x callable=0x%08x lookup=0x%08x esi=0x%08x",
        field0_from, field1_target, field2_object, field3_self,
        field4_callable, lookup_ptr, ctx->Esi);
    exp_log(buf);
}

static void exp_log_callmode_result(DWORD addr, PCONTEXT ctx)
{
    char buf[512];
    DWORD ebp, param1, param2, param3, param4, param5, param6;
    DWORD vtbl, slot3c, slot44, mode;
    if (!ctx) return;
    ebp = ctx->Ebp;
    param1 = exp2_read_safe(ebp + 0x08);
    param2 = exp2_read_safe(ebp + 0x0c);
    param3 = exp2_read_safe(ebp + 0x10);
    param4 = exp2_read_safe(ebp + 0x14);
    param5 = exp2_read_safe(ebp + 0x18);
    param6 = exp2_read_safe(ebp + 0x1c);
    vtbl = param4 ? exp2_read_safe(param4 + 4) : 0;
    slot3c = vtbl ? exp2_read_safe(vtbl + 0x3c) : 0;
    slot44 = vtbl ? exp2_read_safe(vtbl + 0x44) : 0;
    mode = ctx->Eax & 0xffffu;
    wsprintfA(buf,
        "CALLMODE_RESULT addr=0x%08x mode_ax=0x%04x eax=0x%08x param6_in=0x%08x param5=0x%08x",
        addr, mode, ctx->Eax, param6, param5);
    exp_log(buf);
    wsprintfA(buf,
        "CALLMODE_FRAME self=0x%08x field4=0x%08x from=0x%08x stream=0x%08x vtbl=0x%08x slot3c=0x%08x slot44=0x%08x",
        param1, param2, param3, param4, vtbl, slot3c, slot44);
    exp_log(buf);
}

#ifdef EXP_DUMP_EXECUTOR_POSTMODE
static DWORD exp_scope_bucket(DWORD v)
{
    if ((v & 0x000fffffu) != 0)
        return 0x40u;
    if ((v & 0x0ff00000u) != 0)
        return 0x20u;
    if (v == 0)
        return 0x10u;
    return 0;
}

static void exp_log_executor_postmode(DWORD addr, PCONTEXT ctx)
{
    char buf[768];
    DWORD ebp, self, field4, from, stream, param5, param6, carrier_scope;
    if (!ctx) return;
    ebp = ctx->Ebp;
    self = exp2_read_safe(ebp + 0x08);
    field4 = exp2_read_safe(ebp + 0x0c);
    from = exp2_read_safe(ebp + 0x10);
    stream = exp2_read_safe(ebp + 0x14);
    param5 = exp2_read_safe(ebp + 0x18);
    param6 = exp2_read_safe(ebp + 0x1c);
    carrier_scope = self ? exp2_read_safe(self + 0x1c) : 0;
    wsprintfA(buf,
        "POSTMODE addr=0x%08x EAX=0x%08x AL=0x%02x EBX=0x%08x ESI=0x%08x EDI=0x%08x EDX=0x%08x ECX=0x%08x",
        addr, ctx->Eax, ctx->Eax & 0xffu, ctx->Ebx, ctx->Esi, ctx->Edi,
        ctx->Edx, ctx->Ecx);
    exp_log(buf);
    wsprintfA(buf,
        "POSTMODE_FRAME self=0x%08x field4=0x%08x from=0x%08x stream=0x%08x param5=0x%08x param6=0x%08x self+1c=0x%08x",
        self, field4, from, stream, param5, param6, carrier_scope);
    exp_log(buf);
    wsprintfA(buf,
        "POSTMODE_SCOPE bucket(EBX)=0x%02x bucket(ESI)=0x%02x bucket(from)=0x%02x scope_mask_guess=0x%02x",
        exp_scope_bucket(ctx->Ebx), exp_scope_bucket(ctx->Esi),
        exp_scope_bucket(from), ((carrier_scope & 0x70u) | 0x30u));
    exp_log(buf);
}
#endif

#ifdef EXP_DUMP_LUA_CALLABLE_NAME
static void exp_copy_cstr(DWORD ptr, char *out, DWORD out_sz)
{
    DWORD i;
    if (!out || out_sz == 0) return;
    out[0] = 0;
    if (!ptr) return;
    for (i = 0; i + 1 < out_sz; ++i) {
        BYTE c = 0;
        __try {
            c = *(BYTE*)(DWORD_PTR)(ptr + i);
        } __except(1) {
            break;
        }
        if (!c) break;
        out[i] = (c >= 0x20 && c <= 0x7e) ? (char)c : '.';
    }
    out[i] = 0;
}

static DWORD exp_std_string_cstr(DWORD s)
{
    DWORD cap;
    if (!s) return 0;
    cap = exp2_read_safe(s + 0x18);
    if (cap >= 0x10)
        return exp2_read_safe(s + 0x04);
    return s + 0x04;
}

static void exp_log_lua_callable_name(DWORD addr, PCONTEXT ctx)
{
    char name[96];
    char buf[384];
    DWORD stdstr, name_ptr, len, cap;
    if (!ctx) return;
    if (addr == VA_RPC_BC5860_RET) {
        stdstr = ctx->Eax;
        len = exp2_read_safe(stdstr + 0x14);
        cap = exp2_read_safe(stdstr + 0x18);
        name_ptr = exp_std_string_cstr(stdstr);
        exp_copy_cstr(name_ptr, name, sizeof(name));
        wsprintfA(buf,
            "BC5860_RET std=0x%08x len=0x%08x cap=0x%08x name_ptr=0x%08x name=\"%s\"",
            stdstr, len, cap, name_ptr, name);
        exp_log(buf);
        return;
    }
    if (addr == VA_RPC_BA9640_CALL) {
        name_ptr = ctx->Edx;
        exp_copy_cstr(name_ptr, name, sizeof(name));
        wsprintfA(buf,
            "BA9640_ARG name_ptr=0x%08x name=\"%s\" EAX=0x%08x EDI=0x%08x",
            name_ptr, name, ctx->Eax, ctx->Edi);
        exp_log(buf);
        return;
    }
    if (addr == VA_RPC_BA9640_JZ) {
        wsprintfA(buf,
            "BA9640_RET EAX=0x%08x AL=0x%02x EDX=0x%08x EDI=0x%08x",
            ctx->Eax, ctx->Eax & 0xffu, ctx->Edx, ctx->Edi);
        exp_log(buf);
    }
}
#endif

#ifdef EXP_DUMP_LUA_DISPATCH
static void exp_log_lua_dispatch(DWORD addr, PCONTEXT ctx)
{
    char buf[768];
    DWORD esp, ret, a1, a2, a3, a4, a5, a6;
    DWORD ebp, self, field4, from, stream, param5, param6;
    if (!ctx) return;
    esp = ctx->Esp;
    ret = a1 = a2 = a3 = a4 = a5 = a6 = 0;
    __try {
        ret = *(DWORD*)(esp);
        a1 = *(DWORD*)(esp + 4);
        a2 = *(DWORD*)(esp + 8);
        a3 = *(DWORD*)(esp + 0x0c);
        a4 = *(DWORD*)(esp + 0x10);
        a5 = *(DWORD*)(esp + 0x14);
        a6 = *(DWORD*)(esp + 0x18);
    } __except(1) {}
    wsprintfA(buf,
        "LUA_DISPATCH addr=0x%08x ret_or_arg0=0x%08x EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x",
        addr, ret, ctx->Eax, ctx->Ebx, ctx->Ecx, ctx->Edx);
    exp_log(buf);
    wsprintfA(buf,
        "LUA_DISPATCH regs ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x EFLAGS=0x%08x",
        ctx->Esi, ctx->Edi, ctx->Ebp, esp, ctx->EFlags);
    exp_log(buf);
    wsprintfA(buf,
        "LUA_DISPATCH stack a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x a5=0x%08x a6=0x%08x",
        a1, a2, a3, a4, a5, a6);
    exp_log(buf);

    if (addr == VA_RPC_LUA_CALLSITE || addr == VA_RPC_LUA_RET) {
        ebp = ctx->Ebp;
        self = exp2_read_safe(ebp + 0x08);
        field4 = exp2_read_safe(ebp + 0x0c);
        from = exp2_read_safe(ebp + 0x10);
        stream = exp2_read_safe(ebp + 0x14);
        param5 = exp2_read_safe(ebp + 0x18);
        param6 = exp2_read_safe(ebp + 0x1c);
        wsprintfA(buf,
            "LUA_RPC_FRAME self=0x%08x field4=0x%08x from=0x%08x stream=0x%08x param5=0x%08x param6=0x%08x",
            self, field4, from, stream, param5, param6);
        exp_log(buf);
        wsprintfA(buf,
            "LUA_RPC_CALL_ARGS lua=0x%08x count=0x%08x errfunc=0x%08x eax_ret=0x%08x",
            ret, a1, a2, ctx->Eax);
        exp_log(buf);
    }
    if (addr == VA_LUA_DISPATCH_CBDA80) {
        wsprintfA(buf,
            "LUA_CBDA80_ENTRY lua=0x%08x count=0x%08x errfunc=0x%08x",
            a1, a2, a3);
        exp_log(buf);
    }
    if (addr == VA_LUA_PCALL_CBD750) {
        wsprintfA(buf,
            "LUA_PCALL_ENTRY this=0x%08x call_count=0x%08x ECX=0x%08x",
            a1, a2, ctx->Ecx);
        exp_log(buf);
    }
    if (addr == VA_LUA_PCALL_RET_CBDAD2) {
        wsprintfA(buf,
            "LUA_PCALL_RET_PRE_MOV status_eax=0x%08x old_ebx=0x%08x lua=0x%08x",
            ctx->Eax, ctx->Ebx, ctx->Edi);
        exp_log(buf);
    }
    if (addr == VA_LUA_ERROR_UNKNOWN)
        exp_log("LUA_ERROR_PATH unknown-string selected at 0x00cbdb49");
    if (addr == VA_LUA_ERROR_MSGBOX)
        exp_log("LUA_ERROR_PATH _dev_lua_error_msgbox selected at 0x00cbdb77");
}
#endif

#ifdef EXP_FORCE_BF0E30_SCOPE_GATE
static void exp_force_bf0e30_scope_gate(void)
{
    BYTE *p = (BYTE*)(DWORD_PTR)VA_RPC_SCOPE_JZ;
    BYTE b[6];
    DWORD op, i;
    char buf[256];

    __try {
        for (i = 0; i < 6; ++i) b[i] = p[i];
    } __except(1) {
        exp_log("force bf0e30: read failed at P1 scope JZ");
        return;
    }

    wsprintfA(buf,
        "force bf0e30 P1 check @ 0x%08x bytes=%02x %02x %02x %02x %02x %02x expected=0f 84 a0 02 00 00",
        VA_RPC_SCOPE_JZ, b[0], b[1], b[2], b[3], b[4], b[5]);
    exp_log(buf);

    if (b[0] != 0x0f || b[1] != 0x84 || b[2] != 0xa0 ||
        b[3] != 0x02 || b[4] != 0x00 || b[5] != 0x00) {
        exp_log("force bf0e30: expected bytes mismatch; P1 left untouched");
        return;
    }

    if (!VirtualProtect(p, 6, PAGE_EXECUTE_READWRITE, &op)) {
        exp_log("force bf0e30: VirtualProtect failed");
        return;
    }
    for (i = 0; i < 6; ++i) p[i] = 0x90;
    VirtualProtect(p, 6, op, &op);
    FlushInstructionCache(GetCurrentProcess(), p, 6);
    exp_log("force bf0e30: patched P1 JZ to six NOPs");
}
#endif

#ifdef EXP_FORCE_BA9640_GATE
static void exp_force_ba9640_gate(void)
{
    BYTE *p = (BYTE*)(DWORD_PTR)VA_RPC_BA9640_JZ;
    BYTE b[6];
    DWORD op, i;
    char buf[256];

    __try {
        for (i = 0; i < 6; ++i) b[i] = p[i];
    } __except(1) {
        exp_log("force ba9640: read failed at P2 callable JZ");
        return;
    }

    wsprintfA(buf,
        "force ba9640 P2 check @ 0x%08x bytes=%02x %02x %02x %02x %02x %02x expected=0f 84 ee 00 00 00",
        VA_RPC_BA9640_JZ, b[0], b[1], b[2], b[3], b[4], b[5]);
    exp_log(buf);

    if (b[0] != 0x0f || b[1] != 0x84 || b[2] != 0xee ||
        b[3] != 0x00 || b[4] != 0x00 || b[5] != 0x00) {
        exp_log("force ba9640: expected bytes mismatch; P2 left untouched");
        return;
    }

    if (!VirtualProtect(p, 6, PAGE_EXECUTE_READWRITE, &op)) {
        exp_log("force ba9640: VirtualProtect failed");
        return;
    }
    for (i = 0; i < 6; ++i) p[i] = 0x90;
    VirtualProtect(p, 6, op, &op);
    FlushInstructionCache(GetCurrentProcess(), p, 6);
    exp_log("force ba9640: patched P2 JZ to six NOPs");
}
#endif

#if EXP_TARGET_PROFILE == 4
static void exp_force_key6_executor_gate(void)
{
    BYTE *p = (BYTE*)(DWORD_PTR)VA_KEY6_SKIP_EXEC_JZ;
    BYTE b0 = 0, b1 = 0;
    DWORD op;
    char buf[256];

    __try {
        b0 = p[0];
        b1 = p[1];
    } __except(1) {
        exp_log("force key6: read failed at skip JZ");
        return;
    }

    wsprintfA(buf, "force key6 check @ 0x%08x bytes=%02x %02x expected=74 1d",
              VA_KEY6_SKIP_EXEC_JZ, b0, b1);
    exp_log(buf);

    if (b0 != 0x74 || b1 != 0x1d) {
        exp_log("force key6: expected bytes mismatch; branch left untouched");
        return;
    }

    if (!VirtualProtect(p, 2, PAGE_EXECUTE_READWRITE, &op)) {
        exp_log("force key6: VirtualProtect failed");
        return;
    }
    p[0] = 0x90;
    p[1] = 0x90;
    VirtualProtect(p, 2, op, &op);
    FlushInstructionCache(GetCurrentProcess(), p, 2);
    exp_log("force key6: patched JZ skip to NOP NOP");
}
#endif

/* Walk the MSVC std::map at manager+0x1c (the key6 method-descriptor map that
 * FUN_00647a30 searches) and report whether method key 0xeae76408 is present.
 * Node links are _Left(+0)/_Parent(+4)/_Right(+8); we scan each node's dwords
 * for the key so we do not depend on the exact value-slot offset. */
#define SETCTRL_METHOD_KEY 0xeae76408u
static void exp_dump_method_map(DWORD manager)
{
    DWORD map_this, h14, h18, h1c, myhead, root, node;
    DWORD stack[260]; int sp = 0, visited = 0, matches = 0;
    if (!manager) { exp_log("MAPDUMP manager=NULL"); return; }
    map_this = manager + 0x1c;
    h14 = exp2_read_safe(map_this + 0x14);
    h18 = exp2_read_safe(map_this + 0x18);
    h1c = exp2_read_safe(map_this + 0x1c);
    exp_logf("MAPDUMP manager=0x%08x map_this=0x%08x", manager, map_this, 0);
    exp_logf("MAPDUMP +14=0x%08x +18=0x%08x +1c=0x%08x", h14, h18, h1c);
    /* _Myhead is normally +0x18 here. Prefer a sane parent/root over trying to
     * interpret the color/isnil byte, because the first live dump showed +0x1c
     * as size=1 but the old isnil heuristic rejected the valid +0x18 head. */
    myhead = h18;
    root = exp2_read_safe(myhead + 4);
    if (!myhead || !root || root == myhead) {
        DWORD r2;
        myhead = h14;
        r2 = exp2_read_safe(myhead + 4);
        if (myhead && r2 && r2 != myhead) root = r2;
    }
    exp_logf("MAPDUMP myhead=0x%08x root=0x%08x", myhead, root, 0);
    if (root && myhead && root != myhead) {
        stack[sp++] = root;
        while (sp > 0 && visited < 250) {
            DWORD l, r; int k;
            node = stack[--sp];
            if (!node || node == myhead) continue;
            for (k = 8; k <= 0x1c; k += 4) {
                if (exp2_read_safe(node + k) == SETCTRL_METHOD_KEY) {
                    exp_logf("MAPDUMP MATCH key @ node=0x%08x off=0x%02x", node, k, 0);
                    ++matches;
                }
            }
            if (visited < 24)
                exp_logf("MAPNODE 0x%08x dw08=0x%08x dw0c=0x%08x", node,
                         exp2_read_safe(node + 8), exp2_read_safe(node + 0xc));
            ++visited;
            l = exp2_read_safe(node + 0);
            r = exp2_read_safe(node + 8);
            if (l && l != myhead && sp < 256) stack[sp++] = l;
            if (r && r != myhead && sp < 256) stack[sp++] = r;
        }
    }
    exp_logf("MAPDUMP done visited=%u matches=%u key=0xeae76408", visited, matches, 0);
}

/* Log a node's dword window (these compact tree nodes are NOT standard MSVC
 * layout, so we print the raw fields and read keys/values offline). */
static void exp_log_node(const char *tag, DWORD node)
{
    char buf[256];
    if (!node) { exp_logf("%s node=NULL", (DWORD)(DWORD_PTR)tag, 0, 0); return; }
    wsprintfA(buf, "%s node=0x%08x +0=%08x +4=%08x +8=%08x +c=%08x +10=%08x +14=%08x +18=%08x",
        tag, node, exp2_read_safe(node), exp2_read_safe(node + 4), exp2_read_safe(node + 8),
        exp2_read_safe(node + 0xc), exp2_read_safe(node + 0x10), exp2_read_safe(node + 0x14),
        exp2_read_safe(node + 0x18));
    exp_log(buf);
}

static void exp_log_map_candidate(const char *tag, DWORD map)
{
    char buf[256];
    int off;
    DWORD head, root;
    if (!map) {
        wsprintfA(buf, "%s map=NULL", tag);
        exp_log(buf);
        return;
    }
    wsprintfA(buf, "%s map=0x%08x +0=%08x +4=%08x +8=%08x +c=%08x +10=%08x +14=%08x +18=%08x",
        tag, map, exp2_read_safe(map), exp2_read_safe(map + 4), exp2_read_safe(map + 8),
        exp2_read_safe(map + 0xc), exp2_read_safe(map + 0x10), exp2_read_safe(map + 0x14),
        exp2_read_safe(map + 0x18));
    exp_log(buf);
    for (off = 0; off <= 0x18; off += 4) {
        head = exp2_read_safe(map + off);
        root = head ? exp2_read_safe(head + 4) : 0;
        wsprintfA(buf, "%s headoff=0x%02x head=0x%08x root=0x%08x", tag, off, head, root);
        exp_log(buf);
        exp_log_node("MAPROOT", root);
        if (root && root != head) {
            exp_log_node("MAP_L", exp2_read_safe(root + 0));
            exp_log_node("MAP_R", exp2_read_safe(root + 8));
        }
    }
}

static void exp_scan_map_for_key(const char *tag, DWORD map, DWORD wanted)
{
    DWORD myhead, root, node;
    DWORD stack[260];
    int sp = 0, visited = 0, matches = 0;
    char buf[256];
    if (!map) {
        wsprintfA(buf, "%s map=NULL wanted=0x%08x", tag, wanted);
        exp_log(buf);
        return;
    }
    myhead = exp2_read_safe(map + 4);
    root = myhead ? exp2_read_safe(myhead + 4) : 0;
    wsprintfA(buf, "%s map=0x%08x size=0x%08x wanted=0x%08x root=0x%08x",
        tag, map, exp2_read_safe(map + 8), wanted, root);
    exp_log(buf);
    if (!myhead || !root || root == myhead)
        return;
    stack[sp++] = root;
    while (sp > 0 && visited < 250) {
        DWORD l, r;
        int off;
        node = stack[--sp];
        if (!node || node == myhead) continue;
        for (off = 8; off <= 0x18; off += 4) {
            if (exp2_read_safe(node + off) == wanted) {
                wsprintfA(buf, "%s MATCH node=0x%08x off=0x%02x value=0x%08x",
                    tag, node, off, wanted);
                exp_log(buf);
                ++matches;
            }
        }
        ++visited;
        l = exp2_read_safe(node + 0);
        r = exp2_read_safe(node + 8);
        if (l && l != myhead && sp < 256) stack[sp++] = l;
        if (r && r != myhead && sp < 256) stack[sp++] = r;
    }
    wsprintfA(buf, "%s done visited=%u matches=%u wanted=0x%08x",
        tag, visited, matches, wanted);
    exp_log(buf);
}

#ifdef EXP_INJECT_SESSION_OBJ18
static DWORD exp_find_map_value_by_key(DWORD map, DWORD wanted, DWORD *node_out)
{
    DWORD myhead, root, node;
    DWORD stack[260];
    int sp = 0, visited = 0;
    if (node_out) *node_out = 0;
    if (!map) return 0;
    myhead = exp2_read_safe(map + 4);
    root = myhead ? exp2_read_safe(myhead + 4) : 0;
    if (!myhead || !root || root == myhead) return 0;
    stack[sp++] = root;
    while (sp > 0 && visited < 250) {
        DWORD l, r, key;
        node = stack[--sp];
        if (!node || node == myhead) continue;
        key = exp2_read_safe(node + 0x0c);
        if (key == wanted) {
            if (node_out) *node_out = node;
            return exp2_read_safe(node + 0x10);
        }
        ++visited;
        l = exp2_read_safe(node + 0);
        r = exp2_read_safe(node + 8);
        if (l && l != myhead && sp < 256) stack[sp++] = l;
        if (r && r != myhead && sp < 256) stack[sp++] = r;
    }
    return 0;
}
#endif

/* FUN_00bd3660 searches a std::map at M=*(object+0x18); end/_Myhead=*(M+4),
 * size=*(M+8), value@node+0x10. Dump the root + its children so we can see
 * which self/component keys exist (current wire self_key=2). */
static void exp_dump_self_map(DWORD object, DWORD self_key)
{
    DWORD m, myhead, root;
    exp_logf("SELFDUMP object=0x%08x self_key=0x%08x obj+18=0x%08x",
             object, self_key, exp2_read_safe(object + 0x18));
    if (!object) return;
    m = exp2_read_safe(object + 0x18);
    if (!m) { exp_log("SELFDUMP M=NULL"); return; }
    myhead = exp2_read_safe(m + 4);
    root = exp2_read_safe(myhead + 4);
    exp_logf("SELFDUMP M=0x%08x myhead=0x%08x size?=0x%08x", m, myhead, exp2_read_safe(m + 8));
    exp_logf("SELFDUMP root=0x%08x", root, 0, 0);
    exp_log_node("SELFROOT", root);
    exp_log_node("SELF_L", exp2_read_safe(root + 0));
    exp_log_node("SELF_R", exp2_read_safe(root + 8));
    exp_log_map_candidate("SELF_OBJ14", exp2_read_safe(object + 0x14));
    exp_log_map_candidate("SELF_OBJ18", exp2_read_safe(object + 0x18));
    exp_scan_map_for_key("SELF_OBJ14_SCAN_CONNECTOR", exp2_read_safe(object + 0x14), KEY_CONNECTOR_NID);
    exp_scan_map_for_key("SELF_OBJ18_SCAN_KEY2", exp2_read_safe(object + 0x18), 2);
    exp_scan_map_for_key("SELF_OBJ18_SCAN_CONNECTOR", exp2_read_safe(object + 0x18), KEY_CONNECTOR_NID);
}

#ifdef EXP_DUMP_B4_DESCRIPTORS
static void exp_b4_window(const char *label, DWORD addr)
{
    char buf[512];
    wsprintfA(buf,
        "B4DESC WINDOW %s addr=0x%08x +00=%08x +04=%08x +08=%08x +0c=%08x +10=%08x +14=%08x +18=%08x +1c=%08x +20=%08x +24=%08x +28=%08x +2c=%08x",
        label, addr,
        exp2_read_safe(addr + 0x00), exp2_read_safe(addr + 0x04),
        exp2_read_safe(addr + 0x08), exp2_read_safe(addr + 0x0c),
        exp2_read_safe(addr + 0x10), exp2_read_safe(addr + 0x14),
        exp2_read_safe(addr + 0x18), exp2_read_safe(addr + 0x1c),
        exp2_read_safe(addr + 0x20), exp2_read_safe(addr + 0x24),
        exp2_read_safe(addr + 0x28), exp2_read_safe(addr + 0x2c));
    exp_log(buf);
}

static void exp_b4_walk_desc(const char *label, DWORD desc)
{
    DWORD head, node;
    int count = 0, active = 0;
    char buf[768];
    head = exp2_read_safe(desc + 0x28);
    node = exp2_read_safe(head);
    wsprintfA(buf,
        "B4DESC WALK %s desc=0x%08x desc_key=0x%08x desc_kind=0x%08x list_head=0x%08x first=0x%08x",
        label, desc, exp2_read_safe(desc + 0x0c), exp2_read_safe(desc + 0x10), head, node);
    exp_log(buf);
    if (!desc || !head || !node || node == head) {
        exp_log("B4DESC WALK empty-or-unready");
        return;
    }
    while (node && node != head && count < 180) {
        DWORD type_ref = exp2_read_safe(node + 0x0c);
        DWORD mode = exp2_read_safe(node + 0x18);
        DWORD offset = exp2_read_safe(node + 0x20);
        DWORD type_code = type_ref ? exp2_read_safe(type_ref + 0x0c) : 0;
        DWORD next = exp2_read_safe(node);
        if (mode == 1 || mode == 2 || mode == 3 || mode == 4)
            ++active;
        wsprintfA(buf,
            "B4DESC FIELD %s idx=%03d node=0x%08x next=0x%08x mode=%u offset=0x%08x type_ref=0x%08x type_code=0x%08x raw0c=0x%08x raw10=0x%08x raw14=0x%08x raw1c=0x%08x raw24=0x%08x",
            label, count, node, next, mode, offset, type_ref, type_code,
            exp2_read_safe(node + 0x0c), exp2_read_safe(node + 0x10),
            exp2_read_safe(node + 0x14), exp2_read_safe(node + 0x1c),
            exp2_read_safe(node + 0x24));
        exp_log(buf);
        if (type_ref) {
            wsprintfA(buf,
                "B4DESC TYPE %s idx=%03d type_ref=0x%08x +00=%08x +04=%08x +08=%08x +0c=%08x +10=%08x +14=%08x",
                label, count, type_ref,
                exp2_read_safe(type_ref + 0x00), exp2_read_safe(type_ref + 0x04),
                exp2_read_safe(type_ref + 0x08), exp2_read_safe(type_ref + 0x0c),
                exp2_read_safe(type_ref + 0x10), exp2_read_safe(type_ref + 0x14));
            exp_log(buf);
        }
        node = next;
        ++count;
    }
    wsprintfA(buf, "B4DESC DONE %s fields=%d replicated_modes=%d stopped_node=0x%08x",
              label, count, active, node);
    exp_log(buf);
}

static void exp_b4_vector(const char *label, DWORD owner)
{
    DWORD begin = exp2_read_safe(owner + 0x24);
    DWORD end = exp2_read_safe(owner + 0x28);
    DWORD cap = exp2_read_safe(owner + 0x2c);
    DWORD n = 0, i;
    char buf[512];
    if (end >= begin && begin >= 0x00400000u && begin < 0x20000000u)
        n = (end - begin) / 4;
    wsprintfA(buf,
        "B4DESC VECTOR %s owner=0x%08x begin=0x%08x end=0x%08x cap=0x%08x ptr_count=%u",
        label, owner, begin, end, cap, n);
    exp_log(buf);
    if (n > 80) n = 80;
    for (i = 0; i < n; ++i) {
        DWORD p = exp2_read_safe(begin + i * 4);
        wsprintfA(buf,
            "B4DESC VECELT %s idx=%03u ptr=0x%08x +00=%08x +04=%08x +08=%08x +0c=%08x +10=%08x +14=%08x +18=%08x +1c=%08x +20=%08x +24=%08x",
            label, i, p,
            exp2_read_safe(p + 0x00), exp2_read_safe(p + 0x04),
            exp2_read_safe(p + 0x08), exp2_read_safe(p + 0x0c),
            exp2_read_safe(p + 0x10), exp2_read_safe(p + 0x14),
            exp2_read_safe(p + 0x18), exp2_read_safe(p + 0x1c),
            exp2_read_safe(p + 0x20), exp2_read_safe(p + 0x24));
        exp_log(buf);
    }
}

static int exp_b4_seen(DWORD *seen, int seen_count, DWORD node)
{
    int i;
    for (i = 0; i < seen_count; ++i) {
        if (seen[i] == node) return 1;
    }
    return 0;
}

static void exp_b4_bucket_chains(const char *label, DWORD owner)
{
    DWORD begin = exp2_read_safe(owner + 0x24);
    DWORD end = exp2_read_safe(owner + 0x28);
    DWORD sentinel = exp2_read_safe(owner + 0x18);
    DWORD declared = exp2_read_safe(owner + 0x1c);
    DWORD n = 0, i;
    DWORD seen[160];
    int seen_count = 0;
    char buf[768];

    if (end >= begin && begin >= 0x00400000u && begin < 0x20000000u)
        n = (end - begin) / 4;
    wsprintfA(buf,
        "B4DESC BUCKETS %s owner=0x%08x sentinel=0x%08x declared=0x%08x slots=%u",
        label, owner, sentinel, declared, n);
    exp_log(buf);
    if (!n || n > 80) return;

    /*
     * Runtime evidence shows the descriptor vector stores pairs:
     * [owner/sentinel-ish ptr, bucket-head].  Follow odd entries through
     * node+0 until the descriptor sentinel; these are the bb0060 field nodes.
     */
    for (i = 1; i < n; i += 2) {
        DWORD node = exp2_read_safe(begin + i * 4);
        DWORD depth = 0;
        while (node && node != sentinel && node >= 0x00400000u &&
               node < 0x20000000u && depth < 80 && seen_count < 160) {
            DWORD next = exp2_read_safe(node + 0x00);
            DWORD prev = exp2_read_safe(node + 0x04);
            DWORD field_key = exp2_read_safe(node + 0x08);
            DWORD type_key = exp2_read_safe(node + 0x0c);
            DWORD type_a = exp2_read_safe(node + 0x10);
            DWORD type_b = exp2_read_safe(node + 0x14);
            DWORD mode = exp2_read_safe(node + 0x18);
            DWORD raw1c = exp2_read_safe(node + 0x1c);
            DWORD offset = exp2_read_safe(node + 0x20);
            DWORD raw24 = exp2_read_safe(node + 0x24);
            if (exp_b4_seen(seen, seen_count, node)) {
                wsprintfA(buf,
                    "B4DESC BUCKET_LOOP %s bucket=%u depth=%u node=0x%08x",
                    label, i, depth, node);
                exp_log(buf);
                break;
            }
            seen[seen_count++] = node;
            wsprintfA(buf,
                "B4DESC BUCKETFIELD %s bucket=%u depth=%u node=0x%08x next=0x%08x prev=0x%08x field_key=0x%08x type_key=0x%08x type_a=0x%08x type_b=0x%08x mode=%u raw1c=0x%08x offset=0x%08x raw24=0x%08x",
                label, i, depth, node, next, prev, field_key, type_key,
                type_a, type_b, mode, raw1c, offset, raw24);
            exp_log(buf);
            node = next;
            ++depth;
        }
    }
    wsprintfA(buf, "B4DESC BUCKET_DONE %s unique_nodes=%d declared=0x%08x",
              label, seen_count, declared);
    exp_log(buf);
}

static void exp_b4_candidate_ptrs(const char *name, DWORD owner)
{
    int off;
    char label[96];
    for (off = 0x08; off <= 0x2c; off += 4) {
        DWORD p = exp2_read_safe(owner + (DWORD)off);
        if (p < 0x00400000u || p >= 0x20000000u) continue;
        wsprintfA(label, "%s.ptr_%02x", name, off);
        exp_b4_window(label, p);
        exp_b4_walk_desc(label, p);
        exp_b4_vector(label, p);
    }
}

static void exp_b4_dump_pair(const char *name, DWORD base, DWORD key)
{
    char label[96];
    exp_logf("B4DESC BEGIN %s base=0x%08x key=0x%08x", (DWORD)(DWORD_PTR)name, base, key);
    wsprintfA(label, "%s.base", name);
    exp_b4_window(label, base);
    wsprintfA(label, "%s.key", name);
    exp_b4_window(label, key);
    exp_b4_vector(label, key);
    exp_b4_bucket_chains(label, key);
}

static void exp_dump_b4_descriptors(const char *reason)
{
    static volatile LONG dump_count = 0;
    LONG n = InterlockedIncrement(&dump_count);
    if (n > 4) return;
    exp_logf("B4DESC SNAPSHOT #%u reason_ptr=0x%08x", (DWORD)n, (DWORD)(DWORD_PTR)reason, 0);
    if (reason) {
        char buf[128];
        wsprintfA(buf, "B4DESC REASON %s", reason);
        exp_log(buf);
    }
    exp_b4_dump_pair("Player", VA_B4_PLAYER_DESC_BASE, VA_B4_PLAYER_DESC_KEY);
    exp_b4_dump_pair("CStatusPlayer", VA_B4_CSTATUS_DESC_BASE, VA_B4_CSTATUS_DESC_KEY);
    exp_b4_dump_pair("User", VA_B4_USER_DESC_BASE, VA_B4_USER_DESC_KEY);
    exp_b4_dump_pair("Session", VA_SESSION_DESC_BASE, VA_SESSION_DESC_KEY);
    exp_b4_dump_pair("SessionSelf", VA_SESSION_DESC_BASE, VA_SESSION_SELF_DESC);
    exp_b4_dump_pair("ChannelContext", VA_CHANNELCTX_DESC_BASE, VA_CHANNELCTX_DESC_KEY);
}
#endif

#ifdef EXP_INJECT_CONTROL_OBJ18
typedef void (__stdcall *Obj18RegisterFn)(DWORD object, DWORD descriptor);
typedef void (__stdcall *Obj18KeyValueFn)(DWORD object, DWORD key, DWORD value);
static volatile DWORD g_control_obj18_injected = 0;

static void exp_inject_control_obj18(DWORD object, DWORD self_key)
{
    DWORD cls, pid, desc, desc_vtbl, desc_key;
    cls = exp2_read_safe(object + 0x38);
    pid = exp2_read_safe(object + 0x3c);
    desc = VA_CONTROL_DESC_STORAGE;
    desc_vtbl = exp2_read_safe(desc);
    desc_key = exp2_read_safe(desc + 0x0c);

    exp_logf("INJECTCTRL candidate obj=0x%08x cls=0x%08x pid=0x%08x",
             object, cls, pid);
    exp_logf("INJECTCTRL self_key=0x%08x desc=0x%08x desc_vtbl=0x%08x",
             self_key, desc, desc_vtbl);
    exp_logf("INJECTCTRL desc_key=0x%08x already=0x%08x", desc_key,
             g_control_obj18_injected, 0);

    if (!object || cls != KEY_TCLIENT_NID) {
        exp_log("INJECTCTRL skip: not TClient");
        return;
    }
    if (self_key != 2) {
        exp_log("INJECTCTRL skip: self_key is not m_ControlObj key 2");
        return;
    }
    if (!desc_vtbl || desc_key != 2) {
        exp_log("INJECTCTRL skip: DAT_0183ef78 descriptor not initialized as key 2");
        return;
    }
    if (g_control_obj18_injected == object) {
        exp_log("INJECTCTRL skip: object already injected once");
        return;
    }

    __try {
#ifdef EXP_INSERT_CONTROL_DIRECT
        ((Obj18KeyValueFn)(DWORD_PTR)VA_OBJ18_REG_BD3AE0)(object, 2, desc);
        g_control_obj18_injected = object;
        exp_log("INJECTCTRL call FUN_00bd3ae0(object,2,DAT_0183ef78) ok");
#else
        ((Obj18RegisterFn)(DWORD_PTR)VA_OBJ18_REG_BD2CD0)(object, desc);
        g_control_obj18_injected = object;
        exp_log("INJECTCTRL call FUN_00bd2cd0(object,DAT_0183ef78) ok");
#endif
    } __except(1) {
#ifdef EXP_INSERT_CONTROL_DIRECT
        exp_log("INJECTCTRL exception during FUN_00bd3ae0 call");
#else
        exp_log("INJECTCTRL exception during FUN_00bd2cd0 call");
#endif
        return;
    }

    exp_dump_self_map(object, 2);
}
#endif

#ifdef EXP_INJECT_SESSION_OBJ18
typedef void (__stdcall *Obj18SessionKeyValueFn)(DWORD object, DWORD key, DWORD value);
typedef void (__stdcall *Obj18SessionRegisterFn)(DWORD object, DWORD component);
static volatile DWORD g_session_obj18_injected = 0;

static void exp_inject_session_obj18(DWORD object, DWORD self_key)
{
    DWORD cls, pid, map14, map18, node14, node18, value14, value18;
    cls = exp2_read_safe(object + 0x38);
    pid = exp2_read_safe(object + 0x3c);
    map14 = exp2_read_safe(object + 0x14);
    map18 = exp2_read_safe(object + 0x18);
    node14 = 0;
    node18 = 0;
    value14 = exp_find_map_value_by_key(map14, KEY_SESSION_NID, &node14);
    value18 = exp_find_map_value_by_key(map18, KEY_SESSION_NID, &node18);

    exp_logf("INJECTSESS candidate obj=0x%08x cls=0x%08x pid=0x%08x",
             object, cls, pid);
    exp_logf("INJECTSESS self_key=0x%08x map14=0x%08x map18=0x%08x",
             self_key, map14, map18);
    exp_logf("INJECTSESS node14=0x%08x val14=0x%08x node18=0x%08x",
             node14, value14, node18);
    exp_logf("INJECTSESS val18=0x%08x already=0x%08x", value18,
             g_session_obj18_injected, 0);
    exp_logf("INJECTSESS val14_vt=0x%08x val14_key=0x%08x val14_owner=0x%08x",
             exp2_read_safe(value14), exp2_read_safe(value14 + 0x0c),
             exp2_read_safe(value14 + 0x14));

    if (!object || cls != KEY_TACCOUNT_NID) {
        exp_log("INJECTSESS skip: not TAccount");
        return;
    }
    if (self_key != KEY_SESSION_NID) {
        exp_log("INJECTSESS skip: self_key is not Session");
        return;
    }
    if (!value14) {
        exp_log("INJECTSESS skip: Session not found in obj+0x14");
        return;
    }
    if (value18) {
        exp_log("INJECTSESS skip: Session already present in obj+0x18");
        return;
    }
    if (g_session_obj18_injected == object) {
        exp_log("INJECTSESS skip: object already injected once");
        return;
    }

    __try {
#ifdef EXP_INJECT_SESSION_OBJ18_BD2CD0
        ((Obj18SessionRegisterFn)(DWORD_PTR)VA_OBJ18_REG_BD2CD0)(
            object, value14);
        g_session_obj18_injected = object;
        exp_log("INJECTSESS call FUN_00bd2cd0(object,value14) ok");
#else
        ((Obj18SessionKeyValueFn)(DWORD_PTR)VA_OBJ18_REG_BD3AE0)(
            object, KEY_SESSION_NID, value14);
        g_session_obj18_injected = object;
        exp_log("INJECTSESS call FUN_00bd3ae0(object,Session,value14) ok");
#endif
    } __except(1) {
#ifdef EXP_INJECT_SESSION_OBJ18_BD2CD0
        exp_log("INJECTSESS exception during FUN_00bd2cd0 call");
#else
        exp_log("INJECTSESS exception during FUN_00bd3ae0 call");
#endif
        return;
    }

    exp_dump_self_map(object, KEY_SESSION_NID);
}
#endif

#ifdef EXP_INJECT_B4_COMPONENT_OBJ18
typedef void (__stdcall *Obj18ComponentRegisterFn)(DWORD object, DWORD component);

static int exp_is_b4_self_component_key(DWORD key)
{
    return key == KEY_SESSION_NID || key == KEY_PLAYER_NID ||
           key == KEY_CSTATUSPLAYER_NID || key == KEY_USER_NID;
}

static void exp_inject_b4_component_obj18(DWORD object, DWORD component, DWORD key)
{
    DWORD map18, node18, before, after;
    char buf[256];
    if (!object || !component || !exp_is_b4_self_component_key(key))
        return;

    map18 = exp2_read_safe(object + 0x18);
    node18 = 0;
    before = exp_find_map_value_by_key(map18, key, &node18);
    wsprintfA(buf,
              "B4OBJ18 candidate obj=0x%08x component=0x%08x key=0x%08x",
              object, component, key);
    exp_log(buf);
    wsprintfA(buf,
              "B4OBJ18 before map18=0x%08x node=0x%08x value=0x%08x",
              map18, node18, before);
    exp_log(buf);
    if (before) {
        exp_log("B4OBJ18 skip: key already present in obj+0x18");
        return;
    }

    __try {
        ((Obj18ComponentRegisterFn)(DWORD_PTR)VA_OBJ18_REG_BD2CD0)(
            object, component);
        exp_log("B4OBJ18 call FUN_00bd2cd0(object,component) ok");
    } __except(1) {
        exp_log("B4OBJ18 exception during FUN_00bd2cd0 call");
        return;
    }

    after = exp_find_map_value_by_key(exp2_read_safe(object + 0x18), key, &node18);
    wsprintfA(buf,
              "B4OBJ18 after node=0x%08x value=0x%08x component=0x%08x",
              node18, after, component);
    exp_log(buf);
}
#endif

#ifdef EXP_DUMP_OBJ18_WRITERS
static void exp_log_obj18_candidate(const char *tag, DWORD object, int force_dump)
{
    char buf[256];
    DWORD map, head, size, root;
    if (!object) return;
    map = exp2_read_safe(object + 0x18);
    if (!map) return;
    head = exp2_read_safe(map + 4);
    size = exp2_read_safe(map + 8);
    root = head ? exp2_read_safe(head + 4) : 0;
    wsprintfA(buf,
        "%s object=0x%08x obj+18=0x%08x head=0x%08x root=0x%08x size=0x%08x",
        tag, object, map, head, root, size);
    exp_log(buf);
    if (force_dump || size)
        exp_log_map_candidate(tag, map);
}

static void exp_log_obj18_writer(DWORD addr, PCONTEXT c)
{
    DWORD esp, a1, a2, a3, a4;
    DWORD key_ecx, key_edx, key_a1, key_a2, key_a3;
    char buf[512];
    if (!c) return;
    esp = c->Esp;
    a1 = exp2_read_safe(esp + 4);
    a2 = exp2_read_safe(esp + 8);
    a3 = exp2_read_safe(esp + 0xc);
    a4 = exp2_read_safe(esp + 0x10);
    key_ecx = exp2_read_safe(c->Ecx + 0xc);
    key_edx = exp2_read_safe(c->Edx + 0xc);
    key_a1 = exp2_read_safe(a1 + 0xc);
    key_a2 = exp2_read_safe(a2 + 0xc);
    key_a3 = exp2_read_safe(a3 + 0xc);

    wsprintfA(buf,
        "OBJ18HIT addr=0x%08x EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x ESI=0x%08x EDI=0x%08x",
        addr, c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi);
    exp_log(buf);
    wsprintfA(buf,
        "OBJ18ARGS addr=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x",
        addr, a1, a2, a3, a4);
    exp_log(buf);
    wsprintfA(buf,
        "OBJ18KEYCAND ecx+c=0x%08x edx+c=0x%08x a1+c=0x%08x a2+c=0x%08x a3+c=0x%08x",
        key_ecx, key_edx, key_a1, key_a2, key_a3);
    exp_log(buf);

#if EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
    if (addr == VA_OBJ18_REG_BD2CD0 && key_a2 == KEY_CHARMGR_NID) {
        g_last_charmgr = a2;
        g_last_charmgr_owner = a1;
        wsprintfA(buf,
            "CHARMGR_CAPTURE owner=0x%08x component=0x%08x key=0x%08x",
            a1, a2, key_a2);
        exp_log(buf);
    }
#endif

    if (addr == VA_TCLIENT_POP_C5A880) {
        exp_log("OBJ18 TCLIENT_POPULATOR_REACHED");
        exp_log_obj18_candidate("TCLIENT_ECX", c->Ecx, 1);
        exp_log_obj18_candidate("TCLIENT_A1", a1, 1);
    }

    if (addr == VA_OBJ18_WRITE_BD2D4D || addr == VA_OBJ18_WRITE_BD3B15 ||
        addr == VA_OBJ18_WRITE_BD9D5F) {
        DWORD slot = c->Ebx;
        DWORD node = slot >= 0x10 ? slot - 0x10 : 0;
        DWORD key = exp2_read_safe(node + 0xc);
        DWORD oldval = exp2_read_safe(slot);
        wsprintfA(buf,
            "OBJ18WRITE addr=0x%08x slot=0x%08x node=0x%08x key=0x%08x oldval=0x%08x newval(ECX)=0x%08x",
            addr, slot, node, key, oldval, c->Ecx);
        exp_log(buf);
        if (key == 2)
            exp_log("OBJ18WRITE KEY2_SEEN");
        if (key == KEY_SESSION_NID)
            exp_log("OBJ18WRITE SESSION_KEY_SEEN");
        exp_log_node("OBJ18WRITE_NODE", node);
    }

    if (key_ecx == 2 || key_edx == 2 || key_a1 == 2 || key_a2 == 2 ||
        key_a3 == 2 || a1 == 2 || a2 == 2 || a3 == 2)
        exp_log("OBJ18 KEY2_CANDIDATE_AT_ENTRY");
    if (key_ecx == KEY_SESSION_NID || key_edx == KEY_SESSION_NID ||
        key_a1 == KEY_SESSION_NID || key_a2 == KEY_SESSION_NID ||
        key_a3 == KEY_SESSION_NID || a1 == KEY_SESSION_NID ||
        a2 == KEY_SESSION_NID || a3 == KEY_SESSION_NID)
        exp_log("OBJ18 SESSION_KEY_CANDIDATE_AT_ENTRY");

    exp_log_obj18_candidate("OBJ18_ECX", c->Ecx, 0);
    exp_log_obj18_candidate("OBJ18_A1", a1, 0);
    exp_log_obj18_candidate("OBJ18_A2", a2, 0);
    exp_log_obj18_candidate("OBJ18_ESI", c->Esi, 0);
    exp_log_obj18_candidate("OBJ18_EDI", c->Edi, 0);
}

#ifdef EXP_DUMP_WRITER_CENSUS
static void exp_log_census_obj(const char *tag, DWORD obj)
{
    char buf[512];
    if (!obj) {
        wsprintfA(buf, "%s obj=NULL", tag);
        exp_log(buf);
        return;
    }
    wsprintfA(buf,
        "%s obj=0x%08x vt=0x%08x cls38=0x%08x pid3c=0x%08x pid7c=0x%08x +40=0x%08x +48=0x%08x obj14=0x%08x obj18=0x%08x",
        tag, obj, exp2_read_safe(obj), exp2_read_safe(obj + 0x38),
        exp2_read_safe(obj + 0x3c), exp2_read_safe(obj + 0x7c),
        exp2_read_safe(obj + 0x40), exp2_read_safe(obj + 0x48),
        exp2_read_safe(obj + 0x14), exp2_read_safe(obj + 0x18));
    exp_log(buf);
}

static void exp_log_census_desc(const char *tag, DWORD desc)
{
    char buf[384];
    if (!desc) {
        wsprintfA(buf, "%s desc=NULL", tag);
        exp_log(buf);
        return;
    }
    wsprintfA(buf,
        "%s desc=0x%08x vt=0x%08x +04=0x%08x +08=0x%08x key0c=0x%08x +10=0x%08x +14=0x%08x +24=0x%08x +26=0x%08x",
        tag, desc, exp2_read_safe(desc), exp2_read_safe(desc + 4),
        exp2_read_safe(desc + 8), exp2_read_safe(desc + 0xc),
        exp2_read_safe(desc + 0x10), exp2_read_safe(desc + 0x14),
        exp2_read_safe(desc + 0x24), exp2_read_safe(desc + 0x26));
    exp_log(buf);
}

static void exp_log_writer_census(DWORD addr, PCONTEXT c)
{
    DWORD esp, stack0, a1, a2, a3, a4, ebp_ret;
    char buf[512];
    if (!c) return;
    esp = c->Esp;
    stack0 = exp2_read_safe(esp);
    a1 = exp2_read_safe(esp + 4);
    a2 = exp2_read_safe(esp + 8);
    a3 = exp2_read_safe(esp + 0xc);
    a4 = exp2_read_safe(esp + 0x10);
    ebp_ret = exp2_read_safe(c->Ebp + 4);

    wsprintfA(buf,
        "CENSUS_HIT addr=0x%08x stack0=0x%08x ebp_ret=0x%08x EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x ESI=0x%08x EDI=0x%08x",
        addr, stack0, ebp_ret, c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi);
    exp_log(buf);
    wsprintfA(buf,
        "CENSUS_ARGS addr=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x esp=0x%08x ebp=0x%08x",
        addr, a1, a2, a3, a4, esp, c->Ebp);
    exp_log(buf);

    if (addr == VA_OBJ18_REG_BD2CD0) {
        wsprintfA(buf,
            "CENSUS_BD2CD0_ENTRY ret=0x%08x object=0x%08x desc=0x%08x desc_key=0x%08x",
            stack0, a1, a2, exp2_read_safe(a2 + 0xc));
        exp_log(buf);
        exp_log_census_obj("CENSUS_BD2CD0_OBJECT", a1);
        exp_log_census_desc("CENSUS_BD2CD0_DESC", a2);
        if (exp2_read_safe(a2 + 0xc) == 2)
            exp_log("CENSUS_KEY2_SEEN at BD2CD0 descriptor");
        if (exp2_read_safe(a2 + 0xc) == KEY_SESSION_NID)
            exp_log("CENSUS_SESSION_KEY_SEEN at BD2CD0 descriptor");
    } else if (addr == VA_OBJ18_REG_BD3AE0) {
        wsprintfA(buf,
            "CENSUS_BD3AE0_ENTRY ret=0x%08x object=0x%08x key=0x%08x value=0x%08x",
            stack0, a1, a2, a3);
        exp_log(buf);
        exp_log_census_obj("CENSUS_BD3AE0_OBJECT", a1);
        exp_log_census_obj("CENSUS_BD3AE0_VALUE_OBJ?", a3);
        exp_log_census_desc("CENSUS_BD3AE0_VALUE_DESC?", a3);
        if (a2 == 2)
            exp_log("CENSUS_KEY2_SEEN at BD3AE0 key arg");
        if (a2 == KEY_SESSION_NID)
            exp_log("CENSUS_SESSION_KEY_SEEN at BD3AE0 key arg");
    } else if (addr == VA_OBJ18_REG_BD9C50) {
        exp_log("CENSUS_BD9C50_ENTRY signature unknown; dumping pointer candidates");
        exp_log_census_obj("CENSUS_BD9C50_ECX", c->Ecx);
        exp_log_census_obj("CENSUS_BD9C50_ESI", c->Esi);
        exp_log_census_obj("CENSUS_BD9C50_EDI", c->Edi);
        exp_log_census_obj("CENSUS_BD9C50_EAX", c->Eax);
        exp_log_census_obj("CENSUS_BD9C50_A1", a1);
        exp_log_census_obj("CENSUS_BD9C50_A2", a2);
        exp_log_census_obj("CENSUS_BD9C50_A3", a3);
    } else if (addr == VA_OBJ18_WRITE_BD2D4D || addr == VA_OBJ18_WRITE_BD3B15 ||
               addr == VA_OBJ18_WRITE_BD9D5F) {
        DWORD slot, node, key, oldval, newval;
        slot = c->Ebx;
        node = slot >= 0x10 ? slot - 0x10 : 0;
        key = exp2_read_safe(node + 0xc);
        oldval = exp2_read_safe(slot);
        newval = c->Ecx;
        wsprintfA(buf,
            "CENSUS_WRITE addr=0x%08x slot=0x%08x node=0x%08x key=0x%08x oldval=0x%08x newval=0x%08x stack0=0x%08x ebp_ret=0x%08x",
            addr, slot, node, key, oldval, newval, stack0, ebp_ret);
        exp_log(buf);
        if (key == 2)
            exp_log("CENSUS_KEY2_SEEN at map write");
        if (key == KEY_SESSION_NID)
            exp_log("CENSUS_SESSION_KEY_SEEN at map write");
        exp_log_node("CENSUS_WRITE_NODE", node);
        exp_log_census_obj("CENSUS_WRITE_NEWVAL_OBJ?", newval);
        exp_log_census_obj("CENSUS_WRITE_EAX_OBJ?", c->Eax);
        exp_log_census_obj("CENSUS_WRITE_ESI_OBJ?", c->Esi);
        exp_log_census_obj("CENSUS_WRITE_EDI_OBJ?", c->Edi);
    } else if (addr == VA_TAG1_COMP_REGISTER) {
        exp_log("CENSUS_C33CC0_ENTRY Computer construct/register marker");
        exp_log_census_obj("CENSUS_C33CC0_ECX", c->Ecx);
        exp_log_census_obj("CENSUS_C33CC0_ESI", c->Esi);
        exp_log_census_obj("CENSUS_C33CC0_EDI", c->Edi);
        exp_log_census_obj("CENSUS_C33CC0_A1", a1);
    } else if (addr == VA_TAG1_COMP_INSERT_CALL) {
        wsprintfA(buf,
            "CENSUS_C33E06_INSERT_CALLSITE stack0=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x EAX=0x%08x ECX=0x%08x EDX=0x%08x ESI=0x%08x EDI=0x%08x",
            stack0, a1, a2, a3, a4, c->Eax, c->Ecx, c->Edx, c->Esi, c->Edi);
        exp_log(buf);
        exp_log_census_obj("CENSUS_C33E06_ECX", c->Ecx);
        exp_log_census_obj("CENSUS_C33E06_EAX", c->Eax);
        exp_log_census_obj("CENSUS_C33E06_ESI", c->Esi);
        exp_log_census_obj("CENSUS_C33E06_EDI", c->Edi);
        exp_log_census_desc("CENSUS_C33E06_EDI_DESC?", c->Edi);
    }
}
#endif
#endif

#ifdef EXP_DUMP_CONNECTOR_HANDSHAKE
static void exp_log_conn_candidate(const char *tag, DWORD p)
{
    char buf[512];
    if (!p) {
        wsprintfA(buf, "%s ptr=NULL", tag);
        exp_log(buf);
        return;
    }
    wsprintfA(buf,
        "%s ptr=0x%08x vt=0x%08x +54=%08x +58=%08x +5c=%08x +60=%08x",
        tag, p, exp2_read_safe(p), exp2_read_safe(p + 0x54),
        exp2_read_safe(p + 0x58), exp2_read_safe(p + 0x5c),
        exp2_read_safe(p + 0x60));
    exp_log(buf);
    wsprintfA(buf,
        "%s ptr=0x%08x +64=%08x +68=%08x +6c=%08x +70=%08x +74=%08x",
        tag, p, exp2_read_safe(p + 0x64), exp2_read_safe(p + 0x68),
        exp2_read_safe(p + 0x6c), exp2_read_safe(p + 0x70),
        exp2_read_safe(p + 0x74));
    exp_log(buf);
}

static void exp_log_connector_handshake(DWORD addr, PCONTEXT c)
{
    DWORD esp, ret, a1, a2, a3, a4, a5, a6, a7, a8;
    char buf[512];
    if (!c) return;
    esp = c->Esp;
    ret = exp2_read_safe(esp);
    a1 = exp2_read_safe(esp + 4);
    a2 = exp2_read_safe(esp + 8);
    a3 = exp2_read_safe(esp + 0xc);
    a4 = exp2_read_safe(esp + 0x10);
    a5 = exp2_read_safe(esp + 0x14);
    a6 = exp2_read_safe(esp + 0x18);
    a7 = exp2_read_safe(esp + 0x1c);
    a8 = exp2_read_safe(esp + 0x20);
    wsprintfA(buf,
        "CONNHIT addr=0x%08x EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x ESI=0x%08x EDI=0x%08x",
        addr, c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi);
    exp_log(buf);
    wsprintfA(buf,
        "CONNARGS addr=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x esp=0x%08x",
        addr, a1, a2, a3, a4, esp);
    exp_log(buf);
    wsprintfA(buf,
        "CONNARGS2 addr=0x%08x ret=0x%08x a5=0x%08x a6=0x%08x a7=0x%08x a8=0x%08x",
        addr, ret, a5, a6, a7, a8);
    exp_log(buf);

    if (addr == VA_CONN_OPEN_IMPL) {
        exp_log("CONNOPEN_ENTRY Connector::open field initializer reached");
        exp_log_conn_candidate("CONNOPEN_THIS_ECX", c->Ecx);
        exp_log_conn_candidate("CONNOPEN_THIS_EDI", c->Edi);
        exp_log_conn_candidate("CONNOPEN_THIS_ESI", c->Esi);
    }

    if (addr == VA_CONN_RESPONSE_IMPL) {
        DWORD expected = exp2_read_safe(a1 + 0x6c);
        DWORD has55 = exp2_read_safe(a1 + 0x54);
        DWORD has56 = exp2_read_safe(a1 + 0x58);
        DWORD has57 = exp2_read_safe(a1 + 0x5c);
        DWORD plus54_vt = exp2_read_safe(has55);
        DWORD plus54_inner = exp2_read_safe(has55 + 0x14);
        DWORD inner_vt = exp2_read_safe(plus54_inner);
        DWORD inner_slot5c = exp2_read_safe(inner_vt + 0x5c);
        wsprintfA(buf,
            "CONNRESP_GATE a1+6c=0x%08x arg3=0x%08x eq=%u fields54/58/5c=%08x/%08x/%08x",
            expected, a3, expected == a3, has55, has56, has57);
        exp_log(buf);
        wsprintfA(buf,
            "CONNRESP_54 plus54=0x%08x vt=0x%08x inner14=0x%08x inner_vt=0x%08x inner_slot5c=0x%08x",
            has55, plus54_vt, plus54_inner, inner_vt, inner_slot5c);
        exp_log(buf);
    }
    if (addr == VA_PM_CREATEOBJECT) {
        DWORD pm = a1;
        DWORD out_slot = a2;
        DWORD name_ptr = a3;
        DWORD inner = exp2_read_safe(pm + 0x14);
        DWORD inner_vt = exp2_read_safe(inner);
        DWORD inner_slot5c = exp2_read_safe(inner_vt + 0x5c);
        wsprintfA(buf,
            "B8F750_ENTRY pm=0x%08x out_slot=0x%08x name_arg=0x%08x inner14=0x%08x inner_vt=0x%08x slot5c=0x%08x",
            pm, out_slot, name_ptr, inner, inner_vt, inner_slot5c);
        exp_log(buf);
    }
    if (addr == VA_CONN_CONNECT_IMPL) {
        DWORD connected_byte = exp2_read_safe(c->Edi + 0x60) & 0xff;
        wsprintfA(buf,
            "CONNCONNECT_GATE edi+58=0x%08x edi+60.byte=0x%02x edi+68=0x%08x edi+70=0x%08x",
            exp2_read_safe(c->Edi + 0x58), connected_byte,
            exp2_read_safe(c->Edi + 0x68), exp2_read_safe(c->Edi + 0x70));
        exp_log(buf);
    }
    exp_log_conn_candidate("CONN_ECX", c->Ecx);
    exp_log_conn_candidate("CONN_EDI", c->Edi);
    exp_log_conn_candidate("CONN_ESI", c->Esi);
    exp_log_conn_candidate("CONN_A1", a1);
    exp_log_conn_candidate("CONN_A2", a2);
    exp_log_conn_candidate("CONN_A3", a3);
}
#endif

#ifdef EXP_DUMP_PRECONNECT_STAGE
static void exp_log_preconnect_marker(DWORD addr, PCONTEXT c)
{
    DWORD esp, a1, a2, a3, a4;
    char buf[512];
    if (!c) return;
    esp = c->Esp;
    a1 = exp2_read_safe(esp + 4);
    a2 = exp2_read_safe(esp + 8);
    a3 = exp2_read_safe(esp + 0xc);
    a4 = exp2_read_safe(esp + 0x10);
    wsprintfA(buf,
        "PRECONN addr=0x%08x EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x ESI=0x%08x EDI=0x%08x",
        addr, c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi);
    exp_log(buf);
    wsprintfA(buf,
        "PRECONNARGS addr=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x esp=0x%08x",
        addr, a1, a2, a3, a4, esp);
    exp_log(buf);
}
#endif

#ifdef EXP_DUMP_FUSER_STATE
static const char *exp_fuser_marker_name(DWORD addr)
{
    switch (addr) {
    case VA_FUSER_CONNECT: return "FUser::connect";
    case VA_FUSER_STATE1_WRITE_8C: return "state1_write_8c";
    case VA_FUSER_STATE1_WRITE_74: return "state1_write_74";
    case VA_FUSER_ONCONNECTOR: return "FUser::onConnectorReplicated";
    case VA_FUSER_ONSIGNIN: return "FUser::onSignInResult";
    case VA_FUSER_SIGNIN_74_CMP: return "signIn_74_gate";
    case VA_FUSER_SIGNIN_SCS_OK: return "signIn_SCS_OK";
    case VA_FUSER_SIGNIN_EVENT: return "signIn_Master_SignInResult";
    case VA_FUSER_SIGNIN_NOTIFY: return "signIn_notify";
    case VA_FUSER_ONLOGIN: return "FUser::onLogInResult";
    case VA_FUSER_LOGIN_SCS_OK: return "logIn_SCS_OK";
    case VA_FUSER_LOGIN_EVENT: return "logIn_event";
    case VA_FUSER_ONSESSION: return "FUser::onSessionReplicated";
    case VA_FUSER_ONSESSION_RESOLVE: return "session_self_resolve";
    case VA_FUSER_ONSESSION_STATE_CMP: return "session_state_gate";
    case VA_FUSER_ONSESSION_74_CMP: return "session_74_gate";
    case VA_FUSER_ONSESSION_SCS_OK: return "session_SCS_OK";
    case VA_FUSER_ONSESSION_EVENT: return "session_Master_SignIn";
    case VA_FUSER_ONSESSION_NOTIFY: return "session_notify";
    case VA_FUSER_ONSESSION_STATE5_WRITE: return "session_state5_write";
    case VA_FUSER_SETUICONTROL: return "FUser::SetUIControl_actual";
    case VA_FUSER_SETUICONTROL_WRITE_74: return "SetUIControl_write_74";
    case VA_FUSER_FINISH: return "FUser::onFinishLoadingCharacter";
    case VA_FUSER_FINISH_RESULT_CMP: return "finish_result_gate";
    case VA_FUSER_FINISH_74_CMP: return "finish_74_gate";
    case VA_FUSER_FINISH_SCS_OK: return "finish_SCS_OK";
    case VA_FUSER_FINISH_EVENT: return "finish_Master_FinishLoadingCharacter";
    case VA_FUSER_FINISH_NOTIFY: return "finish_notify";
    case VA_FUSER_FINISH_AFTER_EVENT: return "finish_after_006f1ec0";
    default: return "fuser_marker";
    }
}

static void exp_log_fuser_candidate(const char *tag, DWORD p)
{
    char buf[384];
    if (!p) {
        wsprintfA(buf, "FUSERCAND %s ptr=0x00000000", tag);
        exp_log(buf);
        return;
    }
    wsprintfA(buf,
        "FUSERCAND %s ptr=0x%08x +74=0x%08x +8c=0x%08x +78=0x%08x +7c=0x%08x",
        tag, p, exp2_read_safe(p + 0x74), exp2_read_safe(p + 0x8c),
        exp2_read_safe(p + 0x78), exp2_read_safe(p + 0x7c));
    exp_log(buf);
    wsprintfA(buf,
        "FUSERCAND2 %s ptr=0x%08x +80=0x%08x +84=0x%08x +88=0x%08x",
        tag, p, exp2_read_safe(p + 0x80), exp2_read_safe(p + 0x84),
        exp2_read_safe(p + 0x88));
    exp_log(buf);
}

static void exp_log_fuser_state(DWORD addr, PCONTEXT c)
{
    DWORD esp, a1, a2, a3, a4;
    char buf[512];
    if (!c) return;
    esp = c->Esp;
    a1 = exp2_read_safe(esp + 4);
    a2 = exp2_read_safe(esp + 8);
    a3 = exp2_read_safe(esp + 0xc);
    a4 = exp2_read_safe(esp + 0x10);
    wsprintfA(buf,
        "FUSER_STATE addr=0x%08x %s EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x ESI=0x%08x EDI=0x%08x",
        addr, exp_fuser_marker_name(addr), c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi);
    exp_log(buf);
    wsprintfA(buf,
        "FUSER_ARGS addr=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x esp=0x%08x ebp=0x%08x",
        addr, a1, a2, a3, a4, esp, c->Ebp);
    exp_log(buf);
    if (addr == VA_FUSER_STATE1_WRITE_8C)
        exp_log("FUSER_PENDING state1 will write [ESI+0x8c]=1");
    if (addr == VA_FUSER_STATE1_WRITE_74)
        exp_logf("FUSER_PENDING state1 will write [ESI+0x74]=EBX=0x%08x", c->Ebx, 0, 0);
    if (addr == VA_FUSER_ONSESSION_STATE5_WRITE)
        exp_logf("FUSER_PENDING onSession will write [ESI+0x8c]=EBP=0x%08x", c->Ebp, 0, 0);
    if (addr == VA_FUSER_SETUICONTROL_WRITE_74)
        exp_logf("FUSER_PENDING SetUIControl will write [EBX+0x74]=EDI=0x%08x", c->Edi, 0, 0);
    exp_log_fuser_candidate("ECX", c->Ecx);
    exp_log_fuser_candidate("ESI", c->Esi);
    exp_log_fuser_candidate("EDI", c->Edi);
    exp_log_fuser_candidate("EBX", c->Ebx);
}
#endif

#if defined(EXP_FORCE_UPDATECHAR_AFTER_FINISH) || defined(EXP_FORCE_UPDATECHAR_AT_NOTIFY) || defined(EXP_FORCE_UPDATECHAR_WITH_CHARMGR)
static volatile LONG g_force_update_once = 0;

static void exp_force_update_character_after_finish(PCONTEXT c)
{
    DWORD fuser = 0;
    DWORD frame = 0;
    DWORD fn = VA_FUSER_UPDATECHAR;
#ifdef EXP_FORCE_UPDATECHAR_WITH_CHARMGR
    DWORD old_charmgr = 0;
#endif
    if (!c)
        return;
    fuser = c->Edi ? c->Edi : c->Ecx;
    frame = fuser ? exp2_read_safe(fuser + 0x74) : 0;
    exp_logf("FORCE_UPDATECHAR attempt fuser=0x%08x frame=0x%08x",
             fuser, frame, 0);
    if (!fuser || !frame) {
        exp_log("FORCE_UPDATECHAR skipped null fuser/frame");
        return;
    }
#ifdef EXP_FORCE_UPDATECHAR_WITH_CHARMGR
    old_charmgr = exp2_read_safe(fuser + 0x84);
    if (!old_charmgr && g_last_charmgr) {
        __try {
            *(DWORD*)(DWORD_PTR)(fuser + 0x84) = g_last_charmgr;
            exp_logf("FORCE_CHARMGR_SLOT fuser=0x%08x old84=0x%08x new84=0x%08x",
                     fuser, old_charmgr, g_last_charmgr);
            exp_logf("FORCE_CHARMGR_OWNER owner=0x%08x component=0x%08x",
                     g_last_charmgr_owner, g_last_charmgr, 0);
        } __except(1) {
            exp_log("FORCE_CHARMGR_SLOT exception");
        }
    } else {
        exp_logf("FORCE_CHARMGR_SLOT skip old84=0x%08x captured=0x%08x",
                 old_charmgr, g_last_charmgr, 0);
    }
#endif
    __try {
        __asm {
            push frame
            mov ecx, fuser
            mov eax, fn
            call eax
        }
        exp_log("FORCE_UPDATECHAR returned");
    } __except(1) {
        exp_log("FORCE_UPDATECHAR exception");
    }
}
#endif

#ifdef EXP_GUARD_FUSER84_CHARMGR
static void exp_guard_fuser84_charmgr(PCONTEXT c)
{
    DWORD fuser, frame, old_charmgr, captured, captured_list;
    if (!c)
        return;
    fuser = c->Ecx;
    frame = fuser ? exp2_read_safe(fuser + 0x74) : 0;
    old_charmgr = fuser ? exp2_read_safe(fuser + 0x84) : 0;
    captured = (DWORD)g_last_charmgr;
    captured_list = captured ? exp2_read_safe(captured + 0x28) : 0;
    exp_logf("FUSER84_GUARD enter fuser=0x%08x frame74=0x%08x old84=0x%08x",
             fuser, frame, old_charmgr);
    exp_logf("FUSER84_GUARD captured=0x%08x owner=0x%08x list28=0x%08x",
             captured, (DWORD)g_last_charmgr_owner, captured_list);
    if (!fuser || !frame || old_charmgr || !captured || !captured_list) {
        exp_log("FUSER84_GUARD skip");
        return;
    }
    __try {
        *(DWORD*)(DWORD_PTR)(fuser + 0x84) = captured;
        exp_logf("FUSER84_GUARD write fuser=0x%08x new84=0x%08x list28=0x%08x",
                 fuser, captured, captured_list);
    } __except(1) {
        exp_log("FUSER84_GUARD exception");
    }
}
#endif

static void exp_dump_bytes(const char *tag, DWORD base, DWORD n);

#ifdef EXP_DUMP_ONLOAD_TRANSITION
static const char *exp_onload_transition_name(DWORD addr)
{
    switch (addr) {
    case VA_ONLOAD_SELFLOOKUP_RET: return "after_obj18_lookup";
    case VA_ONLOAD_SELFVALUE_TEST: return "self_value_test";
    case VA_ONLOAD_CREATE_HASH: return "create_leg_hash_CharacterManager";
    case VA_ONLOAD_CREATE_RET: return "create_leg_bb2580_ret";
    case VA_ONLOAD_BD2CD0_RET: return "create_leg_bd2cd0_ret";
    case VA_ONLOAD_LISTCOPY_CALL: return "existing_leg_copy_objectList";
    case VA_ONLOAD_LOADLIST_CALL: return "existing_leg_before_LoadList";
    case VA_ONLOAD_FINISH_CALL: return "existing_leg_before_finish_sender";
    default: return "onload_transition";
    }
}

static int exp_is_onload_transition_addr(DWORD addr)
{
    return addr == VA_ONLOAD_SELFLOOKUP_RET ||
           addr == VA_ONLOAD_SELFVALUE_TEST ||
           addr == VA_ONLOAD_CREATE_HASH ||
           addr == VA_ONLOAD_CREATE_RET ||
           addr == VA_ONLOAD_BD2CD0_RET ||
           addr == VA_ONLOAD_LISTCOPY_CALL ||
           addr == VA_ONLOAD_LOADLIST_CALL ||
           addr == VA_ONLOAD_FINISH_CALL;
}

static void exp_log_onload_transition(DWORD addr, PCONTEXT c)
{
    DWORD esp, ret, a1, a2, a3, a4, slotp, slotv, ptr, vt;
    if (!c)
        return;
    esp = c->Esp;
    ret = exp2_read_safe(esp);
    a1 = exp2_read_safe(esp + 4);
    a2 = exp2_read_safe(esp + 8);
    a3 = exp2_read_safe(esp + 0x0c);
    a4 = exp2_read_safe(esp + 0x10);
    exp_logf("ONLOAD_STEP addr=0x%08x EAX=0x%08x ECX=0x%08x",
             addr, c->Eax, c->Ecx);
    exp_logf("ONLOAD_STEP name='%s' EBX=0x%08x EDX=0x%08x",
             (DWORD)(DWORD_PTR)exp_onload_transition_name(addr), c->Ebx, c->Edx);
    exp_logf("ONLOAD_STEP regs ESI=0x%08x EDI=0x%08x EBP=0x%08x",
             c->Esi, c->Edi, c->Ebp);
    exp_logf("ONLOAD_STEP stack ret=0x%08x a1=0x%08x a2=0x%08x",
             ret, a1, a2);
    exp_logf("ONLOAD_STEP stack a3=0x%08x a4=0x%08x ESP=0x%08x",
             a3, a4, esp);

    if (addr == VA_ONLOAD_SELFLOOKUP_RET) {
        slotp = c->Eax;
        slotv = exp2_read_safe(slotp);
        exp_logf("ONLOAD_LOOKUP_RET slotp=0x%08x slotv=0x%08x slotv_vt=0x%08x",
                 slotp, slotv, exp2_read_safe(slotv));
    }
    if (addr == VA_ONLOAD_SELFVALUE_TEST) {
        ptr = c->Esi;
        vt = exp2_read_safe(ptr);
        exp_logf("ONLOAD_SELF_VALUE ptr=0x%08x vt=0x%08x key_or_0c=0x%08x",
                 ptr, vt, exp2_read_safe(ptr + 0x0c));
        exp_dump_bytes("ONLOAD_SELF_VALUE_BYTES", ptr, 48);
    }
    if (addr == VA_ONLOAD_CREATE_RET) {
        ptr = c->Eax;
        exp_logf("ONLOAD_CREATE_RET obj=0x%08x vt=0x%08x key_or_0c=0x%08x",
                 ptr, exp2_read_safe(ptr), exp2_read_safe(ptr + 0x0c));
        exp_dump_bytes("ONLOAD_CREATE_OBJ", ptr, 48);
    }
    if (addr == VA_ONLOAD_BD2CD0_RET || addr == VA_ONLOAD_LISTCOPY_CALL ||
        addr == VA_ONLOAD_LOADLIST_CALL || addr == VA_ONLOAD_FINISH_CALL) {
        exp_dump_bytes("ONLOAD_STACK", esp, 64);
    }
}
#endif

#ifdef EXP_DUMP_POST_FINISH_UI
static const char *exp_post_finish_ui_name(DWORD addr)
{
    switch (addr) {
    case VA_EVENT_EMIT_B: return "event_emit_0071bdd0";
    case VA_FUSER_UI_UPDATE_HELPER: return "FUser_UI_update_helper_010b0490";
    case VA_POST_STATE_EVENT_HELPER: return "post_state_helper_006f1ec0";
    case VA_FUSER_UPDATECHAR: return "FUser::updateCharacter";
    case VA_CHARMGR_GETALL: return "CharacterManager::getAllPlayers";
    case VA_CHARMGR_LOADLIST: return "CharacterManager::LoadList";
    case VA_ONLOAD_AFTER_LOADLIST: return "onLoadPlayers_after_LoadList";
    case VA_SERVER_FINISH_SEND: return "Server_FinishLoadingCharacter_sender";
    case VA_UPDATE_AFTER_GETALL: return "updateCharacter_after_getAllPlayers";
    case VA_UPDATE_ADDCHAR: return "updateCharacter_AddCharacter";
    case VA_UPDATE_HASNOCHAR: return "updateCharacter_HasNoCharacter";
    case VA_UPDATE_SELECTCHAR: return "updateCharacter_SelectCharacter";
    default: return "post_finish_ui_marker";
    }
}

#ifdef EXP_DUMP_UIEVENT_FIELDS
static volatile DWORD g_finish_uievent_ptr = 0;
static volatile DWORD g_finish_uievent_holder = 0;
static void exp_dump_bytes(const char *tag, DWORD base, DWORD n);

static void exp_log_uievent_user_fields(const char *tag, DWORD evt)
{
    char buf[640];
    char p1[96];
    char p2[96];
    DWORD vt, p1s, p2s, p1_len, p1_res, p2_len, p2_res;
    DWORD p3, p4, p5, p6, p10;
    if (!evt)
        return;
    vt = exp2_read_safe(evt);
    p1s = evt + 0x20;
    p2s = evt + 0x3c;
    p1_len = exp2_read_safe(p1s + 0x10);
    p1_res = exp2_read_safe(p1s + 0x14);
    p2_len = exp2_read_safe(p2s + 0x10);
    p2_res = exp2_read_safe(p2s + 0x14);
    p3 = exp2_read_safe(evt + 0x58);
    p4 = exp2_read_safe(evt + 0x5c);
    p5 = exp2_read_safe(evt + 0x60);
    p6 = exp2_read_safe(evt + 0x64);
    p10 = exp2_read_safe(evt + 0x74);
    exp_read_std_string_safe(p1s, p1, sizeof(p1));
    exp_read_std_string_safe(p2s, p2, sizeof(p2));
    wsprintfA(buf,
        "UIEVENT_%s evt=0x%08x vt=0x%08x p1s=0x%08x p2s=0x%08x",
        tag, evt, vt, p1s, p2s);
    exp_log(buf);
    wsprintfA(buf,
        "UIEVENT_%s m_param1='%s' len=0x%08x res=0x%08x",
        tag, p1, p1_len, p1_res);
    exp_log(buf);
    wsprintfA(buf,
        "UIEVENT_%s m_param2='%s' len=0x%08x res=0x%08x",
        tag, p2, p2_len, p2_res);
    exp_log(buf);
    wsprintfA(buf,
        "UIEVENT_%s raw p3=0x%08x p4=0x%08x p5=0x%08x p6=0x%08x p10=0x%08x",
        tag, p3, p4, p5, p6, p10);
    exp_log(buf);
    exp_dump_bytes("UIEVENT_OBJ", evt, 64);
    exp_dump_bytes("UIEVENT_PARAM1", p1s, 32);
    exp_dump_bytes("UIEVENT_PARAM2", p2s, 32);
}
#endif

static void exp_log_post_finish_ui(DWORD addr, PCONTEXT c)
{
    DWORD esp, a1, a2, a3, a4;
    char buf[512];
    if (!c) return;
    esp = c->Esp;
    a1 = exp2_read_safe(esp + 4);
    a2 = exp2_read_safe(esp + 8);
    a3 = exp2_read_safe(esp + 0xc);
    a4 = exp2_read_safe(esp + 0x10);
    wsprintfA(buf,
        "POSTFINISH_UI addr=0x%08x %s EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x",
        addr, exp_post_finish_ui_name(addr), c->Eax, c->Ebx, c->Ecx, c->Edx);
    exp_log(buf);
    wsprintfA(buf,
        "POSTFINISH_UI regs ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x",
        c->Esi, c->Edi, c->Ebp, esp);
    exp_log(buf);
    wsprintfA(buf,
        "POSTFINISH_UI stack a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x",
        a1, a2, a3, a4);
    exp_log(buf);
    wsprintfA(buf,
        "POSTFINISH_UI cand ECX+74=0x%08x ECX+84=0x%08x ECX+8c=0x%08x ESI+74=0x%08x ESI+84=0x%08x ESI+8c=0x%08x",
        exp2_read_safe(c->Ecx + 0x74), exp2_read_safe(c->Ecx + 0x84),
        exp2_read_safe(c->Ecx + 0x8c), exp2_read_safe(c->Esi + 0x74),
        exp2_read_safe(c->Esi + 0x84), exp2_read_safe(c->Esi + 0x8c));
    exp_log(buf);
    if (addr == VA_UPDATE_AFTER_GETALL) {
        DWORD slot50 = exp2_read_safe(esp + 0x50);
        DWORD begin = exp2_read_safe(esp + 0x54);
        DWORD end = exp2_read_safe(esp + 0x58);
        DWORD cap = exp2_read_safe(esp + 0x5c);
        DWORD fuser = c->Edi;
        DWORD count = 0;
        DWORD i;
        if (end >= begin && ((end - begin) & 3u) == 0 && (end - begin) <= 0x400u)
            count = (end - begin) / 4u;
        wsprintfA(buf,
            "AFTER_GETALL vec50=0x%08x begin=0x%08x end=0x%08x cap=0x%08x count=%u",
            slot50, begin, end, cap, count);
        exp_log(buf);
        wsprintfA(buf,
            "AFTER_GETALL fuser=0x%08x +74(frame)=0x%08x +84(charmgr)=0x%08x +8c=0x%08x",
            fuser, exp2_read_safe(fuser + 0x74), exp2_read_safe(fuser + 0x84),
            exp2_read_safe(fuser + 0x8c));
        exp_log(buf);
        for (i = 0; i < count && i < 6; ++i) {
            DWORD row = exp2_read_safe(begin + i * 4u);
            wsprintfA(buf,
                "AFTER_GETALL row%u=0x%08x row+0c=0x%08x row+74=0x%08x row+84=0x%08x",
                i, row, exp2_read_safe(row + 0x0c), exp2_read_safe(row + 0x74),
                exp2_read_safe(row + 0x84));
            exp_log(buf);
        }
        exp_dump_bytes("AFTER_GETALL_STACK48", esp + 0x48, 64);
    }
#ifdef EXP_DUMP_UIEVENT_FIELDS
    if (addr == VA_EVENT_EMIT_B) {
        exp_logf("UIEVENT_EMIT entry outptr=0x%08x pre_value=0x%08x",
                 a1, exp2_read_safe(a1), 0);
    }
    if (addr == VA_FUSER_UI_UPDATE_HELPER) {
        DWORD evt = exp2_read_safe(a1);
        g_finish_uievent_holder = a1;
        g_finish_uievent_ptr = evt;
        exp_logf("UIEVENT_UPDATE_HELPER holder_arg=0x%08x event=0x%08x holder_ecx=0x%08x",
                 a1, evt, c->Ecx);
        exp_log_uievent_user_fields("from_010b0490_arg", evt);
    }
    if (addr == VA_POST_STATE_EVENT_HELPER) {
        DWORD cand_stack = a1;
        DWORD cand_stack_deref = exp2_read_safe(a1);
        DWORD cand_eax_deref = exp2_read_safe(c->Eax);
        exp_logf("UIEVENT_DISPATCH candidates stack=0x%08x stack_deref=0x%08x last=0x%08x",
                 cand_stack, cand_stack_deref, (DWORD)g_finish_uievent_ptr);
        exp_logf("UIEVENT_DISPATCH holder=0x%08x eax=0x%08x eax_deref=0x%08x",
                 (DWORD)g_finish_uievent_holder, c->Eax, cand_eax_deref);
        exp_log_uievent_user_fields("last_finish", (DWORD)g_finish_uievent_ptr);
        exp_log_uievent_user_fields("stack_arg", cand_stack);
        exp_log_uievent_user_fields("stack_deref", cand_stack_deref);
        exp_log_uievent_user_fields("eax_deref", cand_eax_deref);
    }
#endif
}
#endif

#if EXP_TARGET_PROFILE == 69 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
static void exp_log_charmgr_container(const char *tag, DWORD mgr)
{
    DWORD list28, p0, p4, p8, pc;
    char buf[512];
    if (!mgr) {
        exp_logf("%s mgr=NULL", (DWORD)(DWORD_PTR)tag, 0, 0);
        return;
    }
    list28 = exp2_read_safe(mgr + 0x28);
    p0 = exp2_read_safe(list28 + 0x00);
    p4 = exp2_read_safe(list28 + 0x04);
    p8 = exp2_read_safe(list28 + 0x08);
    pc = exp2_read_safe(list28 + 0x0c);
    wsprintfA(buf,
        "%s mgr=0x%08x mgr+28=0x%08x list[0..c]=0x%08x/0x%08x/0x%08x/0x%08x",
        tag, mgr, list28, p0, p4, p8, pc);
    exp_log(buf);
    exp_dump_bytes("CHARMGR_MGR", mgr, 64);
    exp_dump_bytes("CHARMGR_LIST28", list28, 64);
    if (p4 && p4 != list28)
        exp_dump_bytes("CHARMGR_LIST_NODE4", p4, 64);
}

static int exp_is_charmgr_lifetime_addr(DWORD addr)
{
    return addr == VA_CHARMGR_CTOR ||
           addr == VA_CHARMGR_CTOR_STORE28 ||
           addr == VA_CHARMGR_DTOR ||
           addr == VA_CHARMGR_DTOR_ZERO28;
}

static void exp_log_charmgr_lifetime(DWORD addr, PCONTEXT c)
{
    DWORD this_stack, this_ecx, old28, list28;
    if (!c)
        return;
    this_stack = exp2_read_safe(c->Esp + 4);
    this_ecx = c->Ecx;
    if (addr == VA_CHARMGR_CTOR) {
        exp_logf("CHARMGR_LIFE ctor_entry ECX=0x%08x stack_this=0x%08x ret=0x%08x",
                 this_ecx, this_stack, exp2_read_safe(c->Esp));
        exp_log_charmgr_container("CHARMGR_CTOR_ENTRY_ECX", this_ecx);
        exp_log_charmgr_container("CHARMGR_CTOR_ENTRY_STACK", this_stack);
        return;
    }
    if (addr == VA_CHARMGR_CTOR_STORE28) {
        exp_logf("CHARMGR_LIFE ctor_store28 this(EDI)=0x%08x list(ESI)=0x%08x ret=0x%08x",
                 c->Edi, c->Esi, exp2_read_safe(c->Esp));
        exp_log_charmgr_container("CHARMGR_CTOR_PRE_STORE", c->Edi);
        exp_dump_bytes("CHARMGR_CTOR_LIST_ESI", c->Esi, 64);
        return;
    }
    if (addr == VA_CHARMGR_DTOR) {
        exp_logf("CHARMGR_LIFE dtor_entry ECX=0x%08x stack1=0x%08x ret=0x%08x",
                 this_ecx, this_stack, exp2_read_safe(c->Esp));
        exp_log_charmgr_container("CHARMGR_DTOR_ENTRY_ECX", this_ecx);
        return;
    }
    if (addr == VA_CHARMGR_DTOR_ZERO28) {
        old28 = exp2_read_safe(c->Esi + 0x28);
        list28 = old28;
        exp_logf("CHARMGR_LIFE dtor_zero28 this(ESI)=0x%08x old+28=0x%08x ret=0x%08x",
                 c->Esi, old28, exp2_read_safe(c->Esp));
        exp_log_charmgr_container("CHARMGR_DTOR_BEFORE_ZERO", c->Esi);
        exp_dump_bytes("CHARMGR_DTOR_LIST28", list28, 64);
    }
}

static const char *exp_loadlist_step_name(DWORD addr)
{
    switch (addr) {
    case VA_LOADLIST_EMPTY_CHECK: return "empty_check";
    case VA_LOADLIST_EMPTY_BRANCH: return "empty_branch_cleanup";
    case VA_LOADLIST_EMPTY_RELEASE_RET: return "empty_release_ret";
    case VA_LOADLIST_EMPTY_RELEASE_DTOR: return "empty_release_dtor";
    case VA_LOADLIST_EMPTY_PARAM3_SCAN: return "empty_param3_scan";
    case VA_LOADLIST_OLD_BEGIN: return "old_cleanup_begin";
    case VA_LOADLIST_OLD_MARK_ROW: return "old_mark_row";
    case VA_LOADLIST_OLD_HELPER: return "old_helper";
    case VA_LOADLIST_OLD_FETCH_RET: return "old_fetch_ret";
    case VA_LOADLIST_NODE_PAYLOAD: return "node_payload";
    case VA_LOADLIST_REF_RET: return "payload_addref_ret";
    case VA_LOADLIST_DUP_SCAN: return "duplicate_scan";
    case VA_LOADLIST_DUP_RESULT: return "duplicate_result";
    case VA_LOADLIST_PLAYER_DESC: return "player_desc";
    case VA_LOADLIST_PLAYER_RESOLVE: return "player_resolve_call";
    case VA_LOADLIST_PLAYER_RET: return "player_resolve_ret";
    case VA_LOADLIST_PLAYER_TEST: return "player_ptr_test";
    case VA_LOADLIST_PLAYER_FLAGS: return "player_flags";
    case VA_LOADLIST_SCORE_RET: return "isdeleted_score_ret";
    case VA_LOADLIST_ACTIVE_PATH: return "active_path";
    case VA_LOADLIST_OBJ_RESOLVE2: return "second_resolve_call";
    case VA_LOADLIST_OBJ_RET2: return "second_resolve_ret";
    case VA_LOADLIST_TPLAYER_CALL: return "tplayer_lookup_ret_area";
    case VA_LOADLIST_ROW_ADD_CALL: return "row_add_call";
    case VA_LOADLIST_ROW_ADD_RET: return "row_add_ret";
    case VA_LOADLIST_LOOP_NEXT: return "loop_next";
    case VA_LOADLIST_OLD_ROW_VCALL: return "old_row_vcall";
    case VA_LOADLIST_OLD_AFTER_VCALL: return "old_after_vcall";
    case VA_LOADLIST_EMPTY_CLEANUP_RET: return "empty_cleanup_tail";
    case VA_LOADLIST_UPDATECOUNT_CALL: return "update_count_call";
    case VA_LOADLIST_BAD_HELPER_ENTRY: return "bad_helper_entry";
    case VA_LOADLIST_BAD_HELPER_DEREF: return "bad_helper_deref";
    default: return "loadlist_step";
    }
}

static const char *exp_objlist_a_step_name(DWORD addr)
{
    switch (addr) {
    case VA_ONLOAD_ARG3_CALL: return "onload_arg3_desc_call";
    case VA_OBJLIST_A_WRAPPER: return "candidate_a_wrapper";
    case VA_OBJLIST_A_PARAM_TEST: return "entry_param_test";
    case VA_OBJLIST_A_OUTER_TAG_RET: return "outer_tag_ret";
    case VA_OBJLIST_A_COUNT_RET: return "count_ret";
    case VA_OBJLIST_A_TABLE_RET2: return "loop_table_ret";
    case VA_OBJLIST_A_INDEX_RET: return "index_ret";
    case VA_OBJLIST_A_NESTED_RET: return "nested_decode_ret";
    case VA_OBJLIST_A_NODE_ALLOC_RET: return "node_alloc_ret";
    case VA_OBJLIST_A_NODE_LINK_RET: return "node_link_ret";
    case VA_OBJLIST_B_WRAPPER: return "candidate_b_wrapper";
    case VA_OBJLIST_B_ENTRY: return "candidate_b_entry";
    case VA_OBJLIST_B_PARAM_TEST: return "candidate_b_param_test";
    default: return "objlist_a";
    }
}

static void exp_log_variant_slot3(const char *tag, DWORD slot)
{
    DWORD v0, v4, v8, vc;
    char buf[512];
    if (!slot) {
        wsprintfA(buf, "%s slot=NULL", tag);
        exp_log(buf);
        return;
    }
    v0 = exp2_read_safe(slot + 0x00);
    v4 = exp2_read_safe(slot + 0x04);
    v8 = exp2_read_safe(slot + 0x08);
    vc = exp2_read_safe(slot + 0x0c);
    wsprintfA(buf,
        "%s slot=0x%08x v0=0x%08x v4=0x%08x tag=0x%08x vc=0x%08x nil=0x014d01c0",
        tag, slot, v0, v4, v8, vc);
    exp_log(buf);
}

static void exp_log_obj_ptr3(const char *tag, DWORD obj)
{
    char buf[512];
    if (!obj) {
        wsprintfA(buf, "%s obj=NULL", tag);
        exp_log(buf);
        return;
    }
    wsprintfA(buf,
        "%s obj=0x%08x vt=0x%08x id0c=0x%08x cls38=0x%08x pid3c=0x%08x pid7c=0x%08x map14=0x%08x map18=0x%08x",
        tag, obj, exp2_read_safe(obj), exp2_read_safe(obj + 0x0c),
        exp2_read_safe(obj + 0x38), exp2_read_safe(obj + 0x3c),
        exp2_read_safe(obj + 0x7c), exp2_read_safe(obj + 0x14),
        exp2_read_safe(obj + 0x18));
    exp_log(buf);
}

static const char *exp_objptr_resolver_step_name(DWORD addr)
{
    switch (addr) {
    case VA_NEWOBJ_PID_STORED: return "newobject_pid_stored";
    case VA_OBJPTR_MAT_ENTRY: return "objectptr_materializer_entry";
    case VA_OBJPTR_BB2580_CALL: return "objectptr_bb2580_call";
    case VA_BB2580_BC5860_CALL: return "bb2580_bc5860_call";
    case VA_BB2580_BC5860_RET: return "bb2580_bc5860_ret";
    case VA_BB2580_BB5F40_CALL: return "bb2580_bb5f40_call";
    case VA_BB2580_BB5F40_RET: return "bb2580_bb5f40_ret";
    case VA_OBJPTR_BB2580_RET: return "objectptr_bb2580_ret";
    default: return "objectptr_resolver";
    }
}

static void exp_log_newobject_identity69(DWORD addr, PCONTEXT c)
{
    DWORD obj, pid;
    char buf[768];
    if (!c) return;
    obj = c->Edx;
    pid = c->Edi;
    wsprintfA(buf,
        "NEWOBJ_ID addr=0x%08x %s obj(EDX)=0x%08x pid(EDI)=0x%08x stored3c=0x%08x",
        addr, exp_objptr_resolver_step_name(addr), obj, pid,
        obj ? exp2_read_safe(obj + 0x3c) : 0);
    exp_log(buf);
    exp_log_obj_ptr3("NEWOBJ_ID_OBJECT", obj);
    exp_dump_bytes("NEWOBJ_ID_OBJECT_BYTES", obj, 128);
    if (pid == 0x3ebu || exp2_read_safe(obj + 0x3c) == 0x3ebu) {
        g_objptr_last_newobj = obj;
        g_objptr_last_newobj_pid = pid;
    }
}

static void exp_log_objptr_resolver69(DWORD addr, PCONTEXT c)
{
    DWORD esp, a1, a2, a3, active, scalar;
    char buf[768];
    if (!c) return;
    esp = c->Esp;
    a1 = exp2_read_safe(esp + 0x04);
    a2 = exp2_read_safe(esp + 0x08);
    a3 = exp2_read_safe(esp + 0x0c);
    active = (DWORD)g_objptr_resolve_active;
    scalar = (DWORD)g_objptr_last_scalar;

    if (addr == VA_OBJPTR_BB2580_CALL) {
        scalar = c->Eax;
        g_objptr_last_scalar = scalar;
        g_objptr_resolve_active = 1;
        active = 1;
    }

    wsprintfA(buf,
        "OBJPTR_RESOLVE addr=0x%08x %s active=%u scalar=0x%08x EAX=0x%08x ECX=0x%08x",
        addr, exp_objptr_resolver_step_name(addr), active, scalar, c->Eax, c->Ecx);
    exp_log(buf);
    wsprintfA(buf,
        "OBJPTR_RESOLVE regs EBX=0x%08x EDX=0x%08x ESI=0x%08x EDI=0x%08x",
        c->Ebx, c->Edx, c->Esi, c->Edi);
    exp_log(buf);
    wsprintfA(buf,
        "OBJPTR_RESOLVE stack a1=0x%08x a2=0x%08x a3=0x%08x last_newobj=0x%08x last_pid=0x%08x",
        a1, a2, a3, (DWORD)g_objptr_last_newobj, (DWORD)g_objptr_last_newobj_pid);
    exp_log(buf);

    if (addr == VA_OBJPTR_MAT_ENTRY) {
        exp_log_variant_slot3("OBJPTR_ENTRY_DST_SLOT", c->Edi ? exp2_read_safe(c->Edi + 0x08) - 0x10 : 0);
    }
    if (addr == VA_OBJPTR_BB2580_CALL) {
        exp_log_obj_ptr3("OBJPTR_COMPARE_LAST_NEWOBJ", (DWORD)g_objptr_last_newobj);
    }
    if (addr == VA_BB2580_BC5860_RET) {
        exp_log_obj_ptr3("OBJPTR_BC5860_RESULT", c->Eax);
    }
    if (addr == VA_BB2580_BB5F40_RET) {
        exp_log_obj_ptr3("OBJPTR_BB5F40_RESULT", c->Eax);
    }
    if (addr == VA_OBJPTR_BB2580_RET) {
        exp_log_obj_ptr3("OBJPTR_BB2580_RESULT", c->Eax);
        g_objptr_resolve_active = 0;
    }
}

static void exp_log_objlist_a_reader(DWORD addr, PCONTEXT c)
{
    DWORD ebp, esp, param1, param2, out_top, prev_slot, local_vec;
    DWORD list0, list4, list8, listc;
    char buf[768];
    if (!c) return;
    if (addr == VA_ONLOAD_ARG3_CALL) {
        DWORD desc = c->Ecx ? c->Ecx : c->Eax;
        DWORD vt = exp2_read_safe(desc);
        DWORD slot24 = exp2_read_safe(vt + 0x24);
        DWORD cached = exp2_read_safe(0x0184c144u);
        DWORD cached_vt = exp2_read_safe(cached);
        DWORD frame = c->Esi;
        DWORD base = exp2_read_safe(frame + 0x0c);
        DWORD top = exp2_read_safe(frame + 0x08);
        wsprintfA(buf,
            "OBJLIST_ARG3 addr=0x%08x %s desc=0x%08x vt=0x%08x slot24=0x%08x EDX=0x%08x",
            addr, exp_objlist_a_step_name(addr), desc, vt, slot24, c->Edx);
        exp_log(buf);
        wsprintfA(buf,
            "OBJLIST_ARG3 cached0184c144=0x%08x cached_vt=0x%08x frame(ESI)=0x%08x base=0x%08x top=0x%08x",
            cached, cached_vt, frame, base, top);
        exp_log(buf);
        exp_log_variant_slot3("OBJLIST_ARG3_SLOT1", base);
        exp_log_variant_slot3("OBJLIST_ARG3_SLOT2", base + 0x10);
        exp_log_variant_slot3("OBJLIST_ARG3_SLOT3", base + 0x20);
        exp_dump_bytes("OBJLIST_ARG3_DESC", desc, 48);
        return;
    }
    if (addr == VA_OBJLIST_A_WRAPPER || addr == VA_OBJLIST_B_WRAPPER ||
        addr == VA_OBJLIST_B_ENTRY) {
        esp = c->Esp;
        wsprintfA(buf,
            "OBJLIST_DESC addr=0x%08x %s EAX=0x%08x ECX=0x%08x EDX=0x%08x",
            addr, exp_objlist_a_step_name(addr), c->Eax, c->Ecx, c->Edx);
        exp_log(buf);
        wsprintfA(buf,
            "OBJLIST_DESC regs EBX=0x%08x ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x",
            c->Ebx, c->Esi, c->Edi, c->Ebp, esp);
        exp_log(buf);
        wsprintfA(buf,
            "OBJLIST_DESC stack ret=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x",
            exp2_read_safe(esp), exp2_read_safe(esp + 4),
            exp2_read_safe(esp + 8), exp2_read_safe(esp + 0x0c));
        exp_log(buf);
        return;
    }
    ebp = c->Ebp;
    esp = c->Esp;
    param1 = exp2_read_safe(ebp + 0x08);
    param2 = exp2_read_safe(ebp + 0x0c);
    out_top = exp2_read_safe(c->Edi + 0x08);
    prev_slot = out_top ? (out_top - 0x10) : 0;
    local_vec = esp + 0x10;
    list0 = exp2_read_safe(param2 + 0x00);
    list4 = exp2_read_safe(param2 + 0x04);
    list8 = exp2_read_safe(param2 + 0x08);
    listc = exp2_read_safe(param2 + 0x0c);
    wsprintfA(buf,
        "OBJLIST_A addr=0x%08x %s EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x",
        addr, exp_objlist_a_step_name(addr), c->Eax, c->Ebx, c->Ecx, c->Edx);
    exp_log(buf);
    wsprintfA(buf,
        "OBJLIST_A regs ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x param1=0x%08x param2=0x%08x",
        c->Esi, c->Edi, ebp, esp, param1, param2);
    exp_log(buf);
    wsprintfA(buf,
        "OBJLIST_A out_top=0x%08x prev_slot=0x%08x local_vec=0x%08x list[0..c]=0x%08x/0x%08x/0x%08x/0x%08x",
        out_top, prev_slot, local_vec, list0, list4, list8, listc);
    exp_log(buf);

    if (addr == VA_OBJLIST_A_OUTER_TAG_RET || addr == VA_OBJLIST_A_TABLE_RET2)
        exp_log_variant_slot3("OBJLIST_A_TABLE_SLOT", c->Eax);
    if (addr == VA_OBJLIST_A_COUNT_RET)
        exp_logf("OBJLIST_A_COUNT n=0x%08x param1=0x%08x param2=0x%08x", c->Eax, param1, param2);
    if (addr == VA_OBJLIST_A_INDEX_RET)
        exp_log_variant_slot3("OBJLIST_A_ELEM_SLOT", c->Eax);
    if (addr == VA_OBJLIST_A_NESTED_RET || addr == VA_OBJLIST_A_NODE_ALLOC_RET ||
        addr == VA_OBJLIST_A_NODE_LINK_RET) {
        exp_log_variant_slot3("OBJLIST_A_PREV_SLOT", prev_slot);
        exp_dump_bytes("OBJLIST_A_LOCAL_VEC", local_vec, 64);
        exp_dump_bytes("OBJLIST_A_LIST_PARAM2", param2, 64);
    }
}

static const char *exp_tag0b_step_name(DWORD addr)
{
    switch (addr) {
    case VA_VARIANT_TAG0B_ENTRY: return "entry_cmp_0b";
    case VA_VARIANT_TAG0B_DESC_RET: return "descriptor_ret";
    case VA_VARIANT_TAG0B_OBJ_RET: return "materializer_ret";
    case VA_VARIANT_TAG0B_BA9200_RET: return "ba9200_ret";
    case VA_VARIANT_TAG0B_NIL_OBJ: return "nil_object_path";
    default: return "tag0b";
    }
}

static void exp_log_tag0b_materializer(DWORD addr, PCONTEXT c)
{
    DWORD ebp, out_top, prev_slot, desc, obj, stream, dst;
    char buf[768];
    if (!c) return;
    ebp = c->Ebp;
    out_top = exp2_read_safe(c->Edi + 0x08);
    prev_slot = out_top ? (out_top - 0x10) : 0;
    desc = c->Eax;
    obj = exp2_read_safe(ebp - 0x30);
    stream = c->Esi;
    dst = c->Edi;
    wsprintfA(buf,
        "TAG0B addr=0x%08x %s AL=0x%02x EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x",
        addr, exp_tag0b_step_name(addr), c->Eax & 0xff, c->Eax, c->Ebx, c->Ecx, c->Edx);
    exp_log(buf);
    wsprintfA(buf,
        "TAG0B regs ESI(stream)=0x%08x EDI(dst)=0x%08x EBP=0x%08x out_top=0x%08x local_obj=0x%08x",
        stream, dst, ebp, out_top, obj);
    exp_log(buf);
    if (addr == VA_VARIANT_TAG0B_DESC_RET || addr == VA_VARIANT_TAG0B_OBJ_RET ||
        addr == VA_VARIANT_TAG0B_BA9200_RET || addr == VA_VARIANT_TAG0B_NIL_OBJ) {
        wsprintfA(buf,
            "TAG0B_DESC desc=0x%08x vt=0x%08x key=0x%08x f10=0x%08x f14=0x%08x",
            desc, exp2_read_safe(desc), exp2_read_safe(desc + 0x0c),
            exp2_read_safe(desc + 0x10), exp2_read_safe(desc + 0x14));
        exp_log(buf);
        exp_log_obj_ptr3("TAG0B_OBJECT", obj);
        exp_log_variant_slot3("TAG0B_OUT_PREV_SLOT", prev_slot);
    }
}

static void exp_log_loadlist_step(DWORD addr, PCONTEXT c)
{
    DWORD esp, ret, s1, s2, s3, list_arg, list_head, list_node, payload;
    char buf[768];
    if (!c) return;
    esp = c->Esp;
    ret = exp2_read_safe(esp + 0x00);
    s1 = exp2_read_safe(esp + 0x04);
    s2 = exp2_read_safe(esp + 0x08);
    s3 = exp2_read_safe(esp + 0x0c);
    list_arg = s3;
    list_head = exp2_read_safe(list_arg);
    list_node = exp2_read_safe(esp + 0x7c);
    payload = exp2_read_safe(list_node + 0x08);
    wsprintfA(buf,
        "LOADLIST_STEP addr=0x%08x %s EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x",
        addr, exp_loadlist_step_name(addr), c->Eax, c->Ebx, c->Ecx, c->Edx);
    exp_log(buf);
    wsprintfA(buf,
        "LOADLIST_STEP regs ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x ret=0x%08x",
        c->Esi, c->Edi, c->Ebp, esp, ret);
    exp_log(buf);
    wsprintfA(buf,
        "LOADLIST_STEP stack s1=0x%08x s2=0x%08x s3=0x%08x list_head=0x%08x local_node=0x%08x payload=0x%08x",
        s1, s2, s3, list_head, list_node, payload);
    exp_log(buf);
    wsprintfA(buf,
        "LOADLIST_STEP ptrs EAX*=0x%08x EDI*=0x%08x EBP*=0x%08x ESI*=0x%08x",
        exp2_read_safe(c->Eax), exp2_read_safe(c->Edi),
        exp2_read_safe(c->Ebp), exp2_read_safe(c->Esi));
    exp_log(buf);
    if (addr == VA_LOADLIST_PLAYER_RET || addr == VA_LOADLIST_PLAYER_TEST ||
        addr == VA_LOADLIST_PLAYER_FLAGS || addr == VA_LOADLIST_SCORE_RET ||
        addr == VA_LOADLIST_ACTIVE_PATH) {
        exp_dump_bytes("LOADLIST_EBP_PLAYER", c->Ebp, 64);
    }
    if (addr == VA_LOADLIST_NODE_PAYLOAD || addr == VA_LOADLIST_REF_RET ||
        addr == VA_LOADLIST_PLAYER_DESC || addr == VA_LOADLIST_PLAYER_RESOLVE) {
        exp_dump_bytes("LOADLIST_LIST_ARG", list_arg, 64);
        exp_dump_bytes("LOADLIST_LOCAL_NODE", list_node, 64);
        exp_dump_bytes("LOADLIST_PAYLOAD", payload, 64);
    }
    if (addr == VA_LOADLIST_EMPTY_BRANCH ||
        addr == VA_LOADLIST_EMPTY_RELEASE_RET ||
        addr == VA_LOADLIST_EMPTY_RELEASE_DTOR ||
        addr == VA_LOADLIST_EMPTY_PARAM3_SCAN ||
        addr == VA_LOADLIST_OLD_BEGIN ||
        addr == VA_LOADLIST_OLD_MARK_ROW ||
        addr == VA_LOADLIST_OLD_HELPER ||
        addr == VA_LOADLIST_OLD_FETCH_RET ||
        addr == VA_LOADLIST_OLD_ROW_VCALL ||
        addr == VA_LOADLIST_OLD_AFTER_VCALL ||
        addr == VA_LOADLIST_EMPTY_CLEANUP_RET) {
        wsprintfA(buf,
            "LOADLIST_EMPTY_LOCALS +7c=0x%08x +80=0x%08x +84=0x%08x +88=0x%08x +8c=0x%08x +90=0x%08x +94=0x%08x +98=0x%08x",
            exp2_read_safe(esp + 0x7c), exp2_read_safe(esp + 0x80),
            exp2_read_safe(esp + 0x84), exp2_read_safe(esp + 0x88),
            exp2_read_safe(esp + 0x8c), exp2_read_safe(esp + 0x90),
            exp2_read_safe(esp + 0x94), exp2_read_safe(esp + 0x98));
        exp_log(buf);
        wsprintfA(buf,
            "LOADLIST_EMPTY_LISTS local20=0x%08x local20_next=0x%08x local8c=0x%08x local8c_next=0x%08x arg3=0x%08x arg3_next=0x%08x",
            exp2_read_safe(esp + 0x84), exp2_read_safe(exp2_read_safe(esp + 0x84)),
            exp2_read_safe(esp + 0x8c), exp2_read_safe(exp2_read_safe(esp + 0x8c)),
            exp2_read_safe(esp + 0xb0), exp2_read_safe(exp2_read_safe(esp + 0xb0)));
        exp_log(buf);
        exp_dump_bytes("LOADLIST_EMPTY_STACK", esp, 64);
        exp_dump_bytes("LOADLIST_EMPTY_LOCAL20", exp2_read_safe(esp + 0x84), 64);
        exp_dump_bytes("LOADLIST_EMPTY_LOCAL8C", exp2_read_safe(esp + 0x8c), 64);
        exp_dump_bytes("LOADLIST_EMPTY_ARG3", exp2_read_safe(esp + 0xb0), 64);
        exp_dump_bytes("LOADLIST_EMPTY_EAX", c->Eax, 64);
        exp_dump_bytes("LOADLIST_EMPTY_ECX", c->Ecx, 64);
        exp_dump_bytes("LOADLIST_EMPTY_EDX", c->Edx, 64);
        exp_dump_bytes("LOADLIST_EMPTY_ESI", c->Esi, 64);
        exp_dump_bytes("LOADLIST_EMPTY_EDI", c->Edi, 64);
        exp_dump_bytes("LOADLIST_EMPTY_EBP", c->Ebp, 64);
    }
}

static void exp_log_loadlist_bad_helper(DWORD addr, PCONTEXT c)
{
    DWORD esp, ret, a1, a2, a3, eax4;
    char buf[768];
    if (!c) return;
    esp = c->Esp;
    ret = exp2_read_safe(esp + 0x00);
    a1 = exp2_read_safe(esp + 0x04);
    a2 = exp2_read_safe(esp + 0x08);
    a3 = exp2_read_safe(esp + 0x0c);
    eax4 = exp2_read_safe(c->Eax + 0x04);
    wsprintfA(buf,
        "LOADLIST_BADHELPER addr=0x%08x %s EIP=0x%08x EAX=0x%08x ECX=0x%08x EDX=0x%08x",
        addr, exp_loadlist_step_name(addr), c->Eip, c->Eax, c->Ecx, c->Edx);
    exp_log(buf);
    wsprintfA(buf,
        "LOADLIST_BADHELPER regs EBX=0x%08x ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x",
        c->Ebx, c->Esi, c->Edi, c->Ebp, esp);
    exp_log(buf);
    wsprintfA(buf,
        "LOADLIST_BADHELPER stack ret=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x eax_plus4=0x%08x",
        ret, a1, a2, a3, eax4);
    exp_log(buf);
    exp_log_addr_region("LOADLIST_BADHELPER_RET_REGION", ret);
    exp_log_addr_region("LOADLIST_BADHELPER_A1_REGION", a1);
    exp_log_addr_region("LOADLIST_BADHELPER_EAX_REGION", c->Eax);
    exp_dump_bytes("LOADLIST_BADHELPER_STACK", esp, 96);
    exp_dump_bytes("LOADLIST_BADHELPER_A1", a1, 64);
    exp_dump_bytes("LOADLIST_BADHELPER_A2", a2, 64);
    exp_dump_bytes("LOADLIST_BADHELPER_EAX", c->Eax, 64);
}

static void exp_log_loadlist_probe(DWORD addr, PCONTEXT c)
{
    DWORD esp, ret, a1, a2, a3, a4, a5, a6;
    char buf[640];
    if (!c) return;
    esp = c->Esp;
    ret = exp2_read_safe(esp + 0x00);
    a1 = exp2_read_safe(esp + 0x04);
    a2 = exp2_read_safe(esp + 0x08);
    a3 = exp2_read_safe(esp + 0x0c);
    a4 = exp2_read_safe(esp + 0x10);
    a5 = exp2_read_safe(esp + 0x14);
    a6 = exp2_read_safe(esp + 0x18);
    wsprintfA(buf,
        "LOADLIST_PROBE addr=0x%08x %s EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x",
        addr, exp_post_finish_ui_name(addr), c->Eax, c->Ebx, c->Ecx, c->Edx);
    exp_log(buf);
    wsprintfA(buf,
        "LOADLIST_PROBE regs ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x ret=0x%08x",
        c->Esi, c->Edi, c->Ebp, esp, ret);
    exp_log(buf);
    wsprintfA(buf,
        "LOADLIST_PROBE stack a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x a5=0x%08x a6=0x%08x",
        a1, a2, a3, a4, a5, a6);
    exp_log(buf);

    if (addr == VA_CHARMGR_LOADLIST) {
        exp_log("LOADLIST_ENTRY dumping ECX/ESI and stack candidates");
        exp_log_charmgr_container("LOADLIST_ENTRY_ECX", c->Ecx);
        exp_log_charmgr_container("LOADLIST_ENTRY_ESI", c->Esi);
        exp_dump_bytes("LOADLIST_ARG1", a1, 64);
        exp_dump_bytes("LOADLIST_ARG2", a2, 64);
        exp_dump_bytes("LOADLIST_ARG3", a3, 64);
    } else if (addr == VA_ONLOAD_AFTER_LOADLIST) {
#ifdef EXP_CAPTURE_LOADLIST_ESI_CHARMGR
        DWORD esi_list28 = c->Esi ? exp2_read_safe(c->Esi + 0x28) : 0;
        if (c->Esi && esi_list28) {
            g_last_charmgr = c->Esi;
            g_last_charmgr_owner = a1;
            exp_logf("LOADLIST_CAPTURE_ESI_CHARMGR mgr=0x%08x owner=0x%08x list28=0x%08x",
                     c->Esi, a1, esi_list28);
        } else {
            exp_logf("LOADLIST_CAPTURE_ESI_CHARMGR skip esi=0x%08x list28=0x%08x",
                     c->Esi, esi_list28, 0);
        }
#endif
        exp_logf("LOADLIST_RETURN count_or_ptr(EAX)=0x%08x captured=0x%08x esi=0x%08x",
                 c->Eax, (DWORD)g_last_charmgr, c->Esi);
        exp_log_charmgr_container("LOADLIST_AFTER_CAPTURED", (DWORD)g_last_charmgr);
        exp_log_charmgr_container("LOADLIST_AFTER_ESI", c->Esi);
        exp_dump_bytes("LOADLIST_AFTER_STACK", esp, 64);
    } else if (addr == VA_SERVER_FINISH_SEND) {
        exp_log("SERVER_FINISH_SEND entry reached");
        exp_log_charmgr_container("SERVER_FINISH_CAPTURED", (DWORD)g_last_charmgr);
        exp_dump_bytes("SERVER_FINISH_STACK", esp, 64);
    }
}
#endif

/* Dump a window of raw bytes (n<=64) from address base, as hex, via SEH-guarded
 * dword reads so a bad page never crashes us. */
static void exp_dump_bytes(const char *tag, DWORD base, DWORD n)
{
    char buf[256];
    char *w = buf;
    DWORD i;
    if (n > 64) n = 64;
    if (!base) { exp_logf("%s base=NULL", (DWORD)(DWORD_PTR)tag, 0, 0); return; }
    w += wsprintfA(w, "%s base=0x%08x:", tag, base);
    for (i = 0; i < n; ++i) {
        BYTE b = (BYTE)(exp2_read_safe(base + (i & ~3u)) >> ((i & 3u) * 8));
        w += wsprintfA(w, " %02x", b);
    }
    exp_log(buf);
}

#ifdef EXP_DUMP_SESSION_E0_LIFECYCLE
static volatile LONG g_session_e0_budget = 180;

static const char *exp_session_e0_name(DWORD addr)
{
    switch (addr) {
    case VA_SESSION_MASTEROPEN: return "Session::masterOpen";
    case VA_SESSION_MASTERREOPEN: return "Session::masterReopen";
    case VA_SESSION_E0_STORE_HELPER: return "FUN_00a8d1d0_slot_materializer";
    default: return "session_e0_marker";
    }
}

static void exp_log_session_side(const char *tag, DWORD session)
{
    char buf[512];
    if (!session) {
        exp_logf("%s session=NULL", (DWORD)(DWORD_PTR)tag, 0, 0);
        return;
    }
    wsprintfA(buf,
        "%s session=0x%08x +d8=0x%08x +e0=0x%08x +f0=0x%08x",
        tag, session, exp2_read_safe(session + 0xd8),
        exp2_read_safe(session + 0xe0), exp2_read_safe(session + 0xf0));
    exp_log(buf);
    wsprintfA(buf,
        "%s +e4=0x%08x +e8=0x%08x +ec=0x%08x",
        tag, exp2_read_safe(session + 0xe4), exp2_read_safe(session + 0xe8),
        exp2_read_safe(session + 0xec));
    exp_log(buf);
}

static void exp_log_session_e0_lifecycle(DWORD addr, PCONTEXT c)
{
    DWORD esp, ret, a1, a2, a3, a4, slot, oldv, src, base;
    char buf[768];
    LONG left;
    if (!c)
        return;
    left = InterlockedDecrement(&g_session_e0_budget);
    if (left < 0)
        return;

    esp = c->Esp;
    ret = exp2_read_safe(esp + 0x00);
    a1 = exp2_read_safe(esp + 0x04);
    a2 = exp2_read_safe(esp + 0x08);
    a3 = exp2_read_safe(esp + 0x0c);
    a4 = exp2_read_safe(esp + 0x10);

    wsprintfA(buf,
        "SESSION_E0 addr=0x%08x %s left=%d EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x",
        addr, exp_session_e0_name(addr), left, c->Eax, c->Ebx, c->Ecx, c->Edx);
    exp_log(buf);
    wsprintfA(buf,
        "SESSION_E0 regs ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x ret=0x%08x",
        c->Esi, c->Edi, c->Ebp, esp, ret);
    exp_log(buf);
    wsprintfA(buf,
        "SESSION_E0 stack a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x",
        a1, a2, a3, a4);
    exp_log(buf);

    if (addr == VA_SESSION_MASTEROPEN) {
        exp_log("SESSION_E0_MASTEROPEN entry: static caller should later call a8d1d0 on Session+0xe0");
        exp_log_session_side("SESSION_E0_MASTEROPEN_EDX", c->Edx);
        exp_log_session_side("SESSION_E0_MASTEROPEN_ECX", c->Ecx);
        exp_log_session_side("SESSION_E0_MASTEROPEN_ESI", c->Esi);
        exp_log_session_side("SESSION_E0_MASTEROPEN_EDI", c->Edi);
        exp_dump_bytes("SESSION_E0_MASTEROPEN_STACK", esp, 64);
        return;
    }

    if (addr == VA_SESSION_MASTERREOPEN) {
        exp_log("SESSION_E0_MASTERREOPEN entry: static path consumes existing Session+0xe0");
        exp_log_session_side("SESSION_E0_MASTERREOPEN_ECX", c->Ecx);
        exp_log_session_side("SESSION_E0_MASTERREOPEN_ESI", c->Esi);
        exp_log_session_side("SESSION_E0_MASTERREOPEN_EDI", c->Edi);
        exp_dump_bytes("SESSION_E0_MASTERREOPEN_STACK", esp, 64);
        return;
    }

    if (addr == VA_SESSION_E0_STORE_HELPER) {
        slot = a1;
        oldv = exp2_read_safe(slot);
        src = c->Esi;
        base = (slot >= 0xe0) ? slot - 0xe0 : 0;
        wsprintfA(buf,
            "SESSION_E0_HELPER ret=0x%08x slot(arg1)=0x%08x old=0x%08x srcESI=0x%08x srcVT=0x%08x",
            ret, slot, oldv, src, exp2_read_safe(src));
        exp_log(buf);
        wsprintfA(buf,
            "SESSION_E0_HELPER base?=0x%08x base+d8=0x%08x base+e0=0x%08x base+f0=0x%08x",
            base, exp2_read_safe(base + 0xd8), exp2_read_safe(base + 0xe0),
            exp2_read_safe(base + 0xf0));
        exp_log(buf);
        if (ret == 0x00a7f26eu)
            exp_log("SESSION_E0_HELPER_CALLSITE masterOpen_after_a8d1d0");
        if (ret == 0x00a83ef5u)
            exp_log("SESSION_E0_HELPER_CALLSITE Session_copy_state_after_a8d1d0");
        if (ret == 0x00a914d4u)
            exp_log("SESSION_E0_HELPER_CALLSITE Connector_response_after_a8d1d0");
        exp_dump_bytes("SESSION_E0_HELPER_SLOT", slot, 32);
        exp_dump_bytes("SESSION_E0_HELPER_SRC", src, 48);
    }
}
#endif

#ifdef EXP_DUMP_TRANSITION_TEARDOWN
static const char *exp_transition_name(DWORD addr)
{
    switch (addr) {
    case VA_FCLIENTAPP_FINALIZE: return "FClientApp::OnFinalize";
    case VA_DRAIN_TEARDOWN: return "drain_teardown_00bc15a0";
    case VA_NET_REPORT_HELPER: return "NetComputer_report_00ca4070";
    case VA_SESSION_SHUTDOWN: return "session_shutdown_0115de60";
    case VA_SOCKET_CLOSE_HELPER: return "socket_close_helper_011672e0";
    default: return "transition_marker";
    }
}

static void exp_log_transition_stack(const char *tag, PCONTEXT c)
{
    char buf[768];
    DWORD esp, ebp, i, cur;
    if (!c)
        return;
    esp = c->Esp;
    ebp = c->Ebp;
    wsprintfA(buf,
        "%s regs EIP=0x%08x EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x",
        tag, c->Eip, c->Eax, c->Ebx, c->Ecx, c->Edx);
    exp_log(buf);
    wsprintfA(buf,
        "%s regs ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x EFLAGS=0x%08x",
        tag, c->Esi, c->Edi, ebp, esp, c->EFlags);
    exp_log(buf);
    wsprintfA(buf,
        "%s stack ret0=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x",
        tag,
        exp2_read_safe(esp), exp2_read_safe(esp + 4),
        exp2_read_safe(esp + 8), exp2_read_safe(esp + 0xc),
        exp2_read_safe(esp + 0x10));
    exp_log(buf);
    cur = ebp;
    for (i = 0; i < 5 && cur; ++i) {
        DWORD next = exp2_read_safe(cur);
        DWORD ret = exp2_read_safe(cur + 4);
        wsprintfA(buf, "%s ebp_chain[%u] frame=0x%08x ret=0x%08x next=0x%08x",
                  tag, i, cur, ret, next);
        exp_log(buf);
        if (next <= cur || next - cur > 0x100000)
            break;
        cur = next;
    }
}

static void exp_log_transition_teardown(DWORD addr, PCONTEXT c)
{
    char buf[768];
    DWORD sock_guess = 0;
    DWORD close_slot = exp2_read_safe(0x01480ee0u);
    DWORD dup_slot = exp2_read_safe(0x014802f8u);
    if (!c)
        return;
    if (addr == VA_SOCKET_CLOSE_HELPER)
        sock_guess = exp2_read_safe(c->Ecx + 4);
    else if (addr == VA_SESSION_SHUTDOWN)
        sock_guess = exp2_read_safe(c->Ecx + 0x24);
    wsprintfA(buf,
        "TRANSITION_TEARDOWN addr=0x%08x name=%s ecx=0x%08x sock_guess=0x%08x",
        addr, exp_transition_name(addr), c->Ecx, sock_guess);
    exp_log(buf);
    wsprintfA(buf,
        "TRANSITION_TEARDOWN slots closesocket=0x%08x duphandle=0x%08x drain_flag=0x%08x",
        close_slot, dup_slot, exp2_read_safe(VA_DRAIN_FLAG));
    exp_log(buf);
    if (addr == VA_SESSION_SHUTDOWN) {
        wsprintfA(buf,
            "TRANSITION_SHUTDOWN session=0x%08x +20=0x%08x +24=0x%08x +30=0x%08x",
            c->Ecx, exp2_read_safe(c->Ecx + 0x20),
            exp2_read_safe(c->Ecx + 0x24), exp2_read_safe(c->Ecx + 0x30));
        exp_log(buf);
    }
    exp_log_transition_stack("TRANSITION_TEARDOWN", c);
}
#endif

#ifdef EXP_DUMP_PROACTOR_EXCEPTION
static DWORD exp_proactor_packet_size(DWORD packet)
{
    DWORD holder, beginp, endp, p10, p14;
    if (!packet)
        return 0;
    holder = exp2_read_safe(packet + 0x0c);
    beginp = exp2_read_safe(holder + 0x08);
    endp = exp2_read_safe(holder + 0x0c);
    p10 = exp2_read_safe(packet + 0x10);
    p14 = exp2_read_safe(packet + 0x14);
    return (endp - beginp) - p14 + p10;
}

static DWORD exp_proactor_packet_begin(DWORD packet)
{
    DWORD holder;
    if (!packet)
        return 0;
    holder = exp2_read_safe(packet + 0x0c);
    return exp2_read_safe(holder + 0x08);
}

static void exp_log_proactor_pair(const char *tag, DWORD addr, PCONTEXT c,
                                  DWORD computer, DWORD packet)
{
    char buf[768];
    DWORD comp_vt, pkt_vt, handler, comp_id, holder, beginp, endp, p10, p14;
    DWORD size;
    if (!c)
        return;
    comp_vt = exp2_read_safe(computer);
    pkt_vt = exp2_read_safe(packet);
    handler = exp2_read_safe(comp_vt + 0x0c);
    comp_id = exp2_read_safe(computer + 0x0c);
    holder = exp2_read_safe(packet + 0x0c);
    beginp = exp2_read_safe(holder + 0x08);
    endp = exp2_read_safe(holder + 0x0c);
    p10 = exp2_read_safe(packet + 0x10);
    p14 = exp2_read_safe(packet + 0x14);
    size = exp_proactor_packet_size(packet);
    wsprintfA(buf,
        "%s addr=0x%08x computer=0x%08x comp_vt=0x%08x comp_id=0x%08x handler=0x%08x",
        tag, addr, computer, comp_vt, comp_id, handler);
    exp_log(buf);
    wsprintfA(buf,
        "%s packet=0x%08x pkt_vt=0x%08x holder=0x%08x begin=0x%08x end=0x%08x",
        tag, packet, pkt_vt, holder, beginp, endp);
    exp_log(buf);
    wsprintfA(buf,
        "%s packet_fields p10=0x%08x p14=0x%08x computed_size=0x%08x eax=0x%08x",
        tag, p10, p14, size, c->Eax);
    exp_log(buf);
    if (beginp)
        exp_dump_bytes(tag, beginp, 64);
}

static void exp_log_proactor_exception(DWORD addr, PCONTEXT c)
{
    char buf[768], text[192], s0[96], s1[96];
    DWORD esp, ebp, computer, packet, i;
    if (!c)
        return;
    esp = c->Esp;
    ebp = c->Ebp;
    if (addr == VA_PROACTOR_DISPATCH_CALL) {
        exp_log_proactor_pair("PROACTOR_DISPATCH", addr, c, c->Ebx, c->Edi);
        return;
    }
    if (addr == VA_PROACTOR_EXCEPTION_BUILD) {
        computer = exp2_read_safe(ebp - 0x1c);
        packet = exp2_read_safe(ebp - 0x18);
        exp_log_proactor_pair("PROACTOR_EXCEPTION_BUILD", addr, c, computer, packet);
        wsprintfA(buf,
            "PROACTOR_EXCEPTION_LOCALS ebp=0x%08x local_e0=0x%08x local_5c=0x%08x",
            ebp, exp2_read_safe(ebp - 0xe0), exp2_read_safe(ebp - 0x5c));
        exp_log(buf);
        return;
    }
    if (addr == VA_PROACTOR_EXCEPTION_TEXT_RET) {
        exp_read_cstr_safe(c->Eax, text, sizeof(text));
        wsprintfA(buf, "PROACTOR_EXCEPTION_TEXT ptr=0x%08x text=%s", c->Eax, text);
        exp_log(buf);
        return;
    }
    if (addr == VA_PROACTOR_REPORT_CALL) {
        computer = exp2_read_safe(ebp - 0x1c);
        packet = exp2_read_safe(ebp - 0x18);
        exp_log_proactor_pair("PROACTOR_REPORT_CALL", addr, c, computer, packet);
        wsprintfA(buf,
            "PROACTOR_REPORT_STACK esp=0x%08x d0=0x%08x d1=0x%08x d2=0x%08x d3=0x%08x",
            esp, exp2_read_safe(esp), exp2_read_safe(esp + 4),
            exp2_read_safe(esp + 8), exp2_read_safe(esp + 0x0c));
        exp_log(buf);
        wsprintfA(buf,
            "PROACTOR_REPORT_STACK2 d4=0x%08x d5=0x%08x d6=0x%08x d7=0x%08x",
            exp2_read_safe(esp + 0x10), exp2_read_safe(esp + 0x14),
            exp2_read_safe(esp + 0x18), exp2_read_safe(esp + 0x1c));
        exp_log(buf);
        exp_read_std_string_safe(exp2_read_safe(esp), s0, sizeof(s0));
        exp_read_std_string_safe(exp2_read_safe(esp + 4), s1, sizeof(s1));
        if (s0[0] || s1[0]) {
            wsprintfA(buf, "PROACTOR_REPORT_STRINGS s0=%s s1=%s", s0, s1);
            exp_log(buf);
        }
        for (i = 0; i < 2; ++i)
            exp_dump_bytes("PROACTOR_REPORT_STACK_BYTES", esp + i * 0x20, 32);
    }
}
#endif

#ifdef EXP_LOG_ALL_EXCEPTIONS
static volatile LONG g_all_exception_budget = 32;
static volatile LONG g_exception_log_active = 0;

static void exp_log_any_exception(PEXCEPTION_POINTERS ei)
{
    PCONTEXT c;
    DWORD code, exaddr, info0, info1;
    LONG left;
    char buf[768];
    if (!ei || !ei->ExceptionRecord)
        return;
    if (InterlockedCompareExchange(&g_exception_log_active, 1, 0) != 0)
        return;
    c = ei->ContextRecord;
    code = ei->ExceptionRecord->ExceptionCode;
    exaddr = (DWORD)ei->ExceptionRecord->ExceptionAddress;
#ifdef EXP_IGNORE_CXX_EH_SPAM
    if (code == 0xe06d7363u) {
        InterlockedExchange(&g_exception_log_active, 0);
        return;
    }
#endif
    if (code == EXCEPTION_ACCESS_VIOLATION && exp_is_self_module_addr(exaddr)) {
        InterlockedExchange(&g_exception_log_active, 0);
        return;
    }
    left = InterlockedDecrement(&g_all_exception_budget);
    if (left < 0) {
        InterlockedExchange(&g_exception_log_active, 0);
        return;
    }
    info0 = (ei->ExceptionRecord->NumberParameters > 0) ?
        (DWORD)ei->ExceptionRecord->ExceptionInformation[0] : 0;
    info1 = (ei->ExceptionRecord->NumberParameters > 1) ?
        (DWORD)ei->ExceptionRecord->ExceptionInformation[1] : 0;
    wsprintfA(buf,
        "TRANSITION_EXCEPTION code=0x%08x addr=0x%08x info0=0x%08x info1=0x%08x left=%d",
        code, exaddr, info0, info1, left);
    exp_log(buf);
    exp_log_addr_region("TRANSITION_EXCEPTION_EIP_REGION", exaddr);
    exp_log_addr_region("TRANSITION_EXCEPTION_INFO1_REGION", info1);
    if (c) {
        exp_log_addr_region("TRANSITION_EXCEPTION_RET0_REGION",
                            exp2_read_safe(c->Esp));
        exp_log_addr_region("TRANSITION_EXCEPTION_EBP_RET_REGION",
                            exp2_read_safe(c->Ebp + 4));
        exp_log_addr_region("TRANSITION_EXCEPTION_ECX_REGION", c->Ecx);
        exp_log_transition_stack("TRANSITION_EXCEPTION", c);
    }
    InterlockedExchange(&g_exception_log_active, 0);
}
#endif

#ifdef EXP_DUMP_UI_LIFECYCLE
static volatile LONG g_ui_lifecycle_active = 0;
static volatile LONG g_ui_lifecycle_budget = 220;
static volatile LONG g_ui_render_budget = 120;
static volatile DWORD g_last_ui_render_ctrl = 0;
static volatile DWORD g_last_ui_render_vt = 0;

static int exp_is_ui_render_addr(DWORD addr)
{
    return addr == VA_UIMANAGER_RENDER3DUI ||
           addr == VA_UICONTROL_RENDER ||
           addr == VA_UICONTAINER_RENDER ||
           addr == VA_UICONTAINER_RENDER3DUI ||
           addr == VA_UICONTAINER_RENDER3DLAYER;
}

static int exp_is_ui_lifecycle_addr(DWORD addr)
{
    return addr == VA_UIMANAGER_ATTACH ||
           addr == VA_UIMANAGER_MOVEFRONT ||
           exp_is_ui_render_addr(addr) ||
           addr == VA_UICONTROL_SETVISIBLE ||
           addr == VA_UICONTAINER_SETVISIBLE ||
           addr == VA_UICONTAINER_ATTACH ||
           addr == VA_UICONTAINER_ATTACHBACK;
}

static const char *exp_ui_lifecycle_name(DWORD addr)
{
    switch (addr) {
    case VA_UIMANAGER_ATTACH: return "UIControlManager::AttachControl";
    case VA_UIMANAGER_MOVEFRONT: return "UIControlManager::MoveControlToFront";
    case VA_UIMANAGER_RENDER3DUI: return "UIControlManager::Render3DUI";
    case VA_UICONTROL_RENDER: return "UIControl::Render";
    case VA_UICONTROL_SETVISIBLE: return "UIControl::SetVisible";
    case VA_UICONTAINER_SETVISIBLE: return "UIContainer::SetVisible";
    case VA_UICONTAINER_RENDER: return "UIContainer::Render";
    case VA_UICONTAINER_ATTACH: return "UIContainer::AttachControl";
    case VA_UICONTAINER_ATTACHBACK: return "UIContainer::AttachControlToBack";
    case VA_UICONTAINER_RENDER3DUI: return "UIContainer::Render3DUI";
    case VA_UICONTAINER_RENDER3DLAYER: return "UIContainer::Render3DLayer";
    default: return "UI lifecycle";
    }
}

static void exp_ui_name_candidates(DWORD ctrl, char *out, int cap)
{
    DWORD off;
    int count;
    char s[56];
    char tmp[96];
    if (!out || cap <= 0)
        return;
    out[0] = 0;
    if (!ctrl)
        return;
    count = 0;
    for (off = 0x20; off <= 0x160 && count < 5; off += 4) {
        s[0] = 0;
        exp_read_std_string_safe(ctrl + off, s, sizeof(s));
        if (s[0]) {
            if (lstrlenA(out) + lstrlenA(s) + 16 >= cap)
                break;
            wsprintfA(tmp, " +%02x='%s'", off, s);
            lstrcatA(out, tmp);
            ++count;
        }
    }
}

static void exp_log_ui_control_brief(const char *tag, DWORD ctrl)
{
    char buf[768], names[384];
    DWORD vt, visible_word, parent_a, parent_b;
    names[0] = 0;
    if (!ctrl) {
        exp_logf("%s ctrl=NULL", (DWORD)(DWORD_PTR)tag, 0, 0);
        return;
    }
    vt = exp2_read_safe(ctrl);
    visible_word = exp2_read_safe(ctrl + 0x5c);
    parent_a = exp2_read_safe(ctrl + 0x10);
    parent_b = exp2_read_safe(ctrl + 0x14);
    exp_ui_name_candidates(ctrl, names, sizeof(names));
    wsprintfA(buf,
        "%s ctrl=0x%08x vt=0x%08x visible_word(+5c)=0x%08x parent?+10=0x%08x +14=0x%08x names:%s",
        tag, ctrl, vt, visible_word, parent_a, parent_b, names);
    exp_log(buf);
}

static void exp_log_ui_lifecycle(DWORD addr, PCONTEXT c)
{
    DWORD esp, a1, a2, a3;
    DWORD ctrl, vt;
    LONG left;
    char tag[192];
    if (!c)
        return;
    if (g_ui_lifecycle_active == 0)
        return;
    if (exp_is_ui_render_addr(addr)) {
        ctrl = c->Ecx;
        vt = exp2_read_safe(ctrl);
        if (ctrl == g_last_ui_render_ctrl && vt == g_last_ui_render_vt)
            return;
        left = InterlockedDecrement(&g_ui_render_budget);
        if (left < 0)
            return;
        g_last_ui_render_ctrl = ctrl;
        g_last_ui_render_vt = vt;
        esp = c->Esp;
        a1 = exp2_read_safe(esp + 4);
        a2 = exp2_read_safe(esp + 8);
        wsprintfA(tag,
            "UI_RENDER addr=0x%08x %s ECX=0x%08x vt=0x%08x a1=0x%08x a2=0x%08x left=%d",
            addr, exp_ui_lifecycle_name(addr), ctrl, vt, a1, a2, left);
        exp_log(tag);
        exp_log_ui_control_brief("UI_RENDER_THIS", ctrl);
        if (a1)
            exp_log_ui_control_brief("UI_RENDER_A1", a1);
        return;
    }
    left = InterlockedDecrement(&g_ui_lifecycle_budget);
    if (left < 0)
        return;
    esp = c->Esp;
    a1 = exp2_read_safe(esp + 4);
    a2 = exp2_read_safe(esp + 8);
    a3 = exp2_read_safe(esp + 0x0c);
    wsprintfA(tag,
        "UI_LIFECYCLE addr=0x%08x %s ECX=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x left=%d",
        addr, exp_ui_lifecycle_name(addr), c->Ecx, a1, a2, a3, left);
    exp_log(tag);
    if (addr == VA_UICONTROL_SETVISIBLE ||
        addr == VA_UICONTAINER_SETVISIBLE) {
        exp_logf("UI_VISIBLE requested visible_arg=0x%08x", a1, 0, 0);
        exp_log_ui_control_brief("UI_VISIBLE_THIS", c->Ecx);
    } else if (addr == VA_UIMANAGER_MOVEFRONT) {
        exp_log_ui_control_brief("UI_MOVEFRONT_A1", a1);
        exp_log_ui_control_brief("UI_MOVEFRONT_ECX", c->Ecx);
    } else {
        exp_log_ui_control_brief("UI_ATTACH_ECX_PARENT", c->Ecx);
        exp_log_ui_control_brief("UI_ATTACH_A1", a1);
        exp_log_ui_control_brief("UI_ATTACH_A2", a2);
    }
}
#endif

/* At the component-bridge entry, the packet reader is at (a2+4) [vtable at +0].
 * cursor = *(*(reader+0x18)+8) [FUN_005fa970]; the readable length is computed
 * by FUN_005fa980 from a backing std::vector. Compute both and dump the buffer
 * so we can see offline whether the reader even contains the 2nd component key
 * (Connector 0x7d1fec2e) -- i.e. lower-framing truncation vs ServerCom over-read. */
static void exp_dump_reader(DWORD a2)
{
    DWORD reader, S, cursor, B, C, beginp, endp, vecsize, b10, b14, endlen;
    reader = a2 + 4;
    S = exp2_read_safe(a2 + 0x1c);          /* *(reader+0x18) */
    cursor = exp2_read_safe(S + 8);
    B = exp2_read_safe(S + 4);
    C = exp2_read_safe(B + 0xc);
    beginp = exp2_read_safe(C + 8);
    endp = exp2_read_safe(C + 0xc);
    vecsize = endp - beginp;
    b10 = exp2_read_safe(B + 0x10);
    b14 = exp2_read_safe(B + 0x14);
    endlen = (vecsize - b14) + b10;
    exp_logf("READER a2=0x%08x reader=0x%08x vtbl=0x%08x", a2, reader, exp2_read_safe(reader));
    exp_logf("READER S=0x%08x cursor=0x%08x B=0x%08x", S, cursor, B);
    exp_logf("READER C=0x%08x begin=0x%08x end=0x%08x", C, beginp, endp);
    exp_logf("READER vecsize=0x%08x B+10=0x%08x B+14=0x%08x", vecsize, b10, b14);
    exp_logf("READER endlen=0x%08x remaining(end-cursor)=0x%08x cursor=0x%08x", endlen, endlen - cursor, cursor);
    exp_dump_bytes("READERBUF", beginp, 64);
    exp_dump_bytes("READERCUR", beginp + cursor, 64);
}

#ifdef EXP_DUMP_GOLUA_UNPACKFIXED
static void exp_log_lua_slot(const char *tag, DWORD slot)
{
    char buf[256];
    DWORD v0, v4, typ, vc;
    v0 = exp2_read_safe(slot + 0);
    v4 = exp2_read_safe(slot + 4);
    typ = exp2_read_safe(slot + 8);
    vc = exp2_read_safe(slot + 0x0c);
    wsprintfA(buf, "%s slot=0x%08x v0=0x%08x v4=0x%08x type=0x%08x vc=0x%08x",
              tag, slot, v0, v4, typ, vc);
    exp_log(buf);
}

#ifdef EXP_DUMP_GOLUA_KEYSTR
static void exp_log_lua_tstring_slot(const char *tag, DWORD slot)
{
    char buf[384];
    char text[80];
    DWORD typ, ptr, len0c, len10, i, maxn;

    typ = exp2_read_safe(slot + 8);
    ptr = exp2_read_safe(slot + 0);
    len0c = ptr ? exp2_read_safe(ptr + 0x0c) : 0;
    len10 = ptr ? exp2_read_safe(ptr + 0x10) : 0;

    text[0] = 0;
    if (typ == 4 && ptr) {
        maxn = len0c;
        if (maxn > 63) maxn = 63;
        for (i = 0; i < maxn; ++i) {
            DWORD raw = exp2_read_safe(ptr + 0x10 + (i & ~3u));
            BYTE b = (BYTE)(raw >> ((i & 3u) * 8));
            if (b == 0) break;
            text[i] = (b >= 0x20 && b <= 0x7e) ? (char)b : '.';
        }
        text[i] = 0;
    }

    wsprintfA(buf,
        "%s slot=0x%08x type=0x%08x tstr=0x%08x len0c=0x%08x word10=0x%08x text16='%s'",
        tag, slot, typ, ptr, len0c, len10, text);
    exp_log(buf);
    if (typ == 4 && ptr) {
        exp_dump_bytes("GOLUA_KEY_TSTRING", ptr, 32);
        exp_dump_bytes("GOLUA_KEY_TEXT16", ptr + 0x10, 48);
    }
}
#endif

static DWORD exp_reader_body_from_reader(DWORD reader, DWORD *cursor_out)
{
    DWORD S, B, body, cursor;
    S = exp2_read_safe(reader + 0x18);
    cursor = exp2_read_safe(S + 8);
    B = exp2_read_safe(S + 4);
    body = exp2_read_safe(B + 0x14);
    if (cursor_out) *cursor_out = cursor;
    return body;
}

static void exp_log_golua_unpackfixed(DWORD addr, PCONTEXT c)
{
    static DWORD seen = 0;
    DWORD esp, ret, frame, base, cur, top, slot, typ, val, reader, cursor, body;
    char buf[512];
    if (!c) return;
    ++seen;
    if (seen > 240) {
        if (seen == 241)
            exp_log("GOLUA_UNPACKFIXED log budget exhausted at 240 calls");
        return;
    }

    esp = c->Esp;
    ret = exp2_read_safe(esp + 0);
    frame = exp2_read_safe(esp + 4);
    base = exp2_read_safe(frame + 8);
    cur = exp2_read_safe(frame + 0x0c);
    top = exp2_read_safe(frame + 0x10);
    slot = cur;
    typ = exp2_read_safe(slot + 8);
    val = exp2_read_safe(slot + 0);
    reader = 0;
    if (typ == 2)
        reader = val;
    else if (typ == 7)
        reader = val + 0x18;

    cursor = 0;
    body = reader ? exp_reader_body_from_reader(reader, &cursor) : 0;
    wsprintfA(buf,
        "GOLUA_UNPACKFIXED #%u addr=0x%08x ret=0x%08x frame=0x%08x base=0x%08x cur=0x%08x top=0x%08x",
        seen, addr, ret, frame, base, cur, top);
    exp_log(buf);
    wsprintfA(buf,
        "GOLUA_UNPACKFIXED regs EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x ESI=0x%08x EDI=0x%08x",
        c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi);
    exp_log(buf);
    wsprintfA(buf,
        "GOLUA_UNPACKFIXED source slot=0x%08x type=0x%08x val=0x%08x reader=0x%08x cursor=0x%08x body=0x%08x",
        slot, typ, val, reader, cursor, body);
    exp_log(buf);
    exp_log_lua_slot("GOLUA_SLOT_CUR", cur);
    exp_log_lua_slot("GOLUA_SLOT_CUR+10", cur + 0x10);
    exp_log_lua_slot("GOLUA_SLOT_CUR+20", cur + 0x20);
    exp_log_lua_slot("GOLUA_SLOT_CUR+30", cur + 0x30);
#ifdef EXP_DUMP_GOLUA_KEYSTR
    exp_log_lua_tstring_slot("GOLUA_KEY_SLOT+20", cur + 0x20);
#endif
    exp_dump_bytes("GOLUA_FRAME", frame, 48);
    exp_dump_bytes("GOLUA_CUR", cur, 64);
    if (body)
        exp_dump_bytes("GOLUA_READER_CUR", body + cursor, 32);
}
#endif

static void exp_trace_log(DWORD addr, PCONTEXT ctx)
{
    DWORD reader = ctx->Ecx;
    DWORD S = exp2_read_safe(reader + 0x18);
    DWORD cursor = exp2_read_safe(S + 8);
    DWORD B = exp2_read_safe(S + 4);
    DWORD body = exp2_read_safe(B + 0x14);
    DWORD count = exp2_read_safe(ctx->Esp + 8);   /* RawRead arg2 = byte count */
    char buf[256];
    (void)addr;
    wsprintfA(buf, "TRACERD n=%-5u cursor(body)=0x%08x end?=0x%08x reader=0x%08x body=0x%08x",
              count, cursor, exp2_read_safe(reader + 0x18), reader, body);
    exp_log(buf);
    exp_dump_bytes("TRACEBYTES", body + cursor, 12);
}

#ifdef EXP_DUMP_B4_BOUNDARY
static const char *exp_b4_boundary_name(DWORD addr)
{
    switch (addr) {
    case VA_BB0060_UNPACK_BA9640: return "bb0060_before_ba9640_Unpack_lookup";
    case VA_BB0060_UNPACK_CBDA80: return "bb0060_before_cbda80_Unpack_call";
    case VA_BB0060_UNPACK_RET: return "bb0060_after_cbda80_Unpack_call";
    case VA_BEDE50_AFTER_BB0060: return "bede50_after_bb0060";
    case VA_BEDE50_CURSOR_RET: return "bede50_after_reader_cursor_call";
    case VA_BEDE50_CACHE_BE9840: return "bede50_before_cache_stats";
    case VA_BEDE50_SEEK_START: return "bede50_before_seek_to_component_start";
    case VA_BEDE50_CACHE_RESET: return "bede50_before_cache_reset";
    case VA_BEDE50_COPY_RAW: return "bede50_before_raw_cache_copy";
    case VA_BEDE50_SEEK_BACK: return "bede50_before_seek_back";
    default: return "b4_boundary";
    }
}

static void exp_log_b4_boundary(DWORD addr, PCONTEXT c, DWORD reader, const char *reader_reg)
{
    DWORD S, cursor, B, body, esp, a1, a2, a3;
    char buf[512];
    if (!c) return;
    esp = c->Esp;
    a1 = exp2_read_safe(esp + 4);
    a2 = exp2_read_safe(esp + 8);
    a3 = exp2_read_safe(esp + 0x0c);
    S = exp2_read_safe(reader + 0x18);
    cursor = exp2_read_safe(S + 8);
    B = exp2_read_safe(S + 4);
    body = exp2_read_safe(B + 0x14);
    wsprintfA(buf,
        "B4BOUND addr=0x%08x %s reader(%s)=0x%08x cursor=0x%08x body=0x%08x",
        addr, exp_b4_boundary_name(addr), reader_reg, reader, cursor, body);
    exp_log(buf);
    wsprintfA(buf,
        "B4BOUNDREG addr=0x%08x EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x ESI=0x%08x EDI=0x%08x",
        addr, c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi);
    exp_log(buf);
    wsprintfA(buf,
        "B4BOUNDARGS addr=0x%08x arg1=0x%08x arg2=0x%08x arg3=0x%08x ESP=0x%08x",
        addr, a1, a2, a3, esp);
    exp_log(buf);
    if (addr == VA_BB0060_UNPACK_BA9640)
        exp_dump_bytes("B4BOUND_EDX", c->Edx, 24);
    if (addr == VA_BEDE50_COPY_RAW)
        exp_logf("B4BOUND_COPY_ARGS dst(arg1)=0x%08x count(arg2)=0x%08x", a1, a2, 0);
    if (addr == VA_BEDE50_SEEK_START || addr == VA_BEDE50_SEEK_BACK)
        exp_logf("B4BOUND_SEEK_ARGS target(arg1)=0x%08x mode(arg2)=0x%08x", a1, a2, 0);
    if (body)
        exp_dump_bytes("B4BOUNDCUR", body + cursor, 32);
}
#endif

#ifdef EXP_DUMP_BB0060_TAIL
static const char *exp_bb0060_tail_event_name(DWORD addr)
{
    switch (addr) {
    case VA_BB0060_ENTRY: return "entry";
    case VA_BB0060_TYPE2_ENTRY: return "type2_entry";
    case VA_BB0060_TYPE2_BLOB1: return "type2_blob1_call";
    case VA_BB0060_TYPE2_U8_A: return "type2_u8_a";
    case VA_BB0060_TYPE2_BLOB2: return "type2_blob2_call";
    case VA_BB0060_TYPE2_U8_B: return "type2_u8_b";
    case VA_BB0060_TYPE2_F64: return "type2_f64";
    case VA_BB0060_UNPACK_BA9640: return "before_unpack_lookup";
    case VA_BB0060_UNPACK_CBDA80: return "before_unpack_call";
    case VA_BB0060_UNPACK_RET: return "after_unpack_call";
    case VA_BB0060_TYPE2_EXIT: return "type2_exit";
    default: return "tail";
    }
}

static void exp_log_bb0060_tail(DWORD addr, PCONTEXT c)
{
    DWORD esp, a1, a2, a3, a4, reader, S, cursor, B, body;
    char buf[768];
    if (!c) return;

    esp = c->Esp;
    a1 = exp2_read_safe(esp + 0x04);
    a2 = exp2_read_safe(esp + 0x08);
    a3 = exp2_read_safe(esp + 0x0c);
    a4 = exp2_read_safe(esp + 0x10);

    /* Inside bb0060 the packet reader has been hoisted into EBX. At entry,
     * EBX is not guaranteed yet, so fall back to the first stack arg. */
    reader = c->Ebx ? c->Ebx : a1;
    if (addr == VA_BB0060_ENTRY)
        reader = a1;

    S = exp2_read_safe(reader + 0x18);
    cursor = exp2_read_safe(S + 8);
    B = exp2_read_safe(S + 4);
    body = exp2_read_safe(B + 0x14);

    wsprintfA(buf,
        "BB0060TAIL addr=0x%08x %s reader=0x%08x cursor=0x%08x body=0x%08x",
        addr, exp_bb0060_tail_event_name(addr), reader, cursor, body);
    exp_log(buf);
    wsprintfA(buf,
        "BB0060TAIL regs EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x ESI=0x%08x EDI=0x%08x",
        c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi);
    exp_log(buf);
    wsprintfA(buf,
        "BB0060TAIL stack ret=0x%08x a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x EBP=0x%08x",
        exp2_read_safe(esp), a1, a2, a3, a4, c->Ebp);
    exp_log(buf);
    wsprintfA(buf,
        "BB0060TAIL frame ebp8=0x%08x ebpc=0x%08x esp1c=0x%08x esp20=0x%08x",
        exp2_read_safe(c->Ebp + 0x08), exp2_read_safe(c->Ebp + 0x0c),
        exp2_read_safe(esp + 0x1c), exp2_read_safe(esp + 0x20));
    exp_log(buf);
    if (body && cursor < 0x100000u)
        exp_dump_bytes("BB0060TAILCUR", body + cursor, 48);
}
#endif

#ifdef EXP_DUMP_BB0060_FIELDS
static const char *exp_bb0060_field_event_name(DWORD addr)
{
    switch (addr) {
    case VA_BB0060_FIELD_GATE: return "before_special_gate";
    case VA_BB0060_FIELD_GATE_RET: return "after_special_gate";
    default: return "field_loop";
    }
}

static const char *exp_b4_field_key_name(DWORD key)
{
    switch (key) {
    case 0xc2aae39fu: return "User.m_IsHavable";
    case 0x6d6456f9u: return "User.m_SaveRoomName";
    case 0xdc947dd0u: return "User.m_MtoSReplicateType";
    case 0xddead1f6u: return "User.m_RoomID";
    case 0xe7b8626du: return "User.m_PlayerPID";
    case 0xfd625cc2u: return "User.m_StoCReplicateType";
    case 0x1dd3694eu: return "User.m_PlayerName";
    case 0xede2fde8u: return "User.m_StoMReplicateType";
    case 0xb74e634au: return "Session.m_channel_number";
    case 0x4cb43672u: return "Session.m_id";
    case 0x04f6d032u: return "Session.m_pid";
    case 0x8ed4e6d6u: return "Session.m_account_id";
    case 0x8767dafdu: return "Session.m_account_name";
    case 0x40d6b492u: return "Session.m_gender";
    case 0x91a1c4a6u: return "Session.m_pcRoom";
    case 0x3fac5112u: return "Session.m_iUserType";
    case 0xb1f69770u: return "Session.m_iCompanyAlliance";
    case 0xcb9ab434u: return "Session.m_chatserver_addr";
    case 0x010732e7u: return "Session.m_chatserver_port";
    case 0x74c96032u: return "Session.m_subscriber";
    case 0x645c6c00u: return "Session.m_bluedia";
    case 0x31510076u: return "Session.m_channel_ctx";
    default: return "unknown";
    }
}

static const char *exp_b4_type_key_name(DWORD key)
{
    switch (key) {
    case 0x763c2ceeu: return "bool";
    case 0xf787fbf1u: return "String";
    case 0xdfac8a83u: return "ushort";
    case 0x6a87c9d8u: return "RoomID";
    case 0x70eb6160u: return "PID";
    case 0x286119b4u: return "int";
    case 0x9419399du: return "ChannelContext";
    default: return "unknown";
    }
}

static void exp_log_bb0060_field(DWORD addr, PCONTEXT c)
{
    DWORD node, field_key, type_key, type_a, type_b, mode, raw1c, offset, raw24;
    DWORD reader, S, cursor, B, body, special_al;
    char buf[768];
    if (!c) return;

    node = c->Esi;
    reader = c->Ebx;
    field_key = exp2_read_safe(node + 0x08);
    type_key = exp2_read_safe(node + 0x0c);
    type_a = exp2_read_safe(node + 0x10);
    type_b = exp2_read_safe(node + 0x14);
    mode = exp2_read_safe(node + 0x18);
    raw1c = exp2_read_safe(node + 0x1c);
    offset = exp2_read_safe(node + 0x20);
    raw24 = exp2_read_safe(node + 0x24);
    S = exp2_read_safe(reader + 0x18);
    cursor = exp2_read_safe(S + 8);
    B = exp2_read_safe(S + 4);
    body = exp2_read_safe(B + 0x14);
    special_al = c->Eax & 0xffu;

    wsprintfA(buf,
        "BB0060FIELD addr=0x%08x %s node=0x%08x field_key=0x%08x(%s) type_key=0x%08x(%s) type_a=0x%08x type_b=0x%08x",
        addr, exp_bb0060_field_event_name(addr), node, field_key,
        exp_b4_field_key_name(field_key), type_key, exp_b4_type_key_name(type_key),
        type_a, type_b);
    exp_log(buf);
    wsprintfA(buf,
        "BB0060FIELD mode=%u raw1c=0x%08x offset=0x%08x raw24=0x%08x special_al=%u reader(EBX)=0x%08x cursor=0x%08x body=0x%08x",
        mode, raw1c, offset, raw24, special_al, reader, cursor, body);
    exp_log(buf);
    if (body)
        exp_dump_bytes("BB0060FIELDCUR", body + cursor, 32);
}
#endif

#ifdef EXP_DUMP_SIMPLE_FIELD_READ
static DWORD g_simple_last_reader = 0;
static DWORD g_simple_last_cursor = 0;
static DWORD g_simple_last_body = 0;
static DWORD g_simple_last_type_obj = 0;
static DWORD g_simple_last_slot14 = 0;
static DWORD g_simple_seen = 0;

static DWORD exp_reader_body_checked(DWORD reader, DWORD *cursor_out)
{
    DWORD cursor = 0, body = 0;
    if (reader)
        body = exp_reader_body_from_reader(reader, &cursor);
    if (cursor_out)
        *cursor_out = cursor;
    if (!reader || !body || cursor > 0x100000u)
        return 0;
    return body;
}

static const char *exp_simple_addr_name(DWORD addr)
{
    switch (addr) {
    case VA_SIMPLE_FIELD_READ: return "FUN_00baaa80_entry";
    case VA_SIMPLE_UNPACK_CALL: return "type_vtable_14_call";
    case VA_SIMPLE_UNPACK_RET: return "type_vtable_14_return";
    default: return "simple_field";
    }
}

static void exp_log_simple_field_read(DWORD addr, PCONTEXT c)
{
    DWORD esp, ret, p_type_key, reader, dst, type_key, cursor, body;
    DWORD arg0, arg1, cur0, cur1, body0, body1, vtbl, delta;
    char buf[768];
    if (!c) return;
    ++g_simple_seen;
    if (g_simple_seen > 520) {
        if (g_simple_seen == 521)
            exp_log("SIMPLEFIELD log budget exhausted at 520 events");
        return;
    }

    esp = c->Esp;
    if (addr == VA_SIMPLE_FIELD_READ) {
        ret = exp2_read_safe(esp);
        p_type_key = exp2_read_safe(esp + 4);
        reader = exp2_read_safe(esp + 8);
        dst = exp2_read_safe(esp + 0x0c);
        type_key = exp2_read_safe(p_type_key);
        body = exp_reader_body_checked(reader, &cursor);
        wsprintfA(buf,
            "SIMPLEFIELD #%u %s ret=0x%08x p_type=0x%08x type_key=0x%08x reader=0x%08x dst=0x%08x",
            g_simple_seen, exp_simple_addr_name(addr), ret, p_type_key, type_key, reader, dst);
        exp_log(buf);
        wsprintfA(buf,
            "SIMPLEFIELD_ENTRY cursor=0x%08x body=0x%08x regs EAX=0x%08x EBX=0x%08x ECX=0x%08x EDX=0x%08x",
            cursor, body, c->Eax, c->Ebx, c->Ecx, c->Edx);
        exp_log(buf);
        if (body)
            exp_dump_bytes("SIMPLEFIELD_CUR", body + cursor, 24);
        return;
    }

    if (addr == VA_SIMPLE_UNPACK_CALL) {
        arg0 = exp2_read_safe(esp);
        arg1 = exp2_read_safe(esp + 4);
        body0 = exp_reader_body_checked(arg0, &cur0);
        body1 = exp_reader_body_checked(arg1, &cur1);
        if (body0) {
            reader = arg0;
            body = body0;
            cursor = cur0;
            dst = arg1;
        } else {
            reader = arg1;
            body = body1;
            cursor = cur1;
            dst = arg0;
        }
        vtbl = exp2_read_safe(c->Ecx);
        g_simple_last_reader = reader;
        g_simple_last_cursor = cursor;
        g_simple_last_body = body;
        g_simple_last_type_obj = c->Ecx;
        g_simple_last_slot14 = c->Edx;
        wsprintfA(buf,
            "SIMPLEFIELD #%u %s type_obj(ECX)=0x%08x vtbl=0x%08x slot14(EDX)=0x%08x reader=0x%08x dst=0x%08x",
            g_simple_seen, exp_simple_addr_name(addr), c->Ecx, vtbl, c->Edx, reader, dst);
        exp_log(buf);
        wsprintfA(buf,
            "SIMPLEFIELD_CALL cursor_before=0x%08x body=0x%08x arg0=0x%08x arg1=0x%08x EBX(node)=0x%08x",
            cursor, body, arg0, arg1, c->Ebx);
        exp_log(buf);
        if (body)
            exp_dump_bytes("SIMPLEFIELD_BEFORE", body + cursor, 24);
        return;
    }

    if (addr == VA_SIMPLE_UNPACK_RET) {
        body = g_simple_last_body;
        reader = g_simple_last_reader;
        cursor = 0;
        if (reader)
            body = exp_reader_body_checked(reader, &cursor);
        delta = cursor - g_simple_last_cursor;
        wsprintfA(buf,
            "SIMPLEFIELD #%u %s type_obj=0x%08x slot14=0x%08x reader=0x%08x eax=0x%08x",
            g_simple_seen, exp_simple_addr_name(addr), g_simple_last_type_obj,
            g_simple_last_slot14, reader, c->Eax);
        exp_log(buf);
        wsprintfA(buf,
            "SIMPLEFIELD_RET cursor_before=0x%08x cursor_after=0x%08x delta=0x%08x body=0x%08x",
            g_simple_last_cursor, cursor, delta, body);
        exp_log(buf);
        if (body)
            exp_dump_bytes("SIMPLEFIELD_AFTER", body + cursor, 24);
    }
}
#endif

#ifdef EXP_DUMP_OUTBOUND_DRAIN
static void exp_log_outbound_drain(DWORD addr, PCONTEXT c)
{
    DWORD esp, node, node1, node_target, opcode, target, target_vtbl, target_buf;
    DWORD a1, a2, a3;
    char buf[512];
    if (!c) return;
    esp = c->Esp;
    a1 = exp2_read_safe(esp + 4);
    a2 = exp2_read_safe(esp + 8);
    a3 = exp2_read_safe(esp + 0x0c);
    node = 0;
    if (addr == VA_DRAIN_POP_CALL) {
        node = c->Esi;
    } else if (addr == VA_DRAIN_SEND_ENTRY || addr == VA_SEND_LAYER_HELPER) {
        node = a1;
    } else {
        node = c->Ebx;
    }
    node1 = exp2_read_safe(node + 4);
    node_target = exp2_read_safe(node + 8);
    opcode = exp2_read_safe(node);
    target = node_target;
    if (addr == VA_SEND_LAYER_HELPER)
        target = a1;
    if (addr == VA_DRAIN_TARGET_BOOL_RET || addr == VA_DRAIN_SENDHELPER_RET)
        target = c->Esi;
    target_vtbl = exp2_read_safe(target);
    target_buf = exp2_read_safe(target + 0x10);

    wsprintfA(buf,
        "DRAIN hit addr=0x%08x EAX=0x%08x AL=0x%02x ECX=0x%08x EDX=0x%08x",
        addr, c->Eax, c->Eax & 0xff, c->Ecx, c->Edx);
    exp_log(buf);
    wsprintfA(buf,
        "DRAIN regs EBX=0x%08x ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x",
        c->Ebx, c->Esi, c->Edi, c->Ebp, esp);
    exp_log(buf);
    wsprintfA(buf,
        "DRAIN stack a1=0x%08x a2=0x%08x a3=0x%08x node=0x%08x opcode=0x%08x",
        a1, a2, a3, node, opcode);
    exp_log(buf);
    wsprintfA(buf,
        "DRAIN node+4=0x%08x node+8_target=0x%08x target_vtbl=0x%08x target+10=0x%08x",
        node1, node_target, target_vtbl, target_buf);
    exp_log(buf);
    wsprintfA(buf,
        "DRAIN flags shutdown=0x%08x wake_handle=0x%08x",
        exp2_read_safe(VA_DRAIN_FLAG), exp2_read_safe(VA_DRAIN_WAKE_HANDLE));
    exp_log(buf);

    if (addr == VA_DRAIN_POP_CALL)
        exp_log("DRAIN marker: worker about to CALL 0x009302b0");
    if (addr == VA_DRAIN_SEND_ENTRY)
        exp_log("DRAIN marker: FUN_009302b0 entry");
    if (addr == VA_DRAIN_TARGET_BOOL_RET)
        exp_logf("DRAIN gate1 target.vtable+4 AL=0x%02x", c->Eax & 0xff, 0, 0);
    if (addr == VA_SEND_LAYER_HELPER)
        exp_log("DRAIN marker: FUN_0115f970 entry");
    if (addr == VA_DRAIN_SENDHELPER_RET)
        exp_logf("DRAIN gate2 FUN_0115f970 AL=0x%02x", c->Eax & 0xff, 0, 0);
    if (addr == VA_DRAIN_TEARDOWN)
        exp_log("DRAIN marker: FUN_00bc15a0 teardown entered");
}
#endif

/* Arm the re-arming read-primitive int3s. Called once at COMPWRITE so the hot
 * read primitives are only instrumented during the component window. */
static void exp_arm_trace(void)
{
    DWORD i, op;
    for (i = 0; i < N_TRACE; ++i) {
        BYTE *p = (BYTE*)(DWORD_PTR)g_trace_va[i];
        if (g_trace_armed[i]) continue;
        if (VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &op)) {
            g_trace_orig[i] = *p;
            *p = 0xCC;
            VirtualProtect(p, 1, op, &op);
            FlushInstructionCache(GetCurrentProcess(), p, 1);
            g_trace_armed[i] = TRUE;
            exp_logf("trace armed @ 0x%08x (orig=0x%02x)", g_trace_va[i], g_trace_orig[i], 0);
        }
    }
}

#if EXP_TARGET_PROFILE == 55
static BYTE g_deferred_post_state_orig = 0;
static BOOL g_deferred_post_state_armed = FALSE;

static void exp_arm_deferred_post_state(void)
{
    DWORD op;
    BYTE *p = (BYTE*)(DWORD_PTR)VA_POST_STATE_EVENT_HELPER;
    if (g_deferred_post_state_armed)
        return;
    if (VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &op)) {
        g_deferred_post_state_orig = *p;
        *p = 0xCC;
        VirtualProtect(p, 1, op, &op);
        FlushInstructionCache(GetCurrentProcess(), p, 1);
        g_deferred_post_state_armed = TRUE;
        exp_logf("DEFERRED armed post-state helper @ 0x%08x (orig=0x%02x)",
                 VA_POST_STATE_EVENT_HELPER, g_deferred_post_state_orig, 0);
    } else {
        exp_logf("DEFERRED failed to arm post-state helper @ 0x%08x",
                 VA_POST_STATE_EVENT_HELPER, 0, 0);
    }
}

static BOOL exp_handle_deferred_post_state(DWORD addr, PCONTEXT c)
{
    DWORD op;
    if (!g_deferred_post_state_armed || addr != VA_POST_STATE_EVENT_HELPER)
        return FALSE;
    if (VirtualProtect((void*)(DWORD_PTR)addr, 1, PAGE_EXECUTE_READWRITE, &op)) {
        *(BYTE*)(DWORD_PTR)addr = g_deferred_post_state_orig;
        VirtualProtect((void*)(DWORD_PTR)addr, 1, op, &op);
        FlushInstructionCache(GetCurrentProcess(), (void*)(DWORD_PTR)addr, 1);
    }
    g_deferred_post_state_armed = FALSE;
    exp_log("DEFERRED hit post-state helper after finish_notify");
#ifdef EXP_DUMP_POST_FINISH_UI
    exp_log_post_finish_ui(addr, c);
#endif
    if (c)
        c->Eip = addr;
    return TRUE;
}
#endif

#if EXP_TARGET_PROFILE == 56 || EXP_TARGET_PROFILE == 57 || EXP_TARGET_PROFILE == 58 || EXP_TARGET_PROFILE == 59 || EXP_TARGET_PROFILE == 60 || EXP_TARGET_PROFILE == 61 || EXP_TARGET_PROFILE == 62 || EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 64 || EXP_TARGET_PROFILE == 65 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
#if EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 64 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
#define N_DEFERRED_UI 14
#elif EXP_TARGET_PROFILE == 65
#define N_DEFERRED_UI 12
#elif EXP_TARGET_PROFILE == 62
#define N_DEFERRED_UI 12
#elif EXP_TARGET_PROFILE == 61
#define N_DEFERRED_UI 10
#elif EXP_TARGET_PROFILE == 58 || EXP_TARGET_PROFILE == 59
#define N_DEFERRED_UI 15
#elif EXP_TARGET_PROFILE == 57 || EXP_TARGET_PROFILE == 60
#define N_DEFERRED_UI 8
#else
#define N_DEFERRED_UI 6
#endif
static const DWORD g_deferred_ui_va[N_DEFERRED_UI] = {
#if EXP_TARGET_PROFILE == 61 || EXP_TARGET_PROFILE == 62 || EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 64 || EXP_TARGET_PROFILE == 65 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
    VA_EVENT_EMIT_B,
    VA_FUSER_UI_UPDATE_HELPER,
#endif
    VA_POST_STATE_EVENT_HELPER,
    VA_HE_HANDLER_LIST_CALL,
    VA_HE_HANDLER_LIST_RET,
    VA_HE_HANDLER_TARGET_LOAD,
    VA_HE_HANDLER_CALL,
    VA_HE_HANDLER_RET
#if EXP_TARGET_PROFILE == 57 || EXP_TARGET_PROFILE == 58 || EXP_TARGET_PROFILE == 59 || EXP_TARGET_PROFILE == 60 || EXP_TARGET_PROFILE == 61 || EXP_TARGET_PROFILE == 62 || EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 64 || EXP_TARGET_PROFILE == 65 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
    ,
    VA_UICMD_EXEC_DELEGATE_CALL,
    VA_UICMD_EXEC_DELEGATE_RET
#endif
#if EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 64 || EXP_TARGET_PROFILE == 65 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
    ,
    VA_UICMD_LUAFUNC_EXECUTE,
    VA_UICMD_LUAFUNC_LUAEXECUTE
#endif
#if EXP_TARGET_PROFILE == 58 || EXP_TARGET_PROFILE == 59
    ,
    VA_ITEM_ONLOAD_ENTRY,
    VA_ITEM_ONLOAD_RECEIVER_RET,
    VA_ITEM_ONLOAD_CAP_CALL,
    VA_ITEM_ONLOAD_CAP_RET,
    VA_ITEM_ONLOAD_EMIT_CALL,
    VA_ITEM_ONLOAD_EMIT_RET,
    VA_ITEM_ONLOAD_RETURN
#endif
#if EXP_TARGET_PROFILE == 62 || EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 64 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
    ,
    VA_FUSER_MAP_START,
    VA_FUSER_MAP_COMPLETE
#endif
};
static BYTE g_deferred_ui_orig[N_DEFERRED_UI];
static BOOL g_deferred_ui_armed[N_DEFERRED_UI];
static volatile LONG g_deferred_ui_budget = 80;
static volatile DWORD g_deferred_ui_rearm_va = 0;
static volatile LONG g_deferred_ui_active = 0;

static const char *exp_deferred_ui_name(DWORD addr)
{
    switch (addr) {
    case VA_EVENT_EMIT_B: return "event_emit_0071bdd0";
    case VA_FUSER_UI_UPDATE_HELPER: return "FUser_UI_update_helper_010b0490";
    case VA_POST_STATE_EVENT_HELPER: return "HandleEvent_entry_006f1ec0";
    case VA_HE_HANDLER_LIST_CALL: return "handler_list_call_006f201a";
    case VA_HE_HANDLER_LIST_RET: return "handler_list_ret_006f201f";
    case VA_HE_HANDLER_TARGET_LOAD: return "handler_target_load_006f2050";
    case VA_HE_HANDLER_CALL: return "handler_call_006f2055";
    case VA_HE_HANDLER_RET: return "handler_ret_006f2057";
    case VA_UICMD_EXEC_DELEGATE_CALL: return "uicmd_delegate_call_00754dff";
    case VA_UICMD_EXEC_DELEGATE_RET: return "uicmd_delegate_ret_00754e01";
    case VA_UICMD_LUAFUNC_EXECUTE: return "UICommandLuaFunction::Execute";
    case VA_UICMD_LUAFUNC_LUAEXECUTE: return "UICommandLuaFunction::LuaExecute";
    case VA_ITEM_ONLOAD_ENTRY: return "item_onload_entry_006c6410";
    case VA_ITEM_ONLOAD_RECEIVER_RET: return "item_onload_receiver_ret_006c6462";
    case VA_ITEM_ONLOAD_CAP_CALL: return "item_onload_cap_call_006c646f";
    case VA_ITEM_ONLOAD_CAP_RET: return "item_onload_cap_ret_006c6471";
    case VA_ITEM_ONLOAD_EMIT_CALL: return "item_onload_emit_call_006c6489";
    case VA_ITEM_ONLOAD_EMIT_RET: return "item_onload_emit_ret_006c648e";
    case VA_ITEM_ONLOAD_RETURN: return "item_onload_return_006c64bc";
    case VA_FUSER_MAP_START: return "FUser::onMapStartLoading";
    case VA_FUSER_MAP_COMPLETE: return "FUser::onMapCompleteLoading";
    case VA_RUNLOGLN: return "RunLogLn_00cc9390";
    default: return "deferred_ui";
    }
}

static int exp_deferred_ui_index(DWORD addr)
{
    int i;
    for (i = 0; i < N_DEFERRED_UI; ++i) {
        if (g_deferred_ui_va[i] == addr)
            return i;
    }
    return -1;
}

static void exp_disarm_deferred_ui(void)
{
    int i;
    DWORD op;
    for (i = 0; i < N_DEFERRED_UI; ++i) {
        BYTE *p = (BYTE*)(DWORD_PTR)g_deferred_ui_va[i];
        if (!g_deferred_ui_armed[i])
            continue;
        if (VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &op)) {
            *p = g_deferred_ui_orig[i];
            VirtualProtect(p, 1, op, &op);
            FlushInstructionCache(GetCurrentProcess(), p, 1);
        }
        g_deferred_ui_armed[i] = FALSE;
    }
    g_deferred_ui_active = 0;
    g_deferred_ui_rearm_va = 0;
    exp_log("DEFERRED_UI disarmed handler-loop hooks");
}

static void exp_arm_deferred_ui(void)
{
    int i;
    DWORD op;
    if (g_deferred_ui_active)
        return;
    g_deferred_ui_budget = 80;
    for (i = 0; i < N_DEFERRED_UI; ++i) {
        BYTE *p = (BYTE*)(DWORD_PTR)g_deferred_ui_va[i];
        if (VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &op)) {
            g_deferred_ui_orig[i] = *p;
            *p = 0xCC;
            VirtualProtect(p, 1, op, &op);
            FlushInstructionCache(GetCurrentProcess(), p, 1);
            g_deferred_ui_armed[i] = TRUE;
            exp_logf("DEFERRED_UI armed @ 0x%08x (orig=0x%02x)",
                     g_deferred_ui_va[i], g_deferred_ui_orig[i], 0);
        } else {
            exp_logf("DEFERRED_UI failed arm @ 0x%08x", g_deferred_ui_va[i], 0, 0);
        }
    }
    g_deferred_ui_active = 1;
}

static void exp_log_deferred_ui(DWORD addr, PCONTEXT c)
{
    DWORD vt, slot2c, next, stack_a1, stack_a2, stack_a3, stack_a4;
    DWORD uicmd, uicmd_f10, uicmd_f14, uicmd_f18, delegate_vt, delegate_s0, delegate_s4, delegate_s8;
    DWORD delegate_f0, delegate_f4, delegate_f8, delegate_f0_vt, delegate_f4_vt;
    DWORD glob, glob_a, glob_b, item_count, receiver, receiver_vt, receiver_s14, receiver_s34;
    DWORD name_ptr, owner_arg, emit_arg0, emit_arg1;
    char name_buf[64];
    char runlog_guess[96];
    char logbuf[512];
    if (!c)
        return;
    vt = exp2_read_safe(c->Esi);
    slot2c = exp2_read_safe(vt + 0x2c);
    next = exp2_read_safe(c->Esi + 0x14);
    stack_a1 = exp2_read_safe(c->Esp + 4);
    stack_a2 = exp2_read_safe(c->Esp + 8);
    stack_a3 = exp2_read_safe(c->Esp + 0x0c);
    stack_a4 = exp2_read_safe(c->Esp + 0x10);
    exp_logf("DEFERRED_UI hit 0x%08x %s EAX=0x%08x",
             addr, (DWORD)exp_deferred_ui_name(addr), c->Eax);
    exp_logf("DEFERRED_UI regs EBX=0x%08x ECX=0x%08x EDX=0x%08x",
             c->Ebx, c->Ecx, c->Edx);
    exp_logf("DEFERRED_UI regs ESI=0x%08x EDI=0x%08x EBP=0x%08x",
             c->Esi, c->Edi, c->Ebp);
    exp_logf("DEFERRED_UI handler vt=0x%08x slot2c=0x%08x next=0x%08x",
             vt, slot2c, next);
    exp_logf("DEFERRED_UI stack a1=0x%08x a2=0x%08x a3=0x%08x",
             stack_a1, stack_a2, stack_a3);
    exp_logf("DEFERRED_UI stack a4=0x%08x AL=0x%02x budget=%u",
             stack_a4, c->Eax & 0xff, (DWORD)g_deferred_ui_budget);
    if (addr == VA_UICMD_EXEC_DELEGATE_CALL || addr == VA_UICMD_EXEC_DELEGATE_RET) {
        uicmd = c->Edi;
        uicmd_f10 = exp2_read_safe(uicmd + 0x10);
        uicmd_f14 = exp2_read_safe(uicmd + 0x14);
        uicmd_f18 = exp2_read_safe(uicmd + 0x18);
        delegate_vt = exp2_read_safe(uicmd_f18);
        delegate_s0 = exp2_read_safe(delegate_vt);
        delegate_s4 = exp2_read_safe(delegate_vt + 4);
        delegate_s8 = exp2_read_safe(delegate_vt + 8);
        delegate_f0 = exp2_read_safe(uicmd_f18);
        delegate_f4 = exp2_read_safe(uicmd_f18 + 4);
        delegate_f8 = exp2_read_safe(uicmd_f18 + 8);
        delegate_f0_vt = exp2_read_safe(delegate_f0);
        delegate_f4_vt = exp2_read_safe(delegate_f4);
        exp_logf("DEFERRED_UI uicmd this=0x%08x f10=0x%08x f14=0x%08x",
                 uicmd, uicmd_f10, uicmd_f14);
        exp_logf("DEFERRED_UI uicmd f18=0x%08x dvt=0x%08x slot4=0x%08x",
                 uicmd_f18, delegate_vt, delegate_s4);
        exp_logf("DEFERRED_UI uicmd slot0=0x%08x slot8=0x%08x ecx=0x%08x",
                 delegate_s0, delegate_s8, c->Ecx);
        exp_logf("DEFERRED_UI delegate fields f0=0x%08x f4=0x%08x f8=0x%08x",
                 delegate_f0, delegate_f4, delegate_f8);
        exp_logf("DEFERRED_UI delegate vts f0vt=0x%08x f4vt=0x%08x call_ecx=0x%08x",
                 delegate_f0_vt, delegate_f4_vt, c->Ecx);
    }
    if (addr == VA_UICMD_LUAFUNC_EXECUTE || addr == VA_UICMD_LUAFUNC_LUAEXECUTE) {
        DWORD cmd_this = c->Ecx ? c->Ecx : c->Edi;
        DWORD cmd_vt = exp2_read_safe(cmd_this);
        DWORD cmd_f10 = exp2_read_safe(cmd_this + 0x10);
        DWORD cmd_f14 = exp2_read_safe(cmd_this + 0x14);
        DWORD cmd_f18 = exp2_read_safe(cmd_this + 0x18);
        DWORD cmd_f1c = exp2_read_safe(cmd_this + 0x1c);
        exp_logf("UICMD_LUA addr=0x%08x %s this=0x%08x",
                 addr, (DWORD)exp_deferred_ui_name(addr), cmd_this);
        exp_logf("UICMD_LUA vt=0x%08x f10=0x%08x f14=0x%08x",
                 cmd_vt, cmd_f10, cmd_f14);
        exp_logf("UICMD_LUA f18=0x%08x f1c=0x%08x esp4=0x%08x",
                 cmd_f18, cmd_f1c, stack_a1);
    }
    if (addr == VA_FUSER_MAP_START || addr == VA_FUSER_MAP_COMPLETE) {
        wsprintfA(logbuf,
            "MAP_CB addr=0x%08x %s ECX=0x%08x ESI=0x%08x EDI=0x%08x",
            addr, exp_deferred_ui_name(addr), c->Ecx, c->Esi, c->Edi);
        exp_log(logbuf);
        wsprintfA(logbuf,
            "MAP_CB fuser ECX+74=0x%08x ECX+8c=0x%08x ESI+74=0x%08x ESI+8c=0x%08x",
            exp2_read_safe(c->Ecx + 0x74), exp2_read_safe(c->Ecx + 0x8c),
            exp2_read_safe(c->Esi + 0x74), exp2_read_safe(c->Esi + 0x8c));
        exp_log(logbuf);
        wsprintfA(logbuf,
            "MAP_CB args a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x",
            stack_a1, stack_a2, stack_a3, stack_a4);
        exp_log(logbuf);
    }
    if (addr == VA_RUNLOGLN) {
        exp_read_std_string_safe(c->Esi, runlog_guess, sizeof(runlog_guess));
        wsprintfA(logbuf,
            "RUNLOGLN entry lua=0x%08x ESI=0x%08x EDI=0x%08x EBX=0x%08x",
            stack_a1, c->Esi, c->Edi, c->Ebx);
        exp_log(logbuf);
        wsprintfA(logbuf,
            "RUNLOGLN stack a2=0x%08x a3=0x%08x a4=0x%08x esi_std_guess='%s'",
            stack_a2, stack_a3, stack_a4, runlog_guess);
        exp_log(logbuf);
    }
    if (addr == VA_ITEM_ONLOAD_ENTRY || addr == VA_ITEM_ONLOAD_RECEIVER_RET ||
        addr == VA_ITEM_ONLOAD_CAP_CALL || addr == VA_ITEM_ONLOAD_CAP_RET ||
        addr == VA_ITEM_ONLOAD_EMIT_CALL || addr == VA_ITEM_ONLOAD_EMIT_RET ||
        addr == VA_ITEM_ONLOAD_RETURN) {
        glob = exp2_read_safe(0x01825da8u);
        glob_a = exp2_read_safe(glob + 0x8);
        glob_b = exp2_read_safe(glob + 0xc);
        item_count = (glob_a >= glob_b) ? ((glob_a - glob_b) >> 4) : 0xffffffffu;
        receiver = (addr == VA_ITEM_ONLOAD_RECEIVER_RET) ? c->Eax : c->Ecx;
        receiver_vt = exp2_read_safe(receiver);
        receiver_s14 = exp2_read_safe(receiver_vt + 0x14);
        receiver_s34 = exp2_read_safe(receiver_vt + 0x34);
        exp_logf("ITEM_ONLOAD ctx EDI=0x%08x ECX=0x%08x EAX=0x%08x",
                 c->Edi, c->Ecx, c->Eax);
        exp_logf("ITEM_ONLOAD global=0x%08x begin=0x%08x end=0x%08x",
                 glob, glob_b, glob_a);
        exp_logf("ITEM_ONLOAD count=%u receiver=0x%08x rvt=0x%08x",
                 item_count, receiver, receiver_vt);
        exp_logf("ITEM_ONLOAD rslot14=0x%08x rslot34=0x%08x AL=0x%02x",
                 receiver_s14, receiver_s34, c->Eax & 0xff);
    }
    if (addr == VA_ITEM_ONLOAD_CAP_CALL || addr == VA_ITEM_ONLOAD_CAP_RET) {
        name_ptr = exp2_read_safe(c->Esp);
        owner_arg = exp2_read_safe(c->Esp + 4);
        exp_read_cstr_safe(name_ptr, name_buf, sizeof(name_buf));
        exp_logf("ITEM_ONLOAD cap name_ptr=0x%08x owner_arg=0x%08x slot=0x%08x",
                 name_ptr, owner_arg, c->Eax);
        exp_logf("ITEM_ONLOAD cap name='%s' result_AL=0x%02x",
                 (DWORD)name_buf, c->Eax & 0xff, 0);
    }
    if (addr == VA_ITEM_ONLOAD_EMIT_CALL || addr == VA_ITEM_ONLOAD_EMIT_RET) {
        emit_arg0 = exp2_read_safe(c->Esp);
        emit_arg1 = exp2_read_safe(c->Esp + 4);
        exp_logf("ITEM_ONLOAD emit arg0=0x%08x arg1=0x%08x AL=0x%02x",
                 emit_arg0, emit_arg1, c->Eax & 0xff);
    }
#ifdef EXP_DUMP_POST_FINISH_UI
    if (addr == VA_EVENT_EMIT_B ||
        addr == VA_FUSER_UI_UPDATE_HELPER ||
        addr == VA_POST_STATE_EVENT_HELPER)
        exp_log_post_finish_ui(addr, c);
#endif
}

static BOOL exp_handle_deferred_ui(DWORD addr, PCONTEXT c)
{
    int idx;
    DWORD op;
    if (!g_deferred_ui_active)
        return FALSE;
    idx = exp_deferred_ui_index(addr);
    if (idx < 0 || !g_deferred_ui_armed[idx])
        return FALSE;
    if (VirtualProtect((void*)(DWORD_PTR)addr, 1, PAGE_EXECUTE_READWRITE, &op)) {
        *(BYTE*)(DWORD_PTR)addr = g_deferred_ui_orig[idx];
        VirtualProtect((void*)(DWORD_PTR)addr, 1, op, &op);
        FlushInstructionCache(GetCurrentProcess(), (void*)(DWORD_PTR)addr, 1);
    }
    g_deferred_ui_armed[idx] = FALSE;
    if (g_deferred_ui_budget > 0)
        --g_deferred_ui_budget;
    exp_log_deferred_ui(addr, c);
    if (g_deferred_ui_budget > 0) {
        g_deferred_ui_rearm_va = addr;
        if (c)
            c->EFlags |= 0x100u;
    }
    if (c)
        c->Eip = addr;
    return TRUE;
}
#endif

static LONG CALLBACK exp_veh(PEXCEPTION_POINTERS ei)
{
    DWORD addr, i;
    DWORD code = ei->ExceptionRecord->ExceptionCode;

    /* single-step re-arm: after a traced primitive executed one instruction with
     * the original byte, re-plant the int3 so the NEXT call is caught too. */
    if (code == EXCEPTION_SINGLE_STEP) {
#if EXP_TARGET_PROFILE == 56 || EXP_TARGET_PROFILE == 57 || EXP_TARGET_PROFILE == 58 || EXP_TARGET_PROFILE == 59 || EXP_TARGET_PROFILE == 60 || EXP_TARGET_PROFILE == 61 || EXP_TARGET_PROFILE == 62 || EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 64 || EXP_TARGET_PROFILE == 65 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
        if (g_deferred_ui_rearm_va) {
            DWORD op;
            int idx = exp_deferred_ui_index(g_deferred_ui_rearm_va);
            if (idx >= 0 && VirtualProtect((void*)(DWORD_PTR)g_deferred_ui_rearm_va, 1, PAGE_EXECUTE_READWRITE, &op)) {
                *(BYTE*)(DWORD_PTR)g_deferred_ui_rearm_va = 0xCC;
                VirtualProtect((void*)(DWORD_PTR)g_deferred_ui_rearm_va, 1, op, &op);
                FlushInstructionCache(GetCurrentProcess(), (void*)(DWORD_PTR)g_deferred_ui_rearm_va, 1);
                g_deferred_ui_armed[idx] = TRUE;
            }
            g_deferred_ui_rearm_va = 0;
            ei->ContextRecord->EFlags &= ~0x100u;   /* clear TF */
            return EXCEPTION_CONTINUE_EXECUTION;
        }
#endif
        if (g_rearm_va) {
            DWORD op;
            if (VirtualProtect((void*)(DWORD_PTR)g_rearm_va, 1, PAGE_EXECUTE_READWRITE, &op)) {
                *(BYTE*)(DWORD_PTR)g_rearm_va = 0xCC;
                VirtualProtect((void*)(DWORD_PTR)g_rearm_va, 1, op, &op);
                FlushInstructionCache(GetCurrentProcess(), (void*)(DWORD_PTR)g_rearm_va, 1);
            }
            g_rearm_va = 0;
            ei->ContextRecord->EFlags &= ~0x100u;   /* clear TF */
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (code != EXCEPTION_BREAKPOINT) {
        BOOL self_av = (code == EXCEPTION_ACCESS_VIOLATION) &&
                       exp_is_self_module_addr((DWORD)ei->ExceptionRecord->ExceptionAddress);
#ifdef EXP_LOG_ALL_EXCEPTIONS
        if (!self_av)
            exp_log_any_exception(ei);
#endif
#if EXP_TARGET_PROFILE == 69 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
        if (!self_av && InterlockedCompareExchange(&g_loadlist_exception_logged, 1, 0) == 0) {
            PCONTEXT c = ei->ContextRecord;
            DWORD eip = c ? c->Eip : 0;
            exp_logf("LOADLIST_EXCEPTION code=0x%08x addr=0x%08x eip=0x%08x",
                     code, (DWORD)ei->ExceptionRecord->ExceptionAddress, eip);
            exp_log_addr_region("LOADLIST_EXCEPTION_EIP_REGION", eip);
            if (c) {
                exp_log_addr_region("LOADLIST_EXCEPTION_RET0_REGION",
                                    exp2_read_safe(c->Esp));
                exp_log_addr_region("LOADLIST_EXCEPTION_EBP_RET_REGION",
                                    exp2_read_safe(c->Ebp + 4));
                exp_log_addr_region("LOADLIST_EXCEPTION_ECX_REGION", c->Ecx);
                exp_log_addr_region("LOADLIST_EXCEPTION_EBX_REGION", c->Ebx);
                exp_log_addr_region("LOADLIST_EXCEPTION_INFO1_REGION",
                                    (ei->ExceptionRecord->NumberParameters > 1) ?
                                    (DWORD)ei->ExceptionRecord->ExceptionInformation[1] : 0);
                exp_logf("LOADLIST_EXCEPTION regs EAX=0x%08x EBX=0x%08x ECX=0x%08x",
                         c->Eax, c->Ebx, c->Ecx);
                exp_logf("LOADLIST_EXCEPTION regs EDX=0x%08x ESI=0x%08x EDI=0x%08x",
                         c->Edx, c->Esi, c->Edi);
                exp_logf("LOADLIST_EXCEPTION regs EBP=0x%08x ESP=0x%08x EFLAGS=0x%08x",
                         c->Ebp, c->Esp, c->EFlags);
                exp_dump_bytes("LOADLIST_EXCEPTION_STACK", c->Esp, 64);
            }
        }
#endif
        return EXCEPTION_CONTINUE_SEARCH;
    }
    addr = (DWORD)ei->ExceptionRecord->ExceptionAddress;

    /* re-arming trace targets (read primitives) */
    for (i = 0; i < N_TRACE; ++i) {
        if (g_trace_armed[i] && addr == g_trace_va[i]) {
            DWORD op;
            /* restore original byte so the real instruction runs */
            if (VirtualProtect((void*)(DWORD_PTR)addr, 1, PAGE_EXECUTE_READWRITE, &op)) {
                *(BYTE*)(DWORD_PTR)addr = g_trace_orig[i];
                VirtualProtect((void*)(DWORD_PTR)addr, 1, op, &op);
                FlushInstructionCache(GetCurrentProcess(), (void*)(DWORD_PTR)addr, 1);
            }
            if (g_trace_budget > 0) {
                --g_trace_budget;
                exp_trace_log(addr, ei->ContextRecord);
                if (g_trace_budget > 0) {
                    g_rearm_va = addr;                       /* re-arm after 1 insn */
                    ei->ContextRecord->EFlags |= 0x100u;    /* set TF */
                } else {
                    g_trace_armed[i] = FALSE;               /* budget done: stop */
                }
            } else {
                g_trace_armed[i] = FALSE;
            }
            ei->ContextRecord->Eip = addr;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
#if EXP_TARGET_PROFILE == 55
    if (exp_handle_deferred_post_state(addr, ei->ContextRecord))
        return EXCEPTION_CONTINUE_EXECUTION;
#endif
#if EXP_TARGET_PROFILE == 56 || EXP_TARGET_PROFILE == 57 || EXP_TARGET_PROFILE == 58 || EXP_TARGET_PROFILE == 59 || EXP_TARGET_PROFILE == 60 || EXP_TARGET_PROFILE == 61 || EXP_TARGET_PROFILE == 62 || EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 64 || EXP_TARGET_PROFILE == 65 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
    if (exp_handle_deferred_ui(addr, ei->ContextRecord))
        return EXCEPTION_CONTINUE_EXECUTION;
#endif
    for (i = 0; i < N_TARGETS; ++i) {
        if (g_armed[i] && addr == g_targets[i]) {
            DWORD op;
#ifdef EXP_REARM_TARGETS
            BOOL rearm_target = FALSE;
#endif
            /* one-shot: restore the original byte, rewind EIP, disarm */
            if (VirtualProtect((void*)(DWORD_PTR)g_targets[i], 1, PAGE_EXECUTE_READWRITE, &op)) {
                *(BYTE*)(DWORD_PTR)g_targets[i] = g_orig_byte[i];
                VirtualProtect((void*)(DWORD_PTR)g_targets[i], 1, op, &op);
                FlushInstructionCache(GetCurrentProcess(), (void*)(DWORD_PTR)g_targets[i], 1);
            }
#ifdef EXP_REARM_TARGETS
            if (g_target_budget > 0) {
                --g_target_budget;
                rearm_target = TRUE;
                g_armed[i] = TRUE;
            } else {
                g_armed[i] = FALSE;
            }
#else
            g_armed[i] = FALSE;
#endif
            /* log reach + a couple of stack args for context (esp+4, esp+8) */
            {
                DWORD esp = ei->ContextRecord->Esp;
                DWORD a1 = 0, a2 = 0;
                int suppress_reached = 0;
                __try { a1 = *(DWORD*)(esp + 4); a2 = *(DWORD*)(esp + 8); } __except(1) {}
#ifdef EXP_DUMP_UI_LIFECYCLE
                if (exp_is_ui_lifecycle_addr(g_targets[i]) && g_ui_lifecycle_active == 0)
                    suppress_reached = 1;
#endif
                if (!suppress_reached)
                    exp_logf("REACHED 0x%08x  arg1=0x%08x arg2=0x%08x", g_targets[i], a1, a2);
            }
#ifdef EXP_GUARD_FUSER84_CHARMGR
            if (addr == VA_FUSER_UPDATECHAR)
                exp_guard_fuser84_charmgr(ei->ContextRecord);
#endif
#ifdef EXP_GUARD_LOADLIST_UPDATECOUNT
            if (addr == VA_LOADLIST_UPDATECOUNT_CALL) {
                PCONTEXT c = ei->ContextRecord;
                DWORD session = c ? c->Esi : 0;
                DWORD e0 = session ? exp2_read_safe(session + 0xe0) : 0;
                DWORD pushed_delta = c ? exp2_read_safe(c->Esp) : 0;
                exp_logf("LOADLIST_UPDATECOUNT_CALL session=0x%08x e0=0x%08x delta=0x%08x",
                         session, e0, pushed_delta);
                if (c && e0 == 0) {
                    exp_log("LOADLIST_UPDATECOUNT_GUARD skip null Session+0xe0; EIP=0x00a62d4d ESP+=4");
#ifdef EXP_REARM_TARGETS
                    if (rearm_target) {
                        g_rearm_va = addr;
                        c->EFlags |= 0x100u;
                    }
#endif
                    c->Esp += 4;
                    c->Eip = VA_LOADLIST_UPDATECOUNT_AFTER;
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            }
#endif
            if (addr == VA_KEY6_RECEIVER || addr == VA_KEY6_TARGET_CMP ||
                addr == VA_KEY6_TARGET_CUR_JZ || addr == VA_KEY6_TARGET_NEG1_JZ ||
                addr == VA_KEY6_TARGET_REJECT ||
                addr == VA_KEY6_LOOKUP_NULL_JZ || addr == VA_KEY6_RESOLVED_TEST ||
                addr == VA_KEY6_EXEC_CALLSITE || addr == VA_KEY6_EXECUTOR ||
                addr == VA_KEY6_SKIP_EXEC_JZ || addr == VA_KEY6_CLEANUP_ENTRY ||
                addr == VA_KEY6_CLEANUP_CALL) {
                exp_log_key6_context(addr, ei->ContextRecord);
            }
#ifdef EXP_DUMP_KEY6_EARLY_GUARD
            if (addr == VA_KEY6_TARGET_CMP || addr == VA_KEY6_TARGET_CUR_JZ ||
                addr == VA_KEY6_TARGET_NEG1_JZ || addr == VA_KEY6_TARGET_REJECT ||
                addr == VA_KEY6_CLEANUP_ENTRY || addr == VA_KEY6_CLEANUP_CALL) {
                exp_log_key6_early_guard(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_CALLMODE
            if (addr == VA_KEY6_CALLMODE_RESULT) {
                exp_log_callmode_result(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_EXECUTOR_POSTMODE
            if (addr == VA_RPC_SCOPE_RET || addr == VA_RPC_BA9640_CALL ||
                addr == VA_RPC_BA9640_JZ) {
                exp_log_executor_postmode(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_LUA_CALLABLE_NAME
            if (addr == VA_RPC_BC5860_RET || addr == VA_RPC_BA9640_CALL ||
                addr == VA_RPC_BA9640_JZ) {
                exp_log_lua_callable_name(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_LUA_DISPATCH
            if (addr == VA_RPC_LUA_CALLSITE || addr == VA_RPC_LUA_RET ||
                addr == VA_LUA_DISPATCH_CBDA80 || addr == VA_LUA_PCALL_CBD750 ||
                addr == VA_LUA_PCALL_RET_CBDAD2 || addr == VA_LUA_ERROR_UNKNOWN ||
                addr == VA_LUA_ERROR_MSGBOX) {
                exp_log_lua_dispatch(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_INJECT_CONTROL_OBJ18
            if (addr == VA_KEY6_LOOKUP_NULL_JZ) {
                PCONTEXT c = ei->ContextRecord;
                exp_inject_control_obj18(exp2_read_safe(c->Ebp - 0x14),
                                         exp2_read_safe(c->Ebp - 0x1c));
            }
#endif
#ifdef EXP_INJECT_SESSION_OBJ18
            if (addr == VA_KEY6_LOOKUP_NULL_JZ) {
                PCONTEXT c = ei->ContextRecord;
                exp_inject_session_obj18(exp2_read_safe(c->Ebp - 0x14),
                                         exp2_read_safe(c->Ebp - 0x1c));
            }
#endif
#ifdef EXP_DUMP_METHOD_MAP
            if (addr == VA_KEY6_LOOKUP_NULL_JZ)
                exp_dump_method_map(exp2_read_safe(ei->ContextRecord->Ebp - 0x20));
#endif
#ifdef EXP_DUMP_OBJ18_WRITERS
            if (addr == VA_OBJ18_REG_BD2CD0 || addr == VA_OBJ18_WRITE_BD2D4D ||
                addr == VA_OBJ18_REG_BD3AE0 || addr == VA_OBJ18_WRITE_BD3B15 ||
                addr == VA_OBJ18_REG_BD9C50 || addr == VA_OBJ18_WRITE_BD9D5F ||
                addr == VA_TCLIENT_POP_C5A880) {
                exp_log_obj18_writer(addr, ei->ContextRecord);
            }
#ifdef EXP_DUMP_WRITER_CENSUS
            if (addr == VA_OBJ18_REG_BD2CD0 || addr == VA_OBJ18_WRITE_BD2D4D ||
                addr == VA_OBJ18_REG_BD3AE0 || addr == VA_OBJ18_WRITE_BD3B15 ||
                addr == VA_OBJ18_REG_BD9C50 || addr == VA_OBJ18_WRITE_BD9D5F ||
                addr == VA_TAG1_COMP_REGISTER || addr == VA_TAG1_COMP_INSERT_CALL) {
                exp_log_writer_census(addr, ei->ContextRecord);
            }
#endif
#endif
#ifdef EXP_DUMP_CONNECTOR_HANDSHAKE
            if (addr == VA_CONN_CONNECT_BASE || addr == VA_CONN_CONNECT_IMPL ||
                addr == VA_CONN_OPEN_IMPL ||
                addr == VA_CONN_CONNECT_EMIT || addr == VA_CONN_CHALLENGE_BASE ||
                addr == VA_CONN_RESPONSE_BASE || addr == VA_CONN_RESPONSE_IMPL ||
                addr == VA_CONN_ARG_READER || addr == VA_EVAL_MASTERHASH_MARK ||
                addr == VA_PM_CREATEOBJECT) {
                exp_log_connector_handshake(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_LOGINRESULT_GATES
            if (addr == VA_RPC_INVALID_FORMAT)
                exp_logf("RPC_REJECT Invalid Network Call Format @ 0x%08x", addr, 0, 0);
            if (addr == VA_RPC_ARGC_WRONG)
                exp_logf("RPC_REJECT CallNetFunction num args wrong @ 0x%08x", addr, 0, 0);
            if (addr == VA_ADD_COMPONENT_BCF510) {
                PCONTEXT c = ei->ContextRecord;
                DWORD esp = c->Esp, a1 = 0, a2 = 0, a3 = 0;
                __try { a1 = *(DWORD*)(esp + 4); a2 = *(DWORD*)(esp + 8); a3 = *(DWORD*)(esp + 0xc); } __except(1) {}
                exp_logf("ADDCOMP entry ECX=0x%08x ESI=0x%08x EAX=0x%08x", c->Ecx, c->Esi, c->Eax);
                exp_logf("ADDCOMP stack a1=0x%08x a2=0x%08x a3=0x%08x", a1, a2, a3);
            }
            if (addr == VA_LOGIN_DOWNCALL || addr == VA_LOGINMASTER_DOWNCALL ||
                addr == VA_LOGINRESULT_DOWNCALL || addr == VA_LOGINRESULT_A81020) {
                PCONTEXT c = ei->ContextRecord;
                DWORD esp = c->Esp, a1 = 0, a2 = 0, a3 = 0, a4 = 0;
                __try {
                    a1 = *(DWORD*)(esp + 4);
                    a2 = *(DWORD*)(esp + 8);
                    a3 = *(DWORD*)(esp + 0xc);
                    a4 = *(DWORD*)(esp + 0x10);
                } __except(1) {}
                exp_logf("LOGIN_RPC enter addr=0x%08x ECX=0x%08x ESI=0x%08x", addr, c->Ecx, c->Esi);
                exp_logf("LOGIN_RPC stack a1=0x%08x a2=0x%08x a3=0x%08x", a1, a2, a3);
                exp_logf("LOGIN_RPC stack a4=0x%08x ESP=0x%08x EBP=0x%08x", a4, esp, c->Ebp);
            }
            if (addr == VA_LOGIN_NO_WORLD)
                exp_logf("LOGIN_THROW_SITE No World @ 0x%08x", addr, 0, 0);
            if (addr == VA_LOGIN_NO_OBJECT)
                exp_logf("LOGIN_THROW_SITE No Object for OID @ 0x%08x", addr, 0, 0);
            if (addr == VA_LOGIN_NO_CLIENTCOMP)
                exp_logf("LOGIN_THROW_SITE No ClientComputer @ 0x%08x", addr, 0, 0);
            if (addr == VA_LOGIN_NO_COM_PLAYER)
                exp_logf("LOGIN_THROW_SITE No COM: Player @ 0x%08x", addr, 0, 0);
            if (addr == VA_LOGIN_NO_COM_CSTATUS)
                exp_logf("LOGIN_THROW_SITE No COM: CStatusPlayer @ 0x%08x", addr, 0, 0);
            if (addr == VA_LOGIN_NO_COM_USER)
                exp_logf("LOGIN_THROW_SITE No COM: User @ 0x%08x", addr, 0, 0);
#endif
#ifdef EXP_DUMP_GOLUA_ADDCOMP
            if (addr == VA_GOLUA_ADDCOMP || addr == VA_GOLUA_ADDCOMP_BD2CD0) {
                PCONTEXT c = ei->ContextRecord;
                DWORD esp = c->Esp, ret = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0;
                __try {
                    ret = *(DWORD*)(esp);
                    a1 = *(DWORD*)(esp + 4);
                    a2 = *(DWORD*)(esp + 8);
                    a3 = *(DWORD*)(esp + 0xc);
                    a4 = *(DWORD*)(esp + 0x10);
                } __except(1) {}
                exp_logf("GOLUA_ADDCOMP addr=0x%08x ret=0x%08x ECX=0x%08x",
                         addr, ret, c->Ecx);
                exp_logf("GOLUA_ADDCOMP regs EAX=0x%08x EBX=0x%08x EDX=0x%08x",
                         c->Eax, c->Ebx, c->Edx);
                exp_logf("GOLUA_ADDCOMP regs ESI=0x%08x EDI=0x%08x EBP=0x%08x",
                         c->Esi, c->Edi, c->Ebp);
                exp_logf("GOLUA_ADDCOMP stack a1=0x%08x a2=0x%08x a3=0x%08x",
                         a1, a2, a3);
                exp_logf("GOLUA_ADDCOMP stack a4=0x%08x ESP=0x%08x", a4, esp, 0);
            }
#endif
#ifdef EXP_DUMP_GOLUA_BUILDERS
            if (addr == VA_GOLUA_CREATEOBJECT || addr == VA_GOLUA_ADDCOMP ||
                addr == VA_GOLUA_ADDCOMP_BD2CD0 || addr == VA_GOLUA_REGISTOBJECT) {
                PCONTEXT c = ei->ContextRecord;
                DWORD esp = c->Esp, ret = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0;
                char buf[512];
                __try {
                    ret = *(DWORD*)(esp);
                    a1 = *(DWORD*)(esp + 4);
                    a2 = *(DWORD*)(esp + 8);
                    a3 = *(DWORD*)(esp + 0xc);
                    a4 = *(DWORD*)(esp + 0x10);
                } __except(1) {}
                wsprintfA(buf,
                    "GOLUA_BUILDER addr=0x%08x ret=0x%08x ECX=0x%08x EAX=0x%08x EBX=0x%08x EDX=0x%08x",
                    addr, ret, c->Ecx, c->Eax, c->Ebx, c->Edx);
                exp_log(buf);
                wsprintfA(buf,
                    "GOLUA_BUILDER regs ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x",
                    c->Esi, c->Edi, c->Ebp, esp);
                exp_log(buf);
                wsprintfA(buf,
                    "GOLUA_BUILDER stack a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x",
                    a1, a2, a3, a4);
                exp_log(buf);
            }
#endif
#ifdef EXP_DUMP_SIGNINCLIENTRESULT
            if (addr == VA_SIGNINCLIENT_DIRECT || addr == VA_SIGNINCLIENT_FORWARD ||
                addr == VA_SIGNINCLIENT_BASE || addr == VA_SIGNINCLIENT_UP ||
                addr == VA_SIGNINCLIENT_JUMPUP || addr == VA_SIGNINCLIENT_DOWN ||
                addr == VA_SIGNINCLIENT_GHOST || addr == VA_SIGNINCLIENT_JUMPGHOST ||
                addr == VA_SIGNINCLIENT_GENERATED) {
                PCONTEXT c = ei->ContextRecord;
                DWORD esp = c->Esp, ret = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0, a6 = 0;
                char buf[512];
                __try {
                    ret = *(DWORD*)(esp);
                    a1 = *(DWORD*)(esp + 4);
                    a2 = *(DWORD*)(esp + 8);
                    a3 = *(DWORD*)(esp + 0xc);
                    a4 = *(DWORD*)(esp + 0x10);
                    a5 = *(DWORD*)(esp + 0x14);
                    a6 = *(DWORD*)(esp + 0x18);
                } __except(1) {}
                wsprintfA(buf,
                    "SIGNINCLIENT_HIT addr=0x%08x ret=0x%08x ECX=0x%08x EAX=0x%08x EBX=0x%08x EDX=0x%08x",
                    addr, ret, c->Ecx, c->Eax, c->Ebx, c->Edx);
                exp_log(buf);
                wsprintfA(buf,
                    "SIGNINCLIENT regs ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x",
                    c->Esi, c->Edi, c->Ebp, esp);
                exp_log(buf);
                wsprintfA(buf,
                    "SIGNINCLIENT stack a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x",
                    a1, a2, a3, a4);
                exp_log(buf);
                wsprintfA(buf,
                    "SIGNINCLIENT stack a5=0x%08x a6=0x%08x",
                    a5, a6);
                exp_log(buf);
            }
#endif
#ifdef EXP_DUMP_LOGINCLIENTRESULT
            if (addr == VA_LOGINCLIENT_LOGIC || addr == VA_LOGINCLIENT_HANDLER_A ||
                addr == VA_LOGINCLIENT_HANDLER_B) {
                PCONTEXT c = ei->ContextRecord;
                DWORD esp = c->Esp, ret = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0, a6 = 0;
                char buf[512];
                __try {
                    ret = *(DWORD*)(esp);
                    a1 = *(DWORD*)(esp + 4);
                    a2 = *(DWORD*)(esp + 8);
                    a3 = *(DWORD*)(esp + 0xc);
                    a4 = *(DWORD*)(esp + 0x10);
                    a5 = *(DWORD*)(esp + 0x14);
                    a6 = *(DWORD*)(esp + 0x18);
                } __except(1) {}
                wsprintfA(buf,
                    "LOGINCLIENT_HIT addr=0x%08x ret=0x%08x ECX=0x%08x EAX=0x%08x EBX=0x%08x EDX=0x%08x",
                    addr, ret, c->Ecx, c->Eax, c->Ebx, c->Edx);
                exp_log(buf);
                wsprintfA(buf,
                    "LOGINCLIENT regs ESI=0x%08x EDI=0x%08x EBP=0x%08x ESP=0x%08x",
                    c->Esi, c->Edi, c->Ebp, esp);
                exp_log(buf);
                wsprintfA(buf,
                    "LOGINCLIENT stack a1=0x%08x a2=0x%08x a3=0x%08x a4=0x%08x",
                    a1, a2, a3, a4);
                exp_log(buf);
                wsprintfA(buf,
                    "LOGINCLIENT stack a5=0x%08x a6=0x%08x",
                    a5, a6);
                exp_log(buf);
            }
#endif
#ifdef EXP_DUMP_ONLOADPLAYERS
            if (addr == VA_ONLOADPLAYERS_WRAP || addr == VA_ONLOADPLAYERS_INNER) {
                PCONTEXT c = ei->ContextRecord;
                DWORD esp = c->Esp, a1 = 0, a2 = 0, a3 = 0, a4 = 0;
                __try {
                    a1 = *(DWORD*)(esp + 4);
                    a2 = *(DWORD*)(esp + 8);
                    a3 = *(DWORD*)(esp + 0xc);
                    a4 = *(DWORD*)(esp + 0x10);
                } __except(1) {}
                exp_logf("ONLOAD_RPC enter addr=0x%08x ECX=0x%08x ESI=0x%08x", addr, c->Ecx, c->Esi);
                exp_logf("ONLOAD_RPC stack a1=0x%08x a2=0x%08x a3=0x%08x", a1, a2, a3);
                exp_logf("ONLOAD_RPC stack a4=0x%08x ESP=0x%08x EBP=0x%08x", a4, esp, c->Ebp);
                if (addr == VA_ONLOADPLAYERS_WRAP)
                    exp_log("ONLOAD marker: wrapper reached before FUN_00a7fd80");
                if (addr == VA_ONLOADPLAYERS_INNER)
                    exp_log("ONLOAD marker: inner logic reached");
            }
#endif
#ifdef EXP_DUMP_ONLOAD_TRANSITION
            if (exp_is_onload_transition_addr(addr))
                exp_log_onload_transition(addr, ei->ContextRecord);
#endif
#ifdef EXP_DUMP_ONLOADPLAYERDATA
            if (addr == VA_ONLOADPLAYERDATA_RECV || addr == VA_ONLOADPLAYERDATA_INNER ||
                addr == VA_ONLOADPLAYERDATA_EMIT) {
                PCONTEXT c = ei->ContextRecord;
                DWORD esp = c->Esp;
                DWORD ret = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0, a6 = 0, a7 = 0;
                __try {
                    ret = *(DWORD*)(esp);
                    a1 = *(DWORD*)(esp + 4);
                    a2 = *(DWORD*)(esp + 8);
                    a3 = *(DWORD*)(esp + 0x0c);
                    a4 = *(DWORD*)(esp + 0x10);
                    a5 = *(DWORD*)(esp + 0x14);
                    a6 = *(DWORD*)(esp + 0x18);
                    a7 = *(DWORD*)(esp + 0x1c);
                } __except(1) {}
                exp_logf("ONLOADDATA enter addr=0x%08x ECX=0x%08x ESI=0x%08x",
                         addr, c->Ecx, c->Esi);
                exp_logf("ONLOADDATA regs EAX=0x%08x EBX=0x%08x EDX=0x%08x",
                         c->Eax, c->Ebx, c->Edx);
                exp_logf("ONLOADDATA stack ret=0x%08x a1=0x%08x a2=0x%08x",
                         ret, a1, a2);
                exp_logf("ONLOADDATA stack a3=0x%08x a4=0x%08x a5=0x%08x",
                         a3, a4, a5);
                exp_logf("ONLOADDATA stack a6=0x%08x a7=0x%08x ESP=0x%08x",
                         a6, a7, esp);
                if (addr == VA_ONLOADPLAYERDATA_RECV)
                    exp_log("ONLOADDATA marker: generated receiver reached");
                if (addr == VA_ONLOADPLAYERDATA_INNER)
                    exp_log("ONLOADDATA marker: Session::OnLoadPlayerData reached");
                if (addr == VA_ONLOADPLAYERDATA_EMIT)
                    exp_log("ONLOADDATA marker: logInServerResult emitter reached");
            }
#endif
#ifdef EXP_DUMP_ONLOAD_EMIT_BRIDGE
            if (addr == VA_ONLOAD_BRIDGE_A87B90 ||
                addr == VA_ONLOAD_BRIDGE_NO_OWNER_JZ ||
                addr == VA_ONLOAD_BRIDGE_NO_BUILDER_JZ ||
                addr == VA_ONLOAD_BRIDGE_ENQUEUE_CALL ||
                addr == VA_POST_WRITER_ENQUEUE_A ||
                addr == VA_POST_WRITER_QUEUE) {
                PCONTEXT c = ei->ContextRecord;
                DWORD esp = c->Esp;
                DWORD ret = 0, a1 = 0, a2 = 0, a3 = 0, a4 = 0, a5 = 0, a6 = 0, a7 = 0;
                DWORD owner = 0, owner14 = 0, owner0c = 0, builder = 0;
                __try {
                    ret = *(DWORD*)(esp);
                    a1 = *(DWORD*)(esp + 4);
                    a2 = *(DWORD*)(esp + 8);
                    a3 = *(DWORD*)(esp + 0x0c);
                    a4 = *(DWORD*)(esp + 0x10);
                    a5 = *(DWORD*)(esp + 0x14);
                    a6 = *(DWORD*)(esp + 0x18);
                    a7 = *(DWORD*)(esp + 0x1c);
                    owner = a1;
                    owner14 = owner ? *(DWORD*)(owner + 0x14) : 0;
                    owner0c = owner ? *(DWORD*)(owner + 0x0c) : 0;
                    builder = *(DWORD*)(esp + 0x14);
                } __except(1) {}
                exp_logf("ONLOADBRIDGE hit addr=0x%08x EAX=0x%08x ECX=0x%08x",
                         addr, c->Eax, c->Ecx);
                exp_logf("ONLOADBRIDGE regs EBX=0x%08x EDX=0x%08x ESI=0x%08x",
                         c->Ebx, c->Edx, c->Esi);
                exp_logf("ONLOADBRIDGE regs EDI=0x%08x EBP=0x%08x ESP=0x%08x",
                         c->Edi, c->Ebp, esp);
                exp_logf("ONLOADBRIDGE stack ret=0x%08x a1=0x%08x a2=0x%08x",
                         ret, a1, a2);
                exp_logf("ONLOADBRIDGE stack a3=0x%08x a4=0x%08x a5=0x%08x",
                         a3, a4, a5);
                exp_logf("ONLOADBRIDGE stack a6=0x%08x a7=0x%08x owner+0c=0x%08x",
                         a6, a7, owner0c);
                exp_logf("ONLOADBRIDGE owner=0x%08x owner+14=0x%08x builder_slot=0x%08x",
                         owner, owner14, builder);
                if (addr == VA_ONLOAD_BRIDGE_A87B90)
                    exp_log("ONLOADBRIDGE marker: writer entry reached");
                if (addr == VA_ONLOAD_BRIDGE_NO_OWNER_JZ)
                    exp_logf("ONLOADBRIDGE early-owner-test EAX(owner+14)=0x%08x", c->Eax, 0, 0);
                if (addr == VA_ONLOAD_BRIDGE_NO_BUILDER_JZ)
                    exp_logf("ONLOADBRIDGE early-builder-test stack+14=0x%08x", builder, 0, 0);
                if (addr == VA_ONLOAD_BRIDGE_ENQUEUE_CALL)
                    exp_log("ONLOADBRIDGE marker: about to CALL 0x00bbfe80");
                if (addr == VA_POST_WRITER_ENQUEUE_A)
                    exp_log("ONLOADBRIDGE marker: entered 0x00bbfe80 enqueue helper");
                if (addr == VA_POST_WRITER_QUEUE)
                    exp_log("ONLOADBRIDGE marker: entered 0x00bc28c0 queue push");
            }
#endif
#ifdef EXP_DUMP_OUTBOUND_DRAIN
            if (addr == VA_DRAIN_POP_CALL || addr == VA_DRAIN_SEND_ENTRY ||
                addr == VA_DRAIN_TARGET_BOOL_RET || addr == VA_SEND_LAYER_HELPER ||
                addr == VA_DRAIN_SENDHELPER_RET || addr == VA_DRAIN_TEARDOWN) {
                exp_log_outbound_drain(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_TRANSITION_TEARDOWN
            if (addr == VA_FCLIENTAPP_FINALIZE ||
                addr == VA_DRAIN_TEARDOWN ||
                addr == VA_NET_REPORT_HELPER ||
                addr == VA_SESSION_SHUTDOWN ||
                addr == VA_SOCKET_CLOSE_HELPER) {
                exp_log_transition_teardown(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_SESSION_E0_LIFECYCLE
            if (addr == VA_SESSION_MASTEROPEN ||
                addr == VA_SESSION_MASTERREOPEN ||
                addr == VA_SESSION_E0_STORE_HELPER) {
                exp_log_session_e0_lifecycle(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_PROACTOR_EXCEPTION
            if (addr == VA_PROACTOR_DISPATCH_CALL ||
                addr == VA_PROACTOR_EXCEPTION_BUILD ||
                addr == VA_PROACTOR_EXCEPTION_TEXT_RET ||
                addr == VA_PROACTOR_REPORT_CALL) {
                exp_log_proactor_exception(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_B4_DESCRIPTORS
            if (addr == VA_FUSER_CONNECT || addr == VA_ONLOADPLAYERDATA_RECV ||
                addr == VA_BB0060_ENTRY) {
                if (addr == VA_FUSER_CONNECT)
                    exp_dump_b4_descriptors("FUSER_CONNECT");
                else if (addr == VA_BB0060_ENTRY)
                    exp_dump_b4_descriptors("BB0060_ENTRY");
                else
                    exp_dump_b4_descriptors("ONLOADPLAYERDATA_RECV");
            }
#endif
#ifdef EXP_DUMP_FUSER_STATE
            if (addr == VA_FUSER_CONNECT || addr == VA_FUSER_STATE1_WRITE_8C ||
                addr == VA_FUSER_STATE1_WRITE_74 || addr == VA_FUSER_ONCONNECTOR ||
                addr == VA_FUSER_ONSIGNIN || addr == VA_FUSER_SIGNIN_74_CMP ||
                addr == VA_FUSER_SIGNIN_SCS_OK || addr == VA_FUSER_SIGNIN_EVENT ||
                addr == VA_FUSER_SIGNIN_NOTIFY || addr == VA_FUSER_ONLOGIN ||
                addr == VA_FUSER_LOGIN_SCS_OK || addr == VA_FUSER_LOGIN_EVENT ||
                addr == VA_FUSER_ONSESSION || addr == VA_FUSER_ONSESSION_RESOLVE ||
                addr == VA_FUSER_ONSESSION_STATE_CMP || addr == VA_FUSER_ONSESSION_74_CMP ||
                addr == VA_FUSER_ONSESSION_SCS_OK || addr == VA_FUSER_ONSESSION_EVENT ||
                addr == VA_FUSER_ONSESSION_NOTIFY || addr == VA_FUSER_ONSESSION_STATE5_WRITE ||
                addr == VA_FUSER_SETUICONTROL || addr == VA_FUSER_SETUICONTROL_WRITE_74 ||
                addr == VA_FUSER_FINISH || addr == VA_FUSER_FINISH_RESULT_CMP ||
                addr == VA_FUSER_FINISH_74_CMP || addr == VA_FUSER_FINISH_SCS_OK ||
                addr == VA_FUSER_FINISH_EVENT || addr == VA_FUSER_FINISH_NOTIFY ||
                addr == VA_FUSER_FINISH_AFTER_EVENT) {
                exp_log_fuser_state(addr, ei->ContextRecord);
            }
#ifdef EXP_DUMP_UI_LIFECYCLE
#if EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
            if (addr == VA_ONLOADPLAYERS_INNER ||
                addr == VA_ONLOAD_AFTER_LOADLIST ||
                addr == VA_SERVER_FINISH_SEND ||
                addr == VA_FUSER_FINISH_SCS_OK ||
                addr == VA_FUSER_FINISH_NOTIFY) {
#else
            if (addr == VA_FUSER_SETUICONTROL ||
                addr == VA_FUSER_FINISH_SCS_OK ||
                addr == VA_FUSER_FINISH_NOTIFY) {
#endif
                if (InterlockedCompareExchange(&g_ui_lifecycle_active, 1, 0) == 0)
                    exp_logf("UI_LIFECYCLE armed by addr=0x%08x", addr, 0, 0);
            }
#endif
#if EXP_TARGET_PROFILE == 55
            if (addr == VA_FUSER_FINISH_NOTIFY)
                exp_arm_deferred_post_state();
#endif
#if EXP_TARGET_PROFILE == 56 || EXP_TARGET_PROFILE == 57 || EXP_TARGET_PROFILE == 58 || EXP_TARGET_PROFILE == 59 || EXP_TARGET_PROFILE == 60
            if (addr == VA_FUSER_FINISH_NOTIFY)
                exp_arm_deferred_ui();
            if (addr == VA_FUSER_FINISH_AFTER_EVENT)
                exp_disarm_deferred_ui();
#endif
#if EXP_TARGET_PROFILE == 61 || EXP_TARGET_PROFILE == 62 || EXP_TARGET_PROFILE == 63 || EXP_TARGET_PROFILE == 64 || EXP_TARGET_PROFILE == 65 || EXP_TARGET_PROFILE == 66 || EXP_TARGET_PROFILE == 67 || EXP_TARGET_PROFILE == 68 || EXP_TARGET_PROFILE == 69
            if (addr == VA_FUSER_FINISH_SCS_OK)
                exp_arm_deferred_ui();
#if EXP_TARGET_PROFILE == 61 || EXP_TARGET_PROFILE == 65
            if (addr == VA_FUSER_FINISH_AFTER_EVENT)
                exp_disarm_deferred_ui();
#endif
#endif
#ifdef EXP_FORCE_UPDATECHAR_AT_NOTIFY
            if (addr == VA_FUSER_FINISH_NOTIFY &&
                InterlockedCompareExchange(&g_force_update_once, 1, 0) == 0)
                exp_force_update_character_after_finish(ei->ContextRecord);
#endif
#ifdef EXP_FORCE_UPDATECHAR_AFTER_FINISH
            if (addr == VA_FUSER_FINISH_AFTER_EVENT &&
                InterlockedCompareExchange(&g_force_update_once, 1, 0) == 0)
                exp_force_update_character_after_finish(ei->ContextRecord);
#endif
#endif
#ifdef EXP_DUMP_POST_FINISH_UI
            if (addr == VA_EVENT_EMIT_B ||
                addr == VA_FUSER_UI_UPDATE_HELPER ||
                addr == VA_POST_STATE_EVENT_HELPER ||
                addr == VA_FUSER_UPDATECHAR ||
                addr == VA_CHARMGR_GETALL ||
                addr == VA_CHARMGR_LOADLIST ||
                addr == VA_ONLOAD_AFTER_LOADLIST ||
                addr == VA_SERVER_FINISH_SEND ||
                addr == VA_UPDATE_AFTER_GETALL ||
                addr == VA_UPDATE_ADDCHAR ||
                addr == VA_UPDATE_HASNOCHAR ||
                addr == VA_UPDATE_SELECTCHAR) {
                exp_log_post_finish_ui(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_UI_LIFECYCLE
            if (exp_is_ui_lifecycle_addr(addr))
                exp_log_ui_lifecycle(addr, ei->ContextRecord);
#endif
#if EXP_TARGET_PROFILE == 69 || EXP_TARGET_PROFILE == 75 || EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
            if (addr == VA_NEWOBJ_PID_STORED) {
                exp_log_newobject_identity69(addr, ei->ContextRecord);
            }
            if (addr == VA_OBJPTR_MAT_ENTRY ||
                addr == VA_OBJPTR_BB2580_CALL ||
                addr == VA_OBJPTR_BB2580_RET ||
                addr == VA_BB2580_BC5860_CALL ||
                addr == VA_BB2580_BC5860_RET ||
                addr == VA_BB2580_BB5F40_CALL ||
                addr == VA_BB2580_BB5F40_RET) {
                exp_log_objptr_resolver69(addr, ei->ContextRecord);
            }
            if (addr == VA_OBJLIST_A_PARAM_TEST ||
                addr == VA_ONLOAD_ARG3_CALL ||
                addr == VA_OBJLIST_A_WRAPPER ||
                addr == VA_OBJLIST_A_OUTER_TAG_RET ||
                addr == VA_OBJLIST_A_COUNT_RET ||
                addr == VA_OBJLIST_A_TABLE_RET2 ||
                addr == VA_OBJLIST_A_INDEX_RET ||
                addr == VA_OBJLIST_A_NESTED_RET ||
                addr == VA_OBJLIST_A_NODE_ALLOC_RET ||
                addr == VA_OBJLIST_A_NODE_LINK_RET ||
                addr == VA_OBJLIST_B_WRAPPER ||
                addr == VA_OBJLIST_B_ENTRY ||
                addr == VA_OBJLIST_B_PARAM_TEST) {
                exp_log_objlist_a_reader(addr, ei->ContextRecord);
            }
            if (addr == VA_VARIANT_TAG0B_ENTRY ||
                addr == VA_VARIANT_TAG0B_DESC_RET ||
                addr == VA_VARIANT_TAG0B_OBJ_RET ||
                addr == VA_VARIANT_TAG0B_BA9200_RET ||
                addr == VA_VARIANT_TAG0B_NIL_OBJ) {
                exp_log_tag0b_materializer(addr, ei->ContextRecord);
            }
            if (exp_is_charmgr_lifetime_addr(addr)) {
                exp_log_charmgr_lifetime(addr, ei->ContextRecord);
            }
            if (addr == VA_CHARMGR_LOADLIST ||
                addr == VA_LOADLIST_EMPTY_CHECK ||
                addr == VA_LOADLIST_EMPTY_BRANCH ||
                addr == VA_LOADLIST_EMPTY_RELEASE_RET ||
                addr == VA_LOADLIST_EMPTY_RELEASE_DTOR ||
                addr == VA_LOADLIST_EMPTY_PARAM3_SCAN ||
                addr == VA_LOADLIST_OLD_BEGIN ||
                addr == VA_LOADLIST_OLD_MARK_ROW ||
                addr == VA_LOADLIST_OLD_HELPER ||
                addr == VA_LOADLIST_OLD_FETCH_RET ||
                addr == VA_LOADLIST_NODE_PAYLOAD ||
                addr == VA_LOADLIST_REF_RET ||
                addr == VA_LOADLIST_DUP_SCAN ||
                addr == VA_LOADLIST_DUP_RESULT ||
                addr == VA_LOADLIST_PLAYER_DESC ||
                addr == VA_LOADLIST_PLAYER_RESOLVE ||
                addr == VA_LOADLIST_PLAYER_RET ||
                addr == VA_LOADLIST_PLAYER_TEST ||
                addr == VA_LOADLIST_PLAYER_FLAGS ||
                addr == VA_LOADLIST_SCORE_RET ||
                addr == VA_LOADLIST_ACTIVE_PATH ||
                addr == VA_LOADLIST_OBJ_RESOLVE2 ||
                addr == VA_LOADLIST_OBJ_RET2 ||
                addr == VA_LOADLIST_TPLAYER_CALL ||
                addr == VA_LOADLIST_ROW_ADD_CALL ||
                addr == VA_LOADLIST_ROW_ADD_RET ||
                addr == VA_LOADLIST_LOOP_NEXT ||
                addr == VA_LOADLIST_OLD_ROW_VCALL ||
                addr == VA_LOADLIST_OLD_AFTER_VCALL ||
                addr == VA_LOADLIST_EMPTY_CLEANUP_RET ||
                addr == VA_LOADLIST_UPDATECOUNT_CALL ||
                addr == VA_LOADLIST_BAD_HELPER_ENTRY ||
                addr == VA_LOADLIST_BAD_HELPER_DEREF ||
                addr == VA_ONLOAD_AFTER_LOADLIST ||
                addr == VA_SERVER_FINISH_SEND) {
                if (addr == VA_LOADLIST_BAD_HELPER_ENTRY ||
                    addr == VA_LOADLIST_BAD_HELPER_DEREF)
                    exp_log_loadlist_bad_helper(addr, ei->ContextRecord);
                else if (addr == VA_CHARMGR_LOADLIST ||
                    addr == VA_ONLOAD_AFTER_LOADLIST ||
                    addr == VA_SERVER_FINISH_SEND)
                    exp_log_loadlist_probe(addr, ei->ContextRecord);
                else
                    exp_log_loadlist_step(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_PRECONNECT_STAGE
            if (addr == VA_SETCTRL_HANDLER || addr == VA_SETCTRL_SIGNAL_HELPER ||
                addr == VA_ONSIG_SETCONTROL || addr == VA_FUSER_ONCONNECTOR ||
                addr == VA_CONNECT_EMIT_A8E500 || addr == VA_CONNECT_DOWNCALL_A8E5B0 ||
                addr == VA_CONNECT_WRITER_63D2E0 ||
                addr == VA_CHANNEL_OPEN || addr == VA_CHANNEL_CHANGEFINISH ||
                addr == VA_KEY6_EXEC_CALLSITE || addr == VA_FUSER_CONNECT ||
                addr == VA_FCLIENTWORLD_CONNECT || addr == VA_MASTER_ATTACH_BFE230 ||
                addr == VA_CLIENTCOMPUTER_ATTACH) {
                exp_log_preconnect_marker(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_SELF_MAP
            if (addr == VA_KEY6_SKIP_EXEC_JZ) {
                PCONTEXT c = ei->ContextRecord;
                DWORD obj = exp2_read_safe(c->Ebp - 0x14);
                exp_logf("SELFGATE 0x00c00a85 ESI=0x%08x object=0x%08x self_key=0x%08x",
                         c->Esi, obj, exp2_read_safe(c->Ebp - 0x1c));
                exp_logf("STAGE1OBJ object=0x%08x obj+38=0x%08x obj+3c=0x%08x",
                         obj, exp2_read_safe(obj + 0x38), exp2_read_safe(obj + 0x3c));
                exp_logf("STAGE1OBJ object=0x%08x obj+7c=0x%08x obj+40=0x%08x",
                         obj, exp2_read_safe(obj + 0x7c), exp2_read_safe(obj + 0x40));
                exp_dump_self_map(obj, exp2_read_safe(c->Ebp - 0x1c));
            }
#endif
#ifdef EXP_DUMP_COMPONENT_PATH
#ifdef EXP_DUMP_COMPONENT_SHARP
            if (addr == VA_COMP_KEY_READ_RET) {
                PCONTEXT c = ei->ContextRecord;
                DWORD reader = c->Edi;
                DWORD S = exp2_read_safe(reader + 0x18);
                DWORD cursor = exp2_read_safe(S + 8);
                DWORD B = exp2_read_safe(S + 4);
                DWORD body = exp2_read_safe(B + 0x14);
                exp_logf("COMPKEYRET 0x00bd4bfe key(EAX)=0x%08x prev_ESI=0x%08x obj(EBX)=0x%08x",
                         c->Eax, c->Esi, c->Ebx);
                exp_logf("COMPKEYRET reader(EDI)=0x%08x cursor_after=0x%08x body=0x%08x",
                         reader, cursor, body);
                if (body && cursor >= 4)
                    exp_dump_bytes("COMPKEYBYTES", body + cursor - 4, 16);
            }
            if (addr == VA_COMP_LOOKUP_RET) {
                PCONTEXT c = ei->ContextRecord;
                DWORD existing = exp2_read_safe(c->Esp + 0x14);
                DWORD reader = c->Edi;
                DWORD S = exp2_read_safe(reader + 0x18);
                DWORD cursor = exp2_read_safe(S + 8);
                exp_logf("COMPLOOKUPRET 0x00bd4c19 key(ESI)=0x%08x existing[esp+14]=0x%08x obj(EBX)=0x%08x",
                         c->Esi, existing, c->Ebx);
                exp_logf("COMPLOOKUPRET reader=0x%08x cursor=0x%08x EAX=0x%08x",
                         reader, cursor, c->Eax);
            }
            if (addr == VA_COMP_EXISTING_TEST) {
                PCONTEXT c = ei->ContextRecord;
                exp_logf("COMPEXISTTEST 0x00bd4c25 key(ESI)=0x%08x existing(ECX)=0x%08x obj(EBX)=0x%08x",
                         c->Esi, c->Ecx, c->Ebx);
                exp_logf("COMPEXISTTEST comp_key=0x%08x comp_pid=0x%08x reader(EDI)=0x%08x",
                         c->Ecx ? exp2_read_safe(c->Ecx + 0x0c) : 0,
                         c->Ecx ? exp2_read_safe(c->Ecx + 0x7c) : 0, c->Edi);
            }
            if (addr == VA_COMP_SLOT7_ENTRY) {
                PCONTEXT c = ei->ContextRecord;
                DWORD comp = c->Ecx;
                DWORD reader = c->Edi;
                DWORD comp_key = comp ? exp2_read_safe(comp + 0x0c) : 0;
                DWORD S = exp2_read_safe(reader + 0x18);
                DWORD cursor = exp2_read_safe(S + 8);
                DWORD B = exp2_read_safe(S + 4);
                DWORD body = exp2_read_safe(B + 0x14);
                DWORD obj = exp2_read_safe(c->Ebp + 0x8);
                g_last_slot7_comp = comp;
                g_last_slot7_key = comp_key;
                exp_logf("COMPSLOT7ENTRY 0x00bd4d2c comp(ECX)=0x%08x key=0x%08x pid=0x%08x",
                         comp, comp_key,
                         comp ? exp2_read_safe(comp + 0x7c) : 0);
                exp_logf("COMPSLOT7ENTRY obj=0x%08x reader(EDI)=0x%08x param3[ebp+10]=0x%08x",
                         obj, reader, exp2_read_safe(c->Ebp + 0x10));
                exp_logf("COMPSLOT7ENTRY cursor=0x%08x body=0x%08x esp14=0x%08x",
                         cursor, body, exp2_read_safe(c->Esp + 0x14));
                if (body)
                    exp_dump_bytes("COMPSLOT7CUR", body + cursor, 32);
            }
            if (addr == VA_COMP_SLOT7_RET) {
                PCONTEXT c = ei->ContextRecord;
                DWORD comp = (DWORD)g_last_slot7_comp;
                DWORD key = (DWORD)g_last_slot7_key;
                exp_logf("COMPSLOT7RET 0x00bd4d38 last_comp=0x%08x key=0x%08x EAX=0x%08x",
                         comp, key, c->Eax);
                if (key == KEY_CHARMGR_NID)
                    exp_log_charmgr_container("CHARMGR_AFTER_SLOT7_RET", comp);
            }
#endif
            if (addr == VA_NEWOBJ_COMP_INIT) {
                PCONTEXT c = ei->ContextRecord;
                DWORD esp = c->Esp, a1 = 0, a2 = 0, a3 = 0;
                __try { a1 = *(DWORD*)(esp + 4); a2 = *(DWORD*)(esp + 8); a3 = *(DWORD*)(esp + 0xc); } __except(1) {}
                exp_logf("COMPINIT ecx=0x%08x obj(a1)=0x%08x list(a2)=0x%08x", c->Ecx, a1, a2);
                exp_logf("COMPINIT a3=0x%08x obj_pid=0x%08x", a3, a1 ? exp2_read_safe(a1 + 0x7c) : 0, 0);
                /* list window: for a vector +0=begin/+4=end => (end-begin)/elem = count */
                exp_logf("COMPLIST a2=0x%08x +0=0x%08x +4=0x%08x", a2,
                         exp2_read_safe(a2), exp2_read_safe(a2 + 4));
                exp_logf("COMPLIST +8=0x%08x +c=0x%08x +10=0x%08x",
                         exp2_read_safe(a2 + 8), exp2_read_safe(a2 + 0xc), exp2_read_safe(a2 + 0x10));
                exp_dump_reader(a2);
            }
            if (addr == VA_COMP_MATERIALIZE) {
                PCONTEXT c = ei->ContextRecord;
                DWORD esp = c->Esp, a1 = 0, a2 = 0, a3 = 0;
                __try { a1 = *(DWORD*)(esp + 4); a2 = *(DWORD*)(esp + 8); a3 = *(DWORD*)(esp + 0xc); } __except(1) {}
                exp_logf("MATERIALIZE 0x00bb2580 ECX=0x%08x EAX=0x%08x EDX=0x%08x",
                         c->Ecx, c->Eax, c->Edx);
                exp_logf("MATERIALIZE args a1=0x%08x a2=0x%08x a3=0x%08x",
                         a1, a2, a3);
                exp_logf("MATERIALIZE keys ecx+c=0x%08x a1+c=0x%08x a2+c=0x%08x",
                         exp2_read_safe(c->Ecx + 0xc), exp2_read_safe(a1 + 0xc),
                         exp2_read_safe(a2 + 0xc));
            }
            if (addr == VA_COMP_NULL_TEST)
                exp_logf("COMPNULL 0x00bd4c40 ESI(component)=0x%08x EBX=0x%08x EDI=0x%08x",
                         ei->ContextRecord->Esi, ei->ContextRecord->Ebx, ei->ContextRecord->Edi);
            if (addr == VA_COMP_LOOP_RECHECK) {
                /* EAX=cursor (body-relative) after ServerCom slot-7; ESI=end.
                 * EDI=reader. Connector key is at body offset 0x2f. */
                PCONTEXT c = ei->ContextRecord;
                DWORD reader = c->Edi;
                DWORD S = exp2_read_safe(reader + 0x18);
                DWORD B = exp2_read_safe(S + 4);
                DWORD body = exp2_read_safe(B + 0x14);
                exp_logf("RECHECK 0x00bd4d75 cursor(EAX)=0x%08x end(ESI)=0x%08x EDI(reader)=0x%08x",
                         c->Eax, c->Esi, reader);
                exp_logf("RECHECK body=0x%08x conn_key_off=0x2f delta(cursor-0x2f)=0x%08x", body, c->Eax - 0x2f, 0);
                exp_dump_bytes("RECHECKCUR", body + c->Eax, 32);
#if EXP_TARGET_PROFILE == 76 || EXP_TARGET_PROFILE == 77 || EXP_TARGET_PROFILE == 78
                if ((DWORD)g_last_slot7_key == KEY_CHARMGR_NID)
                    exp_log_charmgr_container("CHARMGR_AT_LOOP_RECHECK", (DWORD)g_last_slot7_comp);
#endif
            }
            if (addr == VA_COMP_MAP_WRITE) {
                DWORD slot = ei->ContextRecord->Ebx;
                DWORD obj = exp2_read_safe(ei->ContextRecord->Ebp + 0x8);
                DWORD node = slot >= 0x10 ? slot - 0x10 : 0;
                DWORD key = exp2_read_safe(node + 0xc);
                exp_logf("COMPWRITE 0x00bd4cf4 EBX(slot)=0x%08x ECX(val)=0x%08x EDI=0x%08x",
                         ei->ContextRecord->Ebx, ei->ContextRecord->Ecx, ei->ContextRecord->Edi);
                exp_logf("COMPWRITE obj=0x%08x obj+14=0x%08x obj+18=0x%08x",
                         obj, exp2_read_safe(obj + 0x14), exp2_read_safe(obj + 0x18));
                exp_logf("COMPWRITE node=0x%08x key=0x%08x oldval=0x%08x",
                         node, key, exp2_read_safe(slot));
                exp_log_map_candidate("COMP_OBJ14", exp2_read_safe(obj + 0x14));
                exp_log_map_candidate("COMP_OBJ18", exp2_read_safe(obj + 0x18));
#ifdef EXP_INJECT_B4_COMPONENT_OBJ18
                exp_inject_b4_component_obj18(obj, ei->ContextRecord->Ecx, key);
#endif
#if EXP_TARGET_PROFILE != 62 && EXP_TARGET_PROFILE != 63
                /* s6: open the read-primitive tracer window for ServerCom slot-7,
                 * which the very next instructions enter.  Profile62 asks only
                 * the post-finish intro-map question; profile63 asks the same
                 * late B6 question plus Lua-command entry. The raw-read tracer is
                 * retained for profile65 because profile61's pre-finish behavior
                 * is the known-good baseline. */
                g_trace_budget = 120;
                exp_arm_trace();
                exp_log("TRACE window armed at COMPWRITE (ServerCom slot-7 next)");
#endif
            }
#endif
#if EXP_TARGET_PROFILE == 64
#ifdef EXP_INJECT_B4_COMPONENT_OBJ18
            if (addr == VA_COMP_MAP_WRITE) {
                DWORD slot = ei->ContextRecord->Ebx;
                DWORD obj = exp2_read_safe(ei->ContextRecord->Ebp + 0x8);
                DWORD node = slot >= 0x10 ? slot - 0x10 : 0;
                DWORD key = exp2_read_safe(node + 0xc);
                exp_inject_b4_component_obj18(obj, ei->ContextRecord->Ecx, key);
            }
#endif
#endif
#ifdef EXP_DUMP_BB0060_TAIL
            if (addr == VA_BB0060_ENTRY ||
                addr == VA_BB0060_TYPE2_ENTRY ||
                addr == VA_BB0060_TYPE2_BLOB1 ||
                addr == VA_BB0060_TYPE2_U8_A ||
                addr == VA_BB0060_TYPE2_BLOB2 ||
                addr == VA_BB0060_TYPE2_U8_B ||
                addr == VA_BB0060_TYPE2_F64 ||
                addr == VA_BB0060_UNPACK_BA9640 ||
                addr == VA_BB0060_UNPACK_CBDA80 ||
                addr == VA_BB0060_UNPACK_RET ||
                addr == VA_BB0060_TYPE2_EXIT) {
                exp_log_bb0060_tail(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_BB0060_FIELDS
            if (addr == VA_BB0060_FIELD_GATE ||
                addr == VA_BB0060_FIELD_GATE_RET) {
                exp_log_bb0060_field(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_SIMPLE_FIELD_READ
            if (addr == VA_SIMPLE_FIELD_READ ||
                addr == VA_SIMPLE_UNPACK_CALL ||
                addr == VA_SIMPLE_UNPACK_RET) {
                exp_log_simple_field_read(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_GOLUA_UNPACKFIXED
            if (addr == VA_GOLUA_UNPACKFIXED) {
                exp_log_golua_unpackfixed(addr, ei->ContextRecord);
            }
#endif
#ifdef EXP_DUMP_B4_BOUNDARY
            if (addr == VA_BB0060_UNPACK_BA9640 ||
                addr == VA_BB0060_UNPACK_CBDA80 ||
                addr == VA_BB0060_UNPACK_RET) {
                exp_log_b4_boundary(addr, ei->ContextRecord, ei->ContextRecord->Ebx, "EBX");
            }
            if (addr == VA_BEDE50_AFTER_BB0060 ||
                addr == VA_BEDE50_CURSOR_RET ||
                addr == VA_BEDE50_CACHE_BE9840 ||
                addr == VA_BEDE50_SEEK_START ||
                addr == VA_BEDE50_CACHE_RESET ||
                addr == VA_BEDE50_COPY_RAW ||
                addr == VA_BEDE50_SEEK_BACK) {
                exp_log_b4_boundary(addr, ei->ContextRecord, ei->ContextRecord->Esi, "ESI");
            }
#endif
#ifdef EXP_REARM_TARGETS
            if (rearm_target) {
                g_rearm_va = addr;
                ei->ContextRecord->EFlags |= 0x100u;
            }
#endif
            ei->ContextRecord->Eip = g_targets[i]; /* resume at the real insn */
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void exp_install_reach_hooks(void)
{
    DWORD i, op;
    AddVectoredExceptionHandler(1, exp_veh);
    for (i = 0; i < N_TARGETS; ++i) {
        BYTE *p = (BYTE*)(DWORD_PTR)g_targets[i];
        if (VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &op)) {
            g_orig_byte[i] = *p;
            *p = 0xCC;                 /* int3 */
            VirtualProtect(p, 1, op, &op);
            FlushInstructionCache(GetCurrentProcess(), p, 1);
            g_armed[i] = TRUE;
            exp_logf("armed int3 @ 0x%08x (orig=0x%02x)", g_targets[i], g_orig_byte[i], 0);
        } else {
            exp_logf("FAILED VirtualProtect @ 0x%08x", g_targets[i], 0, 0);
        }
    }
}

#endif /* EXP_MODE == 1 */

/* ------------------------------------------------------------------ */
/* TEST 2 -- read-only FUser statechart poll (mild: no .text write)    */
/* ------------------------------------------------------------------ */
#if EXP_MODE == 2

/* TODO(Claude/Codex): resolve the live pointer chain from a FIXED global to
 * the FUser "User" NObject and its state field. Static anchors known:
 *   - FUser::onConnectorReplicated 0x010b3530 -> Master_Connecting
 *   - FUser::onSessionReplicated   0x010b68c0 -> Master_SignIn
 *   - FUser::onFinishLoadingCharacter 0x010b3d20 -> Master_FinishLoadingCharacter
 *   - state byte/dword written by the above (see codex_fuser_statefield_writers)
 *   - FUser+0x74 (armed by master-connect/account-control path)
 * Fill EXP2_GLOBAL_PTR with the fixed .data global that ultimately reaches the
 * FUser object, and EXP2_OFF_* with the walk + the state-field offset. */
#define EXP2_GLOBAL_PTR   0x00000000u  /* TODO fixed global (e.g. environment) */
#define EXP2_OFF_COUNT    0            /* TODO number of deref hops            */
static const DWORD EXP2_OFFS[] = { 0 };/* TODO offsets per hop                 */
#define EXP2_OFF_STATE    0x00         /* TODO FUser state field offset        */
#define EXP2_OFF_74       0x74         /* FUser+0x74 (known)                   */

static DWORD exp2_read(DWORD p) { DWORD v=0; __try { v=*(DWORD*)p; } __except(1){} return v; }

static void exp_poll_fuser(void)
{
    DWORD last_state = 0xFFFFFFFF;
    int ticks = 0;
    for (;;) {
        DWORD obj = EXP2_GLOBAL_PTR ? exp2_read(EXP2_GLOBAL_PTR) : 0;
        DWORD i;
        for (i = 0; obj && i < EXP2_OFF_COUNT; ++i) obj = exp2_read(obj + EXP2_OFFS[i]);
        if (obj) {
            DWORD st = exp2_read(obj + EXP2_OFF_STATE);
            DWORD f74 = exp2_read(obj + EXP2_OFF_74);
            if (st != last_state) {
                exp_logf("FUser obj=0x%08x state=0x%08x +0x74=0x%08x", obj, st, f74);
                last_state = st;
            }
        } else if ((ticks % 50) == 0) {
            exp_logf("FUser not resolved yet (global=0x%08x)", EXP2_GLOBAL_PTR, 0, 0);
        }
        Sleep(100);
        ++ticks;
    }
}

#endif /* EXP_MODE == 2 */

/* ------------------------------------------------------------------ */
/* Worker: wait until the target .text is unpacked, then run experiment*/
/* ------------------------------------------------------------------ */

static BOOL looks_unpacked(DWORD va)
{
    /* Themida decrypts .text at runtime. Treat a target as "ready" once its
     * page is committed+readable and the first dword is non-zero and not int3. */
    MEMORY_BASIC_INFORMATION mbi;
    DWORD first;
    if (VirtualQuery((void*)(DWORD_PTR)va, &mbi, sizeof(mbi)) == 0) return FALSE;
    if (mbi.State != MEM_COMMIT) return FALSE;
    if (!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE
                         | PAGE_EXECUTE_WRITECOPY | PAGE_READONLY | PAGE_READWRITE)))
        return FALSE;
    __try { first = *(DWORD*)(DWORD_PTR)va; } __except(1) { return FALSE; }
    return (first != 0 && first != 0xCCCCCCCC);
}

static DWORD WINAPI exp_thread(LPVOID arg)
{
    int waited = 0;
    (void)arg;
    exp_log("=== NpSlayer_exp attached, mode "
#if EXP_MODE == 1
            "1 (int3 reach hooks) ==="
#else
            "2 (read-only FUser poll) ==="
#endif
    );
    exp_logf("target profile=%u", EXP_TARGET_PROFILE, 0, 0);
    /* wait up to ~120s for the target page to come alive */
    while (waited < 1200 && !looks_unpacked(VA_READY_GATE)) {
        Sleep(100);
        ++waited;
        if ((waited % 50) == 0)
            exp_logf("waiting for unpack... %u ds, first=0x%08x", waited,
                     (DWORD)exp2_read_safe(VA_READY_GATE), 0);
    }
    exp_logf("unpack gate: waited=%u ds ready_first=0x%08x", waited,
             (DWORD)exp2_read_safe(VA_READY_GATE), 0);
#if EXP_MODE == 1
#ifdef EXP_DUMP_B4_DESCRIPTORS
    exp_dump_b4_descriptors("after_unpack_gate");
#endif
    if (EXP_INSTALL_DELAY_MS) {
        Sleep(EXP_INSTALL_DELAY_MS);
        exp_logf("delayed hook install after %u ms", EXP_INSTALL_DELAY_MS, 0, 0);
    }
#if EXP_TARGET_PROFILE == 4
    exp_force_key6_executor_gate();
#endif
#ifdef EXP_FORCE_BF0E30_SCOPE_GATE
    exp_force_bf0e30_scope_gate();
#endif
#ifdef EXP_FORCE_BA9640_GATE
    exp_force_ba9640_gate();
#endif
    exp_install_reach_hooks();
#else
    exp_poll_fuser();
#endif
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        init_module_dir(hinst);
        install_hooks();
        log_line("NpSlayer QQXJ hook attached (EXP build)");
        CreateThread(NULL, 0, exp_thread, NULL, 0, NULL);
    }
    return TRUE;
}

int NpsInit(void)    { install_hooks(); return NPS_GAMEGUARD_SUCCESS; }
int NpsSetVars(void) { install_hooks(); return NPS_GAMEGUARD_SUCCESS; }
int NpsReport(void)  { install_hooks(); return NPS_GAMEGUARD_SUCCESS; }
int NpsSetMode(void) { install_hooks(); return NPS_GAMEGUARD_SUCCESS; }
