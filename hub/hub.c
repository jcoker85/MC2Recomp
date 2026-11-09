/*
 * MC2Recomp hub (ported from the TwistedMetal1234Recomp hub).
 *
 * A small menu for the PS2 games recompiled with ps2-native-recomp. Each row
 * has two buttons: Play starts the game straight away with its saved
 * settings, and Settings opens this hub's own settings page for that game.
 * The settings are kept in settings.ini beside this executable, one [<id>]
 * section per game. Every option is off until it is switched on, so a fresh
 * install plays the original game.
 *
 * The list comes from games.txt beside this executable, one game per line:
 *   Title|Subtitle|path/to/ps2run.exe|Cover name|id|ELF name
 * The path is relative to this file and lies inside the toolkit folder, as
 * <toolkit>/build/native-<id>/ps2run.exe; the toolkit folder is the game's
 * working directory and holds games/<id>/ (patches.txt, the memory card, keys.ini). The disc image is the player's own: the
 * first Play asks for it (a standard Open dialog) and its path is kept in settings.ini (disc=); Settings > Disc Image changes
 * it. The game boots from that image (ps2run reads the game's program from it), so nothing of the game is kept beside the
 * launcher. A folder <id>-options beside the toolkit folder means the
 * game was built with the switchable widescreen / 60 fps / 4:3 HUD patches:
 * the hub writes their option words into <id>-options/boot.txt before each
 * start. Without that folder those three options are greyed out.
 *
 * Play starts the game like this (the hub is the way to play; mc2.cmd is deprecated):
 *   ps2run.exe --disc "<your disc image>" --window|--fullscreen
 *     --game-dir "games\<id>" --memcard "games\<id>\<id>.ps2" --patches "games\<id>\patches.txt"
 *     --patches "<options>\boot.txt" [--widescreen]
 * (the memory card is the toolkit app's own file for the game, so saves are shared with it)
 * with PS2_SCALE, PS2_TRUE_COLOR, PS2_DITHER, PS2_LOOK, PS2_PACE, PS2_PRESENT_MODE,
 * PS2_WIDE_ONLY_IF/PS2_WIDE_WORD (4:3 Menus) and PS2_WINDOW_TITLE set for it
 * ("MC2Recomp.exe --check [id]" prints all of it without starting anything).
 *
 * The window keeps the shape of a PS2 DVD-case front. The cover of the
 * highlighted game (covers/<id>.png beside the executable) fills the
 * background and cross-fades as the highlight moves. Covers are not shipped:
 * missing ones are downloaded in the background from the libretro-thumbnails
 * project (Sony - PlayStation 2) by the cover name in games.txt.
 *
 * Controls: arrow keys + Enter, mouse, or a controller (D-pad + A, B back).
 */
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <wchar.h>
#include <stdio.h>
#include <stdarg.h>

#define MAX_GAMES 16
#define CLIENT_W 540          /* PS2 box fronts (libretro scans are 512x726) */
#define CLIENT_H 766
#define ROW_H 62
#define ROW_GAP 8
#define MARGIN 28
#define LIST_BOTTOM 58        /* space under the list for the hint line */
#define BTN_W 98
#define BTN_GAP 8
#define COL_PLAY 0
#define COL_SETTINGS 1
#define FADE_MS 160
#define TICK_MS 16

/* Settings page layout. */
#define S_TOP 112
#define S_ROW_H 62
#define S_ROW_GAP 6
#define S_VIEW_TOP 104        /* the rows scroll between these two lines */
#define S_VIEW_BOTTOM (CLIENT_H - 56)
#define S_WHEEL_STEP (S_ROW_H + S_ROW_GAP)
#define S_VAL_W 74            /* ON/OFF box */
#define S_LOOK_W 190          /* the picture mode and texture filtering boxes */
#define S_DISC_W 230          /* the disc image box */
#define S_SEG_W 38            /* one of the 1x..4x boxes */
#define S_SEG2_W 70           /* one of the 4:3 / 16:9 and 16-bit / 32-bit boxes */
#define S_SEG_GAP 4

/* The red of the car on the cover. */
#define ACCENT RGB(222, 30, 24)
#define ACCENT_DIM RGB(118, 14, 10)
#define WHITE RGB(245, 245, 245)
#define GREY RGB(170, 170, 170)
#define DARK_GREY RGB(105, 105, 105)

typedef struct {
    int wide, hud43, menus43, sixty, scale, look, texfilter, smooth, truecolor, dither, pacing, vsync, fullscreen, skipintro;
} Options;

typedef struct {
    wchar_t title[96];
    wchar_t subtitle[128];
    wchar_t exe[MAX_PATH];      /* full path of ps2run.exe */
    wchar_t tool[MAX_PATH];     /* toolkit folder: working directory */
    wchar_t exe_rel[MAX_PATH];  /* ps2run.exe relative to the toolkit folder */
    wchar_t opts[MAX_PATH];     /* <id>-options beside the toolkit folder */
    wchar_t id[32];
    wchar_t elf[64];
    wchar_t cover_name[96];     /* libretro-thumbnails box art name, or empty */
    wchar_t disc[MAX_PATH];     /* the player's disc image (settings.ini disc=), or empty */
    int available;              /* ps2run.exe is built */
    int toggles;                /* <id>-options exists: widescreen/60 fps/HUD patches */
    int menus;                  /* <id>-options/menus43.txt exists: 4:3 menus and videos */
    Options o;
    HBITMAP cover;              /* cover scaled to the client area, or NULL */
} Game;

static Game g_games[MAX_GAMES];
static int g_count;
static int g_sel;
static int g_col;       /* COL_PLAY or COL_SETTINGS */
static HANDLE g_child;
static wchar_t g_dir[MAX_PATH];
static wchar_t g_ini[MAX_PATH];
static HFONT g_font_item, g_font_small, g_font_btn, g_font_head, g_font_set;

/* Pages: the game list, or the settings of one game. */
enum { PAGE_LIST, PAGE_SETTINGS };
/* The picture looks of the toolkit's window (host/keymap.cpp; PS2_LOOK takes the word): how the PS2 picture is enlarged
 * for the window. Crisp is closest to the console; the toolkit's own default is sharp_edges. */
/* 6: FSR (AMD FidelityFX Super Resolution 1, PS2_FSR=1, gpu.cpp), which takes the place of the look; its name word is fsr. */
#define LOOK_FSR 6
static const wchar_t* k_look_words[7] = { L"crisp", L"crisp_edges", L"soft", L"soft_edges", L"sharp", L"sharp_edges", L"fsr" };
/* Texture filtering (PS2_TEX_FILTER, gpu.cpp make_setup), as PCSX2 names them; 0 = the game's own choice. */
static const wchar_t* k_filter_words[3] = { L"ps2", L"nearest", L"bilinear" };
static const wchar_t* k_filter_names[3] = { L"Bilinear (PS2)", L"Nearest", L"Bilinear (Forced)" };
static const wchar_t* k_look_names[7] = { L"Crisp", L"Crisp + edges", L"Soft", L"Soft + edges", L"Sharp", L"Sharp + edges", L"FSR" };

/* The settings rows, in the order they are shown. */
enum { S_FULLSCREEN, S_SCALE, S_VSYNC, S_PACING, S_LOOK, S_TEXFILTER, S_SIXTY, S_WIDE, S_HUD43, S_MENUS43, S_TRUECOLOR, S_DITHER, S_SMOOTH, S_SKIPINTRO, S_DISC, S_BACK, S_COUNT };
static int g_page = PAGE_LIST;
static int g_srow;      /* highlighted settings row */
static int g_sscroll;   /* how far the settings rows are scrolled up, in pixels */

