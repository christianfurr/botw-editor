#ifndef MNG_BOTW_EDITOR_RUNES_H
#define MNG_BOTW_EDITOR_RUNES_H

/* Include after botw_editor_world.h. NX160 acquisition equips a downloaded
   rune by requesting its ID under RuneMgr's mutex. Ownership flags alone do
   not perform that step. Do not call the manager's broad load/reset method:
   it also clears active-rune state and interaction/cooldown slots. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct { int32_t requested, applied, previous; } BrRuneValues;
typedef struct {
    void *context;
    int (*try_lock)(void *, uintptr_t *, BrRuneValues *);
    int (*read_locked)(void *, uintptr_t, BrRuneValues *);
    int (*write_requested)(void *, uintptr_t, int32_t);
    void (*unlock)(void *, uintptr_t);
} BrRuneAccess;

static int br_rune_values_valid(const BrRuneValues *values) {
    return values->requested >= -1 && values->requested <= 7 &&
        values->applied >= -1 && values->applied <= 7 &&
        values->previous >= -1 && values->previous <= 7;
}

/* Request stock round Bomb (ID 0) only when neither a current nor a requested
   rune exists. Valid user selections, including an in-flight change, remain.
   Success establishes a request; the normal game update must still apply it.
   Caller must have verified the base-rune ownership transaction first. */
static int br_rune_select_if_missing(const BrRuneAccess *access, int *changed,
                                    BrRuneValues *after, const char **reason) {
    uintptr_t identity = 0;
    BrRuneValues before = {0};
    *changed = 0;
    *reason = NULL;
    if (!access->try_lock(access->context, &identity, &before)) {
        *reason = "Rune selection is busy or unavailable. Return to gameplay and try again.";
        return 0;
    }
    int ok = br_rune_values_valid(&before);
    if (!ok) {
        *reason = "The native rune-selection state is unsupported; no selection was changed.";
    } else if (before.requested != -1 || before.applied != -1) {
        *after = before;
    } else {
        int wrote = access->write_requested(access->context, identity, 0);
        BrRuneValues check = {0};
        int read = access->read_locked(access->context, identity, &check);
        ok = wrote && read && br_rune_values_valid(&check) && check.requested == 0 &&
            check.applied == before.applied && check.previous == before.previous;
        if (ok) {
            *changed = 1;
            *after = check;
        } else {
            /* A failed first readback does not justify a blind restore. A
               fresh locked read may still prove that only our request exists. */
            if (!read) read = access->read_locked(access->context, identity, &check);
            int restored = read && br_rune_values_valid(&check) &&
                check.applied == before.applied && check.previous == before.previous;
            if (restored && check.requested == 0) {
                restored = access->write_requested(access->context, identity, before.requested) &&
                    access->read_locked(access->context, identity, &check) &&
                    check.requested == before.requested && check.applied == before.applied &&
                    check.previous == before.previous;
            } else if (restored) {
                restored = check.requested == before.requested;
            }
            *reason = restored ? "The game could not request a rune; the previous selection was restored." :
                "Rune selection changed during the request; reopen gameplay before trying again.";
        }
    }
    access->unlock(access->context, identity);
    return ok;
}

