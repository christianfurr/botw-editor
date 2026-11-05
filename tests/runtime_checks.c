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
        "trainer 20 quantity 419 100000000"
    };
    for (size_t i = 0; i < sizeof valid / sizeof *valid; ++i) expect_parse(valid[i], 1);
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
        "trainer 24 quantity 0 1.5"
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
    check_queue_and_unready_state();
    check_synthetic_live_values();
    check_json_escaping();
    printf("Runtime guard checks passed: %u assertions; synthetic memory only, no game launched.\n", checks);
    return 0;
}