/* Background cross-fade: g_bg_from is fully shown, g_bg_to fades in over it. */
static int g_bg_from, g_bg_to;
static int g_fade = 255;    /* 0..255 */

/* XInput is loaded at run time so the hub starts on systems without it. */
typedef struct { WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; } PadState;
typedef struct { DWORD packet; PadState pad; } XState;
typedef DWORD (WINAPI *XGetState)(DWORD, XState*);
static XGetState g_xget;
static WORD g_pad_prev;

/* The few GDI+ flat-API calls used to decode and scale the cover PNGs. */
typedef struct { UINT32 version; void* debug_cb; BOOL no_thread; BOOL no_codecs; } GdipStartupInput;
int WINAPI GdiplusStartup(ULONG_PTR* token, const GdipStartupInput* input, void* output);
int WINAPI GdipLoadImageFromFile(const WCHAR* file, void** image);
int WINAPI GdipDisposeImage(void* image);
int WINAPI GdipCreateFromHDC(HDC dc, void** graphics);
int WINAPI GdipDeleteGraphics(void* graphics);
int WINAPI GdipSetInterpolationMode(void* graphics, int mode);
int WINAPI GdipDrawImageRectI(void* graphics, void* image, INT x, INT y, INT w, INT h);

static HBITMAP load_cover(const wchar_t* id) {
    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%ls\\covers\\%ls.png", g_dir, id);
    void* image = NULL;
    if (GdipLoadImageFromFile(path, &image) != 0 || !image) return NULL;

    HDC screen = GetDC(NULL);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, CLIENT_W, CLIENT_H);
    HGDIOBJ old = SelectObject(mem, bmp);
    void* g = NULL;
    if (GdipCreateFromHDC(mem, &g) == 0 && g) {
        GdipSetInterpolationMode(g, 7);     /* high-quality bicubic */
        GdipDrawImageRectI(g, image, 0, 0, CLIENT_W, CLIENT_H);
        GdipDeleteGraphics(g);
    }
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
    GdipDisposeImage(image);
    return bmp;
}

/* --- settings.ini ----------------------------------------------------------- */

static void load_options(Game* g) {
    Options* o = &g->o;
    o->wide = GetPrivateProfileIntW(g->id, L"widescreen", 0, g_ini) != 0;
    o->hud43 = GetPrivateProfileIntW(g->id, L"hud43", 0, g_ini) != 0;
    o->menus43 = GetPrivateProfileIntW(g->id, L"menus43", 0, g_ini) != 0;
    o->sixty = GetPrivateProfileIntW(g->id, L"60fps", 0, g_ini) != 0;
    o->scale = GetPrivateProfileIntW(g->id, L"scale", 1, g_ini);
    if (o->scale < 1) o->scale = 1;
    if (o->scale > 4) o->scale = 4;
    o->truecolor = GetPrivateProfileIntW(g->id, L"truecolor", 0, g_ini) != 0;
    o->dither = GetPrivateProfileIntW(g->id, L"dither", 1, g_ini) != 0;   /* on: the game turns the PS2's dither on */
    o->look = GetPrivateProfileIntW(g->id, L"look", 0, g_ini);
    o->smooth = GetPrivateProfileIntW(g->id, L"smoothtextures", 0, g_ini) != 0;
    if (o->look < 0 || o->look > LOOK_FSR) o->look = 0;
    o->texfilter = GetPrivateProfileIntW(g->id, L"texfilter", 0, g_ini);
    if (o->texfilter < 0 || o->texfilter > 2) o->texfilter = 0;
    o->pacing = GetPrivateProfileIntW(g->id, L"framepacing", 0, g_ini) != 0;
    o->vsync = GetPrivateProfileIntW(g->id, L"vsync", 0, g_ini) != 0;
    o->fullscreen = GetPrivateProfileIntW(g->id, L"fullscreen", 0, g_ini) != 0;
    o->skipintro = GetPrivateProfileIntW(g->id, L"skipintro", 0, g_ini) != 0;
    GetPrivateProfileStringW(g->id, L"disc", L"", g->disc, MAX_PATH, g_ini);
}

static void save_options(const Game* g) {
    const Options* o = &g->o;
    struct { const wchar_t* key; int v; } kv[] = {
        { L"widescreen", o->wide }, { L"hud43", o->hud43 }, { L"menus43", o->menus43 }, { L"60fps", o->sixty },
        { L"scale", o->scale }, { L"look", o->look }, { L"texfilter", o->texfilter }, { L"smoothtextures", o->smooth }, { L"truecolor", o->truecolor }, { L"dither", o->dither },
        { L"framepacing", o->pacing }, { L"vsync", o->vsync },
        { L"fullscreen", o->fullscreen }, { L"skipintro", o->skipintro },
    };
    for (size_t i = 0; i < sizeof kv / sizeof kv[0]; i++) {
        wchar_t v[16];
        swprintf(v, 16, L"%d", kv[i].v);
        WritePrivateProfileStringW(g->id, kv[i].key, v, g_ini);
    }
}

/* --- games.txt -------------------------------------------------------------- */

