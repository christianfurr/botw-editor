import Foundation
import Darwin

struct CatalogItem: Decodable, Identifiable {
    let id: String
    let name: String
    let category: String
}

struct InventoryEntry: Decodable, Identifiable {
    static let maximumRawDurability = 100_000_000
    let slot: Int
    let name: String
    let type: Int
    let value: Int
    let equipped: Bool
    var id: Int { slot }
    var usesDurability: Bool { [0, 1, 3].contains(type) }
    var maximumEditableValue: Int? {
        if usesDurability { return Self.maximumRawDurability }
        if [2, 7].contains(type) { return 999 }
        return nil
    }
    var canEditValue: Bool { maximumEditableValue != nil && (!equipped || type == 2) }
}

struct WorldDestination: Decodable, Identifiable {
    let id: String
    let name: String
    let kind: String
    let x: Double
    let y: Double
    let z: Double
    let unlocked: Bool?
    let teleportable: Bool?
    var permitsTeleport: Bool { teleportable != false }
}

struct WorldMapProjection {
    let minX: Double
    let maxX: Double
    let minZ: Double
    let maxZ: Double

    init(points: [WorldDestination]) {
        let valid = points.filter { $0.x.isFinite && $0.z.isFinite }
        minX = valid.map(\.x).min() ?? 0
        maxX = valid.map(\.x).max() ?? 1
        minZ = valid.map(\.z).min() ?? 0
        maxZ = valid.map(\.z).max() ?? 1
    }

    func location(x: Double, z: Double, width: Double, height: Double) -> (x: Double, y: Double) {
        let mapX = maxX > minX ? 20 + (x-minX)/(maxX-minX)*max(width-40,1) : width/2
        // Negative world Z is north; Canvas Y grows downward.
        let mapY = maxZ > minZ ? 20 + (z-minZ)/(maxZ-minZ)*max(height-40,1) : height/2
        return (mapX,mapY)
    }
}

struct WorldPosition: Decodable {
    let x: Double
    let y: Double
    let z: Double
    var isFinite: Bool { x.isFinite && y.isFinite && z.isFinite }
}

struct TrainerState: Decodable {
    let schema: Int
    let pid: Int32
    let version: String
    let ready: Bool
    let message: String
    let health: Int?
    let stamina: Double?
    let maxHearts: Double?
    let maxStamina: Double?
    let rupees: Int?
    let healthLock: Bool
    let staminaLock: Bool
    let lastRequest: UInt64
    let lastOK: Bool
    let lastResult: String
    let features: [String: Bool]
    let inventory: [InventoryEntry]?
    let destinations: [WorldDestination]?
    let teleportBusy: Bool?
    let teleportState: Int?
    let playerCacheReady: Bool?
    let maxHeartsLive: Double?
    let maxStaminaLive: Double?
    let towersRaised: Bool?
    let position: WorldPosition?

    enum CodingKeys: String, CodingKey {
        case schema, pid, version, ready, message, health, stamina, rupees, features, inventory, destinations, position
        case healthLock = "health_lock"
        case staminaLock = "stamina_lock"
        case maxHearts = "max_hearts"
        case maxStamina = "max_stamina"
        case teleportBusy = "teleport_busy"
        case teleportState = "teleport_state"
        case playerCacheReady = "player_cache_ready"
        case maxHeartsLive = "max_hearts_live"
        case maxStaminaLive = "max_stamina_live"
        case towersRaised = "towers_raised"
        case lastRequest = "last_request"
        case lastOK = "last_ok"
        case lastResult = "last_result"
    }
}

enum TrainerCommand {
    case status
    case health(Bool)
    case stamina(Bool)
    case rupees(String)
    case give(String, String)
    case lift(String)
    case quantity(InventoryEntry, String)
    case remove(Int)
    case teleport(String)
    case unlockShrine(String)
    case unlockAllShrines
    case revealMap
    case runes
    case maxHearts(String)
    case maxStamina(String)
    case paraglider
    case fullAccess

    var requiresPlayerCache: Bool {
        switch self {
        case .maxHearts, .maxStamina: return true
        default: return false
        }
    }

    var feature: String? {
        switch self {
        case .status: return nil
        case .health: return "health"
        case .stamina: return "stamina"
        case .rupees: return "rupees"
        case .give: return "items"
        case .lift: return "lift"
        case .quantity, .remove: return "inventory_edit"
        case .teleport: return "teleport"
        case .unlockShrine, .unlockAllShrines: return "shrines"
        case .revealMap: return "map"
        case .runes: return "runes"
        case .maxHearts: return "health"
        case .maxStamina: return "stamina"
        case .paraglider, .fullAccess: return "runes"
        }
    }

