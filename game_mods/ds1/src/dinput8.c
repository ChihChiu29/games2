// Dark Souls Remastered (PC, Windows x64) - Natural HP Regen Mod
// Proxy DLL for dinput8.dll (forwards DirectInput8Create to system dinput8.dll or chained DLL).
//
// Verified DarkSoulsRemastered.exe RVAs / Offsets:
//   WorldChrMan ptr RVA = 0x1C77E50  (signature at 0x27BF6D: 48 89 05 dc be 9f 01)
//     +0x68             = PlayerIns*
//     PlayerIns + 0x3E8 = int32_t hp
//     PlayerIns + 0x3EC = int32_t maxHp

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define RVA_WORLD_CHR_MAN   0x1C77E50ULL
#define RVA_SIG_WCM         0x027BF6DULL

#define OFF_WCM_PLAYER_INS  0x0068ULL
#define OFF_PI_HP           0x03E8ULL
#define OFF_PI_MAX_HP       0x03ECULL

#define TICK_MS             333  // ~3 ticks/sec

typedef HRESULT (WINAPI *PFN_DirectInput8Create)(
    HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, void *punkOuter);

static HMODULE g_real_dinput8 = NULL;
static PFN_DirectInput8Create g_real_DirectInput8Create = NULL;
static volatile LONG g_running = 1;
static HANDLE g_worker_thread = NULL;

static char g_dir[MAX_PATH] = {0};
static char g_ini_path[MAX_PATH] = {0};
static char g_log_path[MAX_PATH] = {0};

static int    g_hp_regen_enabled = 1;
static double g_hp_regen_pct_per_sec = 2.0;
static int    g_sound_enabled = 1;
static char   g_chain_dll[MAX_PATH] = {0};
static int    g_logged_ingame = 0;

static void log_msg(const char *fmt, ...) {
    FILE *f = fopen(g_log_path, "a");
    if (!f) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02u:%02u:%02u] ", st.wHour, st.wMinute, st.wSecond);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}

static int is_readable(uintptr_t addr, size_t size) {
    if (addr < 0x10000ULL || addr > 0x00007FFFFFFFFFFFULL) return 0;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) != sizeof(mbi)) return 0;
    if (mbi.State != MEM_COMMIT) return 0;
    DWORD p = mbi.Protect & 0xFF;
    if (p == PAGE_NOACCESS || (mbi.Protect & PAGE_GUARD)) return 0;
    if (!(p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
          p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY)) {
        return 0;
    }
    return (addr + size) <= ((uintptr_t)mbi.BaseAddress + mbi.RegionSize);
}

static int is_writable(uintptr_t addr, size_t size) {
    if (addr < 0x10000ULL || addr > 0x00007FFFFFFFFFFFULL) return 0;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) != sizeof(mbi)) return 0;
    if (mbi.State != MEM_COMMIT) return 0;
    DWORD p = mbi.Protect & 0xFF;
    if (p == PAGE_NOACCESS || (mbi.Protect & PAGE_GUARD)) return 0;
    if (!(p == PAGE_READWRITE || p == PAGE_WRITECOPY ||
          p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY)) {
        return 0;
    }
    return (addr + size) <= ((uintptr_t)mbi.BaseAddress + mbi.RegionSize);
}

static int read_ptr(uintptr_t addr, uintptr_t *out) {
    if (!is_readable(addr, sizeof(uintptr_t))) return 0;
    *out = *(volatile uintptr_t *)addr;
    return 1;
}

static int read_i32(uintptr_t addr, int32_t *out) {
    if (!is_readable(addr, sizeof(int32_t))) return 0;
    *out = *(volatile int32_t *)addr;
    return 1;
}

static int write_i32(uintptr_t addr, int32_t val) {
    if (!is_writable(addr, sizeof(int32_t))) return 0;
    *(volatile int32_t *)addr = val;
    return 1;
}

static void init_paths(HMODULE hMod) {
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(hMod, path, MAX_PATH);
    char *slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    else strcpy(path, ".\\");
    snprintf(g_dir, sizeof(g_dir), "%s", path);
    snprintf(g_ini_path, sizeof(g_ini_path), "%sds1_regen.ini", g_dir);
    snprintf(g_log_path, sizeof(g_log_path), "%sds1_regen.log", g_dir);
}

static void load_config(void) {
    char buf[128];
    g_hp_regen_enabled = GetPrivateProfileIntA("Regen", "hp_regen_enabled", 1, g_ini_path);
    GetPrivateProfileStringA("Regen", "hp_regen_percent_per_sec", "2.0", buf, sizeof(buf), g_ini_path);
    g_hp_regen_pct_per_sec = atof(buf);
    if (g_hp_regen_pct_per_sec < 0.0) g_hp_regen_pct_per_sec = 0.0;
    if (g_hp_regen_pct_per_sec > 50.0) g_hp_regen_pct_per_sec = 50.0;

    g_sound_enabled = GetPrivateProfileIntA("General", "sound_enabled", 1, g_ini_path);
    GetPrivateProfileStringA("General", "chain_dinput8_dll", "", g_chain_dll, sizeof(g_chain_dll), g_ini_path);
}

