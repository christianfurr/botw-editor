#ifndef MNG_BOTW_EDITOR_IMPLEMENTATION_H
#define MNG_BOTW_EDITOR_IMPLEMENTATION_H

/* Local live-editor bridge. The input-file thread queues requests; the game's
   presentation thread applies them. NX 1.6.0 pointer-layout facts are independently
   guarded at runtime (Pistonight/botw-save-state raw_ptr.hpp). Locks default off. */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BE_FEATURE_RUPEES    (1u << 0)
#define BE_FEATURE_ITEMS     (1u << 1)
#define BE_FEATURE_INVENTORY (1u << 2)
#define BE_FEATURE_TELEPORT  (1u << 3)
#define BE_FEATURE_SHRINES   (1u << 4)
#define BE_FEATURE_MAP       (1u << 5)
#define BE_FEATURE_RUNES     (1u << 6)
#define BE_FEATURE_LIFT      (1u << 7)
#define BE_FEATURE_MASTER_SWORD (1u << 8)
#define BE_FEATURE_POUCH_SLOTS (1u << 9)
#define BE_FEATURE_MAP_ACCESS (1u << 10)
#define BE_FEATURE_SHRINE_COMPLETION (1u << 11)
#define BE_FEATURE_HORSE_SPEED (1u << 12)

typedef struct {
    unsigned (*features)(uintptr_t main_base);
    int (*command)(uintptr_t main_base, const char *verb, const char *argument,
                   int value, char *result, size_t capacity);
    /* Append optional JSON fields, including leading commas; do not emit features. */
    void (*snapshot)(FILE *output, uintptr_t main_base, int ready);
} BotwEditorExtensions;

typedef struct {
    uint64_t id;
    char verb[32], argument[96];
    int value;
} BotwEditorRequest;

typedef struct {
    uintptr_t main_base, health_address, stamina_address, max_stamina_address;
    uint32_t health;
    float stamina, max_stamina;
    int health_valid, stamina_valid;
    unsigned extra_features;
} BotwEditorSnapshot;

enum {
    BE_QUEUE_CAPACITY = 32,
    BE_MAX_STACK_QUANTITY = 999,
    BE_MAX_RAW_DURABILITY = 100000000,
    BE_WEAPON_SLOTS_MIN = 8, BE_WEAPON_SLOTS_MAX = 20,
    BE_BOW_SLOTS_MIN = 5, BE_BOW_SLOTS_MAX = 14,
    BE_SHIELD_SLOTS_MIN = 4, BE_SHIELD_SLOTS_MAX = 20
};
static BotwEditorExtensions be_extensions;
static BotwEditorRequest be_queue[BE_QUEUE_CAPACITY];
static unsigned be_queue_head, be_queue_count;
static pthread_mutex_t be_lock = PTHREAD_MUTEX_INITIALIZER;
static int be_health_lock, be_stamina_lock;
static uint32_t be_health_target;
static uint64_t be_last_request, be_last_tick, be_last_publish;
static int be_last_ok;
static char be_last_result[256] = "No changes requested";
static char be_state_path[PATH_MAX];

/* Every read/write checks all crossed VM regions. Unmapped, read-only and
   overflowed spans are rejected; no external process memory is accessed. */
static int be_memory_span(uintptr_t address, size_t size, vm_prot_t required) {
    if (!address || !size || address > UINTPTR_MAX - size) return 0;
    mach_vm_address_t cursor = address, end = address + size;
    while (cursor < end) {
        mach_vm_address_t region = cursor;
        mach_vm_size_t length = 0;
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        kern_return_t kr = mach_vm_region(mach_task_self(), &region, &length,
            VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &count, &object);
        if (object != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), object);
        if (kr != KERN_SUCCESS || region > cursor || !length ||
            region > UINT64_MAX - length || (info.protection & required) != required) return 0;
        mach_vm_address_t next = region + length;
        if (next <= cursor) return 0;
        cursor = next < end ? next : end;
    }
    return 1;
}
static int be_read(uintptr_t address, void *output, size_t size) {
    if (!output || !be_memory_span(address, size, VM_PROT_READ)) return 0;
    mach_vm_size_t got = 0;
    return mach_vm_read_overwrite(mach_task_self(), address, size,
        (mach_vm_address_t)(uintptr_t)output, &got) == KERN_SUCCESS && got == size;
}
static int be_write(uintptr_t address, const void *value, size_t size) {
    if (!value || size > 16 || !be_memory_span(address, size, VM_PROT_READ | VM_PROT_WRITE)) return 0;
    if (mach_vm_write(mach_task_self(), address, (vm_offset_t)(uintptr_t)value,
                      (mach_msg_type_number_t)size) != KERN_SUCCESS) return 0;
    unsigned char check[16];
    return be_read(address, check, size) && !memcmp(check, value, size);
}
static uintptr_t be_pointer(uintptr_t address) {
    uintptr_t value = 0;
    if (!be_read(address, &value, sizeof value) || (value & 7u) ||
        !be_memory_span(value, sizeof(uint64_t), VM_PROT_READ)) return 0;
    return value;
}
static uintptr_t be_chain(uintptr_t base, uintptr_t root, uintptr_t middle, uintptr_t leaf) {
    if (!base || base > UINTPTR_MAX - root) return 0;
    uintptr_t a = be_pointer(base + root);
    if (!a || a > UINTPTR_MAX - middle) return 0;
    uintptr_t b = be_pointer(a + middle);
    return b && b <= UINTPTR_MAX - leaf ? b + leaf : 0;
}
/* The title screen exposes plausible default health/stamina values. They are
   not a playable actor. Require the same real Player in PlayerInfo and the
   health chain, with its verified NX160 vtable and life-accessor slots. World
   coordinates are deliberately not a readiness test: shrine origins may be zero. */
