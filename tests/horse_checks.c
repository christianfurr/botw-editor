/* Synthetic table and fault-injected transactions only. No game/resource bytes,
   live process, mounted actor, profile, save, or private assets are accessed. */
#define BH_PURE_CHECKS 1
#include "runtime/botw_editor_horse.h"
#include <stdlib.h>

static unsigned checks;
static void require(int condition, const char *description) {
    ++checks;
    if (!condition) { fprintf(stderr, "FAIL: %s\n", description); exit(1); }
}
static void put_u32(unsigned char *bytes, uint32_t value) {
    bytes[0] = (unsigned char)value; bytes[1] = (unsigned char)(value >> 8);
    bytes[2] = (unsigned char)(value >> 16); bytes[3] = (unsigned char)(value >> 24);
}
static void put_float(unsigned char *bytes, float value) {
    uint32_t bits; memcpy(&bits, &value, 4); put_u32(bytes, bits);
}
static int compare_key(const void *a, const void *b) {
    uint32_t x, y; memcpy(&x, a, 4); memcpy(&y, b, 4); return x < y ? -1 : x > y;
}
typedef struct {
    unsigned char data[BH_BUFFER_SIZE], replacement[BH_BUFFER_SIZE], original[BH_BUFFER_SIZE];
    BhResource identity;
    int writes, resolves, reads, writable_checks;
    int read_only_cell, fail_write, fail_read_after_write, change_at_write;
    int corrupt_at_write, fail_rollback, unresolved, switched;
    uintptr_t written[400];
} Fixture;

static BhFingerprint fingerprint_of(const unsigned char *bytes) {
    uint64_t keys = UINT64_C(0xcbf29ce484222325);
    for (unsigned i = 0; i < BH_ENTRY_COUNT; ++i) keys = bh_hash_add(keys, bytes + 4 + i * BH_ENTRY_SIZE, 4);
    return (BhFingerprint){bh_hash(bytes, BH_BUFFER_SIZE), keys};
}
static void fixture_init(Fixture *fixture) {
    memset(fixture, 0, sizeof *fixture);
    fixture->read_only_cell = -1;
    uint32_t keys[BH_ENTRY_COUNT];
    unsigned count = 0;
    keys[count++] = bh_crc("GameRomHorse");
    keys[count++] = bh_crc("GameRomHorseEpona");
    keys[count++] = bh_crc("GameRomHorseZelda");
    for (int i = 1; i <= 23; ++i) { char name[24]; snprintf(name, sizeof name, "GameRomHorse%02d", i); keys[count++] = bh_crc(name); }
    const char *excluded[] = {"GameRomHorse00S", "GameRomHorse00L", "GameRomHorseNushi", "GameRomHorseBone", "Animal_Boar_A", "Animal_Sunazarashi_A"};
    for (unsigned i = 0; i < sizeof excluded / sizeof *excluded; ++i) keys[count++] = bh_crc(excluded[i]);
    while (count < BH_ENTRY_COUNT) { keys[count] = UINT32_C(0x30000000) + count; ++count; }
    qsort(keys, BH_ENTRY_COUNT, sizeof *keys, compare_key);
    put_u32(fixture->data, 24);
    for (unsigned i = 0; i < BH_ENTRY_COUNT; ++i) {
        size_t offset = 4 + i * BH_ENTRY_SIZE;
        put_u32(fixture->data + offset, keys[i]);
        for (unsigned j = 0; j < BH_FLOAT_COUNT; ++j) put_float(fixture->data + offset + 4 + 4 * j, (float)(j + 1) / 4.0f);
        if (bh_standard_horse(keys[i])) for (unsigned gear = 0; gear < 4; ++gear) put_float(fixture->data + offset + 4 + gear * 8, 2.0f + gear * 5.0f);
    }
    memcpy(fixture->replacement, fixture->data, BH_BUFFER_SIZE);
    memcpy(fixture->original, fixture->data, BH_BUFFER_SIZE);
    fixture->identity = (BhResource){0x1000, 0x2000, 0x3000, (uintptr_t)fixture->data, BH_BUFFER_SIZE};
}
static int fixture_resolve(void *context, BhResource *resource) {
    Fixture *fixture = context; ++fixture->resolves;
    if (fixture->unresolved) return 0;
    *resource = fixture->identity;
    if (fixture->switched) { resource->unit += 8; resource->resource += 8; resource->data = (uintptr_t)fixture->replacement; }
    return 1;
}
static int inside(uintptr_t address, size_t size, const unsigned char *bytes) {
    uintptr_t base = (uintptr_t)bytes;
    return address >= base && size <= BH_BUFFER_SIZE && address - base <= BH_BUFFER_SIZE - size;
}
static int fixture_read(void *context, uintptr_t address, void *output, size_t size) {
    Fixture *fixture = context; ++fixture->reads;
    if (fixture->fail_read_after_write && fixture->writes == fixture->fail_read_after_write) {
        fixture->fail_read_after_write = 0; return 0;
    }
    if (!inside(address, size, fixture->data) && !inside(address, size, fixture->replacement)) return 0;
    memcpy(output, (const void *)address, size); return 1;
}
static int fixture_writable(void *context, uintptr_t address, size_t size) {
    Fixture *fixture = context; ++fixture->writable_checks;
    if (size != 4 || (!inside(address, size, fixture->data) && !inside(address, size, fixture->replacement))) return 0;
    if (fixture->read_only_cell >= 0 && address == (uintptr_t)fixture->data + (unsigned)fixture->read_only_cell) return 0;
    return 1;
}
static int fixture_write(void *context, uintptr_t address, const void *value, size_t size) {
    Fixture *fixture = context;
    if (!fixture_writable(context, address, size)) return 0;
    ++fixture->writes;
    require(fixture->writes < (int)(sizeof fixture->written / sizeof *fixture->written), "bounded fixture write log");
    fixture->written[fixture->writes - 1] = address;
    if (fixture->fail_rollback && fixture->writes > fixture->fail_write) return 0;
    memcpy((void *)address, value, size);
    if (fixture->corrupt_at_write == fixture->writes) put_float((unsigned char *)address, 49.0f);
    if (fixture->change_at_write == fixture->writes) fixture->switched = 1;
    return fixture->fail_write != fixture->writes;
}
static BhAccess fixture_access(Fixture *fixture) {
    return (BhAccess){fixture, fixture_resolve, fixture_read, fixture_writable, fixture_write};
}
static void inspect(Fixture *fixture, BhSession *session, BhFingerprint fingerprint, int expected) {
    BhAccess access = fixture_access(fixture); BhResource resource; const char *reason = NULL;
    unsigned char bytes[BH_BUFFER_SIZE];
    require(bh_inspect(session, &access, fingerprint, &resource, bytes, &reason) == expected, "inspect expected availability");
    require((reason == NULL) == expected, "availability has actionable reason only when blocked");
}

