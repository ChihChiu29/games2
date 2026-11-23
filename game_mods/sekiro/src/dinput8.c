// Sekiro: Shadows Die Twice (v1.06, Windows x64) - Natural HP Regen & Level-Up Roll Mod
// Proxy DLL for dinput8.dll (forwards DirectInput8Create to system dinput8.dll or chained DLL).
//
// Verified Sekiro 1.06 RVAs / Offsets (from sekiro.exe):
//   WorldChrMan ptr RVA       = 0x3D7A140  (signature at 0xA66D99: 48 89 05 a0 33 31 03)
//     +0x88                   = PlayerIns*
//     PlayerIns + 0x1FF8      = SprjChrModuleBag*
//     ModuleBag + 0x18        = SprjChrDataModule* (index 3)
//     DataModule + 0x130      = int32_t hp
//     DataModule + 0x134      = int32_t maxHp
//     DataModule + 0x138      = int32_t baseMaxHp
//   GameDataMan ptr RVA       = 0x3D5AA20  (signature at 0x7B1EBB: 48 89 05 5e 8b 5a 03)
//     +0x08                   = PlayerGameData*
//     PGD + 0x44              = int32_t baseVitality  (display = val + 9, 1..11 -> 10..20)
//     PGD + 0x48              = int32_t baseAttackPower (1..99, natively saved)
//     PGD + 0x154             = int32_t totalSkillPointsEarned (level; increments on XP bar fill)
//     PGD + 0x248             = int32_t effVitality
//     PGD + 0x24C             = int32_t effAttackPower

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ---- Verified Sekiro 1.06 RVAs & Offsets ----
#define RVA_WORLD_CHR_MAN     0x3D7A140ULL
#define RVA_GAME_DATA_MAN     0x3D5AA20ULL
#define RVA_SIG_WCM           0x0A66D99ULL
#define RVA_SIG_GDM           0x07B1EBBULL

#define OFF_WCM_PLAYER_INS    0x0088ULL
#define OFF_PI_MODULE_BAG     0x1FF8ULL
#define OFF_BAG_DATA_MODULE   0x0018ULL
#define OFF_DM_HP             0x0130ULL
#define OFF_DM_MAX_HP         0x0134ULL
#define OFF_DM_BASE_MAX_HP    0x0138ULL

#define OFF_GDM_PGD           0x0008ULL
#define OFF_PGD_BASE_VIT      0x0044ULL
#define OFF_PGD_BASE_ATK      0x0048ULL
#define OFF_PGD_SKILL_LEVEL   0x0154ULL
#define OFF_PGD_EFF_VIT       0x0248ULL
#define OFF_PGD_EFF_ATK       0x024CULL

#define MAX_ATTACK_POWER      99
#define TICK_MS               333  // ~3 ticks/sec

// ---- Proxy state ----
typedef HRESULT (WINAPI *PFN_DirectInput8Create)(
    HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, void *punkOuter);

static HMODULE g_real_dinput8 = NULL;
static PFN_DirectInput8Create g_real_DirectInput8Create = NULL;
static HMODULE g_self_module = NULL;
static volatile LONG g_running = 1;
static HANDLE g_worker_thread = NULL;

// ---- Paths ----
static char g_dir[MAX_PATH] = {0};
static char g_ini_path[MAX_PATH] = {0};
static char g_state_path[MAX_PATH] = {0};
static char g_log_path[MAX_PATH] = {0};

// ---- Config ----
static int    g_hp_regen_enabled = 1;
static double g_hp_regen_pct_per_sec = 2.0;
static int    g_levelup_roll_enabled = 1;
static int    g_attack_roll_chance = 50;   // 0..100
static double g_hp_bonus_pct_per_roll = 5.0;
static int    g_sound_enabled = 1;
static char   g_chain_dll[MAX_PATH] = {0};

// ---- Persistent state ----
static int g_last_skill_level = -1;
static int g_hp_bonus_rolls = 0;