static int is_dir(const wchar_t* p) {
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static int exists(const wchar_t* p) {
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}

/* Split off the next '|' field; returns the rest, or NULL at the end. */
static wchar_t* next_field(wchar_t* s) {
    wchar_t* bar = wcschr(s, L'|');
    if (!bar) return NULL;
    *bar = 0;
    return bar + 1;
}

static void load_games(void) {
    wchar_t path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%ls\\games.txt", g_dir);
    FILE* f = _wfopen(path, L"rt, ccs=UTF-8");
    if (!f) return;
    wchar_t line[1024];
    while (g_count < MAX_GAMES && fgetws(line, 1024, f)) {
        size_t n = wcslen(line);
        while (n && (line[n - 1] == L'\n' || line[n - 1] == L'\r')) line[--n] = 0;
        if (!n || line[0] == L'#') continue;
        wchar_t* sub = next_field(line);
        wchar_t* exe = sub ? next_field(sub) : NULL;
        wchar_t* cover = exe ? next_field(exe) : NULL;
        wchar_t* id = cover ? next_field(cover) : NULL;
        wchar_t* elf = id ? next_field(id) : NULL;
        if (!elf || !*id || !*elf) continue;

        Game* g = &g_games[g_count];
        memset(g, 0, sizeof *g);
        wcsncpy(g->title, line, 95);
        wcsncpy(g->subtitle, sub, 127);
        wcsncpy(g->cover_name, cover, 95);
        wcsncpy(g->id, id, 31);
        wcsncpy(g->elf, elf, 63);

        wchar_t joined[MAX_PATH];
        swprintf(joined, MAX_PATH, L"%ls\\%ls", g_dir, exe);
        for (wchar_t* c = joined; *c; c++) if (*c == L'/') *c = L'\\';
        if (!GetFullPathNameW(joined, MAX_PATH, g->exe, NULL)) continue;
        /* The toolkit folder is everything before \build\ in the path. */
        wchar_t* build = wcsstr(g->exe, L"\\build\\");
        if (!build) continue;
        size_t tl = (size_t)(build - g->exe);
        wcsncpy(g->tool, g->exe, tl);
        g->tool[tl] = 0;
        wcsncpy(g->exe_rel, build + 1, MAX_PATH - 1);
        swprintf(joined, MAX_PATH, L"%ls\\..\\%ls-options", g->tool, g->id);
        GetFullPathNameW(joined, MAX_PATH, g->opts, NULL);

        g->available = exists(g->exe);
        g->toggles = is_dir(g->opts);
        swprintf(joined, MAX_PATH, L"%ls\\menus43.txt", g->opts);
        g->menus = exists(joined);
        load_options(g);
        g->cover = load_cover(g->id);
        g_count++;
    }
    fclose(f);
}

/* --- cover download ----------------------------------------------------------
 * Runs on its own thread so a slow or absent connection never holds the menu
 * up; each finished cover is handed to the window with WM_COVER_READY. */
#define WM_COVER_READY (WM_APP + 1)
HRESULT WINAPI URLDownloadToFileW(void* caller, const WCHAR* url, const WCHAR* file,
                                  DWORD reserved, void* callback);

static DWORD WINAPI fetch_covers(LPVOID param) {
    HWND hwnd = (HWND)param;
    wchar_t dir[MAX_PATH];
    swprintf(dir, MAX_PATH, L"%ls\\covers", g_dir);
    CreateDirectoryW(dir, NULL);
    for (int i = 0; i < g_count; i++) {
        Game* g = &g_games[i];
        if (g->cover || !g->cover_name[0]) continue;
        wchar_t url[512] = L"https://thumbnails.libretro.com/Sony%20-%20PlayStation%202/Named_Boxarts/";
        size_t n = wcslen(url);
        for (const wchar_t* c = g->cover_name; *c && n < 500; c++) {
            if (*c == L' ') { wcscpy(url + n, L"%20"); n += 3; }
            else url[n++] = *c;
        }
        wcscpy(url + n, L".png");
        wchar_t file[MAX_PATH], part[MAX_PATH];
        swprintf(file, MAX_PATH, L"%ls\\%ls.png", dir, g->id);
        swprintf(part, MAX_PATH, L"%ls.part", file);
        if (URLDownloadToFileW(NULL, url, part, 0, NULL) == 0 &&
            MoveFileExW(part, file, MOVEFILE_REPLACE_EXISTING))
            PostMessageW(hwnd, WM_COVER_READY, (WPARAM)i, 0);
        else
            DeleteFileW(part);
    }
    return 0;
}

/* --- starting a game ---------------------------------------------------------
 * prepare() does everything mc2.cmd does before it starts ps2run: it writes
 * boot.txt, sets the PS2_* variables in this process (the game inherits them;
 * the ones that are off are removed, as mc2.cmd's empty "set" does), and
 * builds the command line. Returns 0 with a message in err on failure. */
typedef struct {
    wchar_t cmd[2048];
    wchar_t name[160];      /* " widescreen 60fps ..." or " original", as mc2.cmd */
    wchar_t title[256];
    wchar_t boot[MAX_PATH];
    char boot_text[128];
} Launch;

/* The player's disc image: a standard Open dialog, the choice kept in settings.ini. Returns 0 when cancelled. ps2run reads
 * plain ISO images (2048-byte sectors); a CHD or BIN/CUE has to be turned into one first (see README). */
static int choose_disc(HWND hwnd, Game* g) {
    wchar_t file[MAX_PATH];
    wcsncpy(file, g->disc, MAX_PATH - 1);
    file[MAX_PATH - 1] = 0;
    wchar_t title[160];
    swprintf(title, 160, L"Choose your %ls disc image", g->title);
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"PS2 disc image (*.iso)\0*.iso\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return 0;
    wcsncpy(g->disc, file, MAX_PATH - 1);
    g->disc[MAX_PATH - 1] = 0;
    WritePrivateProfileStringW(g->id, L"disc", g->disc, g_ini);
    return 1;
}

static int prepare(const Game* g, Launch* L, wchar_t* err, size_t errn) {
    const Options* o = &g->o;
    int wide = g->toggles && o->wide;
    int sixty = g->toggles && o->sixty;
    int hud43 = wide && o->hud43;   /* the 4:3 HUD only does something with widescreen */
    int menus43 = wide && g->menus && o->menus43;
    int dither = o->dither && !o->truecolor;   /* true colour shows the 32-bit picture, which has no dither */

    wchar_t gd[96];
    swprintf(gd, 96, L"games\\%ls", g->id);
    wchar_t p[MAX_PATH];
    if (!g->disc[0] || !exists(g->disc)) {
        swprintf(err, errn, L"No disc image chosen (or it has moved): press Play to choose it.");
        return 0;
    }
    swprintf(p, MAX_PATH, L"%ls\\%ls\\patches.txt", g->tool, gd);
    if (!exists(p)) {
        swprintf(err, errn, L"%ls\\patches.txt is missing: rebuild the game with port.sh.", gd);
        return 0;
    }

    L->name[0] = 0;
    if (wide) wcscat(L->name, L" widescreen");
    if (hud43) wcscat(L->name, L" hud43");
    if (menus43) wcscat(L->name, L" menus43");
    if (sixty) wcscat(L->name, L" 60fps");
    if (o->scale != 1) { wchar_t s[8]; swprintf(s, 8, L" %dx", o->scale); wcscat(L->name, s); }
    if (o->look) { wcscat(L->name, L" "); wcscat(L->name, k_look_words[o->look]); }
    if (o->texfilter) { wcscat(L->name, L" "); wcscat(L->name, k_filter_words[o->texfilter]); }
    if (o->smooth) wcscat(L->name, L" smoothtextures");
    if (o->truecolor) wcscat(L->name, L" truecolor");
    if (!o->truecolor && !o->dither) wcscat(L->name, L" nodither");
    if (o->pacing) wcscat(L->name, L" framepacing");
    if (o->vsync) wcscat(L->name, L" vsync");
    if (o->skipintro) wcscat(L->name, L" skipintro");
    if (!L->name[0]) wcscpy(L->name, L" original");
    swprintf(L->title, 256, L"%ls (%ls)", g->title, L->name + 1);

    /* The option words the build checks, written into memory at boot. Same
     * bytes as mc2.cmd's echo lines (CRLF). */
    L->boot[0] = 0;
    L->boot_text[0] = 0;
    if (g->toggles) {
        swprintf(L->boot, MAX_PATH, L"%ls\\boot.txt", g->opts);
        snprintf(L->boot_text, sizeof L->boot_text,
                 "0 000FFF00 2 %d\r\n0 000FFF02 2 %d\r\n0 000FFF04 2 %d\r\n", wide, sixty, hud43);
        HANDLE h = CreateFileW(L->boot, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        DWORD wrote = 0;
        int ok = h != INVALID_HANDLE_VALUE &&
                 WriteFile(h, L->boot_text, (DWORD)strlen(L->boot_text), &wrote, NULL) &&
                 wrote == strlen(L->boot_text);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        if (!ok) {
            swprintf(err, errn, L"Could not write %ls.", L->boot);
            return 0;
        }
    }

    wchar_t scale[8];
    swprintf(scale, 8, L"%d", o->scale);
    SetEnvironmentVariableW(L"PS2_SCALE", scale);
    SetEnvironmentVariableW(L"PS2_TRUE_COLOR", o->truecolor ? L"1" : NULL);
    SetEnvironmentVariableW(L"PS2_DITHER", dither ? L"1" : NULL);
    SetEnvironmentVariableW(L"PS2_LOOK", k_look_words[o->look == LOOK_FSR ? 0 : o->look]);
    SetEnvironmentVariableW(L"PS2_FSR", o->look == LOOK_FSR ? L"1" : NULL);
    SetEnvironmentVariableW(L"PS2_TEX_FILTER", o->texfilter ? k_filter_words[o->texfilter] : NULL);
    SetEnvironmentVariableW(L"PS2_TEX_SS", o->smooth ? L"1" : NULL);
    SetEnvironmentVariableW(L"PS2_PACE", o->pacing ? L"clock" : NULL);
    SetEnvironmentVariableW(L"PS2_PRESENT_MODE", o->vsync ? L"fifo" : NULL);
    SetEnvironmentVariableW(L"PS2_WINDOW_TITLE", L->title);
    /* the game window shows this launcher's icon (the runtime takes it out of this program) */
    wchar_t self[MAX_PATH];
    if (GetModuleFileNameW(NULL, self, MAX_PATH)) SetEnvironmentVariableW(L"PS2_WINDOW_ICON", self);
    /* 4:3 menus and videos: <id>-options/menus43.txt holds the runtime's NAME=VALUE lines
     * (PS2_WIDE_ONLY_IF, PS2_WIDE_WORD) that tell a race from a menu. */
    SetEnvironmentVariableW(L"PS2_WIDE_ONLY_IF", NULL);
    SetEnvironmentVariableW(L"PS2_WIDE_WORD", NULL);
    SetEnvironmentVariableW(L"PS2_SKIP_TEXTURE", NULL);
    SetEnvironmentVariableW(L"PS2_SKIP_SUBTRACT", NULL);  /* (Remove Car Outlines and its nooutline.txt were taken out 2026-11-08) */
    if (menus43) {
        wchar_t mp[MAX_PATH], line[256];
        swprintf(mp, MAX_PATH, L"%ls\\menus43.txt", g->opts);
        FILE* mf = _wfopen(mp, L"rt, ccs=UTF-8");
        while (mf && fgetws(line, 256, mf)) {
            size_t ln = wcslen(line);
            while (ln && (line[ln - 1] == L'\n' || line[ln - 1] == L'\r' || line[ln - 1] == L' ')) line[--ln] = 0;
            wchar_t* eq = wcschr(line, L'=');
            if (!ln || line[0] == L'#' || !eq) continue;
            *eq = 0;
            SetEnvironmentVariableW(line, eq + 1);
        }
        if (mf) fclose(mf);
    }

    int n = swprintf(L->cmd, 2048,
        L"\"%ls\" --disc \"%ls\" %ls --game-dir \"%ls\" --memcard \"%ls\\%ls.ps2\" --patches \"%ls\\patches.txt\"",
        g->exe_rel, g->disc, o->fullscreen ? L"--fullscreen" : L"--window", gd, gd, g->id, gd);
    if (g->toggles) n += swprintf(L->cmd + n, 2048 - n, L" --patches \"%ls\"", L->boot);
    if (wide) n += swprintf(L->cmd + n, 2048 - n, L" --widescreen");
    /* Skip Intro: the game's own developer switch (-skipintro, read at 0x1003A4): it starts at the front end, without the
     * Rockstar and studio logos, the intro movie and the legal screen. --arg hands it to the game as argv[1]. */
    if (o->skipintro) swprintf(L->cmd + n, 2048 - n, L" --arg -skipintro");
    return 1;
}

static void launch(HWND hwnd) {
    if (g_child || g_sel < 0 || g_sel >= g_count) return;
    Game* g = &g_games[g_sel];
    if (!g->available) { MessageBeep(MB_ICONWARNING); return; }
    if ((!g->disc[0] || !exists(g->disc)) && !choose_disc(hwnd, g)) return;
    Launch L;
    wchar_t err[512];
    if (!prepare(g, &L, err, 512)) {
        MessageBoxW(hwnd, err, g->title, MB_ICONERROR);
        return;
    }
    STARTUPINFOW si = { 0 };
    si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(g->exe, L.cmd, NULL, NULL, FALSE, 0, NULL, g->tool, &si, &pi)) {
        MessageBoxW(hwnd, L"Could not start the game.", g->title, MB_ICONERROR);
        return;
    }
    CloseHandle(pi.hThread);
    g_child = pi.hProcess;
    ShowWindow(hwnd, SW_HIDE);
}

/* "--check [id]": print what Play would start, like "mc2.cmd check", and
 * write boot.txt, but start nothing. Output goes to stdout when it is
 * redirected, else to the console the hub was started from. */
static void out(const wchar_t* fmt, ...) {
    static HANDLE h;
    if (!h) {
        h = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!h || h == INVALID_HANDLE_VALUE) {
            if (AttachConsole(ATTACH_PARENT_PROCESS))
                h = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        }
    }
    wchar_t w[4096];
    va_list ap;
    va_start(ap, fmt);
    vswprintf(w, 4096, fmt, ap);
    va_end(ap);
    char u[8192];
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, u, sizeof u, NULL, NULL);
    DWORD wrote;
    if (h && h != INVALID_HANDLE_VALUE && n > 1) WriteFile(h, u, (DWORD)(n - 1), &wrote, NULL);
}

