#ifndef RAYLIB_LIBRETRO_CONFIG_H
#define RAYLIB_LIBRETRO_CONFIG_H

#include <string.h>
#include <stdio.h>

/* Size limits for individual field strings */
#define RLCONFIG_SECTION_MAX  64
#define RLCONFIG_KEY_MAX      256
#define RLCONFIG_VALUE_MAX    512
#define RLCONFIG_COMPOUND_MAX (RLCONFIG_SECTION_MAX + 1 + RLCONFIG_KEY_MAX + 1)
#define RLCONFIG_LINE_MAX     (RLCONFIG_KEY_MAX + RLCONFIG_VALUE_MAX + 4)

/* Default config filename — raylib-libretro.h defines its own copy as well */
#ifndef RAYLIB_LIBRETRO_CFG_FILE
#define RAYLIB_LIBRETRO_CFG_FILE "raylib-libretro.cfg"
#endif

#define HASHMAP_REALLOC(p, s) MemRealloc(p, (unsigned int)s)
#define HASHMAP_FREE(p)       MemFree(p)
#include "../vendor/hashmap/hashmap.h"

/* Hashmap type: compound key ("section\nkey") → entry index */
HASHMAP_DECLARE_STRING(RLibretroConfigLookup, rlconfiglookup, int)

/* Redirect cvector allocations through raylib's memory functions. */
#define cvector_clib_malloc(sz)    MemAlloc((unsigned int)(sz))
#define cvector_clib_realloc(p,sz) MemRealloc((p), (unsigned int)(sz))
#define cvector_clib_free          MemFree
#define cvector_clib_calloc(n,sz)  memset(MemAlloc((unsigned int)((n)*(sz))), 0, (size_t)((n)*(sz)))
#include "../vendor/c-vector/cvector.h"

/* One config key-value pair with its section. Heap-allocated individually so that
   the compound field address (used as a hashmap key) stays stable across vector growth. */
typedef struct {
    char section[RLCONFIG_SECTION_MAX];
    char key[RLCONFIG_KEY_MAX];
    char value[RLCONFIG_VALUE_MAX];
    char compound[RLCONFIG_COMPOUND_MAX]; /* "section\nkey" — used as hashmap key */
} RLibretroConfigEntry;

/* In-memory config — heap allocated via rlconfig_load() */
typedef struct {
    cvector(RLibretroConfigEntry*) entries; /* individually heap-allocated entries */
    RLibretroConfigLookup lookup;           /* compound_key → index into entries */
} RLibretroConfig;

/* --- Public API (all static) --- */

/* Load config from filename. Pass NULL to create an empty config. Caller must call rlconfig_free(). */
static RLibretroConfig* rlconfig_load(const char *filename);

/* Save config to filename. Returns true on success. */
static bool rlconfig_save(RLibretroConfig *cfg, const char *filename);

/* Get string value for section+key. Returns NULL if not found. */
static const char* rlconfig_get(RLibretroConfig *cfg, const char *section, const char *key);

/* Get integer value for section+key. Returns fallback if not found or non-numeric. */
static int rlconfig_get_int(RLibretroConfig *cfg, const char *section, const char *key, int fallback);

/* Set string value for section+key. Adds entry if new, updates in-place if existing. */
static void rlconfig_set(RLibretroConfig *cfg, const char *section, const char *key, const char *value);

/* Set integer value (formatted as decimal string). */
static void rlconfig_set_int(RLibretroConfig *cfg, const char *section, const char *key, int value);

/* Remove a single key. Returns true if the key existed. */
static bool rlconfig_delete(RLibretroConfig *cfg, const char *section, const char *key);

/* Remove all keys in a section. */
static void rlconfig_clear_section(RLibretroConfig *cfg, const char *section);

/* Free config and all associated memory. */
static void rlconfig_free(RLibretroConfig *cfg);

/* --- Implementation --- */

#ifdef RAYLIB_LIBRETRO_CONFIG_IMPLEMENTATION
#ifndef RAYLIB_LIBRETRO_CONFIG_IMPLEMENTATION_ONCE
#define RAYLIB_LIBRETRO_CONFIG_IMPLEMENTATION_ONCE

