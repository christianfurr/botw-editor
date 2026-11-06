/* Reproducible bridge checks using synthetic memory only. No Nintendo program,
   game asset, live game process, save or extracted player data is required. */
#include <TargetConditionals.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef GAME_BOTW160
#define GAME_BOTW160 1
#endif

static uint8_t *fixture_main;
uint8_t *runtime_module_base(const char *name);
uint8_t *runtime_module_base(const char *name) {
    return !strcmp(name, "main") ? fixture_main : NULL;
}

#include "runtime/botw_editor.h"
#include "runtime/botw_editor_inventory.h"
#include "runtime/botw_editor_world.h"
#include "runtime/botw_editor_horse.h"

static unsigned checks;
static unsigned extension_calls;

static void require(int condition, const char *description) {
    ++checks;
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", description);
        exit(1);
    }
}

static void reset_editor(void) {
    fixture_main = NULL;
    memset(&be_extensions, 0, sizeof be_extensions);
    memset(be_queue, 0, sizeof be_queue);
    be_queue_head = 0;
    be_queue_count = 0;
    be_health_lock = 0;
    be_stamina_lock = 0;
    be_health_target = 0;
    be_last_request = 0;
    be_last_tick = 0;
    be_last_publish = 0;
    be_last_ok = 0;
    be_state_path[0] = 0;
    snprintf(be_last_result, sizeof be_last_result, "No changes requested");
    extension_calls = 0;
}

static void check_memory_guards(void) {
    size_t page = (size_t)getpagesize();
    unsigned char *memory = mmap(NULL, page * 2, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANON, -1, 0);
    require(memory != MAP_FAILED, "allocate VM fixture");
    uint32_t initial = 12, replacement = 20, readback = 0;
    memcpy(memory, &initial, sizeof initial);
    require(be_read((uintptr_t)memory, &readback, 4) && readback == initial,
            "read a mapped value");
    require(be_write((uintptr_t)memory, &replacement, 4), "write and read back a mapped value");
    memcpy(&readback, memory, 4);
    require(readback == replacement, "the permitted write reached the fixture");
    require(!be_read(0, &readback, 4), "reject a null read");
    require(!be_read((uintptr_t)memory, NULL, 4), "reject a null output");
    require(!be_write((uintptr_t)memory, NULL, 4), "reject a null input");
    require(!be_memory_span((uintptr_t)memory, 0, VM_PROT_READ), "reject an empty span");
    require(!be_read(UINTPTR_MAX - 2, &readback, 4), "reject an overflowing read");
    require(!be_write(UINTPTR_MAX - 2, &replacement, 4), "reject an overflowing write");
    unsigned char oversized[17] = {0};
    require(!be_write((uintptr_t)memory, oversized, sizeof oversized), "reject an oversized write");
    require(mprotect(memory, page, PROT_READ) == 0, "protect fixture as read-only");
    require(be_read((uintptr_t)memory, &readback, 4), "read-only memory remains readable");
    require(!be_write((uintptr_t)memory, &initial, 4), "reject a read-only write");
    memcpy(&readback, memory, 4);
    require(readback == replacement, "rejected write leaves the value unchanged");
    require(mprotect(memory, page, PROT_READ | PROT_WRITE) == 0, "restore fixture protection");
    require(mprotect(memory + page, page, PROT_NONE) == 0, "protect the second VM region");
    uintptr_t crossing = (uintptr_t)(memory + page - 4);
    uint64_t pair = UINT64_MAX;
    require(!be_memory_span(crossing, sizeof pair, VM_PROT_READ), "reject a read crossing into an inaccessible region");
    require(!be_write(crossing, &pair, sizeof pair), "reject a write crossing into an inaccessible region");
    require(munmap(memory, page * 2) == 0, "unmap fixture");
    require(!be_read((uintptr_t)memory, &readback, 4), "reject unmapped memory");
}

static void expect_parse(const char *line, int accepted) {
    BotwEditorRequest request = {0};
    char error[256] = {0};
    require(be_parse(line, &request, error, sizeof error) == accepted, line);
}

static void check_parser(void) {
    BotwEditorRequest request = {0};
    char error[256];
    require(be_parse("trainer 41 give Item_Fruit_A 99\n", &request, error, sizeof error), "parse an item request");
    require(request.id == 41 && request.value == 99 && !strcmp(request.argument, "Item_Fruit_A"),
            "preserve the request ID, actor and quantity");
    const char *valid[] = {
        "trainer 1 status", "trainer 2 health on", "trainer 3 stamina off",
        "trainer 4 rupees 0", "trainer 5 rupees 999999", "trainer 6 quantity 419 999",
        "trainer 7 remove 0", "trainer 8 teleport Dungeon001", "trainer 9 unlock_shrine Dungeon119",
        "trainer 10 full_access", "trainer 11 paraglider", "trainer 12 reveal_map",
        "trainer 13 unlock_all_shrines", "trainer 14 runes", "trainer 15 max_hearts 30",
        "trainer 16 max_stamina 3", "trainer 17 quantity 418 2000",
        "trainer 18 quantity 418 3000", "trainer 19 quantity 0 1",
        "trainer 20 quantity 419 100000000", "trainer 21 master_sword 0", "trainer 22 master_sword 3",
        "trainer 23 pouch_slots weapons 8", "trainer 24 pouch_slots weapons 20",
        "trainer 25 pouch_slots bows 5", "trainer 26 pouch_slots bows 14",
        "trainer 27 pouch_slots shields 4", "trainer 28 pouch_slots shields 20", "trainer 29 map_access",
        "trainer 30 complete_shrine Dungeon002", "trainer 31 complete_all_shrines",
        "trainer 32 horse_speed 100", "trainer 33 horse_speed 200", "trainer 34 horse_speed off"
    };
    for (size_t i = 0; i < sizeof valid / sizeof *valid; ++i) expect_parse(valid[i], 1);
    const char *invalid_horse[] = {"trainer 40 horse_speed 99", "trainer 41 horse_speed 201", "trainer 42 horse_speed 1.5",
                                  "trainer 43 horse_speed -1", "trainer 44 horse_speed off extra", "trainer 45 complete_all_shrines extra"};
    for (size_t i = 0; i < sizeof invalid_horse / sizeof *invalid_horse; ++i) expect_parse(invalid_horse[i], 0);
    const char *invalid[] = {
        "trainer 0 status", "trainer -1 status", "trainer 18446744073709551616 status",
        "trainer 2 give ../item 1", "trainer 3 give Item_Fruit_A -1",
        "trainer 4 give Item_Fruit_A 0", "trainer 5 give Item_Fruit_A 1000",
        "trainer 6 rupees 1000000", "trainer 7 rupees -1", "trainer 8 quantity 420 1",
        "trainer 9 remove -1", "trainer 10 health maybe", "trainer 11 full_access extra",
        "trainer 12 max_hearts 0", "trainer 13 max_hearts 31", "trainer 14 max_stamina 0",
        "trainer 15 max_stamina 4", "trainer 16 teleport Dungeon001/../../bad",
        "trainer 17 give Item_Fruit_A 1\ntrainer 18 status", "trainer 19 unknown",
        "trainer 20 quantity 0 0", "trainer 21 quantity 0 -1",
        "trainer 22 quantity 0 100000001", "trainer 23 quantity 0 2147483648",
        "trainer 24 quantity 0 1.5", "trainer 25 master_sword -1", "trainer 26 master_sword 4",
        "trainer 27 master_sword 1.5", "trainer 28 master_sword 1 extra", "trainer 29 master_sword +1",
        "trainer 30 pouch_slots weapons 7", "trainer 31 pouch_slots weapons 21",
        "trainer 32 pouch_slots bows 4", "trainer 33 pouch_slots bows 15",
        "trainer 34 pouch_slots shields 3", "trainer 35 pouch_slots shields 21",
        "trainer 36 pouch_slots materials 160", "trainer 37 pouch_slots weapons -1",
        "trainer 38 pouch_slots bows 10.5", "trainer 39 pouch_slots shields 20 extra",
        "trainer 40 pouch_slots weapons", "trainer 41 pouch_slots weapons +20",
        "trainer 42 map_access on", "trainer 43 map_access 1", "trainer 44 map_access extra extra"
    };
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) expect_parse(invalid[i], 0);
    char long_command[300];
    memset(long_command, 'x', sizeof long_command - 1);
    long_command[sizeof long_command - 1] = 0;
    expect_parse(long_command, 0);
}