static int check(const wchar_t* id) {
    for (int i = 0; i < g_count; i++) {
        Game* g = &g_games[i];
        if (id && *id && _wcsicmp(id, g->id) != 0) continue;
        Launch L;
        wchar_t err[512];
        out(L"%ls:%ls\n", g->title, L"");
        if (!prepare(g, &L, err, 512)) { out(L"error: %ls\n", err); return 1; }
        out(L"name=%ls\n", L.name);
        out(L"cwd=%ls\nexe=%ls\ncmd=%ls\n", g->tool, g->exe, L.cmd);
        static const wchar_t* vars[] = { L"PS2_SCALE", L"PS2_TRUE_COLOR", L"PS2_PACE",
                                         L"PS2_PRESENT_MODE", L"PS2_WINDOW_TITLE", L"PS2_WIDE_ONLY_IF", L"PS2_WIDE_WORD", L"PS2_DITHER", L"PS2_LOOK", L"PS2_TEX_SS", L"PS2_SKIP_TEXTURE", L"PS2_SKIP_SUBTRACT", L"PS2_TEX_FILTER", L"PS2_FSR", L"PS2_WINDOW_ICON" };
        for (int v = 0; v < (int)(sizeof vars / sizeof vars[0]); v++) {
            wchar_t val[512];
            if (GetEnvironmentVariableW(vars[v], val, 512)) out(L"%ls=%ls\n", vars[v], val);
            else out(L"%ls is not set\n", vars[v]);
        }
        if (L.boot[0]) {
            out(L"boot.txt=%ls\n", L.boot);
            wchar_t bt[128];
            MultiByteToWideChar(CP_UTF8, 0, L.boot_text, -1, bt, 128);
            out(L"%ls", bt);
        }
        return 0;
    }
    out(L"no game %ls in games.txt\n", id ? id : L"");
    return 1;
}

/* --- list page -------------------------------------------------------------- */

/* Highlight a row and start fading the background to its cover. */
static void select_row(HWND hwnd, int row) {
    if (row < 0 || row >= g_count) return;
    if (row != g_bg_to) {
        /* Whatever is showing now becomes the base of the new fade. */
        if (g_fade >= 128) g_bg_from = g_bg_to;
        g_bg_to = row;
        g_fade = 0;
    }
    g_sel = row;
    InvalidateRect(hwnd, NULL, FALSE);
}

static void move_sel(HWND hwnd, int d) {
    if (g_count) select_row(hwnd, (g_sel + d + g_count) % g_count);
}

static void set_col(HWND hwnd, int col) {
    g_col = col;
    InvalidateRect(hwnd, NULL, FALSE);
}

