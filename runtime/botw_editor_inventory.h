#ifndef MNG_BOTW_EDITOR_INVENTORY_H
#define MNG_BOTW_EDITOR_INVENTORY_H

/* NX 1.6.0 layout/address facts, checked against the player's verified main ELF.
   References: Pistonight/botw-symbols ld/ld160.ld and zeldaret/botw's
   uiPauseMenuDataMgr. No third-party implementation is included here.
   Include after botw_editor.h; invoke only on the guest presentation thread. */
#if defined(GAME_BOTW160) && !TARGET_OS_IPHONE
enum { BEI_SLOTS = 420, BEI_ITEM_SIZE = 0x298, BEI_BUFFER = 0x98, BEI_LIST = 0x68 };
typedef struct { uintptr_t vtable; const char *text; } BeiString;
typedef struct {
    uintptr_t address;
    int slot, type, value, equipped;
    char name[64];
} BeiItem;
static char bei_published_names[BEI_SLOTS][64];

static int bei_word(uintptr_t base, uintptr_t offset, uint32_t expected) {
    uint32_t found = 0;
    return base <= UINTPTR_MAX - offset && be_read(base + offset, &found, 4) && found == expected;
}
static int bei_api(uintptr_t base) {
    uintptr_t terminate = be_pointer(base + 0x2a6e6c0 + 0x18);
    return terminate >= base && terminate < base + 0x2120000 &&
        bei_word(base, 0x10c0c90, 0xd10343ff) && bei_word(base, 0x10c0c94, 0xa9076ffc) &&
        bei_word(base, 0x10c0c98, 0xa90867fa) && bei_word(base, 0x10c1ad0, 0x97fff054) &&
        bei_word(base, 0x10b8080, 0xd10183ff) && bei_word(base, 0x10b8084, 0xa90357f6) &&
        bei_word(base, 0x10bdc20, 0xd10443ff) && bei_word(base, 0x10bdc24, 0xfd0053e8) &&
        bei_word(base, 0x10c4590, 0xa9ba6ffc) && bei_word(base, 0x10c4594, 0xa90167fa) &&
        bei_word(base, 0x10c4868, 0x97ffe4ee) &&
        bei_word(base, 0x10b6970, 0xd10343ff) && bei_word(base, 0x10b6974, 0xa9076ffc);
}
static int bei_name(const char *name) {
    size_t length = strnlen(name, 64);
    if (!length || length >= 64) return 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}
static uintptr_t bei_manager(uintptr_t base) {
    uintptr_t manager = be_pointer(base + 0x2ca6d50);
    int32_t count = -1, offset = -1;
    if (!manager || !be_read(manager + 0x78, &count, 4) ||
        !be_read(manager + 0x7c, &offset, 4) || offset != 8 || count < 0 || count > BEI_SLOTS)
        return 0;
    return manager;
}
/* Armor uses -1 for a non-dyeable piece, 0 for its original color, and 1..15
   for dye colors. NX160 addToPouch writes this sentinel at 0x10b9f40. It is
   metadata, never a stack quantity or durability value. */
static int bei_native_value_valid(int type, int value) {
    if (type >= 4 && type <= 6) return value >= -1 && value <= 15;
    if (type == 0 || type == 1 || type == 3)
        return value >= 0 && value <= BE_MAX_RAW_DURABILITY;
    return type >= 0 && type <= 9 && value >= 0 && value <= BE_MAX_STACK_QUANTITY;
}
static int bei_item(uintptr_t base, uintptr_t manager, uintptr_t address, BeiItem *item) {
    uintptr_t start = manager + BEI_BUFFER;
    unsigned char header[0x40];
    if (address < start || address >= start + BEI_SLOTS * BEI_ITEM_SIZE ||
        (address - start) % BEI_ITEM_SIZE || !be_read(address, header, sizeof header)) return 0;
    uintptr_t vtable, name_vtable, text;
    int32_t type, value, capacity;
    memcpy(&vtable, header, 8); memcpy(&type, header + 0x18, 4);
    memcpy(&value, header + 0x20, 4); memcpy(&name_vtable, header + 0x28, 8);
    memcpy(&text, header + 0x30, 8); memcpy(&capacity, header + 0x38, 4);
    if (vtable != base + 0x2b877a0 || name_vtable != base + 0x2a6faf0 ||
        text != address + 0x3c || capacity != 64 || type < 0 || type > 9 ||
        header[0x24] > 1 || header[0x25] != 1 || !bei_native_value_valid(type, value) ||
        !be_read(text, item->name, sizeof item->name) || !bei_name(item->name)) return 0;
    item->address = address; item->slot = (int)((address - start) / BEI_ITEM_SIZE);
    item->type = type; item->value = value; item->equipped = header[0x24];
    return 1;
}
/* Walk the bounded, circular native list and verify both links at every node. */
static int bei_scan(uintptr_t base, uintptr_t manager, BeiItem *items, int *count) {
    uintptr_t head = manager + BEI_LIST, node = be_pointer(head + 8), previous = head;
    int32_t expected = -1;
    if (!node || !be_read(head + 0x10, &expected, 4) || expected < 0 || expected > BEI_SLOTS) return 0;
    int found = 0;
    while (node != head) {
        if (found >= BEI_SLOTS || node < 8 || be_pointer(node) != previous ||
            !bei_item(base, manager, node - 8, &items[found])) return 0;
        uintptr_t next = be_pointer(node + 8);
        if (!next || be_pointer(next) != node) return 0;
        previous = node; node = next; ++found;
    }
    if (found != expected || be_pointer(head) != previous) return 0;
    *count = found;
    return 1;
}
static void bei_mutex(uintptr_t base, uintptr_t manager, int lock) {
    ((void (*)(void *))(base + (lock ? 0x2128040 : 0x2128050)))((void *)(manager + 0x48));
}
static int bei_total(const BeiItem *items, int count, const char *name, int type) {
    int total = 0;
    for (int i = 0; i < count; ++i) if (!strcmp(items[i].name, name)) {
        int addition = (type == 2 || type == 7) ? items[i].value : 1;
        if (addition > INT_MAX - total) return INT_MAX;
        total += addition;
    }
    return total;
}
/* Only 070 has NX160's CanGetPouch tag. 056/071/072 are internal world or
   appearance forms; acquiring them is not a capacity/DLC failure. */
