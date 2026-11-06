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
    "IsGet_Obj_Magnetglove", "IsGet_Obj_StopTimer", "IsGet_Obj_IceMaker", "IsGet_Obj_Camera",
    "PortableUnit_PowerOn", "IsGet_PortableUnit"
};
/* NX160 Demo700_0's slate pickup enables these two repeatable saved flags.
   Map access is separate from item acquisition, opening doors and tower maps. */
static const char *const bw_map_access_flags[] = {"PortableUnit_PowerOn", "IsGet_PortableUnit"};

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

/* NX160's own 100enemy event sets additive attack to 10/20, then adds 10
   and enables FullPower after the final course. Bootup GameData defines saved,
   writable S32 0..30 and Bool flags. Beam power and quest flags are independent.
   The full-power Bool is one-trigger: its verified native bypass parameter is
   needed to lower a user-selected level or restore a failed transaction. */
static const char bw_master_sword_power[] = "MasterSword_Add_Power";
static const char bw_master_sword_full[] = "Open_MasterSword_FullPower";
typedef struct { int32_t power; int full; } BwMasterSwordValues;
typedef struct {
    int (*get_s32)(uintptr_t, uintptr_t, const char *, int32_t *);
    int (*get_bool)(uintptr_t, uintptr_t, const char *, int *);
    int (*put_s32)(uintptr_t, uintptr_t, const char *, int32_t);
    int (*put_bool)(uintptr_t, uintptr_t, const char *, int, int);
} BwWorldFlagAccess;

static int bw_master_sword_values_valid(const BwMasterSwordValues *value) {
    return value->power >= 0 && value->power <= 30 && (value->full == 0 || value->full == 1);
}
static int bw_master_sword_level(const BwMasterSwordValues *value) {
    if (!bw_master_sword_values_valid(value)) return -1;
    if (value->full) return value->power == 30 ? 3 : -1;
    return value->power < 30 && value->power % 10 == 0 ? value->power / 10 : -1;
}
static int bw_master_sword_read(uintptr_t base, uintptr_t parameter, BwMasterSwordValues *value,
                                const BwWorldFlagAccess *access) {
    return access->get_s32(base, parameter, bw_master_sword_power, &value->power) &&
        access->get_bool(base, parameter, bw_master_sword_full, &value->full) && bw_master_sword_values_valid(value);
}
/* The same transaction runs against the native adapters and synthetic fixtures.
   Read both stores before writing either, verify both flags afterward, and roll
   back every store even when a setter reports failure after changing its value. */