HASHMAP_DEFINE_STRING(RLibretroConfigLookup, rlconfiglookup, int)

/* Build compound key "section\nkey" into out (size RLCONFIG_COMPOUND_MAX). */
static void RLibretroConfigCompound(const char *section, const char *key,
                                    char *out, size_t outSize) {
    snprintf(out, outSize, "%s\n%s", section, key);
}

static void rlconfig_free(RLibretroConfig *cfg) {
    if (!cfg) return;
    for (size_t i = 0; i < cvector_size(cfg->entries); i++) {
        MemFree(cfg->entries[i]);
    }
    cvector_free(cfg->entries);
    rlconfiglookup_free(&cfg->lookup);
    MemFree(cfg);
}

static bool rlconfig_delete(RLibretroConfig *cfg, const char *section, const char *key) {
    char compound[RLCONFIG_COMPOUND_MAX];
    int idx = -1;
    if (!cfg || !section || !key) return false;
    RLibretroConfigCompound(section, key, compound, sizeof(compound));
    if (!rlconfiglookup_get(&cfg->lookup, compound, &idx) || idx < 0) return false;

    rlconfiglookup_remove(&cfg->lookup, cfg->entries[idx]->compound, NULL);
    MemFree(cfg->entries[idx]);
    cvector_erase(cfg->entries, (size_t)idx);

    /* Fix hashmap indices for entries that shifted left after the erase. */
    for (size_t i = (size_t)idx; i < cvector_size(cfg->entries); i++) {
        rlconfiglookup_insert(&cfg->lookup, cfg->entries[i]->compound, (int)i);
    }
    return true;
}

static void rlconfig_clear_section(RLibretroConfig *cfg, const char *section) {
    if (!cfg || !section) return;

    /* Stable partition: keep non-matching entries, free matching ones. */
    size_t write = 0;
    for (size_t i = 0; i < cvector_size(cfg->entries); i++) {
        if (TextIsEqual(cfg->entries[i]->section, section)) {
            MemFree(cfg->entries[i]);
        } else {
            cfg->entries[write++] = cfg->entries[i];
        }
    }
    cvector_set_size(cfg->entries, write);

    /* Rebuild hashmap from scratch since indices changed. */
    rlconfiglookup_clear(&cfg->lookup);
    for (size_t i = 0; i < cvector_size(cfg->entries); i++) {
        rlconfiglookup_insert(&cfg->lookup, cfg->entries[i]->compound, (int)i);
    }
}

static void rlconfig_set(RLibretroConfig *cfg, const char *section,
                          const char *key, const char *value) {
    char compound[RLCONFIG_COMPOUND_MAX];
    int idx = -1;

    if (!cfg || !section || !key || !value) return;

    RLibretroConfigCompound(section, key, compound, sizeof(compound));

    if (rlconfiglookup_get(&cfg->lookup, compound, &idx) && idx >= 0) {
        snprintf(cfg->entries[idx]->value, RLCONFIG_VALUE_MAX, "%s", value);
        return;
    }

    RLibretroConfigEntry *entry = (RLibretroConfigEntry*)MemAlloc(sizeof(RLibretroConfigEntry));
    if (!entry) return;
    snprintf(entry->section,  RLCONFIG_SECTION_MAX,  "%s", section);
    snprintf(entry->key,      RLCONFIG_KEY_MAX,       "%s", key);
    snprintf(entry->value,    RLCONFIG_VALUE_MAX,     "%s", value);
    RLibretroConfigCompound(section, key, entry->compound, RLCONFIG_COMPOUND_MAX);

    idx = (int)cvector_size(cfg->entries);
    cvector_push_back(cfg->entries, entry);

    /* Store pointer into the entry's own compound buffer — stable while the entry is alive. */
    rlconfiglookup_insert(&cfg->lookup, entry->compound, idx);
}

static void rlconfig_set_int(RLibretroConfig *cfg, const char *section,
                              const char *key, int value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    rlconfig_set(cfg, section, key, buf);
}