// ---- Runtime MaxHP tracking ----
static int32_t g_vanilla_base_max_hp = 0;
static int32_t g_last_applied_base_max_hp = 0;
static int     g_logged_ingame = 0;

// ---- RNG (xorshift64*) ----
static uint64_t g_rng_state = 0;
static void rng_seed(void) {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    g_rng_state = (uint64_t)li.QuadPart ^ ((uint64_t)GetTickCount64() << 17) ^ ((uint64_t)GetCurrentProcessId() << 33);
    if (g_rng_state == 0) g_rng_state = 0x9E3779B97F4A7C15ULL;
}
static int rng_percent(void) {
    uint64_t x = g_rng_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    g_rng_state = x;
    uint64_t r = x * 0x2545F4914F6CDD1DULL;
    return (int)((r >> 32) % 100);
}

// ---- Logging ----
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

// ---- Safe memory access ----
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
    uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return (addr + size) <= end;
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
    uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
    return (addr + size) <= end;
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

// ---- Config & State I/O ----
static void init_paths(HMODULE hMod) {
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(hMod, path, MAX_PATH);
    char *slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    else strcpy(path, ".\\");
    snprintf(g_dir, sizeof(g_dir), "%s", path);
    snprintf(g_ini_path, sizeof(g_ini_path), "%ssekiro_regen.ini", g_dir);
    snprintf(g_state_path, sizeof(g_state_path), "%ssekiro_regen_state.ini", g_dir);
    snprintf(g_log_path, sizeof(g_log_path), "%ssekiro_regen.log", g_dir);
}

static void load_config(void) {
    char buf[128];
    g_hp_regen_enabled = GetPrivateProfileIntA("Regen", "hp_regen_enabled", 1, g_ini_path);
    GetPrivateProfileStringA("Regen", "hp_regen_percent_per_sec", "2.0", buf, sizeof(buf), g_ini_path);
    g_hp_regen_pct_per_sec = atof(buf);
    if (g_hp_regen_pct_per_sec < 0.0) g_hp_regen_pct_per_sec = 0.0;
    if (g_hp_regen_pct_per_sec > 50.0) g_hp_regen_pct_per_sec = 50.0;

    g_levelup_roll_enabled = GetPrivateProfileIntA("LevelUpRoll", "enabled", 1, g_ini_path);
    g_attack_roll_chance = GetPrivateProfileIntA("LevelUpRoll", "attack_roll_chance", 50, g_ini_path);
    if (g_attack_roll_chance < 0) g_attack_roll_chance = 0;
    if (g_attack_roll_chance > 100) g_attack_roll_chance = 100;

    GetPrivateProfileStringA("LevelUpRoll", "hp_bonus_percent", "5.0", buf, sizeof(buf), g_ini_path);
    g_hp_bonus_pct_per_roll = atof(buf);
    if (g_hp_bonus_pct_per_roll < 0.0) g_hp_bonus_pct_per_roll = 0.0;
    if (g_hp_bonus_pct_per_roll > 100.0) g_hp_bonus_pct_per_roll = 100.0;

    g_sound_enabled = GetPrivateProfileIntA("General", "sound_enabled", 1, g_ini_path);
    GetPrivateProfileStringA("General", "chain_dinput8_dll", "", g_chain_dll, sizeof(g_chain_dll), g_ini_path);
}

static void load_state(void) {
    g_last_skill_level = (int)GetPrivateProfileIntA("State", "last_skill_level", -1, g_state_path);
    g_hp_bonus_rolls = (int)GetPrivateProfileIntA("State", "hp_bonus_rolls", 0, g_state_path);
    if (g_hp_bonus_rolls < 0) g_hp_bonus_rolls = 0;
}

static void save_state(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", g_last_skill_level);
    WritePrivateProfileStringA("State", "last_skill_level", buf, g_state_path);
    snprintf(buf, sizeof(buf), "%d", g_hp_bonus_rolls);
    WritePrivateProfileStringA("State", "hp_bonus_rolls", buf, g_state_path);
}

