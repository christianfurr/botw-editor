#ifndef MNG_BOTW_EDITOR_HORSE_H
#define MNG_BOTW_EDITOR_HORSE_H

/* NX160 session-only movement editing for standard horses. This is a shared
   horse-type table, not a mounted-actor pointer or a global time multiplier.
   AnimalUnitSpeed.bin's original bytes remain in the player's installed game.
   Format: https://zeldamods.org/wiki/Content/AnimationDrivenSpeed/AnimalUnitSpeed.bin
   The native getter, table lookup and movement reads below were independently
   disassembled in NX160; NX150 decompilation addresses are not used. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    BH_ENTRY_COUNT = 81, BH_FLOAT_COUNT = 48, BH_ENTRY_SIZE = 196,
    BH_BUFFER_SIZE = 15880, BH_HORSE_COUNT = 26, BH_CELL_COUNT = 104,
    BH_PERCENT_MIN = 100, BH_PERCENT_MAX = 200
};

typedef struct {
    uintptr_t unit, resource, vtable, data;
    size_t size;
} BhResource;

typedef struct {
    void *context;
    int (*resolve)(void *, BhResource *);
    int (*read)(void *, uintptr_t, void *, size_t);
    int (*writable)(void *, uintptr_t, size_t);
    int (*write)(void *, uintptr_t, const void *, size_t);
} BhAccess;

typedef struct { uint64_t whole, keys; } BhFingerprint;
typedef struct {
    int captured, poisoned, percent;
    BhResource identity;
    uint16_t cells[BH_CELL_COUNT];
    unsigned char original[BH_BUFFER_SIZE], owned[BH_BUFFER_SIZE];
} BhSession;

/* Fingerprints, not extracted assets. A modified resource is refused. The full
   fingerprint validates the first baseline; later reads must equal our complete
   expected table byte for byte, including every unrelated animal and neighbor. */
static const BhFingerprint bh_nx160_fingerprint = {
    UINT64_C(0x3026383239aadba5), UINT64_C(0x8026ca2216a5a3c6)
};

static uint64_t bh_hash_add(uint64_t hash, const unsigned char *bytes, size_t size) {
    for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * UINT64_C(0x100000001b3);
    return hash;
}
static uint64_t bh_hash(const unsigned char *bytes, size_t size) {
    return bh_hash_add(UINT64_C(0xcbf29ce484222325), bytes, size);
}
static uint32_t bh_crc(const char *text) {
    uint32_t crc = UINT32_MAX;
    for (; *text; ++text) {
        crc ^= (unsigned char)*text;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & (0u - (crc & 1u)));
    }
    return ~crc;
}
static uint32_t bh_u32(const unsigned char *bytes) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 | (uint32_t)bytes[3] << 24;
}
static float bh_float(const unsigned char *bytes) {
    float value;
    uint32_t bits = bh_u32(bytes);
    memcpy(&value, &bits, sizeof value);
    return value;
}
static int bh_standard_horse(uint32_t key) {
    if (key == bh_crc("GameRomHorse") || key == bh_crc("GameRomHorseEpona") || key == bh_crc("GameRomHorseZelda")) return 1;
    for (int i = 1; i <= 23; ++i) {
        char name[24];
        snprintf(name, sizeof name, "GameRomHorse%02d", i);
        if (key == bh_crc(name)) return 1;
    }
    return 0;
}
static int bh_same_resource(const BhResource *a, const BhResource *b) {
    return a->unit == b->unit && a->resource == b->resource && a->vtable == b->vtable &&
        a->data == b->data && a->size == b->size;
}
static int bh_resource_valid(const BhResource *resource) {
    return resource->unit && resource->resource && resource->vtable && resource->data &&
        !(resource->data & 3u) && resource->size == BH_BUFFER_SIZE && resource->data <= UINTPTR_MAX - BH_BUFFER_SIZE;
}

/* Every key and all 3,888 float values are inspected. Only speed cells for the
   four normal gears are selected; their adjacent animation values stay intact.
   Giant/small/spirit/stalhorse keys and every non-horse key are excluded. */