static int bw_master_sword_transaction(uintptr_t base, const BeWorldRefs *refs, int level,
                                       const BwWorldFlagAccess *access, int *restored) {
    uintptr_t parameters[2] = {refs->pending, refs->primary};
    int count = refs->pending == refs->primary ? 1 : 2;
    BwMasterSwordValues before[2], after;
    *restored = 1;
    if (level < 0 || level > 3 || !refs->pending || !refs->primary) return 0;
    BwMasterSwordValues target = {level * 10, level == 3};
    for (int i = 0; i < count; ++i)
        if (!bw_master_sword_read(base, parameters[i], &before[i], access)) return 0;
    int ok = 1;
    for (int i = 0; i < count && ok; ++i) {
        ok = access->put_s32(base, parameters[i], bw_master_sword_power, target.power) &&
            access->put_bool(base, parameters[i], bw_master_sword_full, target.full, 1);
    }
    for (int i = 0; i < count && ok; ++i)
        ok = bw_master_sword_read(base, parameters[i], &after, access) &&
            after.power == target.power && after.full == target.full;
    if (ok) return 1;
    for (int i = count - 1; i >= 0; --i) {
        int full_ok = access->put_bool(base, parameters[i], bw_master_sword_full, before[i].full, 1);
        int power_ok = access->put_s32(base, parameters[i], bw_master_sword_power, before[i].power);
        int read_ok = bw_master_sword_read(base, parameters[i], &after, access) &&
            after.power == before[i].power && after.full == before[i].full;
        *restored &= full_ok && power_ok && read_ok;
    }
    return 0;
}
static const BwWorldFlagAccess bw_world_native_access = {
    bw_get_s32_param, bw_get_bool_param, bw_put_s32, bw_put_bool
};
static int bw_master_sword_refs(uintptr_t base, BeWorldRefs *refs) {
    BwMasterSwordValues values;
    return bw_refs(base, refs, 1) &&
        bw_flag_writable(refs->primary, bw_master_sword_power, 1) &&
        bw_flag_writable(refs->pending, bw_master_sword_power, 1) &&
        bw_flag_writable(refs->primary, bw_master_sword_full, 0) &&
        bw_flag_writable(refs->pending, bw_master_sword_full, 0) &&
        bw_master_sword_read(base, refs->primary, &values, &bw_world_native_access) &&
        bw_master_sword_read(base, refs->pending, &values, &bw_world_native_access);
}
#if defined(GAME_BOTW160) && !TARGET_OS_IPHONE
static const char *bw_master_sword_item_block(const BeiItem *items, int count) {
    int found = 0, equipped = 0;
    for (int i = 0; i < count; ++i) if (!strcmp(items[i].name, "Weapon_Sword_070")) {
        if (items[i].type != 0) return "The Master Sword inventory entry could not be validated.";
        ++found; equipped |= items[i].equipped;
    }
    if (!found) return "Add the Master Sword (Weapon_Sword_070) to your pouch first.";
    if (found != 1) return "The Master Sword inventory entry is ambiguous; no level was changed.";
    return equipped ? "Unequip the Master Sword in BOTW before changing its level." : NULL;
}
#endif
static int bw_set_master_sword(uintptr_t base, int level, char *result, size_t capacity) {
    if (level < 0 || level > 3) { snprintf(result, capacity, "Choose a Master Sword level from 0 to 3."); return 0; }
#if defined(GAME_BOTW160) && !TARGET_OS_IPHONE
    uintptr_t manager = bei_api(base) ? bei_manager(base) : 0;
    if (!manager) { snprintf(result, capacity, "Wait for the Master Sword inventory to load."); return 0; }
    BeiItem items[BEI_SLOTS]; int count = 0, ok = 0;
    bei_mutex(base, manager, 1);
    const char *blocked = bei_inventory_block_reason(manager, bei_scan(base, manager, items, &count));
    if (!blocked) blocked = bw_master_sword_item_block(items, count);
    if (blocked) { snprintf(result, capacity, "%s", blocked); goto done; }
    BeWorldRefs refs;
    if (!bw_master_sword_refs(base, &refs)) {
        snprintf(result, capacity, "Master Sword flags are unavailable or read-only; nothing was changed."); goto done;
    }
    int restored = 1;
    ok = bw_master_sword_transaction(base, &refs, level, &bw_world_native_access, &restored);
    if (ok) snprintf(result, capacity, "Master Sword level %d configured (%d base attack). Reopen the game menu to refresh; nearby evil can still raise attack to 60.", level, 30 + level * 10);
    else snprintf(result, capacity, "%s", restored ? "The level edit failed; original Master Sword flags were restored." :
        "The level edit failed and rollback was incomplete; restore the profile backup before saving.");
done:
    bei_mutex(base, manager, 0);
    return ok;
#else
    (void)base; snprintf(result, capacity, "Master Sword levels require the supported BOTW 1.6.0 runtime."); return 0;
#endif
}

/* NX160 Bootup GameData defines the same saved, writable S32 ranges as the
   native pouch's hard category limits. Capacity edits never grant equipment or
   alter Korok/Hestu progression. Decreases are refused in both game stores. */
