#include "physfs.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

enum { PATH_CAP = 4096, INI_CAP = 32768, SCRIPT_CAP = 32 * 1024 * 1024 };
static char ini[INI_CAP + 1];
static char buffer[65536];

static int join_path(char *out, const char *dir, const char *name) {
    const int size = snprintf(out, PATH_CAP, "%s/%s", dir, name);
    return size >= 0 && size < PATH_CAP;
}

/* Accept the archive root or a single wrapping game directory. */
static int find_game_ini(const char *dir, char *out, int depth) {
    char **names = PHYSFS_enumerateFiles(dir);
    if (!names) return 0;

    int found = 0;
    for (char **name = names; *name && !found; ++name) {
        PHYSFS_Stat stat;
        if (!strcasecmp(*name, "Game.ini") && join_path(out, dir, *name) &&
            PHYSFS_stat(out, &stat) && stat.filetype == PHYSFS_FILETYPE_REGULAR) {
            found = 1;
        }
    }

    for (char **name = names; depth && *name && !found; ++name) {
        char child[PATH_CAP];
        PHYSFS_Stat stat;
        if (join_path(child, dir, *name) && PHYSFS_stat(child, &stat) &&
            stat.filetype == PHYSFS_FILETYPE_DIRECTORY) {
            found = find_game_ini(child, out, depth - 1);
        }
    }

    PHYSFS_freeList(names);
    return found;
}

static char *trim(char *text) {
    text += strspn(text, " \t\r");
    size_t length = strlen(text);
    while (length && strchr(" \t\r", text[length - 1])) {
        text[--length] = '\0';
    }
    return text;
}

static const char *script_setting(void) {
    int game_section = 0;
    char *rest;
    for (char *line = strtok_r(ini, "\n", &rest); line;
         line = strtok_r(NULL, "\n", &rest)) {
        if (line == ini && !strncmp(line, "\xef\xbb\xbf", 3)) line += 3;
        line = trim(line);
        if (*line == '[') {
            game_section = !strcasecmp(line, "[Game]");
        }
        char *equals = strchr(line, '=');
        if (!game_section || !equals) continue;
        *equals = '\0';
        if (strcasecmp(trim(line), "Scripts")) continue;
        char *path = trim(equals + 1);
        for (char *p = path; *p; ++p) {
            if (*p == '\\') *p = '/';
        }
        return path;
    }
    return NULL;
}

static int inspect_scripts(const char *path) {
    PHYSFS_File *entry = PHYSFS_openRead(path);
    if (!entry) return 0;

    const PHYSFS_sint64 length = PHYSFS_fileLength(entry);
    if (length < 2 || length > SCRIPT_CAP) {
        PHYSFS_close(entry);
        puts("PhysicsFS: Scripts の容量が診断範囲外です");
        return 0;
    }
    PHYSFS_sint64 total = 0;
    int marshal_header = 0;
    PHYSFS_sint64 bytes;
    while ((bytes = PHYSFS_readBytes(entry, buffer, sizeof(buffer))) > 0) {
        if (!total) marshal_header = bytes >= 2 && buffer[0] == 4 && buffer[1] == 8;
        total += bytes;
    }
    const int closed = PHYSFS_close(entry);
    if (bytes < 0 || !closed || total != length || !marshal_header) return 0;

    printf("PhysicsFS: Scripts 読み込み成功 / %lld バイト / Marshal 4.8\n", (long long)total);
    return 1;
}

static int inspect_game(void) {
    char path[PATH_CAP];
    if (!find_game_ini("archive", path, 1)) {
        puts("PhysicsFS: Game.ini が見つかりません");
        return 0;
    }

    PHYSFS_File *file = PHYSFS_openRead(path);
    if (!file) return 0;
    const PHYSFS_sint64 size = PHYSFS_fileLength(file);
    const int read_ok = size > 0 && size <= INI_CAP && PHYSFS_readBytes(file, ini, size) == size;
    const int closed = PHYSFS_close(file);
    if (!read_ok || !closed) return 0;
    ini[size] = '\0';

    const char *scripts = script_setting();
    if (!scripts || !*scripts) {
        puts("PhysicsFS: Game.ini に Scripts 設定がありません");
        return 0;
    }

    puts("PhysicsFS: Game.ini 読み込み成功");
    *strrchr(path, '/') = '\0';
    char script_path[PATH_CAP];
    if (join_path(script_path, path, scripts) && inspect_scripts(script_path)) return 1;

    if (join_path(script_path, path, "Game.rgss2a") && PHYSFS_exists(script_path)) {
        puts("PhysicsFS: Game.rgss2a を検出 / RGSS暗号化アーカイブの確認は未対応");
    } else {
        puts("PhysicsFS: Scripts の読み込みまたは Marshal ヘッダーの確認に失敗");
    }
    return 0;
}

int probe_zip(void) {
    if (!PHYSFS_init("/io-probe")) return 0;

    int success = 0;
    if (PHYSFS_mount("/game/archive.zip", "archive", 1)) {
        puts("PhysicsFS: ZIP マウント成功");
        success = inspect_game();
    } else {
        printf("PhysicsFS: ZIP マウント失敗 / %s\n",
               PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
    }

    return PHYSFS_deinit() && success;
}