static int bh_layout(const unsigned char *bytes, size_t size, uint64_t key_fingerprint,
                     uint16_t cells[BH_CELL_COUNT]) {
    if (!bytes || size != BH_BUFFER_SIZE || bh_u32(bytes) != 24) return 0;
    uint64_t keys = UINT64_C(0xcbf29ce484222325);
    uint32_t previous = 0;
    unsigned horses = 0, count = 0;
    for (unsigned entry = 0; entry < BH_ENTRY_COUNT; ++entry) {
        size_t offset = 4 + entry * BH_ENTRY_SIZE;
        uint32_t key = bh_u32(bytes + offset);
        if (!key || (entry && key <= previous)) return 0;
        previous = key;
        keys = bh_hash_add(keys, bytes + offset, 4);
        for (unsigned field = 0; field < BH_FLOAT_COUNT; ++field) {
            float value = bh_float(bytes + offset + 4 + field * 4);
            if (!isfinite(value) || value < 0 || value > 50) return 0;
        }
        if (!bh_standard_horse(key)) continue;
        ++horses;
        for (unsigned gear = 0; gear < 4; ++gear) {
            size_t cell = offset + 4 + gear * 8;
            float value = bh_float(bytes + cell);
            if (count >= BH_CELL_COUNT || value <= 0 || value > 50) return 0;
            cells[count++] = (uint16_t)cell;
        }
    }
    return keys == key_fingerprint && horses == BH_HORSE_COUNT && count == BH_CELL_COUNT;
}
static int bh_resolve_same(const BhAccess *access, const BhResource *expected) {
    BhResource current = {0};
    return access->resolve(access->context, &current) && bh_resource_valid(&current) && bh_same_resource(&current, expected);
}

/* Inspecting availability never writes game memory or captures a baseline. An
   unloaded/replaced resource discards old addresses without dereferencing them. */
static int bh_inspect(BhSession *session, const BhAccess *access, BhFingerprint fingerprint,
                      BhResource *current, unsigned char bytes[BH_BUFFER_SIZE], const char **reason) {
    *reason = NULL;
    memset(current, 0, sizeof *current);
    if (!access->resolve(access->context, current) || !bh_resource_valid(current)) {
        *reason = "Horse movement data is not loaded. Load a gameplay session and try again.";
        return 0;
    }
    if (session->captured && !bh_same_resource(&session->identity, current)) memset(session, 0, sizeof *session);
    if (!access->read(access->context, current->data, bytes, BH_BUFFER_SIZE) || !bh_resolve_same(access, current)) {
        *reason = "Horse movement data changed while reading; nothing was changed.";
        return 0;
    }
    uint16_t cells[BH_CELL_COUNT];
    if (!bh_layout(bytes, BH_BUFFER_SIZE, fingerprint.keys, cells)) {
        *reason = "Unsupported horse movement table; no speeds were changed.";
        return 0;
    }
    if (session->captured) {
        if (session->poisoned || memcmp(bytes, session->owned, BH_BUFFER_SIZE)) {
            *reason = "Horse movement data changed outside this editor. Restart BOTW before changing horse speed.";
            return 0;
        }
    } else if (bh_hash(bytes, BH_BUFFER_SIZE) != fingerprint.whole) {
        *reason = "The original horse movement table could not be verified; no speeds were changed.";
        return 0;
    }
    for (unsigned i = 0; i < BH_CELL_COUNT; ++i) {
        if (!access->writable(access->context, current->data + cells[i], 4)) {
            *reason = "Horse movement data is read-only; no speeds were changed.";
            return 0;
        }
    }
    return 1;
}

static int bh_rollback(const BhAccess *access, const BhResource *identity,
                       const uint16_t *cells, const unsigned *attempted, unsigned count,
                       const unsigned char *before, const unsigned char *target) {
    int restored = 1;
    while (count) {
        size_t offset = cells[attempted[--count]];
        unsigned char current[4];
        if (!bh_resolve_same(access, identity)) return 0;
        if (!access->read(access->context, identity->data + offset, current, 4)) { restored = 0; continue; }
        if (!memcmp(current, before + offset, 4)) continue;
        /* Never overwrite another owner's replacement, including a partial or
           otherwise unexplained value after a failed native write. */
        if (memcmp(current, target + offset, 4) || !access->writable(access->context, identity->data + offset, 4) ||
            !access->write(access->context, identity->data + offset, before + offset, 4) ||
            !access->read(access->context, identity->data + offset, current, 4) || memcmp(current, before + offset, 4)) restored = 0;
    }
    unsigned char check[BH_BUFFER_SIZE];
    return restored && bh_resolve_same(access, identity) &&
        access->read(access->context, identity->data, check, sizeof check) && !memcmp(check, before, sizeof check);
}