// ---- Sound cues ----
static void play_cue_attack(void) {
    if (!g_sound_enabled) return;
    Beep(880, 75);
    Beep(1175, 100);
}

static void play_cue_hp(void) {
    if (!g_sound_enabled) return;
    Beep(523, 75);
    Beep(659, 100);
}

static void play_cue_toggle(int on) {
    if (!g_sound_enabled) return;
    Beep(on ? 784 : 392, 80);
}

// ---- Proxy loader ----
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
        if (g_real_dinput8) {
            log_msg("Chained dinput8 DLL: %s", chain_path);
        } else {
            log_msg("WARNING: Failed to load chain_dinput8_dll '%s', falling back to System32", chain_path);
        }
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

// ---- Signature verification (handles SteamStub unpack delay) ----
static int verify_sekiro_106(uintptr_t base) {
    static const uint8_t sig_wcm[7] = {0x48, 0x89, 0x05, 0xA0, 0x33, 0x31, 0x03};
    static const uint8_t sig_gdm[7] = {0x48, 0x89, 0x05, 0x5E, 0x8B, 0x5A, 0x03};
    if (!is_readable(base + RVA_SIG_WCM, 7) || !is_readable(base + RVA_SIG_GDM, 7)) return 0;
    return memcmp((const void *)(base + RVA_SIG_WCM), sig_wcm, 7) == 0 &&
           memcmp((const void *)(base + RVA_SIG_GDM), sig_gdm, 7) == 0;
}

// ---- Apply accumulated MaxHP bonus cleanly (no double-scaling) ----
static void apply_hp_bonus(uintptr_t dm, int heal_delta_now) {
    if (g_hp_bonus_rolls <= 0 || g_hp_bonus_pct_per_roll <= 0.0) return;

    int32_t cur_hp = 0, cur_max = 0, cur_base = 0;
    if (!read_i32(dm + OFF_DM_HP, &cur_hp) ||
        !read_i32(dm + OFF_DM_MAX_HP, &cur_max) ||
        !read_i32(dm + OFF_DM_BASE_MAX_HP, &cur_base)) {
        return;
    }
    if (cur_base < 100 || cur_max < 100) return;

    int game_reset_base = 0;
    if (cur_base != g_last_applied_base_max_hp) {
        // Fresh vanilla BaseMaxHP (game load, respawn, or Prayer Necklace upgrade)
        g_vanilla_base_max_hp = cur_base;
        game_reset_base = 1;
    }

    if (g_vanilla_base_max_hp < 100) return;

    double mult = 1.0 + ((double)g_hp_bonus_rolls * g_hp_bonus_pct_per_roll) / 100.0;
    int32_t target_base = (int32_t)llround((double)g_vanilla_base_max_hp * mult);
    if (target_base < g_vanilla_base_max_hp) target_base = g_vanilla_base_max_hp;

    if (cur_base != target_base || cur_max < target_base) {
        int32_t old_max = cur_max;
        int was_full = (cur_hp >= old_max);
        write_i32(dm + OFF_DM_BASE_MAX_HP, target_base);
        write_i32(dm + OFF_DM_MAX_HP, target_base);
        g_last_applied_base_max_hp = target_base;

        if (cur_hp > 0) {
            if (game_reset_base && was_full) {
                write_i32(dm + OFF_DM_HP, target_base);
            } else if (heal_delta_now && target_base > old_max) {
                int32_t new_hp = cur_hp + (target_base - old_max);
                if (new_hp > target_base) new_hp = target_base;
                write_i32(dm + OFF_DM_HP, new_hp);
            }
        }
    }
}