static uintptr_t be_live_player_actor(uintptr_t base) {
    if (!base || base > UINTPTR_MAX - 0x2ca1a80) return 0;
    uintptr_t info = be_pointer(base + 0x2ca1140);
    if (!info || info > UINTPTR_MAX - 0x60) return 0;
    uintptr_t actor = be_pointer(info + 0x60);
    if (!actor || actor > UINTPTR_MAX - 0x186c ||
        be_pointer(info) != base + 0x2b7be80 || be_pointer(actor) != base + 0x2b7bea8 ||
        be_pointer(base + 0x2b7bea8 + 0xf0) != base + 0xf33680 ||
        be_pointer(base + 0x2b7bea8 + 0x2b8) != base + 0xd50 ||
        be_chain(base, 0x2ca1a78, 0x80, 0x848) != actor + 0x848) return 0;
    int32_t maximum_life = 0;
    return be_read(actor + 0x1868, &maximum_life, sizeof maximum_life) &&
        maximum_life >= 4 && maximum_life <= 120 &&
        be_memory_span(actor + 0x848, 4, VM_PROT_READ | VM_PROT_WRITE) ? actor : 0;
}
static BotwEditorSnapshot be_read_snapshot(void) {
    BotwEditorSnapshot result = {0};
#if defined(GAME_BOTW160) && !TARGET_OS_IPHONE
    result.main_base = (uintptr_t)runtime_module_base("main");
    if (!result.main_base) return result;
    uintptr_t actor = be_live_player_actor(result.main_base);
    if (!actor) return result;
    result.health_address = actor + 0x848;
    result.stamina_address = be_chain(result.main_base, 0x2c9fd70, 0x38, 0x2ac);
    if (result.stamina_address) result.max_stamina_address = result.stamina_address + 4;
    result.health_valid = be_read(result.health_address, &result.health, sizeof result.health) &&
                          result.health > 0 && result.health <= 120;
    result.stamina_valid = be_read(result.stamina_address, &result.stamina, sizeof result.stamina) &&
        be_read(result.max_stamina_address, &result.max_stamina, sizeof result.max_stamina) &&
        isfinite(result.stamina) && isfinite(result.max_stamina) &&
        result.max_stamina >= 100 && result.max_stamina <= 3000 &&
        result.stamina >= 0 && result.stamina <= result.max_stamina * 1.5f;
    if (be_extensions.features) result.extra_features = be_extensions.features(result.main_base);
#endif
    return result;
}
static int be_ready(const BotwEditorSnapshot *snapshot) {
    return snapshot->health_valid && snapshot->stamina_valid;
}
static void be_json_string(FILE *output, const char *text) {
    fputc('"', output);
    for (const unsigned char *p = (const unsigned char *)(text ? text : ""); *p; ++p) {
        if (*p == '"' || *p == '\\') { fputc('\\', output); fputc(*p, output); }
        else if (*p < 0x20) fprintf(output, "\\u%04x", *p);
        else fputc(*p, output);
    }
    fputc('"', output);
}
static void be_find_state_path(void) {
    if (be_state_path[0]) return;
    const char *input = getenv("TOTK_INPUT_FILE"), *controls = getenv("TOTK_CONTROLS");
    const char *source = controls && strchr(controls, '/') ? controls : input;
    const char *slash = source ? strrchr(source, '/') : NULL;
    if (!slash || slash - source > PATH_MAX - 64) return;
    snprintf(be_state_path, sizeof be_state_path, "%.*s/trainer-state.json", (int)(slash - source), source);
}
static void be_publish(const BotwEditorSnapshot *snapshot) {
    be_find_state_path();
    if (!be_state_path[0]) return;
    char temporary[PATH_MAX];
    if (snprintf(temporary, sizeof temporary, "%s.%d.tmp", be_state_path, getpid()) >= (int)sizeof temporary) return;
    FILE *output = fopen(temporary, "w");
    if (!output) return;
    pthread_mutex_lock(&be_lock);
    uint64_t request = be_last_request;
    int ok = be_last_ok;
    char message[sizeof be_last_result];
    memcpy(message, be_last_result, sizeof message);
    pthread_mutex_unlock(&be_lock);
    int ready = be_ready(snapshot);
    fprintf(output, "{\"schema\":1,\"pid\":%d,\"version\":\"%s\",\"ready\":%s,\"message\":", getpid(),
#if defined(GAME_BOTW160)
            "1.6.0",
#else
            "unsupported",
#endif
            ready ? "true" : "false");
    be_json_string(output, ready ? "Connected to the gameplay session" : "Waiting for an active gameplay session");
    fputs(",\"health\":", output);
    if (snapshot->health_valid) fprintf(output, "%u", snapshot->health); else fputs("null", output);
    fputs(",\"stamina\":", output);
    if (snapshot->stamina_valid) fprintf(output, "%.3f", (double)snapshot->stamina); else fputs("null", output);
    fprintf(output, ",\"health_lock\":%s,\"stamina_lock\":%s,\"last_request\":%" PRIu64 ",\"last_ok\":%s,\"last_result\":",
        be_health_lock ? "true" : "false", be_stamina_lock ? "true" : "false", request, ok ? "true" : "false");
    be_json_string(output, message);
    unsigned features = ready ? snapshot->extra_features : 0;
    fprintf(output, ",\"features\":{\"health\":%s,\"stamina\":%s,\"rupees\":%s,\"items\":%s,\"inventory_edit\":%s,\"teleport\":%s,\"shrines\":%s,\"map\":%s,\"runes\":%s,\"lift\":%s,\"master_sword\":%s,\"pouch_slots\":%s,\"map_access\":%s,\"shrine_completion\":%s,\"horse_speed\":%s}",
        ready ? "true" : "false", ready ? "true" : "false",
        features & BE_FEATURE_RUPEES ? "true" : "false", features & BE_FEATURE_ITEMS ? "true" : "false",
        features & BE_FEATURE_INVENTORY ? "true" : "false", features & BE_FEATURE_TELEPORT ? "true" : "false",
        features & BE_FEATURE_SHRINES ? "true" : "false", features & BE_FEATURE_MAP ? "true" : "false",
        features & BE_FEATURE_RUNES ? "true" : "false", features & BE_FEATURE_LIFT ? "true" : "false",
        features & BE_FEATURE_MASTER_SWORD ? "true" : "false", features & BE_FEATURE_POUCH_SLOTS ? "true" : "false",
        features & BE_FEATURE_MAP_ACCESS ? "true" : "false",
        features & BE_FEATURE_SHRINE_COMPLETION ? "true" : "false",
        features & BE_FEATURE_HORSE_SPEED ? "true" : "false");
    if (be_extensions.snapshot && snapshot->main_base) be_extensions.snapshot(output, snapshot->main_base, ready);
    fputs("}\n", output);
    int failed = ferror(output);
    if (fclose(output)) failed = 1;
    if (!failed) { if (rename(temporary, be_state_path)) unlink(temporary); }
    else unlink(temporary);
}
static int be_identifier(const char *text) {
    size_t length = strlen(text);
    if (!length || length >= 96) return 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) return 0;
    return 1;
}
static int be_integer(const char *text, int minimum, int maximum, int *value) {
    if (!text || !*text || *text == '+' || *text == '-') return 0;
    for (const char *p = text; *p; ++p) if (*p < '0' || *p > '9') return 0;
    errno = 0;
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (errno || !end || *end || parsed < minimum || parsed > maximum) return 0;
    *value = (int)parsed;
    return 1;
}
static int be_parse(const char *line, BotwEditorRequest *request, char *error, size_t capacity) {
    char buffer[256];
    if (strlen(line) >= sizeof buffer) { snprintf(error, capacity, "Command is too long"); return 0; }
    snprintf(buffer, sizeof buffer, "%s", line);
    char *context = NULL, *token[6] = {0};
    unsigned count = 0;
    for (char *p = strtok_r(buffer, " \t\r\n", &context); p; p = strtok_r(NULL, " \t\r\n", &context)) {
        if (count == 6) { snprintf(error, capacity, "Too many command arguments"); return 0; }
        token[count++] = p;
    }
    if (count < 3 || strcmp(token[0], "trainer")) { snprintf(error, capacity, "Incomplete editor command"); return 0; }
    for (const char *p = token[1]; *p; ++p) if (*p < '0' || *p > '9') { snprintf(error, capacity, "Invalid request identifier"); return 0; }
    errno = 0;
    char *end = NULL;
    unsigned long long id = strtoull(token[1], &end, 10);
    if (errno || !end || *end || !id) { snprintf(error, capacity, "Invalid request identifier"); return 0; }
    request->id = (uint64_t)id;
    if (strlen(token[2]) >= sizeof request->verb) { snprintf(error, capacity, "Unknown editor command"); return 0; }
    strcpy(request->verb, token[2]);
    const char *verb = request->verb;
    if (!strcmp(verb, "status") || !strcmp(verb, "unlock_all_shrines") || !strcmp(verb, "reveal_map") ||
        !strcmp(verb, "runes") || !strcmp(verb, "paraglider") || !strcmp(verb, "full_access") || !strcmp(verb, "map_access") || !strcmp(verb, "complete_all_shrines")) {
        if (count == 3) return 1;
    } else if (!strcmp(verb, "health") || !strcmp(verb, "stamina")) {
        if (count == 4 && (!strcmp(token[3], "on") || !strcmp(token[3], "off"))) { strcpy(request->argument, token[3]); return 1; }
    } else if (!strcmp(verb, "rupees")) {
        if (count == 4 && be_integer(token[3], 0, 999999, &request->value)) return 1;
    } else if (!strcmp(verb, "max_hearts")) {
        if (count == 4 && be_integer(token[3], 1, 30, &request->value)) return 1;
    } else if (!strcmp(verb, "max_stamina")) {
        if (count == 4 && be_integer(token[3], 1, 3, &request->value)) return 1;
    } else if (!strcmp(verb, "master_sword")) {
        if (count == 4 && be_integer(token[3], 0, 3, &request->value)) return 1;
    } else if (!strcmp(verb, "pouch_slots")) {
        int minimum = 0, maximum = 0;
        if (count == 5) {
            if (!strcmp(token[3], "weapons")) { minimum = BE_WEAPON_SLOTS_MIN; maximum = BE_WEAPON_SLOTS_MAX; }
            else if (!strcmp(token[3], "bows")) { minimum = BE_BOW_SLOTS_MIN; maximum = BE_BOW_SLOTS_MAX; }
            else if (!strcmp(token[3], "shields")) { minimum = BE_SHIELD_SLOTS_MIN; maximum = BE_SHIELD_SLOTS_MAX; }
            if (maximum && be_integer(token[4], minimum, maximum, &request->value)) { strcpy(request->argument, token[3]); return 1; }
        }
    } else if (!strcmp(verb, "horse_speed")) {
        if (count == 4 && !strcmp(token[3], "off")) { strcpy(request->argument, "off"); return 1; }
        if (count == 4 && be_integer(token[3], 100, 200, &request->value)) return 1;
    } else if (!strcmp(verb, "give")) {
        if (count == 5 && be_identifier(token[3]) && be_integer(token[4], 1, BE_MAX_STACK_QUANTITY, &request->value)) { strcpy(request->argument, token[3]); return 1; }
    } else if (!strcmp(verb, "quantity")) {
        int slot;
        /* The native inventory entry supplies the final type-specific bound. */
        if (count == 5 && be_integer(token[3], 0, 419, &slot) && be_integer(token[4], 1, BE_MAX_RAW_DURABILITY, &request->value)) { strcpy(request->argument, token[3]); return 1; }
    } else if (!strcmp(verb, "remove")) {
        int slot;
        if (count == 4 && be_integer(token[3], 0, 419, &slot)) { strcpy(request->argument, token[3]); return 1; }
    } else if (!strcmp(verb, "teleport") || !strcmp(verb, "unlock_shrine") || !strcmp(verb, "complete_shrine")) {
        if (count == 4 && be_identifier(token[3])) { strcpy(request->argument, token[3]); return 1; }
    } else if (!strcmp(verb, "lift")) {
        if (count == 4 && be_integer(token[3], 1, 50, &request->value)) return 1;
    }
    snprintf(error, capacity, "Unknown command or invalid value");
    return 0;
}