    func payload() throws -> String {
        switch self {
        case .status: return "status"
        case .health(let enabled): return "health \(enabled ? "on" : "off")"
        case .stamina(let enabled): return "stamina \(enabled ? "on" : "off")"
        case .rupees(let text):
            guard let value = Int(text.trimmingCharacters(in: .whitespaces)), (0...999_999).contains(value) else {
                throw CommandError.invalid("Enter a rupee amount from 0 to 999,999.")
            }
            return "rupees \(value)"
        case .give(let actor, let text):
            guard actor.range(of: "^[A-Za-z0-9_]{1,80}$", options: .regularExpression) != nil else {
                throw CommandError.invalid("Use an item actor ID containing letters, numbers, or underscores.")
            }
            guard let quantity = Int(text.trimmingCharacters(in: .whitespaces)), (1...999).contains(quantity) else {
                throw CommandError.invalid("Enter an item quantity from 1 to 999.")
            }
            return "give \(actor) \(quantity)"
        case .lift(let text):
            guard let meters = Double(text), meters.isFinite, (0.5...10).contains(meters) else {
                throw CommandError.invalid("Enter a lift distance from 0.5 to 10 meters.")
            }
            return "lift \(String(format: "%.2f", locale: Locale(identifier: "en_US_POSIX"), meters))"
        case .quantity(let item, let text):
            guard (0...419).contains(item.slot), let maximum = item.maximumEditableValue else {
                throw CommandError.invalid("Choose a material, arrow, or weapon inventory entry.")
            }
            guard item.canEditValue else {
                throw CommandError.invalid("Unequip this weapon in the game before editing its durability.")
            }
            guard let value = Int(text.trimmingCharacters(in: .whitespaces)), (1...maximum).contains(value) else {
                throw CommandError.invalid(item.usesDurability ? "Enter raw durability from 1 to 100,000,000 (100 units = 1 point)." : "Enter a stack quantity from 1 to 999.")
            }
            return "quantity \(item.slot) \(value)"
        case .remove(let slot):
            guard (0...419).contains(slot) else { throw CommandError.invalid("Choose a valid inventory slot.") }
            return "remove \(slot)"
        case .teleport(let destination):
            try Self.validateIdentifier(destination)
            return "teleport \(destination)"
        case .unlockShrine(let shrine):
            try Self.validateIdentifier(shrine)
            return "unlock_shrine \(shrine)"
        case .unlockAllShrines: return "unlock_all_shrines"
        case .revealMap: return "reveal_map"
        case .runes: return "runes"
        case .maxHearts(let text):
            guard let value = Int(text), (1...30).contains(value) else { throw CommandError.invalid("Choose 1 to 30 hearts.") }
            return "max_hearts \(value)"
        case .maxStamina(let text):
            guard let value = Int(text), (1...3).contains(value) else { throw CommandError.invalid("Choose 1 to 3 stamina wheels.") }
            return "max_stamina \(value)"
        case .paraglider: return "paraglider"
        case .fullAccess: return "full_access"
        }
    }

    private static func validateIdentifier(_ value: String) throws {
        guard value.range(of: "^[A-Za-z0-9_-]{1,100}$", options: .regularExpression) != nil else {
            throw CommandError.invalid("Select a destination supplied by the running game.")
        }
    }
}

enum CommandError: LocalizedError {
    case invalid(String)
    var errorDescription: String? {
        switch self { case .invalid(let text): return text }
    }
}

struct TrainerConnection {
    static let freshnessLimit: TimeInterval = 15

    static func processExists(_ pid: Int32) -> Bool {
        pid > 1 && (kill(pid, 0) == 0 || errno == EPERM)
    }

    static func isFresh(_ modified: Date, now: Date = Date()) -> Bool {
        let age = now.timeIntervalSince(modified)
        return age >= -5 && age <= freshnessLimit
    }

    static func canUse(_ feature: String, state: TrainerState?, alive: Bool, fresh: Bool, inputExists: Bool) -> Bool {
        guard let state else { return false }
        return state.schema == 1 && state.version == "1.6.0" && state.ready && state.teleportBusy != true && alive && fresh && inputExists && state.features[feature] == true
    }

    static func canSetMaximum(_ feature: String, state: TrainerState?, alive: Bool, fresh: Bool, inputExists: Bool) -> Bool {
        canUse(feature,state:state,alive:alive,fresh:fresh,inputExists:inputExists) && state?.playerCacheReady != false
    }
}

#if !MODEL_CHECKS
import Combine

@MainActor
final class PlaygroundModel: ObservableObject {
    let catalog: [CatalogItem] = {
        guard let url = Bundle.main.url(forResource: "catalog", withExtension: "json"),
              let data = try? Data(contentsOf: url),
              let items = try? JSONDecoder().decode([CatalogItem].self, from: data) else { return [] }
        return items.sorted { $0.name.localizedStandardCompare($1.name) == .orderedAscending }
    }()
    @Published private(set) var state: TrainerState?
    @Published private(set) var connected = false
    @Published private(set) var available = false
    @Published private(set) var connectionMessage = "Start BOTW and load your game to connect."
    @Published private(set) var feedback = "No changes sent."
    @Published private(set) var feedbackIsError = false
    @Published private(set) var pending = false