static void check_scope_and_layout(void) {
    Fixture fixture; fixture_init(&fixture); BhFingerprint fingerprint = fingerprint_of(fixture.data);
    uint16_t cells[BH_CELL_COUNT];
    require(bh_crc("123456789") == UINT32_C(0xcbf43926), "standard CRC32 test vector");
    require(bh_layout(fixture.data, sizeof fixture.data, fingerprint.keys, cells), "accept 81 sorted synthetic entries and 26 horse keys");
    require(!bh_standard_horse(bh_crc("GameRomHorse00S")) && !bh_standard_horse(bh_crc("GameRomHorse00L")), "exclude small and giant horses");
    require(!bh_standard_horse(bh_crc("GameRomHorseNushi")) && !bh_standard_horse(bh_crc("GameRomHorseBone")), "exclude spirit and stalhorse");
    require(!bh_standard_horse(bh_crc("Animal_Boar_A")) && !bh_standard_horse(bh_crc("Animal_Sunazarashi_A")), "exclude unrelated animals");
    require(!bh_standard_horse(bh_crc("GameRomHorse24")), "do not accept a wider actor-name pattern");
    require(!bh_layout(NULL, BH_BUFFER_SIZE, fingerprint.keys, cells), "reject null table");
    require(!bh_layout(fixture.data, sizeof fixture.data - 1, fingerprint.keys, cells), "reject truncated table");
    require(!bh_layout(fixture.data, sizeof fixture.data, fingerprint.keys ^ 1, cells), "verify the complete ordered key fingerprint");
    put_u32(fixture.data, 25); require(!bh_layout(fixture.data, sizeof fixture.data, fingerprint.keys, cells), "reject wrong header");
    memcpy(fixture.data, fixture.original, sizeof fixture.data);
    memcpy(fixture.data + 4 + BH_ENTRY_SIZE, fixture.data + 4, 4);
    require(!bh_layout(fixture.data, sizeof fixture.data, fingerprint.keys, cells), "reject duplicate or unsorted keys");
    memcpy(fixture.data, fixture.original, sizeof fixture.data); put_float(fixture.data + 8, NAN);
    require(!bh_layout(fixture.data, sizeof fixture.data, fingerprint.keys, cells), "reject non-finite values anywhere in table");
    memcpy(fixture.data, fixture.original, sizeof fixture.data); put_float(fixture.data + 8, -1);
    require(!bh_layout(fixture.data, sizeof fixture.data, fingerprint.keys, cells), "reject negative values");
    memcpy(fixture.data, fixture.original, sizeof fixture.data); put_float(fixture.data + 8, 51);
    require(!bh_layout(fixture.data, sizeof fixture.data, fingerprint.keys, cells), "reject unsupported numeric bounds");
    memcpy(fixture.data, fixture.original, sizeof fixture.data);
    require(bh_layout(fixture.data, sizeof fixture.data, fingerprint.keys, cells), "restore valid layout");
    for (unsigned i = 0; i < BH_CELL_COUNT; ++i) {
        size_t entry = (cells[i] - 8) / BH_ENTRY_SIZE;
        size_t within = cells[i] - (4 + entry * BH_ENTRY_SIZE);
        require(bh_standard_horse(bh_u32(fixture.data + 4 + entry * BH_ENTRY_SIZE)), "every selected cell belongs to a standard horse");
        require(within == 4 || within == 12 || within == 20 || within == 28, "only the four movement values are selected");
    }
}
static void check_apply_and_off(void) {
    Fixture fixture; fixture_init(&fixture); BhSession session = {0}; BhAccess access = fixture_access(&fixture);
    BhFingerprint fingerprint = fingerprint_of(fixture.data); const char *reason = NULL;
    inspect(&fixture, &session, fingerprint, 1);
    require(!session.captured && fixture.writes == 0, "availability does not capture a baseline or write");
    require(!bh_apply(&session, &access, fingerprint, 99, &reason) && !session.captured, "reject below minimum before capture");
    require(!bh_apply(&session, &access, fingerprint, 201, &reason) && fixture.writes == 0, "reject above maximum without writes");
    require(bh_apply(&session, &access, fingerprint, 150, &reason), "apply 1.50 times standard horse speed");
    require(session.captured && session.percent == 150 && !session.poisoned && reason == NULL, "publish only acknowledged owned configuration");
    require(fixture.writes == BH_CELL_COUNT, "apply precisely 104 movement cells");
    unsigned changed = 0;
    for (size_t offset = 0; offset < BH_BUFFER_SIZE; offset += 4) {
        int selected = 0;
        for (unsigned i = 0; i < BH_CELL_COUNT; ++i) selected |= session.cells[i] == offset;
        if (selected) { ++changed; require(bh_float(fixture.data + offset) == bh_float(fixture.original + offset) * 1.5f, "each selected speed scales from original"); }
        else require(!memcmp(fixture.data + offset, fixture.original + offset, 4), "excluded animals and every neighboring field stay byte exact");
    }
    require(changed == BH_CELL_COUNT, "all and only selected cells changed");
    int previous_writes = fixture.writes;
    require(bh_apply(&session, &access, fingerprint, 150, &reason) && fixture.writes == previous_writes, "repeated selection verifies without compounding or writing");
    require(bh_apply(&session, &access, fingerprint, 200, &reason), "allow bounded maximum");
    for (unsigned i = 0; i < BH_CELL_COUNT; ++i) require(bh_float(fixture.data + session.cells[i]) == bh_float(fixture.original + session.cells[i]) * 2, "maximum derives from immutable baseline");
    require(bh_apply(&session, &access, fingerprint, 100, &reason), "Off restores original speeds");
    require(!memcmp(fixture.data, fixture.original, sizeof fixture.data) && session.percent == 100, "Off restores complete table byte exact");
}
static void check_guards_and_ownership(void) {
    Fixture fixture; BhSession session = {0}; fixture_init(&fixture); BhAccess access = fixture_access(&fixture);
    BhFingerprint fingerprint = fingerprint_of(fixture.data); const char *reason = NULL;
    fixture.unresolved = 1; inspect(&fixture, &session, fingerprint, 0); require(fixture.writes == 0, "unloaded resource never writes");
    fixture.unresolved = 0; fixture.identity.size = BH_BUFFER_SIZE - 1; inspect(&fixture, &session, fingerprint, 0);
    fixture.identity.size = BH_BUFFER_SIZE; fixture.identity.data += 1; inspect(&fixture, &session, fingerprint, 0);
    fixture.identity.data = UINTPTR_MAX - 3; inspect(&fixture, &session, fingerprint, 0);
    fixture.identity.data = (uintptr_t)fixture.data;
    put_float(fixture.data + 8, 1); inspect(&fixture, &session, fingerprint, 0); require(fixture.writes == 0, "reject a modified original baseline");
    memcpy(fixture.data, fixture.original, sizeof fixture.data);
    uint16_t cells[BH_CELL_COUNT]; require(bh_layout(fixture.data, sizeof fixture.data, fingerprint.keys, cells), "layout supplies guard fixture cells");
    fixture.read_only_cell = cells[BH_CELL_COUNT - 1];
    require(!bh_apply(&session, &access, fingerprint, 150, &reason) && fixture.writes == 0, "preflight the final cell before writing the first");
    fixture.read_only_cell = -1;
    require(bh_apply(&session, &access, fingerprint, 150, &reason), "establish ownership");
    int writes = fixture.writes;
    fixture.data[200] ^= 1;
    require(!bh_apply(&session, &access, fingerprint, 100, &reason) && fixture.writes == writes, "Off refuses a table changed by another owner");
    fixture.data[200] ^= 1;
    require(bh_apply(&session, &access, fingerprint, 100, &reason), "Off can restore an intact owned table");
    fixture.switched = 1;
    require(bh_apply(&session, &access, fingerprint, 125, &reason), "new resource gets a fresh verified baseline");
    require(session.identity.data == (uintptr_t)fixture.replacement, "session tracks replacement identity");
    require(!memcmp(fixture.data, fixture.original, sizeof fixture.data), "resource replacement never touches old memory");
}
static void check_failure_transactions(void) {
    Fixture fixture; BhSession session; const char *reason = NULL;
    for (int fail_at = 1; fail_at <= BH_CELL_COUNT; ++fail_at) {
        fixture_init(&fixture); memset(&session, 0, sizeof session); fixture.fail_write = fail_at;
        BhAccess access = fixture_access(&fixture); BhFingerprint fingerprint = fingerprint_of(fixture.data);
        require(!bh_apply(&session, &access, fingerprint, 150, &reason), "failure at every transaction cell is reported");
        require(!memcmp(fixture.data, fixture.original, sizeof fixture.data) && !session.poisoned && session.percent == 100, "failure-after-write restores original ownership and all cells");
    }
    fixture_init(&fixture); memset(&session, 0, sizeof session); fixture.fail_read_after_write = 4;
    BhAccess access = fixture_access(&fixture); BhFingerprint fingerprint = fingerprint_of(fixture.data);
    require(!bh_apply(&session, &access, fingerprint, 150, &reason), "failed readback rejects success");
    require(!memcmp(fixture.data, fixture.original, sizeof fixture.data) && !session.poisoned, "failed readback rolls back touched cells");
    fixture_init(&fixture); memset(&session, 0, sizeof session); fixture.fail_write = 3; fixture.fail_rollback = 1;
    access = fixture_access(&fixture); fingerprint = fingerprint_of(fixture.data);
    require(!bh_apply(&session, &access, fingerprint, 150, &reason) && session.poisoned, "failed rollback poisons ownership instead of claiming restoration");
    int writes = fixture.writes;
    require(!bh_apply(&session, &access, fingerprint, 100, &reason) && fixture.writes == writes, "poisoned session does not attempt blind Off writes");
    fixture_init(&fixture); memset(&session, 0, sizeof session); fixture.corrupt_at_write = 2;
    access = fixture_access(&fixture); fingerprint = fingerprint_of(fixture.data);
    require(!bh_apply(&session, &access, fingerprint, 150, &reason) && session.poisoned, "unexpected readback cannot be treated as our owned value");
    require(bh_float(fixture.data + session.cells[1]) == 49, "rollback preserves an unexplained competing value");
    fixture_init(&fixture); memset(&session, 0, sizeof session); fixture.change_at_write = 3;
    access = fixture_access(&fixture); fingerprint = fingerprint_of(fixture.data);
    require(!bh_apply(&session, &access, fingerprint, 150, &reason) && session.poisoned, "resource identity change interrupts transaction");
    require(fixture.writes == 3 && !memcmp(fixture.replacement, fixture.original, BH_BUFFER_SIZE), "identity loss never restores stale pointers or writes replacement data");
    fixture.change_at_write = 0;
    inspect(&fixture, &session, fingerprint, 1);
    require(!session.captured && !session.poisoned, "fresh canonical replacement discards stale session ownership");
}
int main(void) {
    check_scope_and_layout(); check_apply_and_off(); check_guards_and_ownership(); check_failure_transactions();
    printf("%u horse table/ownership/transaction checks passed (synthetic; no gameplay).\n", checks);
    return 0;
}