#ifndef BR_RUNE_PURE_CHECKS
enum {
    BR_RUNE_MANAGER = 0x2c9a668, BR_RUNE_VTABLE = 0x2b4dd40,
    BR_RUNE_TRY_LOCK = 0x21281c0, BR_RUNE_UNLOCK = 0x2128050
};
static int br_rune_code_valid(uintptr_t base) {
#if defined(GAME_BOTW160) && !TARGET_OS_IPHONE
    if (!base || base > UINTPTR_MAX - 0x2d00000) return 0;
    /* Constructor identity; acquisition UI lock/write/unlock; normal update's
       request->applied transition; exact SDK TryLock/Unlock import stubs. */
    return bw_code(base, 0xb954f8, 0x9000fdc9u) &&
        bw_code(base, 0xb9550c, 0xa9002269u) &&
        bw_code(base, 0x116303c, 0xf9433516u) &&
        bw_code(base, 0x1163040, 0x910882d4u) &&
        bw_code(base, 0x1163050, 0xb90242d5u) &&
        bw_code(base, 0xb9864c, 0xb9424275u) &&
        bw_code(base, 0xb986bc, 0xb9024a75u) &&
        bw_code(base, BR_RUNE_TRY_LOCK, 0xd0005ab0u) &&
        bw_code(base, BR_RUNE_TRY_LOCK + 4, 0xf9469611u) &&
        bw_code(base, BR_RUNE_TRY_LOCK + 8, 0x9134a210u) &&
        bw_code(base, BR_RUNE_TRY_LOCK + 12, 0xd61f0220u) &&
        bw_code(base, BR_RUNE_UNLOCK, 0xd0005ab0u) &&
        bw_code(base, BR_RUNE_UNLOCK + 4, 0xf9463a11u) &&
        bw_code(base, BR_RUNE_UNLOCK + 8, 0x9131c210u) &&
        bw_code(base, BR_RUNE_UNLOCK + 12, 0xd61f0220u);
#else
    (void)base; return 0;
#endif
}
static int br_rune_identity(uintptr_t base, uintptr_t manager) {
    return manager && manager <= UINTPTR_MAX - 0x250 &&
        be_pointer(base + BR_RUNE_MANAGER) == manager &&
        be_pointer(manager) == base + BR_RUNE_VTABLE &&
        be_pointer(manager + 8) == base + BR_RUNE_VTABLE + 0x28 &&
        be_pointer(manager + 0x10) == base + BR_RUNE_VTABLE + 0x50 &&
        be_memory_span(manager + 0x220, 0x20, VM_PROT_READ | VM_PROT_WRITE) &&
        be_memory_span(manager + 0x240, 4, VM_PROT_READ | VM_PROT_WRITE) &&
        be_memory_span(manager + 0x248, 8, VM_PROT_READ);
}
static int br_rune_native_read(void *context, uintptr_t manager, BrRuneValues *values) {
    uintptr_t base = (uintptr_t)context;
    return br_rune_identity(base, manager) &&
        be_read(manager + 0x240, &values->requested, 4) &&
        be_read(manager + 0x248, &values->applied, 4) &&
        be_read(manager + 0x24c, &values->previous, 4);
}
static void br_rune_native_unlock(void *context, uintptr_t manager) {
    uintptr_t base = (uintptr_t)context;
    ((void (*)(void *))(base + BR_RUNE_UNLOCK))((void *)(manager + 0x220));
}
static int br_rune_native_try_lock(void *context, uintptr_t *identity, BrRuneValues *values) {
    uintptr_t base = (uintptr_t)context;
    if (!br_rune_code_valid(base)) return 0;
    uintptr_t manager = be_pointer(base + BR_RUNE_MANAGER);
    if (!br_rune_identity(base, manager) ||
        !(((int (*)(void *))(base + BR_RUNE_TRY_LOCK))((void *)(manager + 0x220)) & 1)) return 0;
    if (!br_rune_native_read(context, manager, values)) {
        br_rune_native_unlock(context, manager);
        return 0;
    }
    *identity = manager;
    return 1;
}
static int br_rune_native_write(void *context, uintptr_t manager, int32_t value) {
    uintptr_t base = (uintptr_t)context;
    return value >= -1 && value <= 7 && br_rune_identity(base, manager) &&
        be_write(manager + 0x240, &value, 4);
}
static BrRuneAccess br_rune_native_access(uintptr_t base) {
    return (BrRuneAccess){(void *)base, br_rune_native_try_lock, br_rune_native_read,
        br_rune_native_write, br_rune_native_unlock};
}
static int br_rune_readback(uintptr_t base, BrRuneValues *values, int32_t *saved) {
    BrRuneAccess access = br_rune_native_access(base);
    uintptr_t identity = 0;
    if (!access.try_lock(access.context, &identity, values)) return 0;
    int ok = br_rune_values_valid(values);
    access.unlock(access.context, identity);
    return ok && be_world_get_s32(base, "PlayerEquipItemType", saved) && *saved >= -1 && *saved <= 7;
}
static int be_runes_available(uintptr_t base) {
    BrRuneAccess access = br_rune_native_access(base);
    BrRuneValues values = {0}; uintptr_t identity = 0;
    if (!access.try_lock(access.context, &identity, &values)) return 0;
    int ok = br_rune_values_valid(&values);
    access.unlock(access.context, identity);
    return ok;
}
static int be_runes_select_if_missing(uintptr_t base, char *result, size_t capacity) {
    int bomb = 0, slate = 0, powered = 0;
    if (!be_world_get_bool(base, "IsGet_Obj_RemoteBomb", &bomb) || !bomb ||
        !be_world_get_bool(base, "IsGet_PortableUnit", &slate) || !slate ||
        !be_world_get_bool(base, "PortableUnit_PowerOn", &powered) || !powered) {
        snprintf(result, capacity, "Rune ownership and Slate access are not ready; no selection was changed.");
        return 0;
    }
    BrRuneAccess access = br_rune_native_access(base);
    BrRuneValues after = {0}; int changed = 0; const char *reason = NULL;
    if (!br_rune_select_if_missing(&access, &changed, &after, &reason)) {
        snprintf(result, capacity, "%s", reason ? reason : "Rune selection was not changed.");
        return 0;
    }
    if (changed || after.applied == -1) {
        snprintf(result, capacity, "Rune access enabled; a selection is pending in the game. Resume gameplay, then hold 3 to choose a rune and press F to use it.");
    } else {
        snprintf(result, capacity, "Rune access enabled and the existing selection preserved. Hold 3 to choose a rune, release, then press F to use it.");
    }
    return 1;
}
static void be_runes_snapshot(FILE *output, uintptr_t base, int ready) {
    BrRuneValues values = {0}; int32_t saved = -1;
    int known = ready && br_rune_readback(base, &values, &saved);
    fprintf(output, ",\"rune_selection_ready\":%s,\"rune_selection_requested\":", known ? "true" : "false");
    if (known) fprintf(output, "%d", values.requested); else fputs("null", output);
    fputs(",\"rune_selection_current\":", output);
    if (known) fprintf(output, "%d", values.applied); else fputs("null", output);
    fputs(",\"rune_selection_previous\":", output);
    if (known) fprintf(output, "%d", values.previous); else fputs("null", output);
    fputs(",\"rune_selection_saved\":", output);
    if (known) fprintf(output, "%d", saved); else fputs("null", output);
}
#endif
#endif