typedef struct {
    const char *token, *flag;
    int minimum, maximum;
} BwPouchSlotKind;
static const BwPouchSlotKind bw_pouch_slot_kinds[] = {
    {"weapons", "WeaponPorchStockNum", BE_WEAPON_SLOTS_MIN, BE_WEAPON_SLOTS_MAX},
    {"bows", "BowPorchStockNum", BE_BOW_SLOTS_MIN, BE_BOW_SLOTS_MAX},
    {"shields", "ShieldPorchStockNum", BE_SHIELD_SLOTS_MIN, BE_SHIELD_SLOTS_MAX}
};
static const BwPouchSlotKind *bw_pouch_slot_kind(const char *token) {
    for (size_t i = 0; i < sizeof bw_pouch_slot_kinds / sizeof *bw_pouch_slot_kinds; ++i)
        if (!strcmp(token, bw_pouch_slot_kinds[i].token)) return &bw_pouch_slot_kinds[i];
    return NULL;
}
static int bw_pouch_slots_valid(const BwPouchSlotKind *kind, int value) {
    return kind && value >= kind->minimum && value <= kind->maximum;
}
static int bw_pouch_slots_ready(uintptr_t base, const BeWorldRefs *refs, const BwPouchSlotKind *kind,
                                int32_t *primary, int32_t *pending) {
    return kind && bw_flag_writable(refs->primary, kind->flag, 1) &&
        bw_flag_writable(refs->pending, kind->flag, 1) &&
        bw_get_s32_param(base, refs->primary, kind->flag, primary) && bw_pouch_slots_valid(kind, *primary) &&
        bw_get_s32_param(base, refs->pending, kind->flag, pending) && bw_pouch_slots_valid(kind, *pending);
}
static int bw_pouch_slots_transaction(uintptr_t base, const BeWorldRefs *refs, const BwPouchSlotKind *kind,
                                      int target, const BwWorldFlagAccess *access, int *restored) {
    uintptr_t parameters[2] = {refs->pending, refs->primary};
    int count = refs->pending == refs->primary ? 1 : 2;
    int32_t before[2], after = 0;
    *restored = 1;
    if (!bw_pouch_slots_valid(kind, target) || !refs->pending || !refs->primary) return 0;
    for (int i = 0; i < count; ++i)
        if (!access->get_s32(base, parameters[i], kind->flag, &before[i]) ||
            !bw_pouch_slots_valid(kind, before[i]) || target < before[i]) return 0;
    int ok = 1;
    for (int i = 0; i < count && ok; ++i)
        ok = access->put_s32(base, parameters[i], kind->flag, target);
    for (int i = 0; i < count && ok; ++i)
        ok = access->get_s32(base, parameters[i], kind->flag, &after) && after == target;
    if (ok) return 1;
    for (int i = count - 1; i >= 0; --i) {
        int write_ok = access->put_s32(base, parameters[i], kind->flag, before[i]);
        int read_ok = access->get_s32(base, parameters[i], kind->flag, &after) && after == before[i];
        *restored &= write_ok && read_ok;
    }
    return 0;
}
static int bw_set_pouch_slots(uintptr_t base, const char *token, int target, char *result, size_t capacity) {
    const BwPouchSlotKind *kind = bw_pouch_slot_kind(token);
    if (!bw_pouch_slots_valid(kind, target)) {
        snprintf(result, capacity, "Choose weapons 8-20, bows 5-14, or shields 4-20 slots."); return 0;
    }
    BeWorldRefs refs; int32_t primary = 0, pending = 0;
    if (!bw_refs(base, &refs, 1) || !bw_pouch_slots_ready(base, &refs, kind, &primary, &pending)) {
        snprintf(result, capacity, "Pouch capacity flags are unavailable or read-only; nothing was changed."); return 0;
    }
    if (target < primary || target < pending) {
        snprintf(result, capacity, "Slots can only increase; choose at least %d for %s.", primary > pending ? primary : pending, kind->token); return 0;
    }
    int restored = 1;
    int ok = bw_pouch_slots_transaction(base, &refs, kind, target, &bw_world_native_access, &restored);
    if (ok) snprintf(result, capacity, "%s capacity set to %d and read back. Reopen the game inventory; no items or Hestu progress changed.", kind->token, target);
    else snprintf(result, capacity, "%s", restored ? "The slot edit failed; original capacities were restored." :
        "The slot edit failed and rollback was incomplete; restore the profile backup before saving.");
    return ok;
}

enum { BW_MAX_TRANSACTION = 160 };
static int bw_unlock_flag_transaction(uintptr_t base, const BeWorldRefs *refs, const char *const *names,
                                       size_t count, const BwWorldFlagAccess *access, int *restored) {
    int before_primary[BW_MAX_TRANSACTION], before_pending[BW_MAX_TRANSACTION];
    *restored = 1;
    if (!count || count > BW_MAX_TRANSACTION || !refs->primary || !refs->pending) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (!access->get_bool(base, refs->primary, names[i], &before_primary[i]) ||
            !access->get_bool(base, refs->pending, names[i], &before_pending[i]) ||
            before_primary[i] < 0 || before_primary[i] > 1 || before_pending[i] < 0 || before_pending[i] > 1) return 0;
    }
    int ok = 1;
    for (size_t i = 0; i < count && ok; ++i)
        ok = access->put_bool(base, refs->pending, names[i], 1, 0) &&
            (refs->primary == refs->pending || access->put_bool(base, refs->primary, names[i], 1, 0));
    for (size_t i = 0; i < count && ok; ++i) {
        int primary = 0, pending = 0;
        ok = access->get_bool(base, refs->primary, names[i], &primary) && primary == 1 &&
            access->get_bool(base, refs->pending, names[i], &pending) && pending == 1;
    }
    if (ok) return 1;
    for (size_t i = count; i-- > 0;) {
        int pending_ok = access->put_bool(base, refs->pending, names[i], before_pending[i], 1);
        int primary_ok = refs->primary == refs->pending || access->put_bool(base, refs->primary, names[i], before_primary[i], 1);
        int primary = -1, pending = -1;
        int read_ok = access->get_bool(base, refs->primary, names[i], &primary) && primary == before_primary[i] &&
            access->get_bool(base, refs->pending, names[i], &pending) && pending == before_pending[i];
        *restored &= pending_ok && primary_ok && read_ok;
    }
    return 0;
}
static int bw_unlock_flags(uintptr_t base, const char *const *names, size_t count,
                           char *result, size_t capacity) {
    BeWorldRefs refs;
    if (!count || count > BW_MAX_TRANSACTION || !bw_refs(base, &refs, 1)) {
        snprintf(result, capacity, "World flags are not ready for editing"); return 0;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!bw_bool_editable(refs.primary, names[i]) || !bw_bool_editable(refs.pending, names[i])) {
            snprintf(result, capacity, "Flag %s is unavailable; nothing was changed", names[i]); return 0;
        }
    }
    int restored = 1;
    if (!bw_unlock_flag_transaction(base, &refs, names, count, &bw_world_native_access, &restored)) {
        snprintf(result, capacity, "%s", restored ? "The game rejected an edit; original flags restored" :
            "The game rejected an edit and rollback was incomplete; restore the profile backup before saving");
        return 0;
    }
    snprintf(result, capacity, "%zu world flags updated and read back", count);
    return 1;
}
static int bw_shrine_identifier(const char *name) {
    if (strlen(name) != 10 || strncmp(name, "Dungeon", 7)) return 0;
    for (int i = 7; i < 10; ++i) if (name[i] < '0' || name[i] > '9') return 0;
    return atoi(name + 7) < 120;
}
/* The game's real map markers use Location_Dungeon### as their SaveFlag;
   Open_Dungeon### alone does not discover a shrine on that map. */
