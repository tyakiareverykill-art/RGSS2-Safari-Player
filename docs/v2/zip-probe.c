#include "physfs.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

enum { PATH_CAP = 4096, INI_CAP = 32768, SCRIPT_CAP = 32 * 1024 * 1024 };
static char ini[INI_CAP + 1];
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(void, retain_script, (const char *data, int offset, int count, int length), {
    if (!offset) Module['scriptArchive'] = new Uint8Array(length);
    Module['scriptArchive'].set(HEAPU8.subarray(data, data + count), offset);
});
#else
#define retain_script(data, offset, count, length) ((void)0)
#endif

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
        if (marshal_header) retain_script(buffer, total, bytes, length);
        total += bytes;
    }
    const int closed = PHYSFS_close(entry);
    if (bytes < 0 || !closed || total != length || !marshal_header) return 0;

    printf("PhysicsFS: Scripts 読み込み成功 / %lld バイト / Marshal 4.8\n", (long long)total);
    return 1;
}

/* RGSSAD v1 is the archive format used by RPG Maker VX (.rgss2a). */
static uint32_t next_key(uint32_t *key) {
    const uint32_t current = *key;
    *key = current * 7u + 3u;
    return current;
}

static int read_word(PHYSFS_File *file, uint32_t *value) {
    unsigned char bytes[4];
    if (PHYSFS_readBytes(file, bytes, sizeof(bytes)) != sizeof(bytes)) return 0;
    *value = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
             ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
    return 1;
}

static int inspect_packed_script(PHYSFS_File *file, uint32_t length, uint32_t key) {
    if (length < 2 || length > SCRIPT_CAP) return 0;

    int marshal_header = 0;
    uint32_t consumed = 0;
    while (consumed < length) {
        const uint32_t remaining = length - consumed;
        const size_t count = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        if (PHYSFS_readBytes(file, buffer, count) != (PHYSFS_sint64)count) return 0;

        uint32_t word_key = 0;
        for (size_t i = 0; i < count; ++i) {
            const unsigned shift = (unsigned)(i % 4) * 8;
            if (!shift) word_key = next_key(&key);
            buffer[i] = (char)((unsigned char)buffer[i] ^ (unsigned char)(word_key >> shift));
        }
        if (!consumed) marshal_header = buffer[0] == 4 && buffer[1] == 8;
        if (marshal_header) retain_script(buffer, consumed, count, length);
        consumed += (uint32_t)count;
    }
    if (!marshal_header) return 0;

    printf("PhysicsFS: Game.rgss2a 内の Scripts 読み込み成功 / %u バイト / Marshal 4.8\n",
           (unsigned)length);
    return 1;
}

static int scan_rgss2a(PHYSFS_File *file, const char *script_path) {
    char header[8];
    if (PHYSFS_readBytes(file, header, sizeof(header)) != sizeof(header) ||
        memcmp(header, "RGSSAD\0\1", sizeof(header))) {
        puts("PhysicsFS: Game.rgss2a のヘッダーが RGSSAD v1 と一致しません");
        return 0;
    }
    puts("PhysicsFS: Game.rgss2a / RGSSAD v1 を確認");

    const PHYSFS_sint64 end = PHYSFS_fileLength(file);
    uint32_t key = UINT32_C(0xDEADCAFE);
    PHYSFS_sint64 position = sizeof(header);
    while (position >= 0 && position < end) {
        uint32_t name_length;
        char name[PATH_CAP];
        if (!read_word(file, &name_length)) break;
        name_length ^= next_key(&key);
        if (!name_length || name_length >= sizeof(name) ||
            end - position < (PHYSFS_sint64)name_length + 8 ||
            PHYSFS_readBytes(file, name, name_length) != name_length) break;

        for (uint32_t i = 0; i < name_length; ++i) {
            name[i] = (char)((unsigned char)name[i] ^ (unsigned char)next_key(&key));
            if (name[i] == '\\') name[i] = '/';
        }
        name[name_length] = '\0';

        uint32_t length;
        if (!read_word(file, &length)) break;
        length ^= next_key(&key);
        position = PHYSFS_tell(file);
        if (position < 0 || end - position < length) break;

        if (!memchr(name, '\0', name_length) && !strcasecmp(name, script_path)) {
            return inspect_packed_script(file, length, key);
        }
        position += length;
        if (!PHYSFS_seek(file, (PHYSFS_uint64)position)) break;
    }
    return 0;
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

    if (join_path(script_path, path, "Game.rgss2a")) {
        PHYSFS_File *packed = PHYSFS_openRead(script_path);
        if (packed) {
            puts("PhysicsFS: Game.rgss2a 内を検索");
            const int success = scan_rgss2a(packed, scripts);
            if (PHYSFS_close(packed) && success) return 1;
        }
    }
    puts("PhysicsFS: Scripts の読み込みまたは Marshal ヘッダーの確認に失敗");
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