static void expect_item_write(BeiItem *item, int value, int accepted, int *stored) {
    int before = *stored;
    require(bei_set_value(item, value) == accepted, "type-specific inventory write decision");
    require(*stored == (accepted ? value : before), "rejected inventory edits leave memory unchanged");
}

static void check_inventory_values(void) {
    BeiItem owned_sword = {.name = "Weapon_Sword_070", .type = 0, .value = 4000};
    for (int i = 0; i < 3; ++i) {
        const char *forms[] = {"Weapon_Sword_056", "Weapon_Sword_071", "Weapon_Sword_072"};
        const char *reason = bei_give_block_reason(forms[i], 1, NULL, 0);
        require(reason && strstr(reason, "internal Master Sword"), "internal forms explain their non-pouch status before native acquisition");
    }
    require(!bei_give_block_reason("Weapon_Sword_070", 1, NULL, 0), "the usable Master Sword can be added when absent");
    require(bei_give_block_reason("Weapon_Sword_070", 2, NULL, 0) != NULL, "only one Master Sword may be requested");
    require(bei_give_block_reason("Weapon_Sword_070", 1, &owned_sword, 1) != NULL, "owned Master Sword explains duplicate refusal");
    require(!bei_give_block_reason("Weapon_Sword_057", 1, &owned_sword, 1), "other swords are not confused with the Master Sword");
    struct { uint64_t prefix[4]; int value; } entry = {0};
    BeiItem item = {.address = (uintptr_t)&entry};
    int *stored = &entry.value;
    const int weapon_types[] = {0, 1, 3};
    for (size_t i = 0; i < sizeof weapon_types / sizeof *weapon_types; ++i) {
        item.type = weapon_types[i];
        require(bei_value_maximum(item.type) == 100000000, "weapon durability uses the existing native-reader bound");
        expect_item_write(&item, 1, 1, stored);
        expect_item_write(&item, 2000, 1, stored);
        expect_item_write(&item, 3000, 1, stored);
        expect_item_write(&item, 100000000, 1, stored);
        expect_item_write(&item, 0, 0, stored);
        expect_item_write(&item, -1, 0, stored);
        expect_item_write(&item, 100000001, 0, stored);
        item.equipped = 1;
        expect_item_write(&item, 2000, 0, stored);
        item.equipped = 0;
    }
    const int stack_types[] = {2, 7};
    for (size_t i = 0; i < sizeof stack_types / sizeof *stack_types; ++i) {
        item.type = stack_types[i];
        require(bei_value_maximum(item.type) == 999, "arrow/material quantities retain their stack bound");
        expect_item_write(&item, 1, 1, stored);
        expect_item_write(&item, 999, 1, stored);
        expect_item_write(&item, 0, 0, stored);
        expect_item_write(&item, 1000, 0, stored);
        expect_item_write(&item, 100000000, 0, stored);
    }
    item.type = 2;
    item.equipped = 1;
    expect_item_write(&item, 999, 1, stored);
    item.equipped = 0;
    const int unsupported_types[] = {-1, 4, 5, 6, 8, 9, 10};
    for (size_t i = 0; i < sizeof unsupported_types / sizeof *unsupported_types; ++i) {
        item.type = unsupported_types[i];
        expect_item_write(&item, 1, 0, stored);
    }
    item.type = 0;
    item.address = UINTPTR_MAX - 0x10;
    expect_item_write(&item, 2000, 0, stored);
    char result[128];
    require(!be_inventory_features(0), "unavailable inventory API advertises no features");
    require(!be_inventory_command(0, "quantity", "0", 2000, result, sizeof result),
            "unavailable inventory API rejects edits before a native function call");
    char *json = NULL; size_t length = 0;
    FILE *output = open_memstream(&json, &length);
    require(output != NULL, "open unavailable inventory snapshot stream");
    be_inventory_snapshot(output, 0, 0);
    require(fclose(output) == 0 && !strcmp(json, ",\"inventory\":[],\"inventory_ready\":false,\"inventory_block_reason\":\"Load a gameplay session to edit inventory.\""),
            "unavailable inventory snapshot contains no actionable rows and explains its state");
    free(json);
}

static void fixture_inventory_item(uintptr_t base, uintptr_t manager, int slot,
                                   int type, int value, const char *name) {
    uintptr_t address = manager + BEI_BUFFER + (uintptr_t)slot * BEI_ITEM_SIZE;
    uintptr_t vtable = base + 0x2b877a0, name_vtable = base + 0x2a6faf0, text = address + 0x3c;
    int32_t capacity = 64;
    memcpy((void *)address, &vtable, 8);
    memcpy((void *)(address + 0x18), &type, 4);
    memcpy((void *)(address + 0x20), &value, 4);
    *(unsigned char *)(address + 0x25) = 1;
    memcpy((void *)(address + 0x28), &name_vtable, 8);
    memcpy((void *)(address + 0x30), &text, 8);
    memcpy((void *)(address + 0x38), &capacity, 4);
    snprintf((char *)text, 64, "%s", name);
}

static void check_inventory_armor_and_readiness(void) {
    for (int type = 4; type <= 6; ++type) {
        require(bei_native_value_valid(type, -1), "accept native non-dyeable armor sentinel");
        require(bei_native_value_valid(type, 0) && bei_native_value_valid(type, 15), "accept original and dyed armor colors");
        require(!bei_native_value_valid(type, -2) && !bei_native_value_valid(type, 16), "reject unknown armor metadata");
    }
    for (int type = 0; type <= 9; ++type) {
        if (type >= 4 && type <= 6) continue;
        require(!bei_native_value_valid(type, -1), "armor sentinel does not relax other item types");
    }
    require(!bei_native_value_valid(-1, 0) && !bei_native_value_valid(10, 0), "reject invalid item types");
    require(!bei_native_value_valid(7, 1000) && bei_native_value_valid(0, 3000), "keep stack and durability ranges separate");

    unsigned char *storage = calloc(1, 0x45000);
    require(storage != NULL, "allocate synthetic mixed inventory");
    uintptr_t manager = (uintptr_t)storage, base = 0x10000000, head = manager + BEI_LIST;
    uintptr_t nodes[3] = {manager + BEI_BUFFER + 8, manager + BEI_BUFFER + BEI_ITEM_SIZE + 8,
                          manager + BEI_BUFFER + 2 * BEI_ITEM_SIZE + 8};
    fixture_inventory_item(base, manager, 0, 5, -1, "Armor_001_Upper");
    fixture_inventory_item(base, manager, 1, 7, 9, "Item_Fruit_A");
    fixture_inventory_item(base, manager, 2, 0, 2000, "Weapon_Sword_001");
    memcpy((void *)head, &nodes[2], 8); memcpy((void *)(head + 8), &nodes[0], 8);
    int32_t expected = 3; memcpy((void *)(head + 0x10), &expected, 4);
    for (int index = 0; index < 3; ++index) {
        uintptr_t previous = index ? nodes[index - 1] : head;
        uintptr_t next = index == 2 ? head : nodes[index + 1];
        memcpy((void *)nodes[index], &previous, 8); memcpy((void *)(nodes[index] + 8), &next, 8);
    }
    BeiItem items[BEI_SLOTS]; int count = 0;
    require(bei_scan(base, manager, items, &count) && count == 3, "armor sentinel does not hide an otherwise valid full inventory");
    require(items[0].value == -1 && items[1].value == 9 && items[2].value == 2000,
            "mixed scan preserves armor metadata, material count, and weapon durability");
    require(!bei_set_value(&items[0], 1), "armor remains read-only despite accepted metadata");
    require(!bei_inventory_block_reason(manager, 1), "validated idle pouch permits edits without requiring a menu to close");
    uintptr_t held = nodes[1]; memcpy((void *)(manager + 0x444a0), &held, 8);
    const char *reason = bei_inventory_block_reason(manager, 1);
    require(reason && strstr(reason, "Put away the materials"), "held materials produce a precise actionable blocker");
    held = 0; memcpy((void *)(manager + 0x444a0), &held, 8);
    require(!bei_inventory_block_reason(manager, 1), "putting away materials restores edit readiness");
    reason = bei_inventory_block_reason(manager, 0);
    require(reason && strstr(reason, "validated") && !strstr(reason, "menu"), "validation failure does not invent a menu condition");
    require(bei_inventory_block_reason(0, 0) != NULL, "unavailable manager blocks mutation");
    int invalid = -2; memcpy((void *)(manager + BEI_BUFFER + 0x20), &invalid, 4);
    require(!bei_scan(base, manager, items, &count), "unsupported negative armor value still rejects the complete scan");
    free(storage);
}