static const char *bei_give_block_reason(const char *name, int amount, const BeiItem *items, int count) {
    if (!strcmp(name, "Weapon_Sword_056") || !strcmp(name, "Weapon_Sword_071") || !strcmp(name, "Weapon_Sword_072"))
        return "This is an internal Master Sword form, not a pouch item. Choose Master Sword (Weapon_Sword_070).";
    if (!strcmp(name, "Weapon_Sword_070")) {
        if (amount != 1) return "BOTW allows one Master Sword. Choose a quantity of 1.";
        if (bei_total(items, count, name, 0)) return "The Master Sword is already in your pouch. BOTW allows one.";
    }
    return NULL;
}
static int bei_value_maximum(int type) {
    if (type == 0 || type == 1 || type == 3) return BE_MAX_RAW_DURABILITY;
    if (type == 2 || type == 7) return BE_MAX_STACK_QUANTITY;
    return 0;
}
/* Weapon life is stored in hundredths of a durability point. Keep its raw
   range separate from stack counts, and repeat all guards at the write. */
static int bei_set_value(const BeiItem *item, int value) {
    int maximum = bei_value_maximum(item->type);
    if (!maximum || value < 1 || value > maximum ||
        (item->equipped && item->type != 2) || item->address > UINTPTR_MAX - 0x20) return 0;
    return be_write(item->address + 0x20, &value, sizeof value);
}
static unsigned be_inventory_features(uintptr_t base) {
    return bei_api(base) && bei_manager(base) ? BE_FEATURE_ITEMS | BE_FEATURE_INVENTORY : 0;
}
/* A game menu is not, by itself, evidence of a changing inventory. Only report
   actual validation/held-material conditions; never prescribe closing a menu
   for a failed layout check. Call while holding the inventory mutex. */