static int list_top(void) {
    return CLIENT_H - LIST_BOTTOM - g_count * (ROW_H + ROW_GAP);
}

static RECT row_rect(int row) {
    RECT r;
    r.left = MARGIN;
    r.right = CLIENT_W - MARGIN;
    r.top = list_top() + row * (ROW_H + ROW_GAP);
    r.bottom = r.top + ROW_H;
    return r;
}

static RECT button_rect(int row, int col) {
    RECT rr = row_rect(row), r;
    r.right = rr.right - 12 - (col == COL_PLAY ? BTN_W + BTN_GAP : 0);
    r.left = r.right - BTN_W;
    r.top = rr.top + 11;
    r.bottom = rr.bottom - 11;
    return r;
}

/* --- settings page ---------------------------------------------------------- */

static Game* sgame(void) { return &g_games[g_sel]; }

static int srow_enabled(int row) {
    const Game* g = sgame();
    if (row == S_WIDE || row == S_SIXTY) return g->toggles;
    if (row == S_HUD43) return g->toggles && g->o.wide;
    if (row == S_DITHER) return !g->o.truecolor;
    if (row == S_MENUS43) return g->menus && g->toggles && g->o.wide;
    return 1;
}

static RECT srow_rect(int row) {
    RECT r;
    r.left = MARGIN;
    r.right = CLIENT_W - MARGIN;
    r.top = S_TOP + row * (S_ROW_H + S_ROW_GAP) - g_sscroll;
    r.bottom = r.top + S_ROW_H;
    if (row == S_BACK) {    /* a button, not a full row */
        r.top += 10;
        r.left = (CLIENT_W - 160) / 2;
        r.right = r.left + 160;
    }
    return r;
}

static RECT sval_rect(int row) {
    RECT rr = srow_rect(row), r;
    r.right = rr.right - 12;
    r.left = r.right - (row == S_LOOK || row == S_TEXFILTER ? S_LOOK_W : row == S_DISC ? S_DISC_W : S_VAL_W);
    r.top = rr.top + 15;
    r.bottom = rr.bottom - 15;
    return r;
}

/* Rows with a row of boxes, one per value (like the resolution's 1x..4x): how many, their text, which one is on. */
static int sseg_count(int row) { return row == S_SCALE ? 4 : row == S_MENUS43 || row == S_TRUECOLOR ? 2 : 0; }
static int sseg_w(int row) { return row == S_SCALE ? S_SEG_W : S_SEG2_W; }

static const wchar_t* sseg_text(int row, int k) {
    static const wchar_t* scale[4] = { L"1x", L"2x", L"3x", L"4x" };
    if (row == S_SCALE) return scale[k];
    if (row == S_MENUS43) return k ? L"16:9" : L"4:3";
    return k ? L"32-bit" : L"16-bit";
}

static int sseg_value(const Options* o, int row) {
    if (row == S_SCALE) return o->scale - 1;
    if (row == S_MENUS43) return !o->menus43;   /* 4:3 menus and movies = the 4:3 Menus option on */
    return o->truecolor;
}

static void sseg_set(Options* o, int row, int k) {
    if (row == S_SCALE) o->scale = k + 1;
    else if (row == S_MENUS43) o->menus43 = !k;
    else o->truecolor = k;
}

static RECT sseg_rect(int row, int k) {   /* k 0..sseg_count(row)-1, left to right */
    RECT rr = srow_rect(row), r;
    int n = sseg_count(row), w = sseg_w(row);
    int right = rr.right - 12;
    r.left = right - (n - k) * w - (n - 1 - k) * S_SEG_GAP;
    r.right = r.left + w;
    r.top = rr.top + 15;
    r.bottom = rr.bottom - 15;
    return r;
}

/* Scrolling: the most the rows can scroll, and keeping the highlighted row in view. */
static int smax_scroll(void) {
    int bottom = S_TOP + S_BACK * (S_ROW_H + S_ROW_GAP) + S_ROW_H + 8;
    return bottom > S_VIEW_BOTTOM ? bottom - S_VIEW_BOTTOM : 0;
}

static void sscroll_to(int y) {
    g_sscroll = y < 0 ? 0 : y > smax_scroll() ? smax_scroll() : y;
}

static void sshow_row(void) {
    RECT r = srow_rect(g_srow);
    if (r.top < S_VIEW_TOP + 6) sscroll_to(g_sscroll - (S_VIEW_TOP + 6 - r.top));
    else if (r.bottom > S_VIEW_BOTTOM - 6) sscroll_to(g_sscroll + r.bottom - (S_VIEW_BOTTOM - 6));
}

static void open_settings(HWND hwnd) {
    if (g_sel < 0 || g_sel >= g_count) return;
    g_page = PAGE_SETTINGS;
    g_srow = 0;
    while (g_srow < S_BACK && !srow_enabled(g_srow)) g_srow++;
    g_sscroll = 0;
    sshow_row();
    InvalidateRect(hwnd, NULL, FALSE);
}

static void close_settings(HWND hwnd) {
    g_page = PAGE_LIST;
    g_col = COL_SETTINGS;
    InvalidateRect(hwnd, NULL, FALSE);
}

static void smove(HWND hwnd, int d) {
    int r = g_srow;
    for (int i = 0; i < S_COUNT; i++) {
        r = (r + d + S_COUNT) % S_COUNT;
        if (srow_enabled(r)) break;
    }
    g_srow = r;
    sshow_row();
    InvalidateRect(hwnd, NULL, FALSE);
}

/* Change the highlighted setting: d = -1 / +1 for left / right, 0 to toggle
 * (cycles the resolution). Saved at once. */
static void schange(HWND hwnd, int d) {
    if (g_srow == S_BACK) { if (d == 0) close_settings(hwnd); return; }
    if (g_srow == S_DISC) {
        if (d == 0 && choose_disc(hwnd, sgame())) InvalidateRect(hwnd, NULL, FALSE);
        return;
    }
    if (!srow_enabled(g_srow)) { MessageBeep(MB_ICONWARNING); return; }
    Game* g = sgame();
    Options* o = &g->o;
    int* t = NULL;
    switch (g_srow) {
    case S_WIDE: t = &o->wide; break;
    case S_HUD43: t = &o->hud43; break;
    case S_SIXTY: t = &o->sixty; break;
    case S_SMOOTH: t = &o->smooth; break;
    case S_DITHER: t = &o->dither; break;
    case S_PACING: t = &o->pacing; break;
    case S_VSYNC: t = &o->vsync; break;
    case S_FULLSCREEN: t = &o->fullscreen; break;
    case S_SKIPINTRO: t = &o->skipintro; break;
    case S_LOOK:
        o->look = (o->look + (d < 0 ? LOOK_FSR : 1)) % (LOOK_FSR + 1);
        break;
    case S_TEXFILTER:
        o->texfilter = (o->texfilter + (d < 0 ? 2 : 1)) % 3;
        break;
    case S_SCALE:
    case S_MENUS43:
    case S_TRUECOLOR: {    /* Enter: the next box (round again after the last); left / right: the box beside it */
        int n = sseg_count(g_srow), k = sseg_value(o, g_srow);
        if (d == 0) k = (k + 1) % n;
        else if (k + d >= 0 && k + d < n) k += d;
        sseg_set(o, g_srow, k);
        break;
    }
    }
    if (t) *t = d == 0 ? !*t : d > 0;
    save_options(g);
    InvalidateRect(hwnd, NULL, FALSE);
}

