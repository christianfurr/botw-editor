#ifndef MNG_BOTW_EDITOR_WORLD_H
#define MNG_BOTW_EDITOR_WORLD_H

/* NX 1.6.0 world/progress editing through the game's own GameData setters.
   Function addresses and layouts were checked against the player's verified main;
   no flag bytes are patched directly. Include after botw_editor.h. */
typedef struct { uintptr_t vtable; const char *text; } BeWorldString;
typedef struct { uintptr_t manager, primary, pending; } BeWorldRefs;
typedef int (*BeWorldGetBool)(void *, unsigned char *, const BeWorldString *, int);
typedef int (*BeWorldSetBool)(void *, int, const BeWorldString *, int, int);
typedef int (*BeWorldGetS32)(void *, int32_t *, const BeWorldString *, int);
typedef int (*BeWorldSetS32)(void *, int32_t, const BeWorldString *, int);
typedef int (*BeWorldGetF32)(void *, float *, const BeWorldString *, int);
typedef int (*BeWorldSetF32)(void *, float, const BeWorldString *, int);

enum {
    BW_MANAGER = 0x2cc5fe8, BW_SAFE_STRING = 0x2a6e6c0,
    BW_GET_BOOL = 0x163a2b0, BW_SET_BOOL = 0x163f8a0,
    BW_GET_S32 = 0x163a590, BW_SET_S32 = 0x163f9c0,
    BW_GET_F32 = 0x163a700, BW_SET_F32 = 0x163fad0
};
static const char *const bw_rune_flags[] = {
    "IsGet_Obj_RemoteBomb", "IsGet_Obj_RemoteBombCube", "IsGet_Obj_RemoteBombBall",
    "IsGet_Obj_Magnetglove", "IsGet_Obj_StopTimer", "IsGet_Obj_IceMaker", "IsGet_Obj_Camera"
};