static struct {
    BwMasterSwordValues values[2];
    const char *expected_s32_name;
    int reads, writes, fail_read, fail_write, silent_write, fail_rollback;
} master_fixture;

static BwMasterSwordValues *master_fixture_store(uintptr_t parameter) {
    require(parameter == 1 || parameter == 2, "flag transaction accesses only its two prepared stores");
    return &master_fixture.values[parameter - 1];
}
static int master_fixture_get_s32(uintptr_t base, uintptr_t parameter, const char *name, int32_t *value) {
    (void)base;
    require(!strcmp(name, master_fixture.expected_s32_name), "transaction reads only its exact prepared S32 flag");
    if (++master_fixture.reads == master_fixture.fail_read) return 0;
    *value = master_fixture_store(parameter)->power; return 1;
}
static int master_fixture_get_bool(uintptr_t base, uintptr_t parameter, const char *name, int *value) {
    (void)base;
    require(!strcmp(name, "Open_MasterSword_FullPower"), "transaction reads only the verified full-power flag");
    if (++master_fixture.reads == master_fixture.fail_read) return 0;
    *value = master_fixture_store(parameter)->full; return 1;
}
static int master_fixture_put_s32(uintptr_t base, uintptr_t parameter, const char *name, int32_t value) {
    (void)base;
    require(!strcmp(name, master_fixture.expected_s32_name), "transaction writes only its exact prepared S32 flag");
    int write = ++master_fixture.writes;
    if (write == master_fixture.fail_rollback) return 0;
    if (write != master_fixture.silent_write) master_fixture_store(parameter)->power = value;
    return write != master_fixture.fail_write;
}
static int master_fixture_put_bool(uintptr_t base, uintptr_t parameter, const char *name, int value, int bypass) {
    (void)base;
    require(!strcmp(name, "Open_MasterSword_FullPower") && bypass == 1,
            "exact full-power flag uses native one-trigger bypass for lowering and rollback");
    int write = ++master_fixture.writes;
    if (write == master_fixture.fail_rollback) return 0;
    if (write != master_fixture.silent_write) master_fixture_store(parameter)->full = value;
    return write != master_fixture.fail_write;
}
static const BwWorldFlagAccess master_fixture_access = {
    master_fixture_get_s32, master_fixture_get_bool, master_fixture_put_s32, master_fixture_put_bool
};
static void master_fixture_reset(void) {
    memset(&master_fixture, 0, sizeof master_fixture);
    master_fixture.expected_s32_name = "MasterSword_Add_Power";
    master_fixture.values[0] = (BwMasterSwordValues){10, 0};
    master_fixture.values[1] = (BwMasterSwordValues){30, 1};
}
static void master_fixture_original(void) {
    require(master_fixture.values[0].power == 10 && master_fixture.values[0].full == 0 &&
        master_fixture.values[1].power == 30 && master_fixture.values[1].full == 1,
        "failure restores both original flags in both stores, including distinct prior levels");
}
static void check_master_sword_levels(void) {
    for (int level = 0; level < 4; ++level) {
        BwMasterSwordValues values = {level * 10, level == 3};
        require(bw_master_sword_level(&values) == level, "decode canonical 30/40/50/60 base levels");
    }
    const BwMasterSwordValues custom[] = {{1, 0}, {30, 0}, {20, 1}, {-1, 0}, {31, 1}, {0, 2}};
    for (size_t i = 0; i < sizeof custom / sizeof *custom; ++i)
        require(bw_master_sword_level(&custom[i]) == -1, "custom or invalid flags have no invented level");
    BeiItem items[2] = {{.type = 0}};
    snprintf(items[0].name, sizeof items[0].name, "Weapon_Sword_070");
    require(bw_master_sword_item_block(items, 0) != NULL, "level editing requires an owned canonical Master Sword");
    require(!bw_master_sword_item_block(items, 1), "an unequipped canonical Master Sword can change level");
    items[0].equipped = 1;
    require(bw_master_sword_item_block(items, 1) != NULL, "equipped Master Sword blocks level changes");
    items[0].equipped = 0; items[1] = items[0];
    require(bw_master_sword_item_block(items, 2) != NULL, "ambiguous duplicate Master Swords block level changes");
    items[0].type = 7;
    require(bw_master_sword_item_block(items, 1) != NULL, "canonical name with wrong inventory type is rejected");
    items[0].type = 0;
    for (int variant = 71; variant <= 72; ++variant) {
        snprintf(items[0].name, sizeof items[0].name, "Weapon_Sword_%03d", variant);
        require(bw_master_sword_item_block(items, 1) != NULL, "internal forms do not satisfy ownership");
    }
    BeWorldRefs refs = {.pending = 1, .primary = 2}; int restored = 0;
    for (int level = 0; level < 4; ++level) {
        master_fixture_reset();
        require(bw_master_sword_transaction(0, &refs, level, &master_fixture_access, &restored) && restored,
                "native-adapter transaction verifies every canonical target, including lowering from full power");
        for (int i = 0; i < 2; ++i)
            require(master_fixture.values[i].power == level * 10 && master_fixture.values[i].full == (level == 3),
                    "both game-data stores contain the selected target");
        require(master_fixture.reads == 8 && master_fixture.writes == 4, "success requires both complete readbacks");
    }
    for (int failure = 1; failure <= 4; ++failure) {
        master_fixture_reset(); master_fixture.fail_write = failure;
        require(!bw_master_sword_transaction(0, &refs, 1, &master_fixture_access, &restored) && restored,
                "a failed setter after mutation rolls back every store");
        master_fixture_original();
    }
    master_fixture_reset(); master_fixture.fail_read = 5;
    require(!bw_master_sword_transaction(0, &refs, 1, &master_fixture_access, &restored) && restored,
            "failed final readback rolls back rather than claiming success");
    master_fixture_original();
    master_fixture_reset(); master_fixture.silent_write = 4;
    require(!bw_master_sword_transaction(0, &refs, 1, &master_fixture_access, &restored) && restored,
            "incorrect readback detects a setter that silently ignored the edit");
    master_fixture_original();
    master_fixture_reset(); master_fixture.fail_read = 1;
    require(!bw_master_sword_transaction(0, &refs, 1, &master_fixture_access, &restored) && !master_fixture.writes,
            "unavailable initial flags refuse the entire transaction before any write");
    master_fixture_reset(); master_fixture.values[1].power = 31;
    require(!bw_master_sword_transaction(0, &refs, 1, &master_fixture_access, &restored) && !master_fixture.writes,
            "out-of-range current flags refuse the transaction before any write");
    master_fixture_reset(); master_fixture.fail_write = 4; master_fixture.fail_rollback = 5;
    require(!bw_master_sword_transaction(0, &refs, 1, &master_fixture_access, &restored) && !restored,
            "incomplete rollback is reported honestly");
    master_fixture_reset();
    require(!bw_master_sword_transaction(0, &refs, INT_MAX, &master_fixture_access, &restored) && !master_fixture.writes,
            "invalid direct target refuses without arithmetic overflow or writes");
    refs.primary = refs.pending;
    master_fixture_reset();
    require(bw_master_sword_transaction(0, &refs, 3, &master_fixture_access, &restored) && master_fixture.writes == 2,
            "aliased primary/pending store is mutated once and verified");
    char message[256];
    require(!be_world_features(0), "unavailable native metadata advertises no level capability");
    require(!be_world_command(0, "master_sword", "", 1, message, sizeof message), "unavailable inventory blocks native level mutation");
    char *json = NULL; size_t length = 0; FILE *out = open_memstream(&json, &length);
    require(out != NULL, "open unavailable world snapshot fixture");
    be_world_snapshot(out, 0, 0);
    require(!fclose(out) && strstr(json, "\"master_sword_level\":null") &&
        strstr(json, "\"master_sword_attack\":null") && strstr(json, "\"master_sword_full_power\":null"),
        "unavailable level fields are null instead of fabricated values");
    free(json);
}