static int srow_at(int x, int y) {
    POINT pt = { x, y };
    if (y < S_VIEW_TOP || y >= S_VIEW_BOTTOM) return -1;
    for (int i = 0; i < S_COUNT; i++) {
        RECT r = srow_rect(i);
        if (PtInRect(&r, pt)) return i;
    }
    return -1;
}

/* --- drawing ---------------------------------------------------------------- */

/* Fill a rectangle with a colour at the given opacity (0..255). */
static void fill_alpha(HDC dc, RECT r, COLORREF c, int alpha) {
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, 1, 1);
    HGDIOBJ old = SelectObject(mem, bmp);
    SetPixel(mem, 0, 0, c);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)alpha, 0 };
    AlphaBlend(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, mem, 0, 0, 1, 1, bf);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static void frame_rect(HDC dc, RECT r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FrameRect(dc, &r, b);
    DeleteObject(b);
}

static void blit_cover(HDC dc, int index, int alpha) {
    if (index < 0 || index >= g_count || !g_games[index].cover) return;
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, g_games[index].cover);
    if (alpha >= 255) {
        BitBlt(dc, 0, 0, CLIENT_W, CLIENT_H, mem, 0, 0, SRCCOPY);
    } else {
        BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)alpha, 0 };
        AlphaBlend(dc, 0, 0, CLIENT_W, CLIENT_H, mem, 0, 0, CLIENT_W, CLIENT_H, bf);
    }
    SelectObject(mem, old);
    DeleteDC(mem);
}

