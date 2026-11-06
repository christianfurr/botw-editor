#ifndef MNG_BOTW_EDITOR_DESTINATIONS_H
#define MNG_BOTW_EDITOR_DESTINATIONS_H

/* Coordinates are extracted locally from each player's own game. The public
   editor carries only this bounded TSV reader, never a game-data bundle. */
#include <sys/stat.h>

typedef struct {
    char id[64], name[128], kind[16];
    float x, y, z;
} BeDestination;
static BeDestination bed_destinations[512];
static size_t bed_count;
static char bed_path[PATH_MAX];
static time_t bed_mtime;
static uint64_t bed_checked;

static int bed_number(const char *text, float *value) {
    char *end = NULL;
    errno = 0;
    float parsed = strtof(text, &end);
    if (errno || !end || end == text || *end || !isfinite(parsed)) return 0;
    *value = parsed;
    return 1;
}
static void bed_load(void) {
    if (!bed_path[0]) {
        const char *controls = getenv("TOTK_CONTROLS"), *input = getenv("TOTK_INPUT_FILE");
        const char *source = controls && strchr(controls, '/') ? controls : input;
        const char *slash = source ? strrchr(source, '/') : NULL;
        if (!slash || slash - source > PATH_MAX - 64) return;
        snprintf(bed_path, sizeof bed_path, "%.*s/editor-destinations.tsv", (int)(slash - source), source);
    }
    uint64_t now = runtime_now_ns();
    if (bed_checked && now >= bed_checked && now - bed_checked < 2000000000ULL) return;
    bed_checked = now;
    struct stat info;
    if (stat(bed_path, &info)) { bed_count = 0; bed_mtime = 0; return; }
    if (bed_count && info.st_mtime == bed_mtime) return;
    if (info.st_size <= 0 || info.st_size > 262144) { bed_count = 0; return; }
    FILE *input = fopen(bed_path, "r");
    if (!input) { bed_count = 0; return; }
    BeDestination entries[512];
    size_t count = 0;
    int valid = 1;
    char line[512];
    while (fgets(line, sizeof line, input)) {
        if (!strchr(line, '\n') && !feof(input)) { valid = 0; break; }
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0] || line[0] == '#') continue;
        char *field[7] = {0}, *context = NULL;
        unsigned fields = 0;
        for (char *token = strtok_r(line, "\t", &context); token && fields < 7;
             token = strtok_r(NULL, "\t", &context)) field[fields++] = token;
        if (fields != 6 || count >= 512 || !be_identifier(field[0]) || strlen(field[0]) >= 64 ||
            !field[1][0] || strlen(field[1]) >= 128 ||
            (strcmp(field[2], "shrine") && strcmp(field[2], "tower") && strcmp(field[2], "location"))) {
            valid = 0; break;
        }
        BeDestination *entry = &entries[count];
        if (!bed_number(field[3], &entry->x) || !bed_number(field[4], &entry->y) ||
            !bed_number(field[5], &entry->z) || fabsf(entry->x) > 6000 || fabsf(entry->z) > 6000 ||
            entry->y < -1000 || entry->y > 3000) { valid = 0; break; }
        for (size_t i = 0; i < count; ++i) if (!strcmp(entries[i].id, field[0])) valid = 0;
        if (!valid) break;
        snprintf(entry->id, sizeof entry->id, "%s", field[0]);
        snprintf(entry->name, sizeof entry->name, "%s", field[1]);
        snprintf(entry->kind, sizeof entry->kind, "%s", field[2]);
        ++count;
    }
    if (ferror(input)) valid = 0;
    fclose(input);
    bed_count = valid ? count : 0;
    if (valid) { memcpy(bed_destinations, entries, count * sizeof *entries); bed_mtime = info.st_mtime; }
}
static int bed_teleport(uintptr_t base, const char *identifier, char *result, size_t capacity) {
    bed_load();
    for (size_t i = 0; i < bed_count; ++i) {
        const BeDestination *entry = &bed_destinations[i];
        if (!strcmp(entry->id, identifier)) {
            int raised = 0;
            if (!strcmp(entry->kind, "tower") &&
                (!be_world_get_bool(base, "MapTower_07_Demo", &raised) || !raised)) {
                snprintf(result, capacity, "Raise the towers with Full Access or activate the first tower in the game before traveling to a tower");
                return 0;
            }
            return be_teleport_to(base, entry->x, entry->y, entry->z, result, capacity);
        }
    }
    snprintf(result, capacity, "Choose a destination extracted from your installed game");
    return 0;
}
static void bed_snapshot(FILE *output, uintptr_t base, int ready) {
    bed_load();
    fputs(",\"destinations\":[", output);
    for (size_t i = 0; i < bed_count; ++i) {
        const BeDestination *entry = &bed_destinations[i];
        if (i) fputc(',', output);
        fputs("{\"id\":", output); be_json_string(output, entry->id);
        fputs(",\"name\":", output); be_json_string(output, entry->name);
        fputs(",\"kind\":", output); be_json_string(output, entry->kind);
        fprintf(output, ",\"x\":%.4f,\"y\":%.4f,\"z\":%.4f", entry->x, entry->y, entry->z);
        if (!strcmp(entry->kind, "tower")) {
            int raised = 0;
            int safe = ready && be_world_get_bool(base, "MapTower_07_Demo", &raised) && raised;
            fprintf(output, ",\"teleportable\":%s", safe ? "true" : "false");
        }
        if (ready) {
            char flag[96]; int unlocked = 0;
            if (!strcmp(entry->kind, "shrine")) snprintf(flag, sizeof flag, "Open_%s", entry->id);
            else if (!strcmp(entry->kind, "tower") && !strncmp(entry->id, "Tower", 5))
                snprintf(flag, sizeof flag, "MapTower_%02d", atoi(entry->id + 5));
            else snprintf(flag, sizeof flag, "%s", entry->id);
            if (be_world_get_bool(base, flag, &unlocked)) {
                int known = 1;
                if (!strcmp(entry->kind, "shrine")) {
                    int32_t discovered = 0;
                    snprintf(flag, sizeof flag, "Location_%s", entry->id);
                    known = be_world_get_s32(base, flag, &discovered);
                    unlocked = unlocked && discovered > 0;
                }
                if (known) fprintf(output, ",\"unlocked\":%s", unlocked ? "true" : "false");
            }
        }
        if (!strcmp(entry->kind, "shrine")) {
            char flag[96]; int completed = 0;
            snprintf(flag, sizeof flag, "Clear_%s", entry->id);
            fputs(",\"completed\":", output);
            if (ready && be_world_get_bool(base, flag, &completed)) fputs(completed ? "true" : "false", output);
            else fputs("null", output);
        }
        fputc('}', output);
    }
    fputc(']', output);
}
#endif