static void pouch_fixture_reset(const BwPouchSlotKind *kind) {
    master_fixture_reset();
    master_fixture.expected_s32_name = kind->flag;
    master_fixture.values[0].power = kind->minimum;
    master_fixture.values[1].power = kind->minimum + 1;
}
static void pouch_fixture_original(const BwPouchSlotKind *kind) {
    require(master_fixture.values[0].power == kind->minimum &&
        master_fixture.values[1].power == kind->minimum + 1 &&
        master_fixture.values[0].full == 0 && master_fixture.values[1].full == 1,
        "slot failure restores distinct capacity stores and preserves unrelated Bool flags");
}
static void check_pouch_slots(void) {
    require(bw_pouch_slot_kind("weapons") == &bw_pouch_slot_kinds[0] &&
        bw_pouch_slot_kind("bows") == &bw_pouch_slot_kinds[1] &&
        bw_pouch_slot_kind("shields") == &bw_pouch_slot_kinds[2], "only three equipment categories resolve to native flags");
    require(!bw_pouch_slot_kind("materials") && !bw_pouch_slot_kind("Weapons") && !bw_pouch_slot_kind("WeaponPorchStockNum"),
            "arbitrary flag names and unknown categories are refused");
    BeWorldRefs refs = {.pending = 1, .primary = 2}; int restored = 0;
    for (size_t k = 0; k < sizeof bw_pouch_slot_kinds / sizeof *bw_pouch_slot_kinds; ++k) {
        const BwPouchSlotKind *kind = &bw_pouch_slot_kinds[k];
        require(bw_pouch_slots_valid(kind, kind->minimum) && bw_pouch_slots_valid(kind, kind->maximum) &&
            !bw_pouch_slots_valid(kind, kind->minimum - 1) && !bw_pouch_slots_valid(kind, kind->maximum + 1),
            "capacity bounds match the native saved flag minimum and hard maximum");
        pouch_fixture_reset(kind);
        require(bw_pouch_slots_transaction(0, &refs, kind, kind->maximum, &master_fixture_access, &restored) && restored,
                "increase each equipment capacity to its native hard maximum");
        require(master_fixture.values[0].power == kind->maximum && master_fixture.values[1].power == kind->maximum &&
            master_fixture.reads == 4 && master_fixture.writes == 2 &&
            master_fixture.values[0].full == 0 && master_fixture.values[1].full == 1,
            "capacity success reads both stores back and leaves unrelated flags unchanged");
        pouch_fixture_reset(kind);
        require(!bw_pouch_slots_transaction(0, &refs, kind, kind->minimum, &master_fixture_access, &restored) &&
            !master_fixture.writes, "request below current primary capacity cannot shrink any slots");
        pouch_fixture_original(kind);
        pouch_fixture_reset(kind);
        master_fixture.values[0].power = kind->minimum + 1; master_fixture.values[1].power = kind->minimum;
        require(!bw_pouch_slots_transaction(0, &refs, kind, kind->minimum, &master_fixture_access, &restored) &&
            !master_fixture.writes, "request below current pending capacity cannot shrink any slots");
        for (int invalid = 0; invalid < 2; ++invalid) {
            pouch_fixture_reset(kind);
            require(!bw_pouch_slots_transaction(0, &refs, kind, invalid ? kind->maximum + 1 : kind->minimum - 1,
                &master_fixture_access, &restored) && !master_fixture.writes, "out-of-range direct capacity refuses before writes");
            pouch_fixture_original(kind);
        }
        for (int failure = 1; failure <= 2; ++failure) {
            pouch_fixture_reset(kind); master_fixture.fail_write = failure;
            require(!bw_pouch_slots_transaction(0, &refs, kind, kind->maximum, &master_fixture_access, &restored) && restored,
                    "setter failure after changing capacity rolls both stores back");
            pouch_fixture_original(kind);
        }
        pouch_fixture_reset(kind); master_fixture.fail_read = 3;
        require(!bw_pouch_slots_transaction(0, &refs, kind, kind->maximum, &master_fixture_access, &restored) && restored,
                "failed capacity readback rolls back rather than claiming success");
        pouch_fixture_original(kind);
        pouch_fixture_reset(kind); master_fixture.silent_write = 2;
        require(!bw_pouch_slots_transaction(0, &refs, kind, kind->maximum, &master_fixture_access, &restored) && restored,
                "wrong capacity readback detects a silently ignored setter");
        pouch_fixture_original(kind);
        pouch_fixture_reset(kind); master_fixture.values[1].power = kind->maximum + 1;
        require(!bw_pouch_slots_transaction(0, &refs, kind, kind->maximum, &master_fixture_access, &restored) &&
            !master_fixture.writes, "invalid current native capacity refuses without normalization");
        pouch_fixture_reset(kind); master_fixture.fail_read = 1;
        require(!bw_pouch_slots_transaction(0, &refs, kind, kind->maximum, &master_fixture_access, &restored) &&
            !master_fixture.writes, "unavailable current capacity refuses before writes");
        pouch_fixture_reset(kind); master_fixture.fail_write = 2; master_fixture.fail_rollback = 3;
        require(!bw_pouch_slots_transaction(0, &refs, kind, kind->maximum, &master_fixture_access, &restored) && !restored,
                "incomplete capacity rollback is reported honestly");
        pouch_fixture_reset(kind);
        BeWorldRefs shared = {.pending = 1, .primary = 1};
        require(bw_pouch_slots_transaction(0, &shared, kind, kind->maximum, &master_fixture_access, &restored) &&
            master_fixture.writes == 1, "aliased game stores apply capacity once");
    }
    char message[256];
    require(!be_world_command(0, "pouch_slots", "weapons", 20, message, sizeof message), "unavailable native metadata refuses capacity edits");
    char *json = NULL; size_t length = 0; FILE *out = open_memstream(&json, &length);
    require(out != NULL, "open capacity snapshot fixture");
    be_world_snapshot(out, 0, 0);
    require(!fclose(out) && strstr(json, "\"weapon_slots\":null") && strstr(json, "\"bow_slots\":null") &&
        strstr(json, "\"shield_slots\":null"), "unknown capacity fields remain null");
    free(json);
}

