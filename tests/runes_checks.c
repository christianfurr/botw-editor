/* Synthetic rune-manager transactions only. No game executable, live process,
   resource, save, profile or input channel is accessed. A successful request is
   deliberately distinct from the game applying or visibly using the rune. */
#define BR_RUNE_PURE_CHECKS 1
#include "runtime/botw_editor_runes.h"
#include <stdlib.h>

static unsigned checks;
static void require(int condition, const char *description) {
    ++checks;
    if (!condition) {
        fprintf(stderr, "FAIL rune check %u: %s\n", checks, description);
        exit(1);
    }
}

typedef struct {
    BrRuneValues value;
    int busy, locks, unlocks, reads, writes;
    int fail_write, silent, corrupt, change_applied, fail_reads;
} Fixture;

static int fixture_lock(void *context, uintptr_t *identity, BrRuneValues *value) {
    Fixture *fixture = context;
    ++fixture->locks;
    if (fixture->busy) return 0;
    *identity = 42;
    *value = fixture->value;
    return 1;
}
static int fixture_read(void *context, uintptr_t identity, BrRuneValues *value) {
    Fixture *fixture = context;
    require(identity == 42, "read retains the locked manager identity");
    ++fixture->reads;
    if (fixture->fail_reads) { --fixture->fail_reads; return 0; }
    *value = fixture->value;
    return 1;
}
static int fixture_write(void *context, uintptr_t identity, int32_t value) {
    Fixture *fixture = context;
    require(identity == 42, "write retains the locked manager identity");
    ++fixture->writes;
    if (!fixture->silent) fixture->value.requested = value;
    if (fixture->corrupt == fixture->writes) fixture->value.requested = 4;
    if (fixture->change_applied == fixture->writes) fixture->value.applied = 3;
    return fixture->writes != fixture->fail_write;
}
static void fixture_unlock(void *context, uintptr_t identity) {
    Fixture *fixture = context;
    require(identity == 42, "unlock retains the locked manager identity");
    ++fixture->unlocks;
}
static int apply(Fixture *fixture, int *changed, const char **reason) {
    BrRuneAccess access = {fixture, fixture_lock, fixture_read, fixture_write, fixture_unlock};
    BrRuneValues after = {0};
    return br_rune_select_if_missing(&access, changed, &after, reason);
}

static void check_initial_request_and_existing_selection(void) {
    Fixture fixture = {.value = {-1, -1, -1}};
    int changed;
    const char *reason;
    require(apply(&fixture, &changed, &reason), "request a round bomb when no rune is selected");
    require(changed == 1 && reason == NULL, "successful write reports a selection request");
    require(fixture.value.requested == 0 && fixture.value.applied == -1 && fixture.value.previous == -1,
            "only the requested ID changes; application remains pending");
    require(fixture.writes == 1 && fixture.unlocks == 1, "one request and a balanced unlock");
    require(apply(&fixture, &changed, &reason), "accept an already pending request");
    require(!changed && fixture.writes == 1 && fixture.unlocks == 2, "do not duplicate a pending request");

    for (int rune = 0; rune < 8; ++rune) {
        fixture = (Fixture){.value = {rune, rune, -1}};
        require(apply(&fixture, &changed, &reason), "accept each valid existing rune selection");
        require(!changed && fixture.writes == 0 && fixture.value.requested == rune &&
                fixture.value.applied == rune && fixture.unlocks == 1,
                "preserve every existing selection without writing");
    }
    fixture = (Fixture){.value = {-1, 2, 3}};
    require(apply(&fixture, &changed, &reason) && fixture.writes == 0 && !changed,
            "an applied rune requires no replacement request");
}

static void check_invalid_or_busy_state(void) {
    int changed;
    const char *reason;
    for (int field = 0; field < 3; ++field) {
        for (int invalid = -2; invalid <= 8; invalid += 10) {
            Fixture fixture = {.value = {-1, -1, -1}};
            int32_t *fields[] = {&fixture.value.requested, &fixture.value.applied, &fixture.value.previous};
            *fields[field] = invalid;
            require(!apply(&fixture, &changed, &reason), "reject invalid values in each native field");
            require(!changed && reason && fixture.writes == 0 && fixture.unlocks == 1,
                    "invalid state reports a blocker and releases the lock without writing");
        }
    }
    Fixture fixture = {.value = {-1, -1, -1}, .busy = 1};
    require(!apply(&fixture, &changed, &reason) && !fixture.writes && !fixture.unlocks && reason,
            "busy manager does not write or unlock an unacquired lock");
}

static void check_failures_and_ownership(void) {
    int changed;
    const char *reason;
    Fixture fixture = {.value = {-1, -1, -1}, .fail_write = 1};
    require(!apply(&fixture, &changed, &reason), "a native failure after mutation is rejected");
    require(fixture.value.requested == -1 && fixture.writes == 2 && fixture.unlocks == 1 && strstr(reason, "restored"),
            "restore the original request after a failed write with proven ownership");

    fixture = (Fixture){.value = {-1, -1, -1}, .silent = 1};
    require(!apply(&fixture, &changed, &reason), "do not trust a write that silently did nothing");
    require(fixture.value.requested == -1 && fixture.writes == 1 && fixture.unlocks == 1,
            "unchanged original needs no restore write");

    fixture = (Fixture){.value = {-1, -1, -1}, .fail_reads = 1};
    require(!apply(&fixture, &changed, &reason), "failed first readback cannot establish success");
    require(fixture.value.requested == -1 && fixture.writes == 2 && fixture.unlocks == 1 && strstr(reason, "restored"),
            "a fresh locked read can prove ownership for restoration");

    fixture = (Fixture){.value = {-1, -1, -1}, .fail_reads = 2};
    require(!apply(&fixture, &changed, &reason), "two unavailable readbacks reject success");
    require(fixture.value.requested == 0 && fixture.writes == 1 && fixture.unlocks == 1 && strstr(reason, "changed"),
            "unreadable ownership must not trigger a blind restore");

    fixture = (Fixture){.value = {-1, -1, -1}, .corrupt = 1};
    require(!apply(&fixture, &changed, &reason), "a competing requested ID rejects success");
    require(fixture.value.requested == 4 && fixture.writes == 1 && fixture.unlocks == 1,
            "preserve another owner's replacement request");

    fixture = (Fixture){.value = {-1, -1, -1}, .change_applied = 1};
    require(!apply(&fixture, &changed, &reason), "an applied-ID change rejects transaction ownership");
    require(fixture.value.applied == 3 && fixture.writes == 1 && fixture.unlocks == 1,
            "do not overwrite a newly applied selection during restoration");
}

int main(void) {
    check_initial_request_and_existing_selection();
    check_invalid_or_busy_state();
    check_failures_and_ownership();
    printf("%u rune-selection checks passed (synthetic; no gameplay).\n", checks);
    return 0;
}