static void load_real_dinput8(void) {
    if (g_real_dinput8) return;

    if (g_chain_dll[0] != '\0') {
        char chain_path[MAX_PATH];
        if (strchr(g_chain_dll, '\\') || strchr(g_chain_dll, ':')) {
            snprintf(chain_path, sizeof(chain_path), "%s", g_chain_dll);
        } else {
            snprintf(chain_path, sizeof(chain_path), "%s%s", g_dir, g_chain_dll);
        }
        g_real_dinput8 = LoadLibraryA(chain_path);
        if (g_real_dinput8) log_msg("Chained dinput8 DLL: %s", chain_path);
        else log_msg("WARNING: Failed to load chain_dinput8_dll '%s', falling back to System32", chain_path);
    }

    if (!g_real_dinput8) {
        char sysdir[MAX_PATH];
        UINT n = GetSystemDirectoryA(sysdir, MAX_PATH);
        if (n > 0 && n < MAX_PATH - 16) {
            char full[MAX_PATH];
            snprintf(full, sizeof(full), "%s\\dinput8.dll", sysdir);
            g_real_dinput8 = LoadLibraryA(full);
        }
    }

    if (g_real_dinput8) {
        g_real_DirectInput8Create = (PFN_DirectInput8Create)GetProcAddress(g_real_dinput8, "DirectInput8Create");
    }
}

__declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, void *punkOuter)
{
    load_real_dinput8();
    if (!g_real_DirectInput8Create) return E_FAIL;
    return g_real_DirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}

static int verify_ds1r(uintptr_t base) {
    static const uint8_t sig_wcm[7] = {0x48, 0x89, 0x05, 0xDC, 0xBE, 0x9F, 0x01};
    if (!is_readable(base + RVA_SIG_WCM, 7)) return 0;
    return memcmp((const void *)(base + RVA_SIG_WCM), sig_wcm, 7) == 0;
}

static DWORD WINAPI worker_main(LPVOID param) {
    (void)param;
    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    log_msg("DS1R Natural HP Regen mod loaded (base=0x%llX)", (unsigned long long)base);
    log_msg("Config: hp_regen=%d (%.2f%%/s), sound=%d",
            g_hp_regen_enabled, g_hp_regen_pct_per_sec, g_sound_enabled);

    int sig_ok = 0;
    for (int i = 0; i < 60 && g_running; ++i) {
        if (verify_ds1r(base)) {
            sig_ok = 1;
            break;
        }
        Sleep(500);
    }
    if (!sig_ok) {
        log_msg("ERROR: DarkSoulsRemastered.exe signature mismatch! Mod disabled.");
        return 0;
    }
    log_msg("Verified DarkSoulsRemastered.exe signature.");

    int f5_prev = 0;
    while (g_running) {
        Sleep(TICK_MS);

        int f5_now = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        if (f5_now && !f5_prev) {
            g_hp_regen_enabled = !g_hp_regen_enabled;
            if (g_sound_enabled) Beep(g_hp_regen_enabled ? 784 : 392, 80);
            log_msg("Hotkey F5: HP Regen %s", g_hp_regen_enabled ? "ON" : "OFF");
        }
        f5_prev = f5_now;

        if (!g_hp_regen_enabled || g_hp_regen_pct_per_sec <= 0.0) continue;

        uintptr_t wcm = 0, player_ins = 0;
        if (!read_ptr(base + RVA_WORLD_CHR_MAN, &wcm) || !wcm) continue;
        if (!read_ptr(wcm + OFF_WCM_PLAYER_INS, &player_ins) || !player_ins) continue;

        int32_t hp = 0, max_hp = 0;
        if (!read_i32(player_ins + OFF_PI_HP, &hp) ||
            !read_i32(player_ins + OFF_PI_MAX_HP, &max_hp)) {
            continue;
        }
        if (max_hp < 100 || max_hp > 100000) continue;

        if (!g_logged_ingame) {
            g_logged_ingame = 1;
            log_msg("In-game detected: HP=%d/%d", hp, max_hp);
        }

        if (hp > 0 && hp < max_hp) {
            double per_tick = (double)max_hp * (g_hp_regen_pct_per_sec / 100.0) * ((double)TICK_MS / 1000.0);
            int32_t heal = (int32_t)llround(per_tick);
            if (heal < 1) heal = 1;
            int32_t new_hp = hp + heal;
            if (new_hp > max_hp) new_hp = max_hp;
            write_i32(player_ins + OFF_PI_HP, new_hp);
        }
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)lpReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
        init_paths(hinstDLL);
        load_config();
        g_worker_thread = CreateThread(NULL, 0, worker_main, NULL, 0, NULL);
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        InterlockedExchange(&g_running, 0);
        if (g_worker_thread) {
            WaitForSingleObject(g_worker_thread, 1000);
            CloseHandle(g_worker_thread);
            g_worker_thread = NULL;
        }
    }
    return TRUE;
}