static struct {
    int values[2][2], reads, writes, fail_read, fail_write, silent_write, fail_rollback;
} map_fixture;
static int map_fixture_flag(const char *name) {
    require(!strcmp(name, "PortableUnit_PowerOn") || !strcmp(name, "IsGet_PortableUnit"),
            "map access reads/writes only the two verified slate capability flags");
    return !strcmp(name, "IsGet_PortableUnit");
}
static int *map_fixture_value(uintptr_t parameter, const char *name) {
    require(parameter == 1 || parameter == 2, "map access touches only prepared primary/pending stores");
    return &map_fixture.values[parameter - 1][map_fixture_flag(name)];
}
static int map_fixture_get_bool(uintptr_t base, uintptr_t parameter, const char *name, int *out) {
    (void)base;
    if (++map_fixture.reads == map_fixture.fail_read) return 0;
    *out = *map_fixture_value(parameter, name); return 1;
}
static int map_fixture_put_bool(uintptr_t base, uintptr_t parameter, const char *name, int value, int bypass) {
    (void)base;
    require((value == 0 || value == 1) && (bypass == 0 || bypass == 1) && (bypass || value == 1),
            "normal map unlock enables flags; only rollback may restore disabled flags");
    int write = ++map_fixture.writes;
    if (write == map_fixture.fail_rollback) return 0;
    if (write != map_fixture.silent_write) *map_fixture_value(parameter, name) = value;
    return write != map_fixture.fail_write;
}
static const BwWorldFlagAccess map_fixture_access = {.get_bool = map_fixture_get_bool, .put_bool = map_fixture_put_bool};
static void map_fixture_reset(void) {
    memset(&map_fixture, 0, sizeof map_fixture);
    map_fixture.values[0][1] = 1; map_fixture.values[1][0] = 1;
}
static void map_fixture_original(void) {
    require(map_fixture.values[0][0] == 0 && map_fixture.values[0][1] == 1 &&
        map_fixture.values[1][0] == 1 && map_fixture.values[1][1] == 0,
        "map transaction restores both original flags in both stores");
}
static void check_map_access(void) {
    BeWorldRefs refs = {.pending = 1, .primary = 2}; int restored = 0;
    map_fixture_reset();
    require(bw_unlock_flag_transaction(0, &refs, bw_map_access_flags, 2, &map_fixture_access, &restored) && restored,
            "map access unlock applies both repeatable native flags");
    require(map_fixture.reads == 8 && map_fixture.writes == 4 && map_fixture.values[0][0] == 1 &&
        map_fixture.values[0][1] == 1 && map_fixture.values[1][0] == 1 && map_fixture.values[1][1] == 1,
        "map access success requires complete readback of both stores");
    for (int failure = 1; failure <= 4; ++failure) {
        map_fixture_reset(); map_fixture.fail_write = failure;
        require(!bw_unlock_flag_transaction(0, &refs, bw_map_access_flags, 2, &map_fixture_access, &restored) && restored,
                "a map setter failure after mutation rolls every flag back");
        map_fixture_original();
    }
    map_fixture_reset(); map_fixture.fail_read = 5;
    require(!bw_unlock_flag_transaction(0, &refs, bw_map_access_flags, 2, &map_fixture_access, &restored) && restored,
            "failed map readback cannot report success");
    map_fixture_original();
    map_fixture_reset(); map_fixture.silent_write = 4;
    require(!bw_unlock_flag_transaction(0, &refs, bw_map_access_flags, 2, &map_fixture_access, &restored) && restored,
            "ignored map write is detected by final readback");
    map_fixture_original();
    map_fixture_reset(); map_fixture.fail_read = 1;
    require(!bw_unlock_flag_transaction(0, &refs, bw_map_access_flags, 2, &map_fixture_access, &restored) && !map_fixture.writes,
            "missing map flag refuses before any mutation");
    map_fixture_reset(); map_fixture.values[1][1] = 2;
    require(!bw_unlock_flag_transaction(0, &refs, bw_map_access_flags, 2, &map_fixture_access, &restored) && !map_fixture.writes,
            "invalid Bool refuses rather than normalizing map state");
    map_fixture_reset(); map_fixture.fail_write = 4; map_fixture.fail_rollback = 6;
    require(!bw_unlock_flag_transaction(0, &refs, bw_map_access_flags, 2, &map_fixture_access, &restored) && !restored,
            "map rollback failure is reported honestly");
    require(map_fixture.values[1][1] == 1, "failed map rollback retains evidence of the unrestored flag");
    map_fixture_reset(); BeWorldRefs shared = {.pending = 1, .primary = 1};
    require(bw_unlock_flag_transaction(0, &shared, bw_map_access_flags, 2, &map_fixture_access, &restored) && map_fixture.writes == 2,
            "aliased game stores apply each map flag once");
    map_fixture_reset();
    require(!bw_unlock_flag_transaction(0, &refs, bw_map_access_flags, 0, &map_fixture_access, &restored) && !map_fixture.writes,
            "empty map transaction refuses without mutation");
    require(!bw_unlock_flag_transaction(0, &refs, bw_map_access_flags, BW_MAX_TRANSACTION + 1, &map_fixture_access, &restored) && !map_fixture.writes,
            "oversized Bool transaction refuses before accessing its names");
    char message[256];
    require(!be_world_command(0, "map_access", "", 0, message, sizeof message), "unavailable native flags block map access edits");
    char *json = NULL; size_t length = 0; FILE *out = open_memstream(&json, &length);
    require(out != NULL, "open map-access snapshot fixture");
    be_world_snapshot(out, 0, 0);
    require(!fclose(out) && strstr(json, "\"map_access_unlocked\":null"), "unknown map access is null rather than invented");
    free(json);
}