static int bw_discovery_permissions(const char *name, int kind, unsigned properties) {
    if (kind != 1 || strncmp(name, "Location_", 9) || !bw_shrine_identifier(name + 9) || !(properties & 1u)) return -1;
    if (properties & 2u) return 1;
    /* Own NX160 metadata: only Location_Dungeon001 is readable/saveable but
       not program-writable (0x05), with no one-trigger property. The verified
       native S32 setter accepts check_permissions=false for this exact flag. */
    return !strcmp(name, "Location_Dungeon001") && properties == 0x05u ? 0 : -1;
}
static int bw_discovery_native_permissions(uintptr_t parameter, const char *name) {
    uintptr_t flag = bw_find_flag(parameter, name, 1); unsigned char properties = 0;
    if (!flag || !be_read(flag + 9, &properties, 1)) return -1;
    return bw_discovery_permissions(name, 1, properties);
}
static int bw_discovery_bypass_code(uintptr_t base) {
    return bw_code(base, BW_SET_S32 + 0xe4, 0x12000283u) && bw_code(base, 0x163c1b4, 0x36000123u);
}
static int bw_put_discovery_s32(uintptr_t base, uintptr_t parameter, const char *name, int32_t value) {
    int permissions = bw_discovery_native_permissions(parameter, name);
    if (permissions < 0) return 0;
    if (permissions) return bw_put_s32(base, parameter, name, value);
    /* Pin both permission forwarding and its native property check before
       using the exceptional call. No other S32 writer changes permissions. */
    if (!bw_discovery_bypass_code(base)) return 0;
    int32_t current = 0;
    if (!bw_get_s32_param(base, parameter, name, &current)) return 0;
    if (current == value) return 1;
    BeWorldString string = bw_string(base, name);
    int result = ((BeWorldSetS32)(base + BW_SET_S32))((void *)parameter, value, &string, 0);
    return (result & 1) && bw_get_s32_param(base, parameter, name, &current) && current == value;
}
/* Full Access uses266 edits; completion uses361 including its single counter. */
enum { BW_MAX_ACTIVATION_EDITS = 384 };
typedef struct { const char *name; int kind; } BwActivationEdit;
static int bw_activation_read(uintptr_t base, uintptr_t parameter, const BwActivationEdit *edit,
                               const BwWorldFlagAccess *access, int32_t *value) {
    if (edit->kind == 1 || edit->kind == 2) return access->get_s32(base, parameter, edit->name, value);
    if (edit->kind != 0) return 0;
    int current = -1;
    if (!access->get_bool(base, parameter, edit->name, &current) || current < 0 || current > 1) return 0;
    *value = current; return 1;
}
static int bw_activation_put(uintptr_t base, uintptr_t parameter, const BwActivationEdit *edit,
                              int32_t value, int rollback, const BwWorldFlagAccess *access) {
    return edit->kind == 1 || edit->kind == 2 ? access->put_s32(base, parameter, edit->name, value) :
        edit->kind == 0 && access->put_bool(base, parameter, edit->name, value, rollback);
}
static int bw_activation_transaction(uintptr_t base, const BeWorldRefs *refs, const BwActivationEdit *edits,
                                      size_t count, const BwWorldFlagAccess *access, int *restored) {
    int32_t before[BW_MAX_ACTIVATION_EDITS][2], targets[BW_MAX_ACTIVATION_EDITS][2], value = 0;
    uintptr_t parameters[2] = {refs->pending, refs->primary};
    int stores = refs->pending == refs->primary ? 1 : 2;
    *restored = 1;
    if (!count || count > BW_MAX_ACTIVATION_EDITS || !refs->pending || !refs->primary) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (!edits[i].name || !*edits[i].name || edits[i].kind < 0 || edits[i].kind > 2 ||
            (edits[i].kind == 2 && strcmp(edits[i].name, "DungeonClearCounter"))) return 0;
        for (size_t j = 0; j < i; ++j) if (!strcmp(edits[i].name, edits[j].name)) return 0;
    }
    for (size_t i = 0; i < count; ++i) for (int store = 0; store < stores; ++store) {
        if (!bw_activation_read(base, parameters[store], &edits[i], access, &before[i][store])) return 0;
        targets[i][store] = edits[i].kind == 1 && before[i][store] > 1 ? before[i][store] : 1;
    }
    /* A completion batch accounts only newly false->true Clear flags. Require
       both stores to agree before calculating its single saved counter target. */
    int counter = -1, clear_flags = 0, newly_cleared = 0;
    for (size_t i = 0; i < count; ++i) {
        if (edits[i].kind == 2) counter = (int)i;
        if (!strncmp(edits[i].name, "Clear_", 6)) {
            if (edits[i].kind || !bw_shrine_identifier(edits[i].name + 6) ||
                (stores == 2 && before[i][0] != before[i][1])) return 0;
            ++clear_flags; newly_cleared += !before[i][0];
        }
    }
    if ((counter >= 0) != (clear_flags > 0)) return 0;
    if (counter >= 0) {
        int32_t old = before[counter][0];
        if (old < 0 || old > 255 || (stores == 2 && old != before[counter][1]) || newly_cleared > 255 - old) return 0;
        for (int store = 0; store < stores; ++store) targets[counter][store] = old + newly_cleared;
    }
    int ok = 1;
    for (size_t i = 0; i < count && ok; ++i) for (int store = 0; store < stores && ok; ++store)
        ok = bw_activation_put(base, parameters[store], &edits[i], targets[i][store], 0, access);
    for (size_t i = 0; i < count && ok; ++i) for (int store = 0; store < stores && ok; ++store)
        ok = bw_activation_read(base, parameters[store], &edits[i], access, &value) && value == targets[i][store];
    if (ok) return 1;
    for (size_t i = count; i-- > 0;) for (int store = stores; store-- > 0;) {
        int write_ok = bw_activation_put(base, parameters[store], &edits[i], before[i][store], 1, access);
        int read_ok = bw_activation_read(base, parameters[store], &edits[i], access, &value) && value == before[i][store];
        *restored &= write_ok && read_ok;
    }
    return 0;
}
static int bw_activate_shrines(uintptr_t base, const char *identifier, const char *const *additional,
                               size_t additional_count, char *result, size_t capacity) {
    BeWorldRefs refs;
    unsigned shrines = identifier ? 1 : 120;
    if ((identifier && !bw_shrine_identifier(identifier)) || additional_count > BW_MAX_ACTIVATION_EDITS - shrines * 2 ||
        !bw_refs(base, &refs, 1)) {
        snprintf(result, capacity, "Shrine discovery flags are not ready; nothing was changed."); return 0;
    }
    char names[240][32]; BwActivationEdit edits[BW_MAX_ACTIVATION_EDITS]; size_t count = 0;
    for (unsigned i = 0; i < shrines; ++i) {
        char id[16]; snprintf(id, sizeof id, "Dungeon%03u", identifier ? (unsigned)atoi(identifier + 7) : i);
        snprintf(names[count], sizeof names[count], "Open_%s", id);
        edits[count] = (BwActivationEdit){names[count], 0}; ++count;
        snprintf(names[count], sizeof names[count], "Location_%s", id);
        edits[count] = (BwActivationEdit){names[count], 1}; ++count;
    }
    for (size_t i = 0; i < additional_count; ++i) edits[count++] = (BwActivationEdit){additional[i], 0};
    for (size_t i = 0; i < count; ++i) {
        int primary = edits[i].kind ? bw_discovery_native_permissions(refs.primary, edits[i].name) : bw_bool_native_permissions(refs.primary, edits[i].name);
        int pending = edits[i].kind ? bw_discovery_native_permissions(refs.pending, edits[i].name) : bw_bool_native_permissions(refs.pending, edits[i].name);
        if (primary < 0 || pending < 0 || ((!primary || !pending) && edits[i].kind && !bw_discovery_bypass_code(base))) {
            snprintf(result, capacity, "Flag %s is unavailable; nothing was changed.", edits[i].name); return 0;
        }
    }
    const BwWorldFlagAccess access = {bw_get_s32_param, bw_get_bool_param, bw_put_discovery_s32, bw_put_bool};
    int restored = 1;
    int ok = bw_activation_transaction(base, &refs, edits, count, &access, &restored);
    if (!ok) snprintf(result, capacity, "%s", restored ? "Shrine activation failed; original discovery and access flags were restored." :
        "Shrine activation failed and rollback was incomplete; restore the profile backup before saving.");
    return ok;
}
/* The monk's separate completion event changes Clear and the saved clear
   counter. This action deliberately grants no orbs/items and advances no quests.
   DungeonClearCounter is an exact saved/readable S32 permission exception. */