// ---- Main worker loop ----
static DWORD WINAPI worker_main(LPVOID param) {
    (void)param;
    rng_seed();

    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    log_msg("Sekiro Regen & LevelUp Roll mod loaded (base=0x%llX)", (unsigned long long)base);
    log_msg("Config: hp_regen=%d (%.2f%%/s), levelup_roll=%d (atk_chance=%d%%, hp_bonus=+%.1f%%/roll), sound=%d",
            g_hp_regen_enabled, g_hp_regen_pct_per_sec, g_levelup_roll_enabled,
            g_attack_roll_chance, g_hp_bonus_pct_per_roll, g_sound_enabled);
    log_msg("Loaded state: last_skill_level=%d, hp_bonus_rolls=%d", g_last_skill_level, g_hp_bonus_rolls);

    // Wait up to 30 seconds for SteamStub / exe to be ready and verify 1.06 signature
    int sig_ok = 0;
    for (int i = 0; i < 60 && g_running; ++i) {
        if (verify_sekiro_106(base)) {
            sig_ok = 1;
            break;
        }
        Sleep(500);
    }
    if (!sig_ok) {
        log_msg("ERROR: sekiro.exe v1.06 signature not found! Mod disabled to prevent crashes.");
        return 0;
    }
    log_msg("Verified Sekiro v1.06 executable signature.");

    int f5_prev = 0, f6_prev = 0;

    while (g_running) {
        Sleep(TICK_MS);

        // Hotkeys (F5 = toggle HP regen, F6 = toggle Level-Up rolls)
        int f5_now = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        if (f5_now && !f5_prev) {
            g_hp_regen_enabled = !g_hp_regen_enabled;
            play_cue_toggle(g_hp_regen_enabled);
            log_msg("Hotkey F5: HP Regen %s", g_hp_regen_enabled ? "ON" : "OFF");
        }
        f5_prev = f5_now;

        int f6_now = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
        if (f6_now && !f6_prev) {
            g_levelup_roll_enabled = !g_levelup_roll_enabled;
            play_cue_toggle(g_levelup_roll_enabled);
            log_msg("Hotkey F6: Level-Up Rolls %s", g_levelup_roll_enabled ? "ON" : "OFF");
        }
        f6_prev = f6_now;

        // Resolve WorldChrMan -> PlayerIns -> ModuleBag -> SprjChrDataModule
        uintptr_t wcm = 0, player_ins = 0, mod_bag = 0, dm = 0;
        if (!read_ptr(base + RVA_WORLD_CHR_MAN, &wcm) || !wcm) continue;
        if (!read_ptr(wcm + OFF_WCM_PLAYER_INS, &player_ins) || !player_ins) continue;
        if (!read_ptr(player_ins + OFF_PI_MODULE_BAG, &mod_bag) || !mod_bag) continue;
        if (!read_ptr(mod_bag + OFF_BAG_DATA_MODULE, &dm) || !dm) continue;

        // Resolve GameDataMan -> PlayerGameData
        uintptr_t gdm = 0, pgd = 0;
        if (!read_ptr(base + RVA_GAME_DATA_MAN, &gdm) || !gdm) continue;
        if (!read_ptr(gdm + OFF_GDM_PGD, &pgd) || !pgd) continue;

        int32_t hp = 0, max_hp = 0, base_max_hp = 0;
        if (!read_i32(dm + OFF_DM_HP, &hp) ||
            !read_i32(dm + OFF_DM_MAX_HP, &max_hp) ||
            !read_i32(dm + OFF_DM_BASE_MAX_HP, &base_max_hp)) {
            continue;
        }
        if (max_hp < 100 || base_max_hp < 100) continue; // Not fully in-game yet

        int32_t skill_level = 0, base_atk = 0, eff_atk = 0, base_vit = 0;
        if (!read_i32(pgd + OFF_PGD_SKILL_LEVEL, &skill_level) ||
            !read_i32(pgd + OFF_PGD_BASE_ATK, &base_atk) ||
            !read_i32(pgd + OFF_PGD_EFF_ATK, &eff_atk) ||
            !read_i32(pgd + OFF_PGD_BASE_VIT, &base_vit)) {
            continue;
        }
        if (base_atk < 1 || base_atk > MAX_ATTACK_POWER || skill_level < 0) continue;

        if (!g_logged_ingame) {
            g_logged_ingame = 1;
            log_msg("In-game detected: HP=%d/%d (base=%d), Vitality=%d, AttackPower=%d, SkillLevel=%d",
                    hp, max_hp, base_max_hp, base_vit + 9, base_atk, skill_level);
        }

        // 1. Keep accumulated MaxHP bonus applied across loads/respawns/Prayer Necklaces
        apply_hp_bonus(dm, 0);

        // 2. Check for Skill Point level-up (XP bar filled!)
        if (g_last_skill_level < 0) {
            g_last_skill_level = skill_level;
            save_state();
            log_msg("Initialized baseline SkillLevel=%d", g_last_skill_level);
        } else if (skill_level < g_last_skill_level) {
            // Loaded an older save or started a new game
            log_msg("SkillLevel decreased (%d -> %d); syncing baseline.", g_last_skill_level, skill_level);
            g_last_skill_level = skill_level;
            save_state();
        } else if (skill_level > g_last_skill_level) {
            int gained = skill_level - g_last_skill_level;
            if (gained > 20) {
                // Guard against loading a much higher-level save file
                log_msg("Large SkillLevel jump (%d -> %d, +%d); treating as save switch, syncing baseline.",
                        g_last_skill_level, skill_level, gained);
                g_last_skill_level = skill_level;
                save_state();
            } else {
                for (int i = 1; i <= gained; ++i) {
                    int lvl = g_last_skill_level + i;
                    if (!g_levelup_roll_enabled) {
                        log_msg("[Level %d] Level-Up Roll disabled, skipping.", lvl);
                        continue;
                    }
                    int roll = rng_percent();
                    int choose_atk = (roll < g_attack_roll_chance) && (base_atk < MAX_ATTACK_POWER);
                    if (choose_atk) {
                        base_atk += 1;
                        eff_atk += 1;
                        if (eff_atk > MAX_ATTACK_POWER) eff_atk = MAX_ATTACK_POWER;
                        write_i32(pgd + OFF_PGD_BASE_ATK, base_atk);
                        write_i32(pgd + OFF_PGD_EFF_ATK, eff_atk);
                        log_msg("[Level %d] Rolled ATTACK POWER +1 -> now %d (roll=%d)", lvl, base_atk, roll);
                        play_cue_attack();
                    } else {
                        g_hp_bonus_rolls += 1;
                        apply_hp_bonus(dm, 1);
                        double total_pct = (double)g_hp_bonus_rolls * g_hp_bonus_pct_per_roll;
                        log_msg("[Level %d] Rolled MAX HP +%.1f%% -> rolls=%d (total +%.1f%%, roll=%d)",
                                lvl, g_hp_bonus_pct_per_roll, g_hp_bonus_rolls, total_pct, roll);
                        play_cue_hp();
                    }
                }
                g_last_skill_level = skill_level;
                save_state();
            }
        }

        // 3. Natural HP regeneration (~2% MaxHP/sec while alive)
        if (g_hp_regen_enabled && g_hp_regen_pct_per_sec > 0.0) {
            if (read_i32(dm + OFF_DM_HP, &hp) && read_i32(dm + OFF_DM_MAX_HP, &max_hp)) {
                if (hp > 0 && hp < max_hp) {
                    double per_tick = (double)max_hp * (g_hp_regen_pct_per_sec / 100.0) * ((double)TICK_MS / 1000.0);
                    int32_t heal = (int32_t)llround(per_tick);
                    if (heal < 1) heal = 1;
                    int32_t new_hp = hp + heal;
                    if (new_hp > max_hp) new_hp = max_hp;
                    write_i32(dm + OFF_DM_HP, new_hp);
                }
            }
        }
    }

    return 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    (void)lpReserved;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_self_module = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        init_paths(hinstDLL);
        load_config();
        load_state();
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
