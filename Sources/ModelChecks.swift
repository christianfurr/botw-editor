import Foundation

@main
struct ModelChecks {
    static func main() throws {
        let decoder = JSONDecoder()
        func inventoryItem(type: Int, slot: Int = 12, equipped: Bool = false) -> InventoryEntry {
            InventoryEntry(slot: slot, name: "FixtureItem", type: type, value: 2000, equipped: equipped)
        }
        func reject(_ command: TrainerCommand) {
            do { _ = try command.payload(); fatalError("Invalid command accepted") }
            catch is CommandError {}
            catch { fatalError("Unexpected validation error: \(error)") }
        }
        let json = #"{"schema":1,"pid":42,"version":"1.6.0","ready":true,"message":"Ready","health":12,"stamina":1000,"rupees":25,"health_lock":false,"stamina_lock":true,"last_request":123,"last_ok":false,"last_result":"Rejected","features":{"health":true,"stamina":true,"rupees":true,"items":false,"lift":false}}"#
        let state = try decoder.decode(TrainerState.self, from: Data(json.utf8))
        precondition(state.lastRequest == 123 && !state.lastOK && state.health == 12)
        precondition(state.playerCacheReady == nil && state.maxHeartsLive == nil && state.towersRaised == nil)
        precondition(state.position == nil)
        precondition(state.inventoryReady == nil && state.inventoryBlockReason == nil)
        precondition(!TrainerConnection.canEditInventory("items",state:state,alive:true,fresh:true,inputExists:true))
        precondition(TrainerConnection.canSetMaximum("health",state:state,alive:true,fresh:true,inputExists:true))
        precondition(TrainerConnection.canUse("health", state: state, alive: true, fresh: true, inputExists: true))
        precondition(!TrainerConnection.canUse("health", state: state, alive: false, fresh: true, inputExists: true))
        precondition(!TrainerConnection.canUse("health", state: state, alive: true, fresh: false, inputExists: true))
        precondition(!TrainerConnection.canUse("health", state: state, alive: true, fresh: true, inputExists: false))
        precondition(!TrainerConnection.canUse("items", state: state, alive: true, fresh: true, inputExists: true))
        precondition(!TrainerConnection.isFresh(Date(timeIntervalSinceNow: -20)))
        precondition(!TrainerConnection.processExists(-1))
        let item = try TrainerCommand.give("Item_Fruit_A", "20").payload()
        let rupees = try TrainerCommand.rupees("999999").payload()
        let lift = try TrainerCommand.lift("2").payload()
        precondition(item == "give Item_Fruit_A 20")
        let outfits = [
            CatalogItem(id:"Armor_181_Head",name:"Vah Ruta Divine Helm (v1.3.3)",category:"clothes"),
            CatalogItem(id:"Armor_186_Head",name:"Vah Ruta Divine Helm ★ (v1.3.3)",category:"clothes"),
            CatalogItem(id:"Armor_140_Lower",name:"Snow Boots?",category:"clothes"),
            CatalogItem(id:"Armor_141_Lower",name:"Snow Boots",category:"clothes"),
            CatalogItem(id:"Weapon_Sword_001",name:"Traveler's Sword",category:"weapons"),
        ]
        let upgraded = InventoryEntry(slot:12,name:"Armor_186_Head",type:4,value:-1,equipped:true)
        precondition(OutfitGrantRules.missingBasePieces(in:outfits,owned:[]).map(\.id) == ["Armor_181_Head","Armor_141_Lower"])
        precondition(OutfitGrantRules.missingBasePieces(in:outfits,owned:[upgraded]).map(\.id) == ["Armor_141_Lower"])
        precondition(OutfitGrantRules.freeSlots(in:[upgraded, inventoryItem(type:7)]) == 99)
        precondition(OutfitGrantRules.freeSlots(in:Array(repeating:upgraded,count:101)) == 0)
        let fullArmor = Array(repeating:upgraded,count:100)
        precondition(ItemGrantRules.blockedReason(for:"Armor_001_Head",inventory:fullArmor)?.contains("full") == true)
        precondition(ItemGrantRules.blockedReason(for:"Item_Fruit_A",inventory:fullArmor) == nil)
        for actor in ["Weapon_Sword_056", "Weapon_Sword_071", "Weapon_Sword_072"] {
            reject(.give(actor, "1"))
            precondition(ItemGrantRules.isInternalMasterSwordForm(actor))
            precondition(ItemGrantRules.blockedReason(for: actor, inventory: []) != nil)
        }
        let singleMasterSword = try TrainerCommand.give("Weapon_Sword_070", "1").payload()
        precondition(singleMasterSword == "give Weapon_Sword_070 1")
        reject(.give("Weapon_Sword_070", "2"))
        precondition(ItemGrantRules.blockedReason(for: "Weapon_Sword_070", inventory: []) == nil)
        let masterSword = InventoryEntry(slot:409,name:"Weapon_Sword_070",type:0,value:4000,equipped:false)
        precondition(ItemGrantRules.blockedReason(for: "Weapon_Sword_070", inventory:[masterSword]) != nil)
        precondition(ItemGrantRules.blockedReason(for: "Weapon_Sword_057", inventory:[masterSword]) == nil)
        for level in MasterSwordLevel.allCases {
            let command = TrainerCommand.masterSwordLevel(level)
            let payload = try command.payload()
            precondition(payload == "master_sword \(level.rawValue)")
            precondition(command.feature == "master_sword" && !command.requiresPlayerCache)
            precondition(level.attack == 30 + level.rawValue * 10)
        }
        precondition(state.masterSwordLevel == nil && state.masterSwordAttack == nil && state.masterSwordFullPower == nil)
        precondition(state.mapAccessUnlocked == nil)
        let mapAccess = TrainerCommand.enableMapAccess
        let mapAccessPayload = try mapAccess.payload()
        precondition(mapAccessPayload == "map_access" && mapAccess.feature == "map_access")
        let clearPayload = try TrainerCommand.markShrineCleared("Dungeon119").payload()
        let clearAllPayload = try TrainerCommand.markAllShrinesCleared.payload()
        precondition(clearPayload == "complete_shrine Dungeon119" && clearAllPayload == "complete_all_shrines")
        precondition(TrainerCommand.markAllShrinesCleared.feature == "shrine_completion")
        for id in ["Dungeon120","Tower07","Dungeon001\nhealth on","../Dungeon001"] { reject(.markShrineCleared(id)) }
        for speed in [100,125,200] {
            let payload = try TrainerCommand.horseSpeed(speed).payload()
            precondition(payload == "horse_speed \(speed)")
        }
        let horseOff = try TrainerCommand.horseSpeed(nil).payload()
        precondition(horseOff == "horse_speed off" && TrainerCommand.horseSpeed(nil).feature == "horse_speed")
        reject(.horseSpeed(99));reject(.horseSpeed(201))
        precondition(state.shrineClearCount == nil && state.horseSpeedMultiplier == nil && state.horseSpeedAvailable == nil)
        for kind in PouchSlotKind.allCases {
            precondition(state.slots(for:kind) == nil)
            for value in [kind.minimum,kind.maximum] {
                let command = TrainerCommand.pouchSlots(kind,String(value))
                let payload = try command.payload()
                precondition(payload == "pouch_slots \(kind.rawValue) \(value)")
                precondition(command.feature == "pouch_slots")
            }
            for value in [String(kind.minimum-1),String(kind.maximum+1),"-1","2.5","nan","20\nhealth on"] {
                reject(.pouchSlots(kind,value))
            }
        }
        precondition(rupees == "rupees 999999")
        precondition(lift == "lift 2.00")
        let teleport = try TrainerCommand.teleport("Dungeon001").payload()
        let quantity = try TrainerCommand.quantity(inventoryItem(type: 7),"99").payload()
        let unlock = try TrainerCommand.unlockShrine("Dungeon001").payload()
        precondition(teleport == "teleport Dungeon001")
        precondition(quantity == "quantity 12 99")
        precondition(unlock == "unlock_shrine Dungeon001")
        for type in [0, 1, 3] {
            let weapon = inventoryItem(type: type, slot: 419)
            precondition(weapon.usesDurability && weapon.canEditValue)
            for value in [1, 999, 2000, 3000, 100_000_000] {
                let payload = try TrainerCommand.quantity(weapon, String(value)).payload()
                precondition(payload == "quantity 419 \(value)")
            }
            for value in ["0", "-1", "100000001", "2147483648", "1.5"] {
                reject(.quantity(weapon, value))
            }
            let equipped = inventoryItem(type: type, equipped: true)
            precondition(!equipped.canEditValue)
            reject(.quantity(equipped, "2000"))
        }
        for type in [2, 7] {
            let stack = inventoryItem(type: type)
            precondition(!stack.usesDurability && stack.maximumEditableValue == 999)
            for value in [1, 999] {
                let payload = try TrainerCommand.quantity(stack, String(value)).payload()
                precondition(payload == "quantity 12 \(value)")
            }
            for value in ["0", "1000", "100000000"] { reject(.quantity(stack, value)) }
        }
        let equippedArrows = try TrainerCommand.quantity(inventoryItem(type: 2, equipped: true), "999").payload()
        precondition(equippedArrows == "quantity 12 999")
        for type in [-1, 4, 5, 6, 8, 9, 10] {
            let unsupported = inventoryItem(type: type)
            precondition(!unsupported.canEditValue)
            reject(.quantity(unsupported, "1"))
        }
        let hearts = try TrainerCommand.maxHearts("30").payload()
        let stamina = try TrainerCommand.maxStamina("3").payload()
        let full = try TrainerCommand.fullAccess.payload()
        precondition(hearts == "max_hearts 30" && stamina == "max_stamina 3" && full == "full_access")
        var extended = try JSONSerialization.jsonObject(with: Data(json.utf8)) as! [String:Any]
        extended["features"] = ["health":true,"stamina":true,"items":true,"inventory_edit":true]
        extended["master_sword_level"] = 3
        extended["master_sword_attack"] = 60
        extended["master_sword_full_power"] = true
        extended["weapon_slots"] = 20
        extended["bow_slots"] = 14
        extended["shield_slots"] = 20
        extended["map_access_unlocked"] = true
        extended["shrine_clear_count"] = 3
        extended["horse_speed_multiplier"] = 1.5
        extended["horse_speed_active"] = true
        extended["horse_speed_available"] = true
        extended["horse_speed_reason"] = NSNull()
        extended["inventory"] = [["slot":12,"name":"Item_Fruit_A","type":5,"value":10,"equipped":false]]
        extended["destinations"] = [["id":"Dungeon001","name":"A destination","kind":"shrine","x":10.0,"y":20.0,"z":30.0,"unlocked":false]]
        let extendedState = try decoder.decode(TrainerState.self, from: JSONSerialization.data(withJSONObject: extended))
        precondition(extendedState.inventory?.first?.slot == 12)
        precondition(extendedState.masterSwordLevel == 3 && extendedState.masterSwordAttack == 60 && extendedState.masterSwordFullPower == true)
        precondition(extendedState.slots(for:.weapons) == 20 && extendedState.slots(for:.bows) == 14 && extendedState.slots(for:.shields) == 20)
        precondition(extendedState.mapAccessUnlocked == true)
        precondition(extendedState.shrineClearCount == 3 && extendedState.horseSpeedMultiplier == 1.5 && extendedState.horseSpeedActive == true && extendedState.horseSpeedAvailable == true)
        precondition(extendedState.destinations?.first?.z == 30)
        precondition(extendedState.destinations?.first?.permitsTeleport == true)
        precondition(TrainerConnection.canEditInventory("items",state:extendedState,alive:true,fresh:true,inputExists:true))
        extended["player_cache_ready"] = false
        extended["max_hearts_live"] = 20.0
        extended["max_stamina_live"] = 2.0
        extended["towers_raised"] = false
        extended["position"] = ["x": -666.043, "y": 173.104, "z": 1518.725]
        extended["inventory_ready"] = false
        extended["inventory_block_reason"] = "Put away held materials first."
        extended["destinations"] = [
            ["id":"north","name":"North","kind":"tower","x":-10.0,"y":20.0,"z":-100.0,"teleportable":false],
            ["id":"south","name":"South","kind":"shrine","x":10.0,"y":20.0,"z":100.0,"teleportable":true]
        ]
        let cacheLoading = try decoder.decode(TrainerState.self,from:JSONSerialization.data(withJSONObject:extended))
        precondition(cacheLoading.maxHeartsLive == 20 && cacheLoading.maxStaminaLive == 2 && cacheLoading.towersRaised == false)
        precondition(cacheLoading.position?.isFinite == true && cacheLoading.position?.x == -666.043)
        precondition(cacheLoading.inventoryReady == false && cacheLoading.inventoryBlockReason == "Put away held materials first.")
        precondition(!TrainerConnection.canEditInventory("items",state:cacheLoading,alive:true,fresh:true,inputExists:true))
        precondition(TrainerConnection.canUse("health",state:cacheLoading,alive:true,fresh:true,inputExists:true))
        precondition(!TrainerConnection.canSetMaximum("health",state:cacheLoading,alive:true,fresh:true,inputExists:true))
        precondition(cacheLoading.destinations?.first?.permitsTeleport == false)
        let locations = cacheLoading.destinations!
        let projection = WorldMapProjection(points:locations)
        let north = projection.location(x:locations[0].x,z:locations[0].z,width:600,height:400)
        let south = projection.location(x:locations[1].x,z:locations[1].z,width:600,height:400)
        precondition(north.y < south.y && north.x < south.x, "Map must keep negative Z north and positive X east")
        let single = WorldMapProjection(points:[locations[0]]).location(x:locations[0].x,z:locations[0].z,width:600,height:400)
        precondition(single.x == 300 && single.y == 200)
        extended["player_cache_ready"] = true
        extended["inventory_ready"] = true
        let cacheReady = try decoder.decode(TrainerState.self,from:JSONSerialization.data(withJSONObject:extended))
        precondition(TrainerConnection.canSetMaximum("stamina",state:cacheReady,alive:true,fresh:true,inputExists:true))
        precondition(TrainerConnection.canEditInventory("inventory_edit",state:cacheReady,alive:true,fresh:true,inputExists:true))
        extended["teleport_busy"] = true
        extended["teleport_state"] = 4
        extended["max_hearts"] = 12.0
        extended["max_stamina"] = 3.0
        let loadingState = try decoder.decode(TrainerState.self, from: JSONSerialization.data(withJSONObject: extended))
        precondition(loadingState.maxHearts == 12 && loadingState.maxStamina == 3)
        precondition(!TrainerConnection.canUse("health", state: loadingState, alive:true, fresh:true, inputExists:true))
        let invalid: [TrainerCommand] = [.give("Apple\nhealth on", "1"), .give("Item_Fruit_A", "1000"), .give("Item_Fruit_A", "0"), .rupees("1000000"), .rupees("-1"), .lift("nan"), .lift("100"), .quantity(inventoryItem(type: 7, slot: -1),"2"), .quantity(inventoryItem(type: 7, slot: 420),"2"), .remove(-1), .teleport("Dungeon001\nhealth on"), .unlockShrine("../Dungeon001"),.maxHearts("0"),.maxHearts("31"),.maxStamina("4"),.maxStamina("1.5")]
        for command in invalid { reject(command) }
        print("Model checks passed: typed durability/stack bounds, equipped-weapon refusal, live-stat/cache gates, tower travel availability, north-up map projection, and command validation.")
    }
}