static struct {
    int32_t values[2][BW_MAX_ACTIVATION_EDITS], original[2][BW_MAX_ACTIVATION_EDITS];
    BwActivationEdit edits[BW_MAX_ACTIVATION_EDITS];
    char names[BW_MAX_ACTIVATION_EDITS][40];
    size_t count;
    int reads, writes, fail_read, fail_write, silent_write, fail_rollback;
} activation_fixture;
static size_t activation_fixture_index(uintptr_t parameter, const char *name, int kind) {
    require(parameter == 1 || parameter == 2, "mixed transaction touches only its prepared stores");
    for (size_t i = 0; i < activation_fixture.count; ++i) if (!strcmp(name, activation_fixture.edits[i].name)) {
        require(kind == activation_fixture.edits[i].kind || (kind == 1 && activation_fixture.edits[i].kind == 2),
                "mixed transaction uses the verified native flag type");
        return i;
    }
    require(0, "mixed transaction cannot write undeclared Clear/Enter/reward/story flags"); return 0;
}
static int activation_fixture_get(uintptr_t parameter, const char *name, int kind, int32_t *out) {
    if (++activation_fixture.reads == activation_fixture.fail_read) return 0;
    size_t i = activation_fixture_index(parameter, name, kind);
    *out = activation_fixture.values[parameter - 1][i]; return 1;
}
static int activation_fixture_get_s32(uintptr_t base, uintptr_t parameter, const char *name, int32_t *out) {
    (void)base; return activation_fixture_get(parameter, name, 1, out);
}
static int activation_fixture_get_bool(uintptr_t base, uintptr_t parameter, const char *name, int *out) {
    (void)base; int32_t value = 0;
    if (!activation_fixture_get(parameter, name, 0, &value)) return 0;
    *out = value; return 1;
}
static int activation_fixture_put(uintptr_t parameter, const char *name, int kind, int32_t value) {
    size_t i = activation_fixture_index(parameter, name, kind); int write = ++activation_fixture.writes;
    if (write == activation_fixture.fail_rollback) return 0;
    if (write != activation_fixture.silent_write) activation_fixture.values[parameter - 1][i] = value;
    return write != activation_fixture.fail_write;
}
static int activation_fixture_put_s32(uintptr_t base, uintptr_t parameter, const char *name, int32_t value) {
    (void)base; return activation_fixture_put(parameter, name, 1, value);
}
static int activation_fixture_put_bool(uintptr_t base, uintptr_t parameter, const char *name, int value, int rollback) {
    (void)base;
    require((value == 0 || value == 1) && (rollback || value == 1), "normal mixed unlock enables Bool flags; only rollback restores false");
    return activation_fixture_put(parameter, name, 0, value);
}
static const BwWorldFlagAccess activation_fixture_access = {
    activation_fixture_get_s32, activation_fixture_get_bool, activation_fixture_put_s32, activation_fixture_put_bool
};
static void activation_fixture_add(const char *name, int kind) {
    size_t i = activation_fixture.count++;
    snprintf(activation_fixture.names[i], sizeof activation_fixture.names[i], "%s", name);
    activation_fixture.edits[i] = (BwActivationEdit){activation_fixture.names[i], kind};
    activation_fixture.values[0][i] = kind ? (i % 3 ? 0 : 3) : (int)(i % 2);
    activation_fixture.values[1][i] = kind ? (i % 3 == 1 ? 7 : 0) : 0;
}
static void activation_fixture_reset(int full_access) {
    memset(&activation_fixture, 0, sizeof activation_fixture);
    for (int i = 0; i < (full_access ? 120 : 2); ++i) {
        char name[40]; snprintf(name, sizeof name, "Open_Dungeon%03d", i); activation_fixture_add(name, 0);
        snprintf(name, sizeof name, "Location_Dungeon%03d", i); activation_fixture_add(name, 1);
    }
    if (full_access) {
        for (int i = 1; i <= 15; ++i) { char name[40]; snprintf(name, sizeof name, "MapTower_%02d", i); activation_fixture_add(name, 0); }
        for (size_t i = 0; i < sizeof bw_rune_flags / sizeof *bw_rune_flags; ++i) activation_fixture_add(bw_rune_flags[i], 0);
        activation_fixture_add("IsGet_PlayerStole2", 0); activation_fixture_add("MapTower_07_Demo", 0);
    }
    memcpy(activation_fixture.original, activation_fixture.values, sizeof activation_fixture.original);
}
static void activation_fixture_restored(void) {
    require(!memcmp(activation_fixture.values, activation_fixture.original, sizeof activation_fixture.values),
            "mixed rollback restores every original Bool/discovery value in both stores");
}
static void check_shrine_activation(void) {
    require(bw_discovery_permissions("Location_Dungeon000", 1, 0x07) == 1, "ordinary discovery flags retain native permission checks");
    require(bw_discovery_permissions("Location_Dungeon001", 1, 0x05) == 0, "exact native read-only discovery exception is recognized");
    require(bw_discovery_permissions("Location_Dungeon001", 1, 0x07) == 1, "writable metadata never requests a permission bypass");
    const char *not_exceptions[] = {"Location_Dungeon000", "Location_Dungeon002", "Location_Dungeon120", "Dungeon001", "Location_Dungeon001_extra", "DungeonClearCounter", "CurrentRupee"};
    for (size_t i = 0; i < sizeof not_exceptions / sizeof *not_exceptions; ++i)
        require(bw_discovery_permissions(not_exceptions[i], 1, 0x05) == -1, "read-only S32 exception never extends to another flag");
    require(bw_discovery_permissions("Location_Dungeon001", 0, 0x05) == -1 &&
        bw_discovery_permissions("Location_Dungeon001", 1, 0x0d) == -1 &&
        bw_discovery_permissions("Location_Dungeon001", 1, 0x04) == -1, "exception rejects wrong type, one-trigger or unreadable metadata");
    require(!bw_discovery_bypass_code(0), "native permission exception requires matching executable instructions");
    require(sizeof bw_rune_flags / sizeof *bw_rune_flags == 9 &&
        !strcmp(bw_rune_flags[7], "PortableUnit_PowerOn") && !strcmp(bw_rune_flags[8], "IsGet_PortableUnit"),
        "rune unlock includes both verified Slate pickup gates");
    BeWorldRefs refs = {.pending = 1, .primary = 2}; int restored = 0;
    activation_fixture_reset(0);
    require(bw_activation_transaction(0, &refs, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, &restored),
            "mixed shrine activation commits discovery and Open together");
    for (size_t i = 0; i < activation_fixture.count; ++i) for (int store = 0; store < 2; ++store) {
        int32_t old = activation_fixture.original[store][i];
        require(activation_fixture.values[store][i] == (activation_fixture.edits[i].kind && old > 1 ? old : 1),
                "each store preserves its own positive discovery state while enabling access");
    }
    for (int failure = 1; failure <= 8; ++failure) {
        activation_fixture_reset(0); activation_fixture.fail_write = failure;
        require(!bw_activation_transaction(0, &refs, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, &restored) && restored,
                "each mixed setter failure after mutation rolls every store back");
        activation_fixture_restored();
    }
    activation_fixture_reset(0); activation_fixture.fail_read = 9;
    require(!bw_activation_transaction(0, &refs, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, &restored) && restored,
            "mixed final readback failure prevents success");
    activation_fixture_restored();
    activation_fixture_reset(0); activation_fixture.silent_write = 8;
    require(!bw_activation_transaction(0, &refs, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, &restored) && restored,
            "silently ignored discovery setter is detected and rolled back");
    activation_fixture_restored();
    activation_fixture_reset(0); activation_fixture.fail_read = 8;
    require(!bw_activation_transaction(0, &refs, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, &restored) && !activation_fixture.writes,
            "all mixed old values must be available before any write");
    activation_fixture_restored();
    activation_fixture_reset(0); activation_fixture.values[0][0] = 2;
    require(!bw_activation_transaction(0, &refs, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, &restored) && !activation_fixture.writes,
            "invalid current Bool refuses the entire mixed transaction");
    activation_fixture_reset(0); activation_fixture.edits[0].kind = 3;
    require(!bw_activation_transaction(0, &refs, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, &restored) && !activation_fixture.writes,
            "unknown mixed flag type refuses before mutation");
    activation_fixture_reset(0); activation_fixture.values[0][1] = INT_MAX;
    require(bw_activation_transaction(0, &refs, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, &restored) &&
        activation_fixture.values[0][1] == INT_MAX, "large positive discovery values remain unchanged without overflow");
    activation_fixture_reset(0); activation_fixture.fail_write = 8; activation_fixture.fail_rollback = 9;
    require(!bw_activation_transaction(0, &refs, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, &restored) && !restored,
            "mixed rollback failure is reported honestly");
    activation_fixture_reset(0); BeWorldRefs shared = {.pending = 1, .primary = 1};
    require(bw_activation_transaction(0, &shared, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, &restored) &&
        activation_fixture.writes == 4, "aliased stores mutate each mixed flag once");
    require(!memcmp(activation_fixture.values[1], activation_fixture.original[1], sizeof activation_fixture.values[1]),
            "aliased transaction does not touch the other fixture store");
    activation_fixture_reset(1);
    require(activation_fixture.count == 266, "Full Access includes146 Bool and120 discovery S32 edits within the exact bound");
    require(bw_activation_transaction(0, &refs, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, &restored) &&
        activation_fixture.writes == 532 && activation_fixture.reads == 1064, "all120 shrine/FullAccess edits verify both complete stores");
    activation_fixture_reset(1); activation_fixture.fail_write = 532;
    require(!bw_activation_transaction(0, &refs, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, &restored) && restored,
            "failure at the final Full Access setter rolls the entire mixed batch back");
    activation_fixture_restored();
    activation_fixture_reset(0);
    require(!bw_activation_transaction(0, &refs, activation_fixture.edits, 0, &activation_fixture_access, &restored) && !activation_fixture.writes,
            "empty mixed transaction refuses before access");
    require(!bw_activation_transaction(0, &refs, activation_fixture.edits, BW_MAX_ACTIVATION_EDITS + 1, &activation_fixture_access, &restored) && !activation_fixture.writes,
            "oversized mixed transaction refuses before accessing entries");
    char message[256];
    require(!be_world_command(0, "unlock_shrine", "Dungeon002", 0, message, sizeof message) &&
        !be_world_command(0, "unlock_all_shrines", "", 0, message, sizeof message) &&
        !be_world_command(0, "full_access", "", 0, message, sizeof message), "unavailable native state refuses every shrine-discovery entry point");
}