static void draw_button(HDC dc, RECT r, const wchar_t* text, int focused, int enabled) {
    fill_alpha(dc, r, focused && enabled ? ACCENT : RGB(0, 0, 0), focused && enabled ? 255 : 100);
    frame_rect(dc, r, enabled ? ACCENT : ACCENT_DIM);
    SetTextColor(dc, enabled ? WHITE : GREY);
    DrawTextW(dc, text, -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}

/* An ON/OFF or 1x..4x box: filled when it is the current value. */
static void draw_value(HDC dc, RECT r, const wchar_t* text, int on, int enabled) {
    fill_alpha(dc, r, on && enabled ? ACCENT : RGB(0, 0, 0), on && enabled ? 255 : 110);
    frame_rect(dc, r, enabled ? (on ? WHITE : ACCENT) : ACCENT_DIM);
    SetTextColor(dc, enabled ? WHITE : DARK_GREY);
    DrawTextW(dc, text, -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
}

static void paint_list(HDC mem) {
    RECT rc = { 0, 0, CLIENT_W, CLIENT_H };
    /* Darken the cover: lightly overall, heavily behind the list. */
    fill_alpha(mem, rc, RGB(0, 0, 0), 70);
    RECT panel = { 0, list_top() - 22, CLIENT_W, CLIENT_H };
    fill_alpha(mem, panel, RGB(0, 0, 0), 75);
    RECT rule = { 0, panel.top, CLIENT_W, panel.top + 3 };
    fill_alpha(mem, rule, ACCENT, 255);

    for (int i = 0; i < g_count; i++) {
        RECT r = row_rect(i);
        fill_alpha(mem, r, i == g_sel ? ACCENT_DIM : RGB(0, 0, 0), i == g_sel ? 95 : 75);
        if (i == g_sel) {
            RECT bar = { r.left, r.top, r.left + 5, r.bottom };
            fill_alpha(mem, bar, ACCENT, 255);
        }

        /* Column 1: the label, not clickable. */
        int label_right = r.right - 12 - 2 * BTN_W - BTN_GAP - 12;
        RECT a = { r.left + 18, r.top + 4, label_right, r.top + 36 };
        SelectObject(mem, g_font_item);
        SetTextColor(mem, g_games[i].available ? WHITE : GREY);
        DrawTextW(mem, g_games[i].title, -1, &a, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        RECT s = { r.left + 18, r.top + 36, label_right, r.bottom - 4 };
        SelectObject(mem, g_font_small);
        SetTextColor(mem, GREY);
        DrawTextW(mem, g_games[i].available ? g_games[i].subtitle : L"Not built",
                  -1, &s, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        /* Columns 2 and 3: Play and Settings. */
        SelectObject(mem, g_font_btn);
        draw_button(mem, button_rect(i, COL_PLAY), L"PLAY",
                    i == g_sel && g_col == COL_PLAY, g_games[i].available);
        draw_button(mem, button_rect(i, COL_SETTINGS), L"SETTINGS",
                    i == g_sel && g_col == COL_SETTINGS, 1);
    }
    if (!g_count) {
        RECT e = { MARGIN, CLIENT_H / 2 - 20, CLIENT_W - MARGIN, CLIENT_H / 2 + 20 };
        SelectObject(mem, g_font_item);
        SetTextColor(mem, WHITE);
        DrawTextW(mem, L"games.txt is missing or empty.", -1, &e, DT_CENTER | DT_SINGLELINE);
    }

    RECT h = { MARGIN, CLIENT_H - 44, CLIENT_W - MARGIN, CLIENT_H - 14 };
    RECT strip = { 0, CLIENT_H - 50, CLIENT_W, CLIENT_H };
    fill_alpha(mem, strip, RGB(0, 0, 0), 140);
    SelectObject(mem, g_font_small);
    SetTextColor(mem, GREY);
    DrawTextW(mem, L"Arrows to choose   Enter or A to select   Esc or B to quit",
              -1, &h, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
}

static void paint_settings(HDC mem) {
    const Game* g = sgame();
    const Options* o = &g->o;
    RECT rc = { 0, 0, CLIENT_W, CLIENT_H };
    fill_alpha(mem, rc, RGB(0, 0, 0), 185);

    RECT head = { MARGIN, 22, CLIENT_W - MARGIN, 66 };
    SelectObject(mem, g_font_head);
    SetTextColor(mem, WHITE);
    DrawTextW(mem, L"SETTINGS", -1, &head, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    RECT sub = { MARGIN, 66, CLIENT_W - MARGIN, 90 };
    SelectObject(mem, g_font_small);
    SetTextColor(mem, GREY);
    DrawTextW(mem, g->title, -1, &sub, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    RECT rule = { 0, 96, CLIENT_W, 99 };
    fill_alpha(mem, rule, ACCENT, 255);

    static const wchar_t* labels[S_COUNT] = {
        [S_FULLSCREEN] = L"Fullscreen", [S_SCALE] = L"Resolution", [S_VSYNC] = L"Vsync", [S_PACING] = L"Frame Pacing Fix",
        [S_LOOK] = L"Picture Mode", [S_TEXFILTER] = L"Texture Filtering", [S_SIXTY] = L"60 FPS", [S_WIDE] = L"Widescreen", [S_HUD43] = L"Fixed Widescreen HUD",
        [S_MENUS43] = L"Menu/Movie Aspect Ratio", [S_TRUECOLOR] = L"Color Settings", [S_DITHER] = L"Dithering",
        [S_SMOOTH] = L"Smooth Distant Textures", [S_SKIPINTRO] = L"Skip Intro", [S_DISC] = L"Disc Image", [S_BACK] = L"BACK",
    };
    /* Only the patch credits have a line under the name. */
    static const wchar_t* notes[S_COUNT] = { [S_WIDE] = L"Patch by Arapapa", [S_SIXTY] = L"Patch by PeterDelta" };
    int vals[S_COUNT] = {
        [S_FULLSCREEN] = o->fullscreen, [S_VSYNC] = o->vsync, [S_PACING] = o->pacing, [S_SIXTY] = o->sixty, [S_WIDE] = o->wide,
        [S_HUD43] = o->hud43, [S_DITHER] = o->dither, [S_SMOOTH] = o->smooth, [S_SKIPINTRO] = o->skipintro,
    };

    int saved = SaveDC(mem);
    IntersectClipRect(mem, 0, S_VIEW_TOP, CLIENT_W, S_VIEW_BOTTOM);
    for (int i = 0; i < S_COUNT; i++) {
        int en = srow_enabled(i);
        RECT r = srow_rect(i);
        if (r.bottom <= S_VIEW_TOP || r.top >= S_VIEW_BOTTOM) continue;
        if (i == S_BACK) {
            SelectObject(mem, g_font_btn);
            draw_button(mem, r, L"BACK", g_srow == S_BACK, 1);
            continue;
        }
        fill_alpha(mem, r, i == g_srow ? ACCENT_DIM : RGB(0, 0, 0), i == g_srow ? 110 : 80);
        if (i == g_srow) {
            RECT bar = { r.left, r.top, r.left + 5, r.bottom };
            fill_alpha(mem, bar, ACCENT, 255);
        }
        int n = sseg_count(i);
        int text_right = r.right - 12 - (n ? n * sseg_w(i) + (n - 1) * S_SEG_GAP : i == S_LOOK || i == S_TEXFILTER ? S_LOOK_W : i == S_DISC ? S_DISC_W : S_VAL_W) - 10;
        /* the name in the middle of the row, or above the credit line */
        RECT a = { r.left + 18, notes[i] ? r.top + 4 : r.top, text_right, notes[i] ? r.top + 34 : r.bottom };
        SelectObject(mem, g_font_item);
        SetTextColor(mem, en ? WHITE : DARK_GREY);
        DrawTextW(mem, labels[i], -1, &a, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        if (notes[i]) {
            RECT s = { r.left + 18, r.top + 35, text_right, r.bottom - 4 };
            SelectObject(mem, g_font_small);
            SetTextColor(mem, en ? GREY : DARK_GREY);
            DrawTextW(mem, notes[i], -1, &s, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }

        SelectObject(mem, g_font_btn);
        if (n) {
            int v = sseg_value(o, i);
            for (int k = 0; k < n; k++) draw_value(mem, sseg_rect(i, k), sseg_text(i, k), v == k, en);
        } else if (i == S_LOOK) {
            draw_value(mem, sval_rect(i), k_look_names[o->look], 0, 1);
        } else if (i == S_DISC) {
            const wchar_t* name = g->disc[0] ? wcsrchr(g->disc, L'\\') : NULL;
            draw_value(mem, sval_rect(i), name ? name + 1 : L"Choose...", 0, 1);
        } else if (i == S_TEXFILTER) {
            draw_value(mem, sval_rect(i), k_filter_names[o->texfilter], 0, 1);
        } else {
            draw_value(mem, sval_rect(i), vals[i] ? L"ON" : L"OFF", vals[i], en);
        }
    }
    RestoreDC(mem, saved);
    if (smax_scroll() > 0) {    /* scroll bar in the right margin */
        int track = S_VIEW_BOTTOM - S_VIEW_TOP - 8, content = track + smax_scroll();
        int thumb = track * track / content, at = S_VIEW_TOP + 4 + (track - thumb) * g_sscroll / smax_scroll();
        RECT bar = { CLIENT_W - MARGIN / 2 - 2, S_VIEW_TOP + 4, CLIENT_W - MARGIN / 2 + 2, S_VIEW_TOP + 4 + track };
        fill_alpha(mem, bar, RGB(0, 0, 0), 120);
        RECT th = { bar.left, at, bar.right, at + thumb };
        fill_alpha(mem, th, ACCENT, 255);
    }

    RECT h = { MARGIN, CLIENT_H - 44, CLIENT_W - MARGIN, CLIENT_H - 14 };
    RECT strip = { 0, CLIENT_H - 50, CLIENT_W, CLIENT_H };
    fill_alpha(mem, strip, RGB(0, 0, 0), 140);
    SelectObject(mem, g_font_small);
    SetTextColor(mem, GREY);
    DrawTextW(mem, L"Arrows to choose   Enter or A to change   Esc or B back",
              -1, &h, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
}

static void paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, CLIENT_W, CLIENT_H);
    HGDIOBJ old = SelectObject(mem, bmp);
    RECT rc = { 0, 0, CLIENT_W, CLIENT_H };

    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(mem, &rc, black);
    DeleteObject(black);
    blit_cover(mem, g_bg_from, 255);
    blit_cover(mem, g_bg_to, g_fade);
    SetBkMode(mem, TRANSPARENT);

    if (g_page == PAGE_SETTINGS && g_count) paint_settings(mem);
    else paint_list(mem);

    /* The layout is drawn at its design size and stretched to the window. */
    RECT client;
    GetClientRect(hwnd, &client);
    if (client.right == CLIENT_W && client.bottom == CLIENT_H) {
        BitBlt(dc, 0, 0, CLIENT_W, CLIENT_H, mem, 0, 0, SRCCOPY);
    } else {
        SetStretchBltMode(dc, HALFTONE);
        SetBrushOrgEx(dc, 0, 0, NULL);
        StretchBlt(dc, 0, 0, client.right, client.bottom, mem, 0, 0, CLIENT_W, CLIENT_H, SRCCOPY);
    }
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

/* --- input ------------------------------------------------------------------ */

/* Map a point in the (possibly resized) window back to design coordinates. */
static void to_design(HWND hwnd, int* x, int* y) {
    RECT client;
    GetClientRect(hwnd, &client);
    if (client.right > 0 && client.bottom > 0) {
        *x = *x * CLIENT_W / client.right;
        *y = *y * CLIENT_H / client.bottom;
    }
}

/* Keep the client area cover-shaped while the user drags a window edge. */
static void keep_aspect(HWND hwnd, WPARAM edge, RECT* r) {
    RECT frame = { 0, 0, 0, 0 };
    AdjustWindowRect(&frame, (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE), FALSE);
    int extra_w = frame.right - frame.left, extra_h = frame.bottom - frame.top;
    int cw = r->right - r->left - extra_w, ch = r->bottom - r->top - extra_h;
    int side;   /* client height */
    if (edge == WMSZ_LEFT || edge == WMSZ_RIGHT) side = cw * CLIENT_H / CLIENT_W;
    else if (edge == WMSZ_TOP || edge == WMSZ_BOTTOM) side = ch;
    else side = cw * CLIENT_H / CLIENT_W > ch ? cw * CLIENT_H / CLIENT_W : ch;
    if (side < 400) side = 400;
    int w = side * CLIENT_W / CLIENT_H + extra_w, h = side + extra_h;
    if (edge == WMSZ_LEFT || edge == WMSZ_TOPLEFT || edge == WMSZ_BOTTOMLEFT) r->left = r->right - w;
    else r->right = r->left + w;
    if (edge == WMSZ_TOP || edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT) r->top = r->bottom - h;
    else r->bottom = r->top + h;
}

static int row_at(int x, int y) {
    POINT pt = { x, y };
    for (int i = 0; i < g_count; i++) {
        RECT r = row_rect(i);
        if (PtInRect(&r, pt)) return i;
    }
    return -1;
}

/* Which button, if any, is under the point. The label column is not clickable. */
static int button_at(int x, int y, int* row, int* col) {
    POINT pt = { x, y };
    for (int i = 0; i < g_count; i++) {
        for (int c = COL_PLAY; c <= COL_SETTINGS; c++) {
            RECT r = button_rect(i, c);
            if (PtInRect(&r, pt)) { *row = i; *col = c; return 1; }
        }
    }
    return 0;
}

/* Enter on the list: Play starts the game, Settings opens its page. */
static void activate(HWND hwnd) {
    if (g_col == COL_SETTINGS) open_settings(hwnd);
    else launch(hwnd);
}

static void key(HWND hwnd, WPARAM vk) {
    if (g_page == PAGE_SETTINGS) {
        if (vk == VK_UP) smove(hwnd, -1);
        else if (vk == VK_DOWN || vk == VK_TAB) smove(hwnd, 1);
        else if (vk == VK_LEFT) schange(hwnd, -1);
        else if (vk == VK_RIGHT) schange(hwnd, 1);
        else if (vk == VK_RETURN || vk == VK_SPACE) schange(hwnd, 0);
        else if (vk == VK_ESCAPE || vk == VK_BACK) close_settings(hwnd);
        return;
    }
    if (vk == VK_UP) move_sel(hwnd, -1);
    else if (vk == VK_DOWN) move_sel(hwnd, 1);
    else if (vk == VK_LEFT) set_col(hwnd, COL_PLAY);
    else if (vk == VK_RIGHT) set_col(hwnd, COL_SETTINGS);
    else if (vk == VK_TAB) set_col(hwnd, !g_col);
    else if (vk == VK_RETURN || vk == VK_SPACE) activate(hwnd);
    else if (vk == VK_ESCAPE) DestroyWindow(hwnd);
}

static void tick(HWND hwnd) {
    if (g_child) {
        if (WaitForSingleObject(g_child, 0) == WAIT_OBJECT_0) {
            CloseHandle(g_child);
            g_child = NULL;
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        }
        return;
    }
    if (g_fade < 255) {
        g_fade += 255 * TICK_MS / FADE_MS;
        if (g_fade >= 255) { g_fade = 255; g_bg_from = g_bg_to; }
        InvalidateRect(hwnd, NULL, FALSE);
    }
    if (!g_xget || GetForegroundWindow() != hwnd) return;
    WORD now = 0;
    for (DWORD i = 0; i < 4; i++) {
        XState st;
        if (g_xget(i, &st) == ERROR_SUCCESS) now |= st.pad.buttons;
    }
    WORD pressed = now & ~g_pad_prev;
    g_pad_prev = now;
    if (pressed & 0x0001) key(hwnd, VK_UP);       /* D-pad up */
    if (pressed & 0x0002) key(hwnd, VK_DOWN);     /* D-pad down */
    if (pressed & 0x0004) key(hwnd, VK_LEFT);     /* D-pad left */
    if (pressed & 0x0008) key(hwnd, VK_RIGHT);    /* D-pad right */
    if (pressed & 0x1000) key(hwnd, VK_RETURN);   /* A */
    if (pressed & 0x2000) key(hwnd, VK_ESCAPE);   /* B: back, or quit on the list */
}

static void mouse_move(HWND hwnd, int x, int y) {
    if (g_page == PAGE_SETTINGS) {
        int r = srow_at(x, y);
        if (r >= 0 && r != g_srow && srow_enabled(r)) {
            g_srow = r;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return;
    }
    int row = row_at(x, y), brow, col;
    if (row >= 0 && row != g_sel) select_row(hwnd, row);
    if (button_at(x, y, &brow, &col) && col != g_col) set_col(hwnd, col);
}

static void mouse_click(HWND hwnd, int x, int y) {
    if (g_page == PAGE_SETTINGS) {
        int r = srow_at(x, y);
        if (r < 0) return;
        g_srow = r;
        if (sseg_count(r) && srow_enabled(r)) {
            POINT pt = { x, y };
            for (int k = 0; k < sseg_count(r); k++) {
                RECT s = sseg_rect(r, k);
                if (PtInRect(&s, pt)) {
                    sseg_set(&sgame()->o, r, k);
                    save_options(sgame());
                    InvalidateRect(hwnd, NULL, FALSE);
                    return;
                }
            }
        }
        schange(hwnd, 0);
        return;
    }
    int row, col;
    if (button_at(x, y, &row, &col)) {
        select_row(hwnd, row);
        g_col = col;
        activate(hwnd);
    }
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: paint(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_TIMER: tick(hwnd); return 0;
    case WM_KEYDOWN: key(hwnd, wp); return 0;
    case WM_SIZING: keep_aspect(hwnd, wp, (RECT*)lp); return TRUE;
    case WM_SIZE: InvalidateRect(hwnd, NULL, FALSE); return 0;
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        to_design(hwnd, &x, &y);
        mouse_move(hwnd, x, y);
        return 0;
    }
    case WM_MOUSEWHEEL:
        if (g_page == PAGE_SETTINGS) {
            sscroll_to(g_sscroll - (short)HIWORD(wp) * S_WHEEL_STEP / WHEEL_DELTA);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        to_design(hwnd, &x, &y);
        mouse_click(hwnd, x, y);
        return 0;
    }
    case WM_COVER_READY:
        if (wp < (WPARAM)g_count && !g_games[wp].cover) {
            g_games[wp].cover = load_cover(g_games[wp].id);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdline, int show) {
    (void)prev;
    GetModuleFileNameW(NULL, g_dir, MAX_PATH);
    wchar_t* slash = wcsrchr(g_dir, L'\\');
    if (slash) *slash = 0;
    swprintf(g_ini, MAX_PATH, L"%ls\\settings.ini", g_dir);

    ULONG_PTR gdip_token = 0;
    GdipStartupInput gdip_in = { 1, NULL, FALSE, FALSE };
    GdiplusStartup(&gdip_token, &gdip_in, NULL);
    load_games();

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc >= 2 && _wcsicmp(argv[1], L"--check") == 0)
        return check(argc >= 3 ? argv[2] : NULL);
    (void)cmdline;

    static const wchar_t* xinput_dlls[] = { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" };
    for (int i = 0; i < 3 && !g_xget; i++) {
        HMODULE m = LoadLibraryW(xinput_dlls[i]);
        if (m) g_xget = (XGetState)(void*)GetProcAddress(m, "XInputGetState");
    }

    /* Impact, slanted, is close to the cover's heavy italic logo lettering. */
    g_font_head = CreateFontW(-38, 0, 0, 0, FW_NORMAL, TRUE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Impact");
    g_font_item = CreateFontW(-26, 0, 0, 0, FW_NORMAL, TRUE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Impact");
    g_font_set = CreateFontW(-23, 0, 0, 0, FW_NORMAL, TRUE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Impact");
    g_font_btn = CreateFontW(-19, 0, 0, 0, FW_NORMAL, FALSE, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Impact");
    g_font_small = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"MC2RecompHub";
    RegisterClassW(&wc);

    /* Resizable, but WM_SIZING keeps the client area cover-shaped; there is no
     * maximize box because a maximized window could not keep that shape. */
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME;
    RECT r = { 0, 0, CLIENT_W, CLIENT_H };
    AdjustWindowRect(&r, style, FALSE);
    int w = r.right - r.left, h = r.bottom - r.top;
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"MC2Recomp", style,
                              (GetSystemMetrics(SM_CXSCREEN) - w) / 2,
                              (GetSystemMetrics(SM_CYSCREEN) - h) / 2,
                              w, h, NULL, NULL, inst, NULL);
    if (!hwnd) return 1;
    ShowWindow(hwnd, show);
    HANDLE covers = CreateThread(NULL, 0, fetch_covers, hwnd, 0, NULL);
    if (covers) CloseHandle(covers);
    SetTimer(hwnd, 1, TICK_MS, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