static int bw_completion_counter_permissions(const char *name, int kind, unsigned properties) {
    return kind == 1 && properties == 0x05u && !strcmp(name, "DungeonClearCounter") ? 0 : -1;
}
static int bw_completion_counter_native_permissions(uintptr_t parameter) {
    uintptr_t flag = bw_find_flag(parameter, "DungeonClearCounter", 1); unsigned char properties = 0;
    return flag && be_read(flag + 9, &properties, 1) ?
        bw_completion_counter_permissions("DungeonClearCounter", 1, properties) : -1;
}
static int bw_put_completion_s32(uintptr_t base, uintptr_t parameter, const char *name, int32_t value) {
    if (strcmp(name, "DungeonClearCounter")) return bw_put_discovery_s32(base, parameter, name, value);
    if (value < 0 || value > 255 || bw_completion_counter_native_permissions(parameter) < 0 || !bw_discovery_bypass_code(base)) return 0;
    int32_t current = 0;
    if (!bw_get_s32_param(base, parameter, name, &current) || current < 0 || current > 255) return 0;
    if (current == value) return 1;
    BeWorldString string = bw_string(base, name);
    int result = ((BeWorldSetS32)(base + BW_SET_S32))((void *)parameter, value, &string, 0);
    return (result & 1) && bw_get_s32_param(base, parameter, name, &current) && current == value;
}
static int bw_complete_shrines(uintptr_t base, const char *identifier, char *result, size_t capacity) {
    BeWorldRefs refs;
    if ((identifier && !bw_shrine_identifier(identifier)) || !bw_refs(base, &refs, 1) || !bw_discovery_bypass_code(base) ||
        bw_completion_counter_native_permissions(refs.primary) < 0 || bw_completion_counter_native_permissions(refs.pending) < 0) {
        snprintf(result, capacity, "Shrine completion flags are unavailable; nothing was changed."); return 0;
    }
    char names[360][32]; BwActivationEdit edits[BW_MAX_ACTIVATION_EDITS]; size_t count = 0;
    for (unsigned i = 0; i < (identifier ? 1u : 120u); ++i) {
        char id[16]; snprintf(id, sizeof id, "Dungeon%03u", identifier ? (unsigned)atoi(identifier + 7) : i);
        snprintf(names[count], sizeof names[count], "Open_%s", id); edits[count] = (BwActivationEdit){names[count], 0}; ++count;
        snprintf(names[count], sizeof names[count], "Location_%s", id); edits[count] = (BwActivationEdit){names[count], 1}; ++count;
        snprintf(names[count], sizeof names[count], "Clear_%s", id); edits[count] = (BwActivationEdit){names[count], 0}; ++count;
    }
    edits[count++] = (BwActivationEdit){"DungeonClearCounter", 2};
    for (size_t i = 0; i + 1 < count; ++i) {
        int primary = edits[i].kind ? bw_discovery_native_permissions(refs.primary, edits[i].name) : bw_bool_native_permissions(refs.primary, edits[i].name);
        int pending = edits[i].kind ? bw_discovery_native_permissions(refs.pending, edits[i].name) : bw_bool_native_permissions(refs.pending, edits[i].name);
        if (primary < 0 || pending < 0) {
            snprintf(result, capacity, "Flag %s is unavailable; nothing was changed.", edits[i].name); return 0;
        }
    }
    const BwWorldFlagAccess access = {bw_get_s32_param, bw_get_bool_param, bw_put_completion_s32, bw_put_bool};
    int restored = 1;
    int ok = bw_activation_transaction(base, &refs, edits, count, &access, &restored);
    if (ok) snprintf(result, capacity, "Marked %s cleared; Spirit Orbs and quests unchanged. Reopen BOTW's map.", identifier ? identifier : "all 120 base shrines");
    else snprintf(result, capacity, "%s", restored ?
        "Completion was refused or failed; check that both game stores agree and the counter has room. Original flags were preserved or restored." :
        "Shrine completion failed and rollback was incomplete; restore the profile backup before saving.");
    return ok;
}
static unsigned be_world_features(uintptr_t base) {
    BeWorldRefs refs;
    if (!bw_refs(base, &refs, 1)) return 0;
    unsigned features = 0;
    int32_t rupees = 0;
    if (bw_get_s32_param(base, refs.primary, "CurrentRupee", &rupees) && rupees >= 0 && rupees <= 999999 &&
        bw_flag_writable(refs.pending, "CurrentRupee", 1)) features |= BE_FEATURE_RUPEES;
    int value = 0;
    if (bw_get_bool_param(base, refs.primary, "Open_Dungeon000", &value) &&
        bw_bool_editable(refs.primary, "Open_Dungeon000") && bw_bool_editable(refs.pending, "Open_Dungeon000") &&
        bw_discovery_native_permissions(refs.primary, "Location_Dungeon000") >= 0 &&
        bw_discovery_native_permissions(refs.pending, "Location_Dungeon000") >= 0 &&
        bw_discovery_native_permissions(refs.primary, "Location_Dungeon001") >= 0 &&
        bw_discovery_native_permissions(refs.pending, "Location_Dungeon001") >= 0 && bw_discovery_bypass_code(base)) features |= BE_FEATURE_SHRINES;
    int32_t clear_primary = 0, clear_pending = 0;
    if ((features & BE_FEATURE_SHRINES) && bw_completion_counter_native_permissions(refs.primary) >= 0 &&
        bw_completion_counter_native_permissions(refs.pending) >= 0 &&
        bw_get_s32_param(base, refs.primary, "DungeonClearCounter", &clear_primary) && clear_primary >= 0 && clear_primary <= 255 &&
        bw_get_s32_param(base, refs.pending, "DungeonClearCounter", &clear_pending) && clear_primary == clear_pending &&
        bw_bool_editable(refs.primary, "Clear_Dungeon000") && bw_bool_editable(refs.pending, "Clear_Dungeon000") &&
        bw_get_bool_param(base, refs.primary, "Clear_Dungeon000", &value)) features |= BE_FEATURE_SHRINE_COMPLETION;
    if (bw_bool_editable(refs.primary, "MapTower_01") && bw_bool_editable(refs.pending, "MapTower_01") &&
        bw_get_bool_param(base, refs.primary, "MapTower_01", &value)) features |= BE_FEATURE_MAP;
    if (bw_get_bool_param(base, refs.primary, bw_rune_flags[0], &value)) features |= BE_FEATURE_RUNES;
    BeWorldRefs master_refs;
    if (bw_master_sword_refs(base, &master_refs)) features |= BE_FEATURE_MASTER_SWORD;
    int pouch_ready = 1;
    for (size_t i = 0; i < sizeof bw_pouch_slot_kinds / sizeof *bw_pouch_slot_kinds; ++i) {
        int32_t primary = 0, pending = 0;
        pouch_ready &= bw_pouch_slots_ready(base, &refs, &bw_pouch_slot_kinds[i], &primary, &pending);
    }
    if (pouch_ready) features |= BE_FEATURE_POUCH_SLOTS;
    int map_access_ready = 1;
    for (size_t i = 0; i < sizeof bw_map_access_flags / sizeof *bw_map_access_flags; ++i) {
        int primary = 0, pending = 0;
        map_access_ready &= bw_flag_writable(refs.primary, bw_map_access_flags[i], 0) &&
            bw_flag_writable(refs.pending, bw_map_access_flags[i], 0) &&
            bw_get_bool_param(base, refs.primary, bw_map_access_flags[i], &primary) &&
            bw_get_bool_param(base, refs.pending, bw_map_access_flags[i], &pending);
    }
    if (map_access_ready) features |= BE_FEATURE_MAP_ACCESS;
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
    if (!strcmp(verb, "master_sword")) return bw_set_master_sword(base, value, result, capacity);
    if (!strcmp(verb, "pouch_slots")) return bw_set_pouch_slots(base, argument, value, result, capacity);
    if (!strcmp(verb, "map_access")) {
        int ok = bw_unlock_flags(base, bw_map_access_flags, sizeof bw_map_access_flags / sizeof *bw_map_access_flags, result, capacity);
        if (ok) snprintf(result, capacity, "Sheikah Slate map access enabled and read back. Focus BOTW and press Tab or M.");
        return ok;
    }
    if (!strcmp(verb, "rupees")) {
        int ok = value >= 0 && value <= 999999 && be_world_set_s32(base, "CurrentRupee", value);
        snprintf(result, capacity, "%s", ok ? "Rupees changed and read back from the game" : "The game could not apply the rupee edit");
        return ok;
    }
    if (!strcmp(verb, "unlock_shrine")) {
        if (!bw_shrine_identifier(argument)) { snprintf(result, capacity, "Select a base-game shrine"); return 0; }
        int ok = bw_activate_shrines(base, argument, NULL, 0, result, capacity);
        if (ok) snprintf(result, capacity, "%s discovered and activated. Reopen BOTW's map; shrine completion and rewards are unchanged.", argument);
        return ok;
    }
    if (!strcmp(verb, "unlock_all_shrines")) {
        int ok = bw_activate_shrines(base, NULL, NULL, 0, result, capacity);
        if (ok) snprintf(result, capacity, "All 120 base-game shrines discovered and activated. Reopen BOTW's map; completion and rewards are unchanged.");
        return ok;
    }
    if (!strcmp(verb, "complete_shrine")) return bw_complete_shrines(base, argument, result, capacity);
    if (!strcmp(verb, "complete_all_shrines")) return bw_complete_shrines(base, NULL, result, capacity);
    if (!strcmp(verb, "reveal_map")) {
        char flags[15][32]; const char *names[15];
        for (int i = 0; i < 15; ++i) { snprintf(flags[i], sizeof flags[i], "MapTower_%02d", i + 1); names[i] = flags[i]; }
        int ok = bw_unlock_flags(base, names, 15, result, capacity);
        if (ok) snprintf(result, capacity, "All 15 tower map regions activated and read back");
        return ok;
    }
    if (!strcmp(verb, "runes")) {
        int ok = bw_unlock_flags(base, bw_rune_flags, sizeof bw_rune_flags / sizeof *bw_rune_flags, result, capacity);
        if (ok) snprintf(result, capacity, "Slate and rune access enabled. In BOTW, hold 3 to choose a rune, release, then press F to use it.");
        return ok;
    }
    if (!strcmp(verb, "paraglider")) {
        const char *names[] = {"IsGet_PlayerStole2"};
        int ok = bw_unlock_flags(base, names, 1, result, capacity);
        if (ok) snprintf(result, capacity, "Paraglider acquisition flag enabled and read back");
        return ok;
    }
    if (!strcmp(verb, "full_access")) {
        char flags[15][32]; const char *names[26]; size_t count = 0;
        for (int i = 0; i < 15; ++i) {
            snprintf(flags[i], sizeof flags[i], "MapTower_%02d", i + 1); names[count++] = flags[i];
        }
        for (size_t i = 0; i < sizeof bw_rune_flags / sizeof *bw_rune_flags; ++i) names[count++] = bw_rune_flags[i];
        names[count++] = "IsGet_PlayerStole2";
        names[count++] = "MapTower_07_Demo";
        int ok = bw_activate_shrines(base, NULL, names, count, result, capacity);
        if (ok) snprintf(result, capacity, "Travel access enabled: 120 discovered shrines, tower maps, Slate/runes, paraglider and raised towers. Reopen BOTW's map; completion and rewards are unchanged.");
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
    BeWorldRefs master_refs;
    BwMasterSwordValues master_values;
    int master_valid = ready && bw_refs(base, &master_refs, 0) &&
        bw_master_sword_read(base, master_refs.primary, &master_values, &bw_world_native_access);
    int master_level = master_valid ? bw_master_sword_level(&master_values) : -1;
    fputs(",\"master_sword_level\":", output);
    if (master_level >= 0) fprintf(output, "%d", master_level); else fputs("null", output);
    fputs(",\"master_sword_attack\":", output);
    if (master_valid) fprintf(output, "%d", 30 + master_values.power); else fputs("null", output);
    fputs(",\"master_sword_full_power\":", output);
    if (master_valid) fputs(master_values.full ? "true" : "false", output); else fputs("null", output);
    static const char *const slot_fields[] = {"weapon_slots", "bow_slots", "shield_slots"};
    for (size_t i = 0; i < sizeof bw_pouch_slot_kinds / sizeof *bw_pouch_slot_kinds; ++i) {
        int32_t slots = 0;
        fprintf(output, ",\"%s\":", slot_fields[i]);
        if (ready && be_world_get_s32(base, bw_pouch_slot_kinds[i].flag, &slots) &&
            bw_pouch_slots_valid(&bw_pouch_slot_kinds[i], slots)) fprintf(output, "%d", slots);
        else fputs("null", output);
    }
    int slate_power = 0, slate_acquired = 0;
    fputs(",\"map_access_unlocked\":", output);
    if (ready && be_world_get_bool(base, bw_map_access_flags[0], &slate_power) &&
        be_world_get_bool(base, bw_map_access_flags[1], &slate_acquired))
        fputs(slate_power && slate_acquired ? "true" : "false", output);
    else fputs("null", output);
    int32_t shrine_clears = 0;
    fputs(",\"shrine_clear_count\":", output);
    if (ready && be_world_get_s32(base, "DungeonClearCounter", &shrine_clears) && shrine_clears >= 0 && shrine_clears <= 255)
        fprintf(output, "%d", shrine_clears);
    else fputs("null", output);
}

#endif