static void completion_fixture_reset(int all) {
    memset(&activation_fixture, 0, sizeof activation_fixture);
    for (int i = 0; i < (all ? 120 : 2); ++i) {
        char name[40]; snprintf(name, sizeof name, "Open_Dungeon%03d", i); activation_fixture_add(name, 0);
        snprintf(name, sizeof name, "Location_Dungeon%03d", i); activation_fixture_add(name, 1);
        snprintf(name, sizeof name, "Clear_Dungeon%03d", i); activation_fixture_add(name, 0);
        size_t at = activation_fixture.count - 1;
        activation_fixture.values[0][at] = activation_fixture.values[1][at] = i % 2;
    }
    activation_fixture_add("DungeonClearCounter", 2);
    size_t at = activation_fixture.count - 1;
    activation_fixture.values[0][at] = activation_fixture.values[1][at] = 10;
    memcpy(activation_fixture.original, activation_fixture.values, sizeof activation_fixture.original);
}
static int completion_fixture_apply(const BeWorldRefs *refs, int *restored) {
    return bw_activation_transaction(0, refs, activation_fixture.edits, activation_fixture.count, &activation_fixture_access, restored);
}
static void check_shrine_completion(void) {
    require(bw_completion_counter_permissions("DungeonClearCounter", 1, 0x05) == 0,
            "only exact read/save counter metadata permits the native completion exception");
    require(bw_completion_counter_permissions("DungeonClearCounter", 1, 0x07) == -1 &&
        bw_completion_counter_permissions("DungeonClearCounter", 0, 0x05) == -1 &&
        bw_completion_counter_permissions("CurrentRupee", 1, 0x05) == -1 &&
        bw_completion_counter_permissions("Location_Dungeon001", 1, 0x05) == -1,
        "completion exception cannot bypass another name, type or unexpected metadata");
    BeWorldRefs refs = {.pending = 1, .primary = 2}; int restored = 0;
    completion_fixture_reset(0);
    require(completion_fixture_apply(&refs, &restored) && restored, "separate completion commits activation, discovery, Clear and counter together");
    require(activation_fixture.values[0][6] == 11 && activation_fixture.values[1][6] == 11,
            "completion increments its counter once only for the newly cleared shrine");
    for (size_t i = 0; i < 6; ++i) for (int store = 0; store < 2; ++store) {
        int32_t old = activation_fixture.original[store][i];
        require(activation_fixture.values[store][i] == (activation_fixture.edits[i].kind == 1 && old > 1 ? old : 1),
                "completion preserves discovered states and enables target access/Clear flags");
    }
    require(completion_fixture_apply(&refs, &restored) && activation_fixture.values[0][6] == 11 && activation_fixture.values[1][6] == 11,
            "repeated completion is idempotent and does not add another counter increment");
    for (int failure = 1; failure <= 14; ++failure) {
        completion_fixture_reset(0); activation_fixture.fail_write = failure;
        require(!completion_fixture_apply(&refs, &restored) && restored, "every completion setter failure after mutation rolls the complete batch back");
        activation_fixture_restored();
    }
    completion_fixture_reset(0); activation_fixture.fail_read = 15;
    require(!completion_fixture_apply(&refs, &restored) && restored, "failed completion readback cannot report success");
    activation_fixture_restored();
    completion_fixture_reset(0); activation_fixture.silent_write = 14;
    require(!completion_fixture_apply(&refs, &restored) && restored, "wrong counter readback rolls Clear and discovery back");
    activation_fixture_restored();
    completion_fixture_reset(0); activation_fixture.fail_write = 14; activation_fixture.fail_rollback = 15;
    require(!completion_fixture_apply(&refs, &restored) && !restored, "incomplete completion rollback is reported honestly");
    completion_fixture_reset(0); activation_fixture.values[1][6] = 11;
    require(!completion_fixture_apply(&refs, &restored) && !activation_fixture.writes, "disagreeing native counters refuse before mutation");
    completion_fixture_reset(0); activation_fixture.values[1][2] = 1;
    require(!completion_fixture_apply(&refs, &restored) && !activation_fixture.writes, "disagreeing target Clear states cannot double-count completion");
    completion_fixture_reset(0); activation_fixture.values[0][6] = activation_fixture.values[1][6] = 255;
    require(!completion_fixture_apply(&refs, &restored) && !activation_fixture.writes, "counter overflow refuses before setting Clear flags");
    completion_fixture_reset(0); activation_fixture.values[0][6] = activation_fixture.values[1][6] = -1;
    require(!completion_fixture_apply(&refs, &restored) && !activation_fixture.writes, "invalid current counter refuses without normalization");
    completion_fixture_reset(0); activation_fixture.values[0][6] = activation_fixture.values[1][6] = 254;
    require(completion_fixture_apply(&refs, &restored) && activation_fixture.values[0][6] == 255,
            "last available counter increment remains within the native maximum");
    require(completion_fixture_apply(&refs, &restored) && activation_fixture.values[0][6] == 255,
            "already-cleared targets remain idempotent even at the counter maximum");
    completion_fixture_reset(0); activation_fixture.edits[6].name = "CurrentRupee";
    require(!completion_fixture_apply(&refs, &restored) && !activation_fixture.writes, "counter edit kind refuses every unowned scalar name");
    completion_fixture_reset(0); activation_fixture.edits[1].name = activation_fixture.edits[0].name;
    require(!completion_fixture_apply(&refs, &restored) && !activation_fixture.writes, "duplicate transaction names cannot double-count Clear deltas");
    completion_fixture_reset(0); activation_fixture.count = 6;
    require(!completion_fixture_apply(&refs, &restored) && !activation_fixture.writes, "Clear changes require their single counter entry");
    completion_fixture_reset(0); BeWorldRefs shared = {.pending = 1, .primary = 1};
    require(completion_fixture_apply(&shared, &restored) && activation_fixture.writes == 7 && activation_fixture.values[0][6] == 11,
            "aliased stores apply each completion field and counter once");
    completion_fixture_reset(1);
    require(activation_fixture.count == 361 && activation_fixture.count <= BW_MAX_ACTIVATION_EDITS,
            "all120 completion operations and counter fit the bounded mixed transaction");
    require(completion_fixture_apply(&refs, &restored) && activation_fixture.values[0][360] == 70 &&
        activation_fixture.values[1][360] == 70 && activation_fixture.writes == 722,
        "all-shrine completion increments only the60 newly cleared targets");
    require(completion_fixture_apply(&refs, &restored) && activation_fixture.values[0][360] == 70,
            "all-shrine completion does not increment counters again");
    completion_fixture_reset(1); activation_fixture.fail_write = 722;
    require(!completion_fixture_apply(&refs, &restored) && restored, "final all-shrine counter failure restores every original shrine state");
    activation_fixture_restored();
    char message[256];
    require(!be_world_command(0, "complete_shrine", "Dungeon000", 0, message, sizeof message) &&
        !be_world_command(0, "complete_all_shrines", "", 0, message, sizeof message), "unavailable native state refuses completion before editing");
    char *json = NULL; size_t length = 0; FILE *out = open_memstream(&json, &length);
    require(out != NULL, "open completion snapshot fixture");
    be_world_snapshot(out, 0, 0);
    require(!fclose(out) && strstr(json, "\"shrine_clear_count\":null"), "unknown shrine counter remains null");
    free(json);
}

static void check_queue_and_unready_state(void) {
    reset_editor();
    require(!runtime_botw_editor_command("shot unused.png\n"), "leave non-editor commands to their own handlers");
    require(runtime_botw_editor_command("trainer 101 health on\n"), "handle a health request");
    require(be_queue_count == 1 && !be_health_lock && !be_last_request,
            "input handler queues without applying a gameplay edit");
    runtime_botw_editor_tick(1000000000ULL);
    require(be_queue_count == 0 && !be_health_lock && be_last_request == 101 && !be_last_ok,
            "reject an edit when gameplay pointers are unavailable");
    require(runtime_botw_editor_command("trainer 102 status\n"), "queue a status request");
    runtime_botw_editor_tick(1100000000ULL);
    require(be_last_request == 102 && be_last_ok, "acknowledge status even before gameplay is ready");
    require(runtime_botw_editor_command("trainer 103 health off\n"), "queue an off request");
    runtime_botw_editor_tick(1200000000ULL);
    require(be_last_request == 103 && be_last_ok && !be_health_lock, "allow disabling a lock without gameplay");
    require(runtime_botw_editor_command("trainer 104 stamina off\n"), "queue a stamina-off request");
    runtime_botw_editor_tick(1300000000ULL);
    require(be_last_request == 104 && be_last_ok && !be_stamina_lock, "acknowledge stamina-off without gameplay");
    for (int i = 0; i < BE_QUEUE_CAPACITY + 1; ++i)
        runtime_botw_editor_command("trainer 105 status\n");
    require(be_queue_count == BE_QUEUE_CAPACITY && !be_last_ok, "bound the request queue and reject overflow");
}

static unsigned fixture_features(uintptr_t base) {
    return base ? BE_FEATURE_RUPEES : 0;
}

static int fixture_command(uintptr_t base, const char *verb, const char *argument,
                           int value, char *result, size_t capacity) {
    (void)argument;
    require(base == (uintptr_t)fixture_main, "native callback gets the validated fixture base");
    require(!strcmp(verb, "rupees") && value == 27, "native callback gets the parsed action");
    ++extension_calls;
    snprintf(result, capacity, "Fixture callback acknowledged");
    return 1;
}