static int bh_apply(BhSession *session, const BhAccess *access, BhFingerprint fingerprint,
                    int percent, const char **reason) {
    if (percent < BH_PERCENT_MIN || percent > BH_PERCENT_MAX) {
        *reason = "Choose a horse speed from 1.00 to 2.00 times normal, or Off.";
        return 0;
    }
    BhResource current;
    unsigned char before[BH_BUFFER_SIZE], target[BH_BUFFER_SIZE];
    if (!bh_inspect(session, access, fingerprint, &current, before, reason)) return 0;
    if (!session->captured) {
        session->identity = current;
        memcpy(session->original, before, sizeof before);
        memcpy(session->owned, before, sizeof before);
        if (!bh_layout(before, sizeof before, fingerprint.keys, session->cells)) return 0;
        session->captured = 1;
        session->percent = 100;
    }
    memcpy(target, before, sizeof target);
    for (unsigned i = 0; i < BH_CELL_COUNT; ++i) {
        size_t offset = session->cells[i];
        float value = bh_float(session->original + offset) * ((float)percent / 100.0f);
        if (!isfinite(value) || value <= 0 || value > 50) {
            *reason = "The requested horse speed exceeds the verified movement bounds; nothing was changed.";
            return 0;
        }
        memcpy(target + offset, &value, 4);
    }
    unsigned attempted[BH_CELL_COUNT], count = 0;
    int ok = 1;
    for (unsigned i = 0; i < BH_CELL_COUNT && ok; ++i) {
        size_t offset = session->cells[i];
        if (!memcmp(before + offset, target + offset, 4)) continue;
        unsigned char previous[4], check[4];
        ok = bh_resolve_same(access, &current) && access->read(access->context, current.data + offset, previous, 4) &&
            !memcmp(previous, before + offset, 4) && access->writable(access->context, current.data + offset, 4);
        if (!ok) break;
        attempted[count++] = i;
        ok = access->write(access->context, current.data + offset, target + offset, 4) &&
            access->read(access->context, current.data + offset, check, 4) && !memcmp(check, target + offset, 4);
    }
    unsigned char check[BH_BUFFER_SIZE];
    if (ok) ok = bh_resolve_same(access, &current) && access->read(access->context, current.data, check, sizeof check) &&
        !memcmp(check, target, sizeof check);
    if (!ok) {
        int restored = bh_rollback(access, &current, session->cells, attempted, count, before, target);
        if (!restored) session->poisoned = 1;
        *reason = restored ? "Horse speed could not be applied; the previous speeds were restored." :
            "Horse speed lost ownership during the edit. Restart BOTW before changing horse speed; no save values were edited.";
        return 0;
    }
    memcpy(session->owned, target, sizeof target);
    session->percent = percent;
    *reason = NULL;
    return 1;
}

#ifndef BH_PURE_CHECKS
/* Include after botw_editor.h. getResource takes its handle in x0 and returns a
   DirectResource pointer in x0. Native code checks the resource RTTI, and reads
   its data pointer at +8 and byte count at +0x10 before the CRC table lookup. */
enum { BH_HANDLE = 0x2cc6eb8, BH_RESOURCE_GET = 0x1c718f0 };
static BhSession bh_session;

