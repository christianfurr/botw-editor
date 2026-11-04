#ifndef MNG_BOTW_EDITOR_TELEPORT_H
#define MNG_BOTW_EDITOR_TELEPORT_H

/* NX 1.6.0 native Resetter request, independently checked in main.elf.
   Original actions at 0x4456bc, 0x446220 and 0x446618 establish the ABI.
   The root task consumes the request at 0xe1a300: actor deletion, player warp,
   resource loading, reset completion and fade-in. Include after botw_editor.h;
   call on the guest presentation thread. Destinations are outdoor MainField
   coordinates; the StageInfo map guard rejects other scenes. */
#if defined(GAME_BOTW160) && !TARGET_OS_IPHONE
typedef struct { float x, y, z; } BetVector;
typedef struct { uintptr_t vtable; const char *text; } BetString;
typedef struct { uintptr_t resetter; uint32_t state; int blocked; } BetContext;

static int bet_word(uintptr_t main, uintptr_t offset, uint32_t expected) {
    uint32_t found = 0;
    return main && main <= UINTPTR_MAX - offset &&
        be_memory_span(main + offset, sizeof found, VM_PROT_READ | VM_PROT_EXECUTE) &&
        be_read(main + offset, &found, sizeof found) && found == expected;
}
static int bet_api(uintptr_t main) {
    if (!main || main > UINTPTR_MAX - 0x2ca00a8) return 0;
    uintptr_t terminate = be_pointer(main + 0x2a6e6c0 + 0x18);
    return terminate >= main && terminate < main + 0x2120000 &&
        bet_word(main, 0xe4e0e0, 0x6dbb23e9) &&
        bet_word(main, 0xe4e0e4, 0xf9000bf7) &&
        bet_word(main, 0xe4e0e8, 0xa90257f6) &&
        bet_word(main, 0xe4e0ec, 0xa9034ff4) &&
        bet_word(main, 0xe4e1b0, 0xad0a1e65) &&
        bet_word(main, 0xe4e1b4, 0x3d805a61) &&
        bet_word(main, 0xe4e258, 0xb9402268) &&
        bet_word(main, 0xe4e298, 0x29042260) &&
        bet_word(main, 0xe4e2a0, 0xb9017674) &&
        bet_word(main, 0x4456bc, 0x94282289) &&
        bet_word(main, 0x446220, 0x94281fb0) &&
        bet_word(main, 0x446618, 0x94281eb2);
}
static int bet_map(uintptr_t main, char name[32]) {
    uintptr_t vtable = 0, text = 0;
    int32_t capacity = 0;
    /* PlayReport::addMapType (0x122fca0) compares this StageInfo string with
       MainField, AocField, CDungeon and MainFieldDungeon. Its static initializer
       supplies the exact FixedSafeString<32> layout and vtable. */
    if (!bet_word(main, 0x122fdb8, 0xd000cd95) ||
        !bet_word(main, 0x122fdbc, 0x911c22b5) ||
        !be_read(main + 0x2be1708, &vtable, sizeof vtable) || vtable != main + 0x2a701c8 ||
        !be_read(main + 0x2be1710, &text, sizeof text) || text != main + 0x2be171c ||
        !be_read(main + 0x2be1718, &capacity, sizeof capacity) || capacity != 32 ||
        !be_read(text, name, 32) || !memchr(name, 0, 32)) return 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p)
        if (*p < 'A' || (*p > 'Z' && *p < 'a') || *p > 'z') return 0;
    return name[0] != 0;
}
static int bet_context(uintptr_t main, BetContext *context) {
    uintptr_t vtable = 0, string_vtable = 0, text = 0;
    int32_t capacity = 0;
    unsigned char blocked = 0;
    memset(context, 0, sizeof *context);
    if (!bet_api(main)) return 0;
    uintptr_t resetter = be_pointer(main + 0x2ca00a0);
    if (!resetter || !be_read(resetter, &vtable, sizeof vtable) ||
        vtable != main + 0x2b6d6a8 ||
        !be_memory_span(resetter, 0x390, VM_PROT_READ | VM_PROT_WRITE) ||
        !be_read(resetter + 0x20, &context->state, sizeof context->state) ||
        context->state > 13 ||
        !be_read(resetter + 0x178, &string_vtable, sizeof string_vtable) ||
        string_vtable != main + 0x2b6d6c8 ||
        !be_read(resetter + 0x180, &text, sizeof text) || text != resetter + 0x18c ||
        !be_read(resetter + 0x188, &capacity, sizeof capacity) || capacity != 512)
        return 0;
    /* This is the same event/busy flag checked by the native request itself. */
    uintptr_t event = 0;
    if (!be_read(main + 0x2c9f680, &event, sizeof event)) return 0;
    if (event && ((event & 7u) || event > UINTPTR_MAX - 0x6e6 ||
        !be_read(event + 0x6e6, &blocked, sizeof blocked) || blocked > 1)) return 0;
    context->resetter = resetter;
    context->blocked = blocked != 0;
    return 1;
}
static unsigned be_teleport_features(uintptr_t main) {
    BetContext context;
    char map[32];
    return bet_context(main, &context) && bet_map(main, map) && !strcmp(map, "MainField") ?
        BE_FEATURE_TELEPORT : 0;
}
static int be_teleport_to(uintptr_t main, float x, float y, float z,
                          char *result, size_t capacity) {
    BetContext context;
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) ||
        fabsf(x) > 6000.0f || fabsf(z) > 6000.0f || y < -1000.0f || y > 3000.0f) {
        snprintf(result, capacity, "The destination is outside the supported map bounds");
        return 0;
    }
    if (!bet_context(main, &context)) {
        snprintf(result, capacity, "The native teleport interface is unavailable for this session");
        return 0;
    }
    char map[32];
    if (!bet_map(main, map) || strcmp(map, "MainField")) {
        snprintf(result, capacity, "Return to the outdoor Hyrule map before using these destinations");
        return 0;
    }
    if (context.state || context.blocked) {
        snprintf(result, capacity, "Wait for the current warp or event to finish before teleporting");
        return 0;
    }
    BetVector position = {x, y, z}, rotation = {0, 0, 0}, scale = {1, 1, 1};
    BetString additional = {main + 0x2a6e6c0, ""};
    typedef int (*RequestWarp)(uintptr_t, int, const BetVector *, const BetVector *,
                               const BetVector *, const BetString *);
    /* SystemResetOption 0 is the ordinary full reset. Option 1, used by some
       near-warp scripts, skips part of the reset pipeline. */
    int accepted = ((RequestWarp)(main + 0xe4e0e0))(
        context.resetter, 0, &position, &rotation, &scale, &additional);
    uint32_t state = 0;
    float actual[3] = {0};
    int verified = accepted && be_read(context.resetter + 0x20, &state, sizeof state) &&
        state == 1 && be_read(context.resetter + 0x14c, &actual[0], 4) &&
        be_read(context.resetter + 0x15c, &actual[1], 4) &&
        be_read(context.resetter + 0x16c, &actual[2], 4) &&
        actual[0] == x && actual[1] == y && actual[2] == z;
    snprintf(result, capacity, "%s", verified ?
        "Teleport requested; the game is loading the destination" :
        "The game did not confirm the teleport request");
    return verified;
}
static void be_teleport_snapshot(FILE *output, uintptr_t main, int ready) {
    BetContext context;
    (void)ready;
    if (bet_context(main, &context))
        fprintf(output, ",\"teleport_busy\":%s,\"teleport_state\":%u",
            context.state || context.blocked ? "true" : "false", context.state);
    else fputs(",\"teleport_busy\":false,\"teleport_state\":null", output);
    /* Read-only NX160 Havok position chain, also used by world_survey.c.
       Keep publishing during loading/game-over so request and arrival can be
       distinguished. No transform or physics values are changed here. */
    float position[3] = {0};
    uintptr_t first = main && main <= UINTPTR_MAX - 0x2d37af0 && bet_api(main) ?
        be_pointer(main + 0x2d37af0) : 0;
    uintptr_t second = first && first <= UINTPTR_MAX - 0xf0 ? be_pointer(first + 0xf0) : 0;
    uintptr_t body = second && second <= UINTPTR_MAX - 0xd28 ? be_pointer(second + 0xd28) : 0;
    int valid = body && body <= UINTPTR_MAX - 0x5ec &&
        be_read(body + 0x5e0, position, sizeof position);
    for (int i = 0; i < 3; ++i)
        valid &= isfinite(position[i]) && fabsf(position[i]) <= 10000;
    if (valid) fprintf(output, ",\"position\":{\"x\":%.3f,\"y\":%.3f,\"z\":%.3f}",
        (double)position[0], (double)position[1], (double)position[2]);
    else fputs(",\"position\":null", output);
}
#endif
#endif