static int bw_code(uintptr_t base, uintptr_t offset, uint32_t expected) {
    uint32_t actual = 0;
    return be_memory_span(base + offset, 4, VM_PROT_READ | VM_PROT_EXECUTE) &&
           be_read(base + offset, &actual, 4) && actual == expected;
}
static int bw_valid_code(uintptr_t base) {
#if defined(GAME_BOTW160) && !TARGET_OS_IPHONE
    return base && bw_code(base, BW_GET_BOOL, 0xf81a0ffb) &&
        bw_code(base, BW_GET_BOOL + 0x50, 0xb9400aa8) &&
        bw_code(base, BW_SET_BOOL, 0xa9ba6ffc) &&
        bw_code(base, BW_SET_BOOL + 0x54, 0xb9400aa8) &&
        bw_code(base, BW_SET_BOOL + 0xec, 0x12000283) &&
        bw_code(base, BW_SET_BOOL + 0x114, 0x17ffeec7) &&
        bw_code(base, 0x163b530, 0x36000123) &&
        bw_code(base, 0x163b548, 0x36080408) &&
        bw_code(base, BW_GET_S32, 0xf81a0ffb) &&
        bw_code(base, BW_GET_S32 + 0x50, 0xb9401aa8) &&
        bw_code(base, BW_SET_S32, 0xf81a0ffb) &&
        bw_code(base, BW_SET_S32 + 0x50, 0xb9401aa8) &&
        bw_code(base, BW_GET_F32, 0xf81a0ffb) &&
        bw_code(base, BW_GET_F32 + 0x50, 0xb9402aa8) &&
        bw_code(base, BW_SET_F32, 0xfc1a0fe8) &&
        bw_code(base, BW_SET_F32 + 0x50, 0xb9402a88) &&
        be_pointer(base + BW_SAFE_STRING + 0x18);
#else
    (void)base; return 0;
#endif
}
static int bw_refs(uintptr_t base, BeWorldRefs *refs, int writing) {
    memset(refs, 0, sizeof *refs);
    if (!bw_valid_code(base)) return 0;
    refs->manager = be_pointer(base + BW_MANAGER);
    if (!refs->manager) return 0;
    uintptr_t primary_ref = be_pointer(refs->manager + 0xbe0);
    uintptr_t pending_ref = be_pointer(refs->manager + 0xbd8);
    if (!primary_ref || !pending_ref) return 0;
    refs->primary = be_pointer(primary_ref);
    refs->pending = be_pointer(pending_ref);
    uint32_t manager_flags = 0;
    unsigned char once = 1;
    if (!refs->primary || !refs->pending ||
        !be_read(refs->manager + 0xc18, &manager_flags, 4) ||
        !be_read(refs->manager + 0xbea, &once, 1)) return 0;
    return !writing || (!(manager_flags & 0x40000u) && !once);
}
static BeWorldString bw_string(uintptr_t base, const char *name) {
    return (BeWorldString){base + BW_SAFE_STRING, name};
}
static uint32_t bw_hash(const char *text) {
    uint32_t crc = UINT32_MAX;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        crc ^= *p;
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
/* Validate the live object layout using its virtual hash/properties accessors
   before using property metadata to preflight a transaction. */
static uintptr_t bw_flag_metadata(uintptr_t flag, int kind) {
    for (unsigned depth = 0; depth < 2 && flag; ++depth) {
        uintptr_t table = be_pointer(flag), hash_fn = table ? be_pointer(table + 0x10) : 0;
        uint32_t words[4] = {0};
        if (!hash_fn || !be_read(hash_fn, words, sizeof words)) return 0;
        if (words[0] == (kind ? 0xb9401c00u : 0xb9400c00u)) {
            uintptr_t props_fn = be_pointer(table + 0x30);
            uint32_t props_word = 0;
            unsigned char type = 255;
            if (!be_read(flag + 8, &type, 1) || type != kind || !props_fn ||
                !be_read(props_fn, &props_word, 4) || props_word != 0x91002400u) return 0;
            return flag;
        }
        /* FlagProxy<T>::getHash, verified in NX160 at 0x16388a0 (Bool):
           load underlying Flag at +8, dispatch its getHash through vtable+0x10. */
        if (words[0] != 0xf9400400u || words[1] != 0xf9400008u ||
            words[2] != 0xf9400901u || words[3] != 0xd61f0020u) return 0;
        flag = be_pointer(flag + 8);
    }
    return 0;
}
static uintptr_t bw_find_flag(uintptr_t parameter, const char *name, int kind) {
    int32_t count = 0;
    uintptr_t array = 0;
    if (kind < 0 || kind > 2) return 0;
    uintptr_t count_offset = 8 + (uintptr_t)kind * 0x10, array_offset = 0x10 + (uintptr_t)kind * 0x10;
    if (!be_read(parameter + count_offset, &count, 4) || count < 1 || count > 300000 ||
        !be_read(parameter + array_offset, &array, 8) ||
        !be_memory_span(array, (size_t)count * 8, VM_PROT_READ)) return 0;
    uint32_t wanted = bw_hash(name);
    int32_t low = 0, high = count - 1;
    while (low <= high) {
        int32_t middle = low + (high - low) / 2;
        uintptr_t flag = bw_flag_metadata(be_pointer(array + (uintptr_t)middle * 8), kind);
        uint32_t hash = 0;
        if (!flag || !be_read(flag + (kind ? 0x1c : 0xc), &hash, 4)) return 0;
        if (hash < wanted) { low = middle + 1; continue; }
        if (hash > wanted) { high = middle - 1; continue; }
        return flag;
    }
    return 0;
}
static int bw_flag_writable(uintptr_t parameter, const char *name, int integer) {
    uintptr_t flag = bw_find_flag(parameter, name, integer);
    unsigned char properties = 0;
    return flag && be_read(flag + 9, &properties, 1) && (properties & 2u);
}
static int bw_map_flag_number(const char *name) {
    if (strlen(name) != 11 || strncmp(name, "MapTower_", 9) ||
        name[9] < '0' || name[9] > '1' || name[10] < '0' || name[10] > '9') return 0;
    int number = (name[9] - '0') * 10 + name[10] - '0';
    return number >= 1 && number <= 15 ? number : 0;
}
static int bw_bool_native_permissions(uintptr_t parameter, const char *name) {
    uintptr_t flag = bw_find_flag(parameter, name, 0);
    unsigned char properties = 0;
    if (!flag || !be_read(flag + 9, &properties, 1)) return -1;
    if (properties & 2u) return 1;
    /* Own NX160 Bootup.pack / GameData/gamedata.ssarc definitions make the
       fifteen MapTower booleans and MapTower_07_Demo readable/saveable but not program-writable.
       All are one-trigger except MapTower_07 (property bytes 0x0d / 0x05).
       Manager::getParamBypassPerm uses this native setter with permissions=false;
       use that existing game API only for these exact names and metadata.
       MapTower_07_Demo (0x0d) creates the raised world towers. Other flags,
       including rune/paraglider acquisition, keep normal checks. */
    int tower = bw_map_flag_number(name);
    if (!strcmp(name, "MapTower_07_Demo") && properties == 0x0du) return 0;
    return tower && properties == (tower == 7 ? 0x05u : 0x0du) ? 0 : -1;
}
static int bw_bool_editable(uintptr_t parameter, const char *name) {
    return bw_bool_native_permissions(parameter, name) >= 0;
}
static int bw_get_bool_param(uintptr_t base, uintptr_t parameter, const char *name, int *value) {
    if (!bw_find_flag(parameter, name, 0)) return 0;
    BeWorldString string = bw_string(base, name);
    unsigned char current = 0;
    int result = ((BeWorldGetBool)(base + BW_GET_BOOL))((void *)parameter, &current, &string, 1);
    if (!(result & 1) || current > 1) return 0;
    *value = current;
    return 1;
}
static int be_world_get_bool(uintptr_t base, const char *name, int *value) {
    BeWorldRefs refs;
    return bw_refs(base, &refs, 0) && bw_get_bool_param(base, refs.primary, name, value);
}
static int bw_get_s32_param(uintptr_t base, uintptr_t parameter, const char *name, int32_t *value) {
    if (!bw_find_flag(parameter, name, 1)) return 0;
    BeWorldString string = bw_string(base, name);
    return ((BeWorldGetS32)(base + BW_GET_S32))((void *)parameter, value, &string, 1) & 1;
}
static int be_world_get_s32(uintptr_t base, const char *name, int32_t *value) {
    BeWorldRefs refs;
    return bw_refs(base, &refs, 0) && bw_get_s32_param(base, refs.primary, name, value);
}
static int bw_get_f32_param(uintptr_t base, uintptr_t parameter, const char *name, float *value) {
    if (!bw_find_flag(parameter, name, 2)) return 0;
    BeWorldString string = bw_string(base, name);
    int result = ((BeWorldGetF32)(base + BW_GET_F32))((void *)parameter, value, &string, 1);
    return (result & 1) && isfinite(*value);
}
static int be_world_get_f32(uintptr_t base, const char *name, float *value) {
    BeWorldRefs refs;
    return bw_refs(base, &refs, 0) && bw_get_f32_param(base, refs.primary, name, value);
}
static int bw_put_bool(uintptr_t base, uintptr_t parameter, const char *name, int value, int rollback) {
    int current = -1;
    int check_permissions = bw_bool_native_permissions(parameter, name);
    if (check_permissions < 0 || !bw_get_bool_param(base, parameter, name, &current)) return 0;
    if (current == !!value) return 1;
    BeWorldString string = bw_string(base, name);
    int result = ((BeWorldSetBool)(base + BW_SET_BOOL))((void *)parameter, !!value, &string, check_permissions, rollback);
    return (result & 1) && bw_get_bool_param(base, parameter, name, &current) && current == !!value;
}
static int bw_put_s32(uintptr_t base, uintptr_t parameter, const char *name, int32_t value) {
    int32_t current = 0;
    if (!bw_get_s32_param(base, parameter, name, &current)) return 0;
    if (current == value) return 1;
    BeWorldString string = bw_string(base, name);
    int result = ((BeWorldSetS32)(base + BW_SET_S32))((void *)parameter, value, &string, 1);
    return (result & 1) && bw_get_s32_param(base, parameter, name, &current) && current == value;
}
static int bw_put_f32(uintptr_t base, uintptr_t parameter, const char *name, float value) {
    float current = 0;
    if (!isfinite(value) || !bw_get_f32_param(base, parameter, name, &current)) return 0;
    if (current == value) return 1;
    BeWorldString string = bw_string(base, name);
    int result = ((BeWorldSetF32)(base + BW_SET_F32))((void *)parameter, value, &string, 1);
    return (result & 1) && bw_get_f32_param(base, parameter, name, &current) && current == value;
}
static int be_world_set_s32(uintptr_t base, const char *name, int32_t value) {
    BeWorldRefs refs;
    int32_t old_primary, old_pending;
    if (!bw_refs(base, &refs, 1) || !bw_flag_writable(refs.primary, name, 1) ||
        !bw_flag_writable(refs.pending, name, 1) ||
        !bw_get_s32_param(base, refs.primary, name, &old_primary) ||
        !bw_get_s32_param(base, refs.pending, name, &old_pending)) return 0;
    if (bw_put_s32(base, refs.pending, name, value) &&
        (refs.primary == refs.pending || bw_put_s32(base, refs.primary, name, value))) return 1;
    bw_put_s32(base, refs.pending, name, old_pending);
    if (refs.primary != refs.pending) bw_put_s32(base, refs.primary, name, old_primary);
    return 0;
}
static inline int be_world_set_bool(uintptr_t base, const char *name, int value) {
    BeWorldRefs refs;
    int old_primary, old_pending;
    if ((value != 0 && value != 1) || !bw_refs(base, &refs, 1) ||
        !bw_bool_editable(refs.primary, name) || !bw_bool_editable(refs.pending, name) ||
        !bw_get_bool_param(base, refs.primary, name, &old_primary) ||
        !bw_get_bool_param(base, refs.pending, name, &old_pending)) return 0;
    if (bw_put_bool(base, refs.pending, name, value, 0) &&
        (refs.primary == refs.pending || bw_put_bool(base, refs.primary, name, value, 0))) return 1;
    bw_put_bool(base, refs.pending, name, old_pending, 1);
    if (refs.primary != refs.pending) bw_put_bool(base, refs.primary, name, old_primary, 1);
    return 0;
}
static int be_world_set_f32(uintptr_t base, const char *name, float value) {
    BeWorldRefs refs;
    float old_primary, old_pending;
    if (!isfinite(value) || !bw_refs(base, &refs, 1) ||
        !bw_flag_writable(refs.primary, name, 2) || !bw_flag_writable(refs.pending, name, 2) ||
        !bw_get_f32_param(base, refs.primary, name, &old_primary) ||
        !bw_get_f32_param(base, refs.pending, name, &old_pending)) return 0;
    if (bw_put_f32(base, refs.pending, name, value) &&
        (refs.primary == refs.pending || bw_put_f32(base, refs.primary, name, value))) return 1;
    bw_put_f32(base, refs.pending, name, old_pending);
    if (refs.primary != refs.pending) bw_put_f32(base, refs.primary, name, old_primary);
    return 0;
}

enum { BW_MAX_TRANSACTION = 160 };
static int bw_unlock_flags(uintptr_t base, const char *const *names, size_t count,
                           char *result, size_t capacity) {
    BeWorldRefs refs;
    int old_primary[BW_MAX_TRANSACTION], old_pending[BW_MAX_TRANSACTION];
    if (!count || count > BW_MAX_TRANSACTION || !bw_refs(base, &refs, 1)) {
        snprintf(result, capacity, "World flags are not ready for editing"); return 0;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!bw_bool_editable(refs.primary, names[i]) || !bw_bool_editable(refs.pending, names[i]) ||
            !bw_get_bool_param(base, refs.primary, names[i], &old_primary[i]) ||
            !bw_get_bool_param(base, refs.pending, names[i], &old_pending[i])) {
            snprintf(result, capacity, "Flag %s is unavailable; nothing was changed", names[i]); return 0;
        }
    }
    for (size_t i = 0; i < count; ++i) {
        if (!bw_put_bool(base, refs.pending, names[i], 1, 0) ||
            (refs.primary != refs.pending && !bw_put_bool(base, refs.primary, names[i], 1, 0))) {
            int restored = 1;
            for (size_t j = 0; j <= i; ++j) {
                restored &= bw_put_bool(base, refs.pending, names[j], old_pending[j], 1);
                if (refs.primary != refs.pending) restored &= bw_put_bool(base, refs.primary, names[j], old_primary[j], 1);
            }
            snprintf(result, capacity, "%s", restored ? "The game rejected an edit; original flags restored" :
                "The game rejected an edit and rollback was incomplete; restore the profile backup before saving");
            return 0;
        }
    }
    snprintf(result, capacity, "%zu world flags updated and read back", count);
    return 1;
}
static int bw_shrine_identifier(const char *name) {
    if (strlen(name) != 10 || strncmp(name, "Dungeon", 7)) return 0;
    for (int i = 7; i < 10; ++i) if (name[i] < '0' || name[i] > '9') return 0;
    return atoi(name + 7) < 120;
}
static unsigned be_world_features(uintptr_t base) {
    BeWorldRefs refs;
    if (!bw_refs(base, &refs, 1)) return 0;
    unsigned features = 0;
    int32_t rupees = 0;
    if (bw_get_s32_param(base, refs.primary, "CurrentRupee", &rupees) && rupees >= 0 && rupees <= 999999 &&
        bw_flag_writable(refs.pending, "CurrentRupee", 1)) features |= BE_FEATURE_RUPEES;
    int value = 0;
    if (bw_get_bool_param(base, refs.primary, "Open_Dungeon000", &value)) features |= BE_FEATURE_SHRINES;
    if (bw_bool_editable(refs.primary, "MapTower_01") && bw_bool_editable(refs.pending, "MapTower_01") &&
        bw_get_bool_param(base, refs.primary, "MapTower_01", &value)) features |= BE_FEATURE_MAP;
    if (bw_get_bool_param(base, refs.primary, bw_rune_flags[0], &value)) features |= BE_FEATURE_RUNES;
    return features;
}
typedef struct { uintptr_t info, actor; } BwLivePlayer;
static int bw_live_player(uintptr_t base, BwLivePlayer *player) {
    memset(player, 0, sizeof *player);
    /* PlayerInfo max-heart cache is a float at +0xa0: native heart-container
       code and initialization write it at 0xd218f0 / 0xf341c4. Player's virtual
       getMaxLife returns the int at +0x1868; getLife returns the pointer +0x848.
       These are verified NX160 instructions, not adjacent-field assumptions. */
    if (!bw_code(base, 0xd218f0, 0xbd00a2c0) || !bw_code(base, 0xf341c4, 0xbd00a280) ||
        !bw_code(base, 0xf33680, 0xb9586800) || !bw_code(base, 0xf33684, 0xd65f03c0) ||
        !bw_code(base, 0xd50, 0x91212000) || !bw_code(base, 0xd54, 0xd65f03c0) ||
        !bw_code(base, 0xf7e12c, 0xb9186909) ||
        !bw_code(base, 0xf20524, 0xb900a668) || !bw_code(base, 0xf20624, 0xb900aa68) ||
        !bw_code(base, 0xf4ecdc, 0xbd186d01)) return 0;
    uintptr_t info = be_pointer(base + 0x2ca1140);
    uintptr_t actor = info ? be_pointer(info + 0x60) : 0;
    uintptr_t info_table = info ? be_pointer(info) : 0, actor_table = actor ? be_pointer(actor) : 0;
    if (!info || !actor || info_table != base + 0x2b7be80 || actor_table != base + 0x2b7bea8 ||
        be_pointer(actor_table + 0xf0) != base + 0xf33680 ||
        be_pointer(actor_table + 0x2b8) != base + 0xd50 ||
        be_chain(base, 0x2ca1a78, 0x80, 0x848) != actor + 0x848 ||
        !be_memory_span(info + 0xa0, 12, VM_PROT_READ | VM_PROT_WRITE) ||
        !be_memory_span(actor + 0x1868, 8, VM_PROT_READ | VM_PROT_WRITE) ||
        !be_memory_span(actor + 0x848, 4, VM_PROT_READ | VM_PROT_WRITE)) return 0;
    player->info = info; player->actor = actor;
    return 1;
}
static int bw_same_player(uintptr_t base, const BwLivePlayer *player) {
    return be_pointer(base + 0x2ca1140) == player->info &&
        be_pointer(player->info + 0x60) == player->actor;
}
static int bw_set_live_hearts(uintptr_t base, int hearts, char *result, size_t capacity) {
    BwLivePlayer player;
    int32_t old_flag = 0, old_current_flag = 0, old_cap = 0, old_health = 0;
    int32_t target = hearts >= 1 && hearts <= 30 ? hearts * 4 : 0;
    float old_cache = 0, cache = (float)target;
    if (hearts < 1 || hearts > 30 || !bw_live_player(base, &player) ||
        !be_world_get_s32(base, "MaxHartValue", &old_flag) || old_flag < 4 || old_flag > 120 ||
        !be_world_get_s32(base, "CurrentHart", &old_current_flag) || old_current_flag < 0 || old_current_flag > 120 ||
        !be_read(player.info + 0xa0, &old_cache, 4) || !isfinite(old_cache) || old_cache < 4 || old_cache > 120 ||
        !be_read(player.actor + 0x1868, &old_cap, 4) || old_cap < 4 || old_cap > 120 ||
        !be_read(player.actor + 0x848, &old_health, 4) || old_health < 1 || old_health > 120) {
        snprintf(result, capacity, "The verified live-heart cache is unavailable; nothing was changed");
        return 0;
    }
    if (!be_world_set_s32(base, "MaxHartValue", target)) {
        snprintf(result, capacity, "The game could not apply the maximum-heart edit");
        return 0;
    }
    /* Native player initialization (0xf34164) restores CurrentHart before
       reading MaxHartValue. Refill that saved value along with the live actor. */
    if (be_world_set_s32(base, "CurrentHart", target) && bw_same_player(base, &player) &&
        be_write(player.info + 0xa0, &cache, 4) &&
        be_write(player.actor + 0x1868, &target, 4) && be_write(player.actor + 0x848, &target, 4)) {
        snprintf(result, capacity, "Maximum hearts updated and health refilled in the live player");
        return 1;
    }
    int restored = be_world_set_s32(base, "MaxHartValue", old_flag);
    restored &= be_world_set_s32(base, "CurrentHart", old_current_flag);
    if (bw_same_player(base, &player)) {
        restored &= be_write(player.actor + 0x848, &old_health, 4);
        restored &= be_write(player.actor + 0x1868, &old_cap, 4);
        restored &= be_write(player.info + 0xa0, &old_cache, 4);
    } else restored = 0;
    snprintf(result, capacity, "%s", restored ? "The live heart edit failed; original values restored" :
        "The live heart edit failed and rollback was incomplete; restore the profile backup before saving");
    return 0;
}
static int bw_set_live_stamina(uintptr_t base, int wheels, char *result, size_t capacity) {
    BwLivePlayer player;
    float old_flags[2] = {0}, old_live[2] = {0}, old_cache[2] = {0}, old_actor = 0;
    float target[2] = {(float)wheels * 1000, (float)wheels * 1000};
    /* NX160 Pistonight raw_ptr layout: current/max stamina are the adjacent
       floats at [[main+0x2c9fd70]+0x38]+0x2ac / +0x2b0. Validate both before
       changing GameData, then update the live component in one checked write. */
    uintptr_t address = be_chain(base, 0x2c9fd70, 0x38, 0x2ac);
    if (wheels < 1 || wheels > 3 || !address || !bw_live_player(base, &player) ||
        !be_memory_span(address, sizeof old_live, VM_PROT_READ | VM_PROT_WRITE) ||
        !be_read(address, old_live, sizeof old_live) ||
        !isfinite(old_live[0]) || !isfinite(old_live[1]) ||
        old_live[1] < 1000 || old_live[1] > 3000 ||
        old_live[0] < 0 || old_live[0] > old_live[1] * 1.5f ||
        !be_world_get_f32(base, "StaminaCurrentMax", &old_flags[0]) || old_flags[0] < 1000 || old_flags[0] > 3000 ||
        !be_world_get_f32(base, "StaminaMax", &old_flags[1]) || old_flags[1] < 1000 || old_flags[1] > 3000 ||
        !be_read(player.info + 0xa4, old_cache, sizeof old_cache) ||
        !isfinite(old_cache[0]) || !isfinite(old_cache[1]) ||
        old_cache[0] < 1000 || old_cache[0] > 3000 || old_cache[1] < 1000 || old_cache[1] > 3000 ||
        !be_read(player.actor + 0x186c, &old_actor, 4) || !isfinite(old_actor) || old_actor < 1000 || old_actor > 3000) {
        snprintf(result, capacity, "The live stamina component is unavailable; nothing was changed");
        return 0;
    }
    if (!be_world_set_f32(base, "StaminaMax", target[1])) {
        snprintf(result, capacity, "The game could not apply the maximum-stamina edit");
        return 0;
    }
    if (be_world_set_f32(base, "StaminaCurrentMax", target[0]) && bw_same_player(base, &player) &&
        be_chain(base, 0x2c9fd70, 0x38, 0x2ac) == address &&
        be_write(player.info + 0xa4, target, sizeof target) &&
        be_write(player.actor + 0x186c, &target[1], 4) && be_write(address, target, sizeof target)) {
        snprintf(result, capacity, "Maximum stamina updated and refilled in the live player");
        return 1;
    }
    int restored = be_world_set_f32(base, "StaminaMax", old_flags[1]);
    restored &= be_world_set_f32(base, "StaminaCurrentMax", old_flags[0]);
    if (bw_same_player(base, &player)) {
        restored &= be_write(player.info + 0xa4, old_cache, sizeof old_cache);
        restored &= be_write(player.actor + 0x186c, &old_actor, 4);
    } else restored = 0;
    if (be_chain(base, 0x2c9fd70, 0x38, 0x2ac) == address)
        restored &= be_write(address, old_live, sizeof old_live);
    else restored = 0;
    snprintf(result, capacity, "%s", restored ? "The live stamina edit failed; original values restored" :
        "The live stamina edit failed and rollback was incomplete; restore the profile backup before saving");
    return 0;
}
static int be_world_command(uintptr_t base, const char *verb, const char *argument, int value,
                            char *result, size_t capacity) {
    if (!strcmp(verb, "max_hearts")) {
        return bw_set_live_hearts(base, value, result, capacity);
    }
    if (!strcmp(verb, "max_stamina")) {
        return bw_set_live_stamina(base, value, result, capacity);
    }
    if (!strcmp(verb, "rupees")) {
        int ok = value >= 0 && value <= 999999 && be_world_set_s32(base, "CurrentRupee", value);
        snprintf(result, capacity, "%s", ok ? "Rupees changed and read back from the game" : "The game could not apply the rupee edit");
        return ok;
    }
    if (!strcmp(verb, "unlock_shrine")) {
        if (!bw_shrine_identifier(argument)) { snprintf(result, capacity, "Select a base-game shrine"); return 0; }
        char flag[32]; snprintf(flag, sizeof flag, "Open_%s", argument);
        const char *names[] = {flag};
        int ok = bw_unlock_flags(base, names, 1, result, capacity);
        if (ok) snprintf(result, capacity, "%s activated for travel; shrine completion unchanged", argument);
        return ok;
    }
    if (!strcmp(verb, "unlock_all_shrines")) {
        char flags[120][32]; const char *names[120];
        for (int i = 0; i < 120; ++i) { snprintf(flags[i], sizeof flags[i], "Open_Dungeon%03d", i); names[i] = flags[i]; }
        int ok = bw_unlock_flags(base, names, 120, result, capacity);
        if (ok) snprintf(result, capacity, "All 120 base-game shrines activated; shrine completion unchanged");
        return ok;
    }
    if (!strcmp(verb, "reveal_map")) {
        char flags[15][32]; const char *names[15];
        for (int i = 0; i < 15; ++i) { snprintf(flags[i], sizeof flags[i], "MapTower_%02d", i + 1); names[i] = flags[i]; }
        int ok = bw_unlock_flags(base, names, 15, result, capacity);
        if (ok) snprintf(result, capacity, "All 15 tower map regions activated and read back");
        return ok;
    }
    if (!strcmp(verb, "runes")) {
        int ok = bw_unlock_flags(base, bw_rune_flags, sizeof bw_rune_flags / sizeof *bw_rune_flags, result, capacity);
        if (ok) snprintf(result, capacity, "Base rune acquisition flags enabled; reopen the rune menu to refresh");
        return ok;
    }
    if (!strcmp(verb, "paraglider")) {
        const char *names[] = {"IsGet_PlayerStole2"};
        int ok = bw_unlock_flags(base, names, 1, result, capacity);
        if (ok) snprintf(result, capacity, "Paraglider acquisition flag enabled and read back");
        return ok;
    }
    if (!strcmp(verb, "full_access")) {
        char flags[135][32]; const char *names[144];
        for (int i = 0; i < 120; ++i) {
            snprintf(flags[i], sizeof flags[i], "Open_Dungeon%03d", i); names[i] = flags[i];
        }
        for (int i = 0; i < 15; ++i) {
            snprintf(flags[120 + i], sizeof flags[120 + i], "MapTower_%02d", i + 1); names[120 + i] = flags[120 + i];
        }
        for (size_t i = 0; i < 7; ++i) names[135 + i] = bw_rune_flags[i];
        names[142] = "IsGet_PlayerStole2";
        names[143] = "MapTower_07_Demo";
        int ok = bw_unlock_flags(base, names, 144, result, capacity);
        if (ok) snprintf(result, capacity, "Travel access enabled: 120 shrines, 15 map regions, base runes, paraglider and towers raised (opening tower sequence skipped)");
        return ok;
    }
    return 0;
}
static void be_world_snapshot(FILE *output, uintptr_t base, int ready) {
    int32_t rupees = 0;
    fputs(",\"rupees\":", output);
    if (ready && be_world_get_s32(base, "CurrentRupee", &rupees)) fprintf(output, "%d", rupees);
    else fputs("null", output);
    int32_t hearts = 0;
    fputs(",\"max_hearts\":", output);
    if (ready && be_world_get_s32(base, "MaxHartValue", &hearts) && hearts >= 4 && hearts <= 120) fprintf(output, "%.2f", hearts / 4.0);
    else fputs("null", output);
    float stamina = 0;
    fputs(",\"max_stamina\":", output);
    if (ready && be_world_get_f32(base, "StaminaMax", &stamina) && stamina >= 1000 && stamina <= 3000) fprintf(output, "%.2f", (double)stamina / 1000);
    else fputs("null", output);
    BwLivePlayer player;
    int32_t live_hearts = 0;
    float live_stamina = 0, caches[3] = {0};
    int cache_ready = bw_live_player(base, &player) &&
        be_read(player.info + 0xa0, caches, sizeof caches) &&
        isfinite(caches[0]) && caches[0] >= 4 && caches[0] <= 120 &&
        isfinite(caches[1]) && caches[1] >= 1000 && caches[1] <= 3000 &&
        isfinite(caches[2]) && caches[2] >= 1000 && caches[2] <= 3000 &&
        be_read(player.actor + 0x1868, &live_hearts, 4) && live_hearts >= 4 && live_hearts <= 120 &&
        be_read(player.actor + 0x186c, &live_stamina, 4) &&
        isfinite(live_stamina) && live_stamina >= 1000 && live_stamina <= 3000;
    fprintf(output, ",\"player_cache_ready\":%s,\"max_hearts_live\":", cache_ready ? "true" : "false");
    if (cache_ready) fprintf(output, "%.2f", live_hearts / 4.0); else fputs("null", output);
    fputs(",\"max_stamina_live\":", output);
    if (cache_ready) fprintf(output, "%.2f", (double)live_stamina / 1000); else fputs("null", output);
    int raised = 0;
    fputs(",\"towers_raised\":", output);
    if (ready && be_world_get_bool(base, "MapTower_07_Demo", &raised)) fputs(raised ? "true" : "false", output);
    else fputs("null", output);
}

#endif