static const char *bei_inventory_block_reason(uintptr_t manager, int scan_valid) {
    if (!manager) return "Wait for BOTW's inventory to load.";
    if (!scan_valid) return "Inventory data could not be validated. Wait for a refresh before trying again.";
    for (int i = 0; i < 5; ++i) {
        uintptr_t held = 0;
        if (!be_read(manager + 0x444a0 + (uintptr_t)i * 16, &held, sizeof held))
            return "Inventory data is not available yet. Wait for a refresh.";
        if (held) return "Put away the materials Link is holding in BOTW, then try again.";
    }
    return NULL;
}
static void be_inventory_snapshot(FILE *output, uintptr_t base, int ready) {
    BeiItem items[BEI_SLOTS]; int count = 0;
    uintptr_t manager = ready && bei_api(base) ? bei_manager(base) : 0;
    if (manager) bei_mutex(base, manager, 1);
    int valid = manager && bei_scan(base, manager, items, &count);
    const char *blocked = !ready ? "Load a gameplay session to edit inventory." : bei_inventory_block_reason(manager, valid);
    memset(bei_published_names, 0, sizeof bei_published_names);
    fputs(",\"inventory\":[", output);
    for (int i = 0; valid && i < count; ++i) {
        const BeiItem *item = &items[i];
        if (i) fputc(',', output);
        fprintf(output, "{\"slot\":%d,\"name\":", item->slot);
        be_json_string(output, item->name);
        fprintf(output, ",\"type\":%d,\"value\":%d,\"equipped\":%s}",
            item->type, item->value, item->equipped ? "true" : "false");
        memcpy(bei_published_names[item->slot], item->name, sizeof item->name);
    }
    fputc(']', output);
    fprintf(output, ",\"inventory_ready\":%s,\"inventory_block_reason\":", blocked ? "false" : "true");
    if (blocked) be_json_string(output, blocked); else fputs("null", output);
    if (manager) bei_mutex(base, manager, 0);
}
static int be_inventory_command(uintptr_t base, const char *verb, const char *argument,
                                int value, char *result, size_t capacity) {
    if (strcmp(verb, "give") && strcmp(verb, "quantity") && strcmp(verb, "remove")) return 0;
    uintptr_t manager = bei_api(base) ? bei_manager(base) : 0;
    if (!manager) { snprintf(result, capacity, "Inventory is not ready or this game build differs"); return 0; }
    BeiItem items[BEI_SLOTS]; int count = 0, ok = 0;
    bei_mutex(base, manager, 1);
    int valid = bei_scan(base, manager, items, &count);
    const char *blocked = bei_inventory_block_reason(manager, valid);
    if (blocked) {
        snprintf(result, capacity, "%s", blocked);
        goto done;
    }
    if (!strcmp(verb, "give")) {
        if (!bei_name(argument) || strstr(argument, "Default") || strstr(argument, "Extra") || value < 1 || value > BE_MAX_STACK_QUANTITY) {
            snprintf(result, capacity, "Choose a valid item and amount from 1 to 999"); goto done;
        }
        const char *restriction = bei_give_block_reason(argument, value, items, count);
        if (restriction) { snprintf(result, capacity, "%s", restriction); goto done; }
        BeiString name = {base + 0x2a6e6c0, argument};
        int type = ((int (*)(const BeiString *, void *))(base + 0x10b8080))(&name, NULL);
        if (type < 0 || type > 9) { snprintf(result, capacity, "This actor is not an inventory item"); goto done; }
        int before = bei_total(items, count, argument, type);
        ((void (*)(void *, const BeiString *, int, int))(base + 0x10c0c90))((void *)manager, &name, value, 0);
        if (!bei_scan(base, manager, items, &count)) { snprintf(result, capacity, "Item request applied; inventory refresh is pending"); goto done; }
        int added = bei_total(items, count, argument, type) - before;
        ok = added > 0;
        if (ok) snprintf(result, capacity, "Added %d × %s", added, argument);
        else snprintf(result, capacity, "Item was not added: check capacity, duplicates and DLC requirements");
        goto done;
    }
    char *end = NULL; long slot = strtol(argument, &end, 10);
    if (!*argument || !end || *end || slot < 0 || slot >= BEI_SLOTS) {
        snprintf(result, capacity, "Choose an inventory row"); goto done;
    }
    BeiItem *item = NULL;
    for (int i = 0; i < count; ++i) if (items[i].slot == slot) { item = &items[i]; break; }
    if (!item || strcmp(item->name, bei_published_names[slot])) {
        snprintf(result, capacity, "That inventory row changed; select it again"); goto done;
    }
    if (!strcmp(verb, "quantity")) {
        int maximum = bei_value_maximum(item->type);
        if (!maximum) {
            snprintf(result, capacity, "Quantity/value editing supports materials, arrows and weapon durability"); goto done;
        }
        if (item->equipped && item->type != 2) {
            snprintf(result, capacity, "Unequip this weapon before editing its durability"); goto done;
        }
        if (value < 1 || value > maximum) {
            snprintf(result, capacity, "Enter %s from 1 to %d", item->type == 2 || item->type == 7 ? "a stack quantity" : "raw durability", maximum); goto done;
        }
        if (!bei_set_value(item, value)) {
            snprintf(result, capacity, "The item value could not be written"); goto done;
        }
        ((void (*)(void *, void *))(base + 0x10bdc20))((void *)manager, (void *)(manager + BEI_LIST));
        int check = 0;
        ok = be_read(item->address + 0x20, &check, 4) && check == value;
        snprintf(result, capacity, ok ? "Updated %s to %d" : "Could not confirm the item value", item->name, value);
    } else {
        if (item->equipped) { snprintf(result, capacity, "Unequip this item in the game before removing it"); goto done; }
        unsigned char absent = 0;
        if (!be_write(item->address + 0x25, &absent, 1)) {
            snprintf(result, capacity, "The item could not be marked for removal"); goto done;
        }
        char removed[64]; memcpy(removed, item->name, sizeof removed);
        uintptr_t old_address = item->address;
        ((void (*)(void *))(base + 0x10c4590))((void *)manager);
        ok = bei_scan(base, manager, items, &count);
        for (int i = 0; ok && i < count; ++i) if (items[i].address == old_address) ok = 0;
        snprintf(result, capacity, ok ? "Removed %s" : "Removal requested; inventory refresh is pending", removed);
    }
done:
    bei_mutex(base, manager, 0);
    return ok;
}
#else
static unsigned be_inventory_features(uintptr_t base) { (void)base; return 0; }
static void be_inventory_snapshot(FILE *output, uintptr_t base, int ready) {
    (void)base; (void)ready; fputs(",\"inventory\":[]", output);
}
static int be_inventory_command(uintptr_t base, const char *verb, const char *argument,
                                int value, char *result, size_t capacity) {
    (void)base; (void)verb; (void)argument; (void)value; (void)result; (void)capacity; return 0;
}
#endif
#endif