static const char* rlconfig_get(RLibretroConfig *cfg, const char *section,
                                  const char *key) {
    char compound[RLCONFIG_COMPOUND_MAX];
    int idx = -1;
    if (!cfg || !section || !key) return NULL;
    RLibretroConfigCompound(section, key, compound, sizeof(compound));
    if (rlconfiglookup_get(&cfg->lookup, compound, &idx) && idx >= 0)
        return cfg->entries[idx]->value;
    return NULL;
}

static int rlconfig_get_int(RLibretroConfig *cfg, const char *section,
                             const char *key, int fallback) {
    const char *v = rlconfig_get(cfg, section, key);
    if (!v || !v[0]) return fallback;
    return TextToInteger(v);
}

/* Trim leading and trailing ASCII whitespace in-place. */
static void RLibretroConfigTrim(char *s) {
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;
    if (start != s) memmove(s, start, TextLength(start) + 1);
    unsigned int len = TextLength(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\r' || s[len-1] == '\n')) {
        s[--len] = '\0';
    }
}

static RLibretroConfig* rlconfig_load(const char *filename) {
    RLibretroConfig *cfg = (RLibretroConfig*)MemAlloc(sizeof(RLibretroConfig));
    if (!cfg) return NULL;
    memset(cfg, 0, sizeof(RLibretroConfig));

    if (!filename) return cfg;

    char *text = LoadFileText(filename);
    if (!text) return cfg;

    char line[RLCONFIG_LINE_MAX];
    char section[RLCONFIG_SECTION_MAX] = {0};
    const char *p = text;

    while (*p) {
        int i = 0;
        while (*p && *p != '\n' && i < RLCONFIG_LINE_MAX - 1)
            line[i++] = *p++;
        if (*p == '\n') p++;
        line[i] = '\0';

        RLibretroConfigTrim(line);

        /* Skip blank lines and comments */
        if (line[0] == '\0' || line[0] == '#' || line[0] == ';') continue;

        /* Section header */
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                snprintf(section, sizeof(section), "%s", line + 1);
                RLibretroConfigTrim(section);
            }
            continue;
        }

        /* Key=value */
        char *eq = strchr(line, '=');
        if (!eq || section[0] == '\0') continue;

        *eq = '\0';
        char *k = line;
        char *v = eq + 1;
        RLibretroConfigTrim(k);
        RLibretroConfigTrim(v);

        if (k[0] != '\0') rlconfig_set(cfg, section, k, v);
    }

    UnloadFileText(text);
    return cfg;
}

static bool rlconfig_save(RLibretroConfig *cfg, const char *filename) {
    if (!cfg || !filename) return false;

    FILE *f = fopen(filename, "w");
    if (!f) return false;

    /* Collect unique section names in insertion order (pointers into entry data — stable). */
    cvector(const char*) sections = NULL;
    for (size_t i = 0; i < cvector_size(cfg->entries); i++) {
        const char *sec = cfg->entries[i]->section;
        bool found = false;
        for (size_t s = 0; s < cvector_size(sections); s++) {
            if (TextIsEqual(sections[s], sec)) { found = true; break; }
        }
        if (!found) cvector_push_back(sections, sec);
    }

    /* Write each section */
    for (size_t s = 0; s < cvector_size(sections); s++) {
        fprintf(f, "[%s]\n", sections[s]);
        for (size_t i = 0; i < cvector_size(cfg->entries); i++) {
            if (TextIsEqual(cfg->entries[i]->section, sections[s])) {
                fprintf(f, "%s=%s\n", cfg->entries[i]->key, cfg->entries[i]->value);
            }
        }
        fprintf(f, "\n");
    }

    cvector_free(sections);
    fclose(f);
    return true;
}

#endif /* RAYLIB_LIBRETRO_CONFIG_IMPLEMENTATION_ONCE */
#endif /* RAYLIB_LIBRETRO_CONFIG_IMPLEMENTATION */

#endif /* RAYLIB_LIBRETRO_CONFIG_H */