    let folder = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/MacNativeGames/BotW", isDirectory: true)
    private var stateURL: URL { folder.appendingPathComponent("trainer-state.json") }
    private var inputURL: URL { folder.appendingPathComponent("input.txt") }
    private var timer: Timer?
    private var lastRequestID: UInt64 = 0
    private var waiting: (id: UInt64, started: Date)?
    private var requestedPID: Int32?
    private var processAlive = false
    private var stateFresh = false
    private var inputExists = false

    init() {
        refresh()
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }
    }

    func canUse(_ feature: String) -> Bool {
        !pending && TrainerConnection.canUse(feature, state: state, alive: processAlive, fresh: stateFresh, inputExists: inputExists)
    }

    func canSetMaximum(_ feature: String) -> Bool {
        !pending && TrainerConnection.canSetMaximum(feature,state:state,alive:processAlive,fresh:stateFresh,inputExists:inputExists)
    }

    func supports(_ feature: String) -> Bool { state?.features[feature] == true }

    func name(for actor: String) -> String { catalog.first(where: { $0.id == actor })?.name ?? actor }

    func refresh() {
        do {
            let data = try Data(contentsOf: stateURL)
            let next = try JSONDecoder().decode(TrainerState.self, from: data)
            let attributes = try FileManager.default.attributesOfItem(atPath: stateURL.path)
            let modified = attributes[.modificationDate] as? Date ?? .distantPast
            state = next
            processAlive = TrainerConnection.processExists(next.pid)
            stateFresh = TrainerConnection.isFresh(modified)
            inputExists = FileManager.default.fileExists(atPath: inputURL.path)
            connected = next.schema == 1 && next.version == "1.6.0" && processAlive && stateFresh && inputExists
            available = connected && next.ready && next.teleportBusy != true

            if next.schema != 1 || next.version != "1.6.0" {
                connectionMessage = "This editor requires the BOTW 1.6.0 editor runtime."
            } else if !processAlive {
                connectionMessage = "BOTW is closed. Start it and load your game to connect."
            } else if !stateFresh {
                connectionMessage = "Waiting for a fresh response from BOTW."
            } else if !inputExists {
                connectionMessage = "The game's command channel is not available yet."
            } else if next.teleportBusy == true {
                connectionMessage = "Teleporting — waiting for the game to finish loading."
            } else {
                connectionMessage = next.message.isEmpty ? (next.ready ? "Connected to BOTW 1.6.0" : "Load a gameplay session to use the editor.") : next.message
            }

            if let request = waiting {
                if next.lastRequest == request.id {
                    feedback = next.lastResult.isEmpty ? (next.lastOK ? "The game confirmed the change." : "The game rejected the request.") : next.lastResult
                    feedbackIsError = !next.lastOK
                    waiting = nil
                    pending = false
                } else if Date().timeIntervalSince(request.started) > 10 {
                    feedback = "No confirmation received. The change has not been verified."
                    feedbackIsError = true
                    waiting = nil
                    pending = false
                }
            }

            if processAlive && inputExists && requestedPID != next.pid && !pending {
                requestedPID = next.pid
                #if !UI_SNAPSHOT
                send(.status)
                #endif
            }
        } catch {
            state = nil
            connected = false
            available = false
            processAlive = false
            stateFresh = false
            inputExists = false
            connectionMessage = "Waiting for BOTW. Open the game and load your session to connect."
            if let request = waiting, Date().timeIntervalSince(request.started) > 10 {
                feedback = "The game did not confirm this request."
                feedbackIsError = true
                waiting = nil
                pending = false
            }
        }
    }

    func send(_ command: TrainerCommand) {
        do {
            let payload = try command.payload()
            guard let current = state, current.schema == 1, current.version == "1.6.0", TrainerConnection.processExists(current.pid), FileManager.default.fileExists(atPath: inputURL.path) else {
                throw CommandError.invalid("BOTW is not connected. Start the trainer-enabled game first.")
            }
            guard !pending else { return }
            if let feature = command.feature, !canUse(feature) {
                throw CommandError.invalid("This action is not available in the current gameplay session.")
            }
            if command.requiresPlayerCache, let feature = command.feature, !canSetMaximum(feature) {
                throw CommandError.invalid("Player stats are still loading. Wait for the gameplay session to be ready.")
            }
            if case .teleport(let id) = command,
               let destination = current.destinations?.first(where: { $0.id == id }), !destination.permitsTeleport {
                throw CommandError.invalid("Raise towers with Full Access, or activate the first tower in BOTW.")
            }
            let requestID = max(UInt64(Date().timeIntervalSince1970 * 1_000_000), lastRequestID + 1)
            lastRequestID = requestID
            let line = "trainer \(requestID) \(payload)\n"
            let handle = try FileHandle(forWritingTo: inputURL)
            defer { try? handle.close() }
            try handle.seekToEnd()
            try handle.write(contentsOf: Data(line.utf8))
            waiting = (requestID, Date())
            pending = true
            feedback = "Waiting for confirmation from the game…"
            feedbackIsError = false
        } catch {
            feedback = error.localizedDescription
            feedbackIsError = true
        }
    }
}
#endif