static int bh_code(uintptr_t base, uintptr_t offset, uint32_t instruction) {
    uint32_t value = 0;
    return base && base <= UINTPTR_MAX - 0x2d00000 &&
        be_memory_span(base + offset, 4, VM_PROT_READ | VM_PROT_EXECUTE) &&
        be_read(base + offset, &value, 4) && value == instruction;
}
static int bh_code_valid(uintptr_t base) {
#if defined(GAME_BOTW160) && !TARGET_OS_IPHONE
    return bh_code(base, 0x1c718f0, 0xf81d0ff5) && bh_code(base, 0x1c71900, 0x39402008) &&
        bh_code(base, 0x1c71920, 0xf9400a75) && bh_code(base, 0x1c71928, 0xb94132a8) &&
        bh_code(base, 0x1c71974, 0xf9401eb4) && bh_code(base, 0x1c71998, 0xd63f0100) &&
        bh_code(base, 0x16da3b4, 0x9000af60) && bh_code(base, 0x16da3bc, 0x94165d4d) &&
        bh_code(base, 0x16da3f4, 0xf9400697) && bh_code(base, 0x16da3f8, 0xb94002e8) &&
        bh_code(base, 0x16da428, 0xb9401288) && bh_code(base, 0x16da434, 0x331f7928) &&
        bh_code(base, 0x16da438, 0x531e7509) && bh_code(base, 0x16da870, 0xbd400140) &&
        bh_code(base, 0x16da874, 0xbd416661) && bh_code(base, 0x16da878, 0x1e210803);
#else
    (void)base; return 0;
#endif
}
static int bh_native_resolve(void *context, BhResource *resource) {
#if defined(GAME_BOTW160) && !TARGET_OS_IPHONE
    uintptr_t base = (uintptr_t)context;
    if (!bh_code_valid(base)) return 0;
    uintptr_t handle = base + BH_HANDLE, unit = be_pointer(handle + 0x10);
    unsigned char flags = 0;
    uint32_t status = 0, state = 0, pending_state = 0;
    uintptr_t pending = 0;
    if (!be_read(handle + 8, &flags, 1) || (flags & 10u) != 10u || !unit ||
        !be_read(unit + 0x130, &status, 4) || (status != 0 && status != 1 && status != 6) ||
        !be_read(unit + 0x10, &state, 4) || (state != 8 && state != 14) ||
        !be_read(handle + 0x24, &pending_state, 4) || !be_read(handle + 0x30, &pending, sizeof pending) ||
        (pending && pending_state == 1)) return 0;
    uintptr_t candidate = be_pointer(unit + 0x38);
    uintptr_t vtable = candidate ? be_pointer(candidate) : 0;
    if (!vtable || vtable < base + 0x2a6d000 || vtable > base + 0x2c89d68 - 0x38) return 0;
    for (unsigned i = 0; i <= 6; i += 6) {
        uintptr_t function = 0;
        if (!be_read(vtable + i * 8, &function, sizeof function) || (function & 3u) ||
            function < base || function >= base + 0x2129ba4 ||
            !be_memory_span(function, 4, VM_PROT_READ | VM_PROT_EXECUTE)) return 0;
    }
    uintptr_t verified = (uintptr_t)((void *(*)(void *))(base + BH_RESOURCE_GET))((void *)handle);
    uint32_t size = 0;
    uintptr_t data = verified ? be_pointer(verified + 8) : 0;
    if (verified != candidate || be_pointer(handle + 0x10) != unit || be_pointer(unit + 0x38) != candidate ||
        be_pointer(candidate) != vtable || !be_read(candidate + 0x10, &size, 4) || size != BH_BUFFER_SIZE ||
        !be_memory_span(data, size, VM_PROT_READ | VM_PROT_WRITE)) return 0;
    *resource = (BhResource){unit, candidate, vtable, data, size};
    return 1;
#else
    (void)context; (void)resource; return 0;
#endif
}
static int bh_native_read(void *context, uintptr_t address, void *output, size_t size) {
    (void)context; return be_read(address, output, size);
}
static int bh_native_writable(void *context, uintptr_t address, size_t size) {
    (void)context; return be_memory_span(address, size, VM_PROT_READ | VM_PROT_WRITE);
}
static int bh_native_write(void *context, uintptr_t address, const void *value, size_t size) {
    (void)context; return be_write(address, value, size);
}
static BhAccess bh_native_access(uintptr_t base) {
    return (BhAccess){(void *)base, bh_native_resolve, bh_native_read, bh_native_writable, bh_native_write};
}
static unsigned be_horse_features(uintptr_t base) {
    /* Capability and live availability are distinct. A temporary resource unload
       must not misleadingly look like an old bridge needing installation. */
    return bh_code_valid(base) ? BE_FEATURE_HORSE_SPEED : 0;
}
static int be_horse_command(uintptr_t base, const char *verb, const char *argument, int value, char *result, size_t capacity) {
    if (strcmp(verb, "horse_speed")) return 0;
    int percent = !strcmp(argument, "off") ? 100 : value;
    if (argument[0] && strcmp(argument, "off")) {
        snprintf(result, capacity, "Choose horse_speed 100-200 or horse_speed off."); return 0;
    }
    const char *reason = NULL;
    BhAccess access = bh_native_access(base);
    int ok = bh_apply(&bh_session, &access, bh_nx160_fingerprint, percent, &reason);
    if (ok && percent == 100) snprintf(result, capacity, "Original standard-horse speeds restored and read back. This session only; no save values changed.");
    else if (ok) snprintf(result, capacity, "Standard-horse movement set to %.2fx and read back for all 26 standard horse types. This session only; special mounts are unchanged.", percent / 100.0);
    else snprintf(result, capacity, "%s", reason ? reason : "Horse speed was not changed.");
    return ok;
}
static void be_horse_snapshot(FILE *output, uintptr_t base, int ready) {
    BhResource resource;
    unsigned char bytes[BH_BUFFER_SIZE];
    const char *reason = NULL;
    BhAccess access = bh_native_access(base);
    int available = ready && bh_inspect(&bh_session, &access, bh_nx160_fingerprint, &resource, bytes, &reason);
    int percent = bh_session.captured ? bh_session.percent : 100;
    fprintf(output, ",\"horse_speed_available\":%s,\"horse_speed_active\":%s,\"horse_speed_multiplier\":",
        available ? "true" : "false", available && percent > 100 ? "true" : "false");
    if (available) fprintf(output, "%.2f", percent / 100.0); else fputs("null", output);
    fputs(",\"horse_speed_reason\":", output);
    if (available) fputs("null", output);
    else be_json_string(output, reason ? reason : "Load a gameplay session to change horse speed.");
}
#endif
#endif