static void check_synthetic_live_values(void) {
    reset_editor();
    /* Only identity pointer slots/value fields are populated. The allocated address
       range reproduces NX160 data-layout offsets, not game code or game data. */
    fixture_main = calloc(1, 0x2ca1a80);
    unsigned char *health_root = calloc(1, 0x100), *health_object = calloc(1, 0x2000);
    unsigned char *stamina_root = calloc(1, 0x100), *stamina_object = calloc(1, 0x1000);
    unsigned char *player_info = calloc(1, 0x100);
    require(fixture_main && health_root && health_object && stamina_root && stamina_object && player_info,
            "allocate pointer-chain fixtures");
    memcpy(fixture_main + 0x2ca1a78, &health_root, 8);
    memcpy(health_root + 0x80, &health_object, 8);
    memcpy(fixture_main + 0x2c9fd70, &stamina_root, 8);
    memcpy(stamina_root + 0x38, &stamina_object, 8);
    uint32_t health = 12;
    float stamina = 500, maximum = 1000;
    memcpy(health_object + 0x848, &health, 4);
    memcpy(stamina_object + 0x2ac, &stamina, 4);
    memcpy(stamina_object + 0x2b0, &maximum, 4);
    BotwEditorSnapshot snapshot = be_read_snapshot();
    require(!be_ready(&snapshot) && !snapshot.health_valid && !snapshot.stamina_valid,
            "title-like numeric defaults without a real Player identity are not gameplay");
    uintptr_t info_table = (uintptr_t)fixture_main + 0x2b7be80;
    uintptr_t actor_table = (uintptr_t)fixture_main + 0x2b7bea8;
    uintptr_t max_life_accessor = (uintptr_t)fixture_main + 0xf33680;
    uintptr_t life_accessor = (uintptr_t)fixture_main + 0xd50;
    memcpy(fixture_main + 0x2ca1140, &player_info, 8);
    memcpy(player_info, &info_table, 8);
    memcpy(player_info + 0x60, &health_object, 8);
    memcpy(health_object, &actor_table, 8);
    memcpy((void *)(actor_table + 0xf0), &max_life_accessor, 8);
    memcpy((void *)(actor_table + 0x2b8), &life_accessor, 8);
    int32_t maximum_life = 12;
    memcpy(health_object + 0x1868, &maximum_life, 4);
    snapshot = be_read_snapshot();
    require(be_ready(&snapshot) && snapshot.health == 12 && snapshot.stamina == 500,
            "validate synthetic NX160 Player identity and health/stamina pointer chains");
    uintptr_t wrong_table = actor_table + 8;
    memcpy(health_object, &wrong_table, 8);
    snapshot = be_read_snapshot();
    require(!be_ready(&snapshot), "reject an actor with the wrong Player vtable");
    memcpy(health_object, &actor_table, 8);
    memcpy(player_info + 0x60, &stamina_object, 8);
    snapshot = be_read_snapshot();
    require(!be_ready(&snapshot), "reject a PlayerInfo actor different from the health-chain actor");
    memcpy(player_info + 0x60, &health_object, 8);
    uintptr_t wrong_accessor = life_accessor + 8;
    memcpy((void *)(actor_table + 0x2b8), &wrong_accessor, 8);
    snapshot = be_read_snapshot();
    require(!be_ready(&snapshot), "reject a mismatched native life accessor");
    memcpy((void *)(actor_table + 0x2b8), &life_accessor, 8);
    maximum_life = 0;
    memcpy(health_object + 0x1868, &maximum_life, 4);
    snapshot = be_read_snapshot();
    require(!be_ready(&snapshot), "reject an uninitialized Player life capacity");
    maximum_life = 12;
    memcpy(health_object + 0x1868, &maximum_life, 4);
    runtime_botw_editor_extensions((BotwEditorExtensions){.features = fixture_features, .command = fixture_command});
    runtime_botw_editor_command("trainer 201 stamina on\n");
    memcpy(&stamina, stamina_object + 0x2ac, 4);
    require(stamina == 500 && !be_stamina_lock, "queueing does not refill stamina on the command thread");
    runtime_botw_editor_tick(1000000000ULL);
    memcpy(&stamina, stamina_object + 0x2ac, 4);
    require(be_last_request == 201 && be_last_ok && be_stamina_lock && stamina == 1000,
            "apply, read back and acknowledge a stamina refill on tick");
    runtime_botw_editor_command("trainer 202 health on\n");
    runtime_botw_editor_tick(1100000000ULL);
    require(be_last_request == 202 && be_last_ok && be_health_lock && be_health_target == 12,
            "lock health at its observed value");
    health = 4;
    stamina = 200;
    memcpy(health_object + 0x848, &health, 4);
    memcpy(stamina_object + 0x2ac, &stamina, 4);
    runtime_botw_editor_tick(1200000000ULL);
    memcpy(&health, health_object + 0x848, 4);
    memcpy(&stamina, stamina_object + 0x2ac, 4);
    require(health == 12 && stamina == 1000, "locks restore later valid values");
    runtime_botw_editor_command("trainer 203 rupees 27\n");
    require(extension_calls == 0, "native extension does not run on the command handler");
    runtime_botw_editor_tick(1300000000ULL);
    require(extension_calls == 1 && be_last_request == 203 && be_last_ok,
            "native extension executes and acknowledges on tick");
    health = 0;
    memcpy(health_object + 0x848, &health, 4);
    runtime_botw_editor_tick(1400000000ULL);
    require(!be_health_lock && !be_stamina_lock && !be_health_target,
            "loss of a valid gameplay context clears locks");
    health = 12;
    stamina = NAN;
    memcpy(health_object + 0x848, &health, 4);
    memcpy(stamina_object + 0x2ac, &stamina, 4);
    require(!be_ready(&((BotwEditorSnapshot){0})), "empty state is not ready");
    snapshot = be_read_snapshot();
    require(!snapshot.stamina_valid && !be_ready(&snapshot), "reject non-finite live stamina");
    free(health_object);
    free(health_root);
    free(stamina_object);
    free(stamina_root);
    free(player_info);
    free(fixture_main);
    fixture_main = NULL;
}

static void check_json_escaping(void) {
    char *text = NULL;
    size_t length = 0;
    FILE *output = open_memstream(&text, &length);
    require(output != NULL, "open JSON fixture stream");
    be_json_string(output, "quoted\" text\nslash\\");
    require(fclose(output) == 0, "finish JSON fixture stream");
    require(!strcmp(text, "\"quoted\\\" text\\u000aslash\\\\\""), "escape JSON quotes, controls and slashes");
    free(text);
}

static void check_unavailable_horse_adapter(void) {
    require(!be_horse_features(0), "unknown native code cannot advertise horse editing");
    char result[256] = {0};
    require(!be_horse_command(0, "horse_speed", "", 150, result, sizeof result), "unavailable horse resource refuses before a native getter call");
    require(result[0] != 0, "horse resource refusal explains the unavailable state");
    char *text = NULL; size_t size = 0;
    FILE *output = open_memstream(&text, &size);
    require(output != NULL, "open unavailable horse snapshot fixture");
    be_horse_snapshot(output, 0, 0);
    require(!fclose(output) && strstr(text, "\"horse_speed_available\":false") && strstr(text, "\"horse_speed_multiplier\":null"), "horse snapshot does not invent active speed without live data");
    free(text);
}

int main(void) {
    /* A caller may have launched these checks from a game development shell.
       Never allow the fixture publisher to use a real profile/control path. */
    unsetenv("TOTK_INPUT_FILE");
    unsetenv("TOTK_CONTROLS");
    reset_editor();
    check_memory_guards();
    check_parser();
    check_inventory_values();
    check_inventory_armor_and_readiness();
    check_master_sword_levels();
    check_pouch_slots();
    check_map_access();
    check_shrine_activation();
    check_shrine_completion();
    check_queue_and_unready_state();
    check_synthetic_live_values();
    check_json_escaping();
    check_unavailable_horse_adapter();
    printf("Runtime guard checks passed: %u assertions; synthetic memory only, no game launched.\n", checks);
    return 0;
}