void runtime_botw_editor_extensions(BotwEditorExtensions extensions) {
    /* Register once, before the first frame. */
    be_extensions = extensions;
}
int runtime_botw_editor_command(const char *line) {
    if (!line || strncmp(line, "trainer ", 8)) return 0;
    BotwEditorRequest request = {0};
    char error[256];
    int parsed = be_parse(line, &request, error, sizeof error);
    pthread_mutex_lock(&be_lock);
    if (!parsed || be_queue_count == BE_QUEUE_CAPACITY) {
        if (request.id) be_last_request = request.id;
        be_last_ok = 0;
        snprintf(be_last_result, sizeof be_last_result, "%s", parsed ? "Editor queue is busy; retry after the current request" : error);
    } else {
        unsigned index = (be_queue_head + be_queue_count) % BE_QUEUE_CAPACITY;
        be_queue[index] = request;
        ++be_queue_count;
    }
    pthread_mutex_unlock(&be_lock);
    return 1;
}
static void be_apply(const BotwEditorRequest *request, BotwEditorSnapshot *snapshot) {
    int ok = 0;
    char result[256];
    snprintf(result, sizeof result, "Waiting for an active gameplay session");
    if (!strcmp(request->verb, "status")) {
        ok = 1;
        snprintf(result, sizeof result, "%s", be_ready(snapshot) ? "Live values refreshed" : "Waiting for an active gameplay session");
    } else if (!strcmp(request->verb, "health") && !strcmp(request->argument, "off")) {
        be_health_lock = 0; be_health_target = 0; ok = 1;
        snprintf(result, sizeof result, "Health lock disabled");
    } else if (!strcmp(request->verb, "stamina") && !strcmp(request->argument, "off")) {
        be_stamina_lock = 0; ok = 1;
        snprintf(result, sizeof result, "Stamina lock disabled");
    } else if (be_ready(snapshot)) {
        if (!strcmp(request->verb, "health")) {
            be_health_target = snapshot->health;
            ok = be_write(snapshot->health_address, &be_health_target, sizeof be_health_target);
            be_health_lock = ok;
            snprintf(result, sizeof result, "%s", ok ? "Health locked at its current value" : "Health changed or became unavailable; no lock applied");
        } else if (!strcmp(request->verb, "stamina")) {
            ok = be_write(snapshot->stamina_address, &snapshot->max_stamina, sizeof snapshot->max_stamina);
            be_stamina_lock = ok;
            snprintf(result, sizeof result, "%s", ok ? "Stamina refilled and locked" : "Stamina changed or became unavailable; no lock applied");
        } else if (be_extensions.command) {
            ok = be_extensions.command(snapshot->main_base, request->verb, request->argument, request->value, result, sizeof result);
            if (ok && !strcmp(request->verb, "max_hearts")) {
                be_health_lock = 0;
                be_health_target = 0;
            }
        } else snprintf(result, sizeof result, "This edit is not supported by the connected runtime");
    }
    pthread_mutex_lock(&be_lock);
    be_last_request = request->id;
    be_last_ok = ok;
    snprintf(be_last_result, sizeof be_last_result, "%s", result);
    pthread_mutex_unlock(&be_lock);
}
void runtime_botw_editor_tick(uint64_t now_ns) {
    if (!now_ns || (be_last_tick && now_ns >= be_last_tick && now_ns - be_last_tick < 100000000ULL)) return;
    be_last_tick = now_ns;
    BotwEditorSnapshot snapshot = be_read_snapshot();
    if (!be_ready(&snapshot)) { be_health_lock = 0; be_stamina_lock = 0; be_health_target = 0; }
    else {
        if (be_health_lock && snapshot.health < be_health_target && !be_write(snapshot.health_address, &be_health_target, sizeof be_health_target)) be_health_lock = 0;
        if (be_stamina_lock && snapshot.stamina < snapshot.max_stamina && !be_write(snapshot.stamina_address, &snapshot.max_stamina, sizeof snapshot.max_stamina)) be_stamina_lock = 0;
    }
    int processed = 0;
    BotwEditorRequest request;
    pthread_mutex_lock(&be_lock);
    if (be_queue_count) {
        request = be_queue[be_queue_head];
        be_queue_head = (be_queue_head + 1) % BE_QUEUE_CAPACITY;
        --be_queue_count;
        processed = 1;
    }
    pthread_mutex_unlock(&be_lock);
    /* One native mutation per tick lets the game settle between edits. */
    if (processed) { be_apply(&request, &snapshot); snapshot = be_read_snapshot(); }
    if (processed || !be_last_publish || now_ns < be_last_publish || now_ns - be_last_publish >= 1000000000ULL) {
        be_publish(&snapshot);
        be_last_publish = now_ns;
    }
}

#endif
