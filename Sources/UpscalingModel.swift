import Foundation

enum UpscalingPreset: String, CaseIterable, Identifiable {
    case off
    case quality1440 = "quality-1440"
    case balanced1440 = "balanced-1440"
    case quality2160 = "quality-2160"
    case balanced2160 = "balanced-2160"
    var id: String { rawValue }
    var title: String {
        switch self {
        case .off: return "Off — original scaling"
        case .quality1440: return "Quality — 1440p"
        case .balanced1440: return "Balanced — 1440p"
        case .quality2160: return "Quality — 4K"
        case .balanced2160: return "Balanced — 4K"
        }
    }
    var dimensions: (inputWidth: Int, inputHeight: Int, outputWidth: Int, outputHeight: Int)? {
        switch self {
        case .off: return nil
        case .quality1440: return (1920, 1080, 2560, 1440)
        case .balanced1440: return (1280, 720, 2560, 1440)
        case .quality2160: return (2560, 1440, 3840, 2160)
        case .balanced2160: return (1920, 1080, 3840, 2160)
        }
    }
    var nextLaunchDetail: String {
        guard let d = dimensions else { return "Use the port’s original scaling path on the next launch." }
        return "Next launch: render \(d.inputWidth) × \(d.inputHeight) → output \(d.outputWidth) × \(d.outputHeight)."
    }
    static func parse(_ text: String) throws -> Self {
        var token = text
        if token.hasSuffix("\r\n") { token.removeLast() }
        else if token.hasSuffix("\n") { token.removeLast() }
        guard token.utf8.count <= 32, let preset = Self(rawValue: token) else {
            throw UpscalingError.invalid("The saved upscaling setting is invalid. Choose a preset and save it again.")
        }
        return preset
    }
}

enum UpscalingError: LocalizedError {
    case invalid(String)
    var errorDescription: String? { switch self { case .invalid(let text): return text } }
}

enum UpscalingPreference {
    static func read(from url: URL) throws -> UpscalingPreset? {
        guard FileManager.default.fileExists(atPath: url.path) else { return nil }
        let handle = try FileHandle(forReadingFrom: url)
        defer { try? handle.close() }
        let data = try handle.read(upToCount: 65) ?? Data()
        guard data.count <= 64, let text = String(data: data, encoding: .utf8) else {
            throw UpscalingError.invalid("The saved upscaling setting is invalid. Choose a preset and save it again.")
        }
        return try UpscalingPreset.parse(text)
    }
    static func write(_ preset: UpscalingPreset, to url: URL) throws {
        try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true, attributes: [.posixPermissions: 0o700])
        try Data((preset.rawValue + "\n").utf8).write(to: url, options: .atomic)
        try FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: url.path)
    }
}

struct UpscalingRuntimeStatus: Decodable {
    let schema: Int
    let pid: Int32
    let version: String
    let requested: Bool
    let mode: String
    let encoded: UInt64
    let gpuCompleted: UInt64
    let gpuErrors: UInt64
    let inputWidth: Int
    let inputHeight: Int
    let outputWidth: Int
    let outputHeight: Int
    let historyResets: UInt64
    let missingInputs: UInt64
    let fallbackReason: String
    let motionSource: String
    let updatedAt: Double
    let experimental: Bool
    let savedPreset: String?
    let capable: Bool?
    let configurationValid: Bool?
    let backendActive: Bool?

    enum CodingKeys: String, CodingKey {
        case schema, pid, version, requested, mode, encoded, experimental, capable
        case gpuCompleted = "gpu_completed", gpuErrors = "gpu_errors"
        case inputWidth = "input_width", inputHeight = "input_height"
        case outputWidth = "output_width", outputHeight = "output_height"
        case historyResets = "history_resets", missingInputs = "missing_inputs"
        case fallbackReason = "fallback_reason", motionSource = "motion_source"
        case updatedAt = "updated_at", savedPreset = "saved_preset"
        case configurationValid = "config_valid", backendActive = "active"
    }
    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        schema = try c.decode(Int.self, forKey: .schema)
        pid = try c.decode(Int32.self, forKey: .pid)
        version = try c.decode(String.self, forKey: .version)
        if let value = try? c.decode(Bool.self, forKey: .requested) { requested = value }
        else {
            let value = try c.decode(Int.self, forKey: .requested)
            guard value == 0 || value == 1 else { throw DecodingError.dataCorruptedError(forKey: .requested, in: c, debugDescription: "Expected Boolean or 0/1") }
            requested = value == 1
        }
        mode = try c.decode(String.self, forKey: .mode)
        encoded = try c.decode(UInt64.self, forKey: .encoded)
        gpuCompleted = try c.decode(UInt64.self, forKey: .gpuCompleted)
        gpuErrors = try c.decode(UInt64.self, forKey: .gpuErrors)
        inputWidth = try c.decode(Int.self, forKey: .inputWidth)
        inputHeight = try c.decode(Int.self, forKey: .inputHeight)
        outputWidth = try c.decode(Int.self, forKey: .outputWidth)
        outputHeight = try c.decode(Int.self, forKey: .outputHeight)
        historyResets = try c.decodeIfPresent(UInt64.self, forKey: .historyResets) ?? 0
        missingInputs = try c.decodeIfPresent(UInt64.self, forKey: .missingInputs) ?? 0
        fallbackReason = try c.decodeIfPresent(String.self, forKey: .fallbackReason) ?? ""
        motionSource = try c.decodeIfPresent(String.self, forKey: .motionSource) ?? ""
        updatedAt = try c.decode(Double.self, forKey: .updatedAt)
        experimental = try c.decodeIfPresent(Bool.self, forKey: .experimental) ?? true
        savedPreset = try c.decodeIfPresent(String.self, forKey: .savedPreset)
        capable = try c.decodeIfPresent(Bool.self, forKey: .capable)
        configurationValid = try c.decodeIfPresent(Bool.self, forKey: .configurationValid)
        backendActive = try c.decodeIfPresent(Bool.self, forKey: .backendActive)
    }
    func isFresh(processAlive: Bool, modified: Date, now: Date = Date()) -> Bool {
        let age = now.timeIntervalSince1970 - updatedAt
        let fileAge = now.timeIntervalSince(modified)
        return schema == 1 && version == "1.6.0" && pid > 1 && processAlive && updatedAt.isFinite &&
            ["temporal_upscale", "fallback", "off", "original"].contains(mode) &&
            age >= -5 && age <= 15 && fileAge >= -5 && fileAge <= 15
    }
    var dimensionsValid: Bool {
        [inputWidth, inputHeight, outputWidth, outputHeight].allSatisfy { (1...16_384).contains($0) }
    }
    func isActive(processAlive: Bool, modified: Date, now: Date = Date()) -> Bool {
        isFresh(processAlive: processAlive, modified: modified, now: now) && requested &&
            capable != false && configurationValid != false && backendActive != false &&
            mode == "temporal_upscale" && gpuCompleted > 0 && encoded >= gpuCompleted && gpuErrors == 0 &&
            dimensionsValid && outputWidth > inputWidth && outputHeight > inputHeight && fallbackReason.isEmpty
    }
    func confirms(_ preset: UpscalingPreset, afterPID: Int32?, processAlive: Bool, modified: Date, now: Date = Date()) -> Bool {
        guard isFresh(processAlive: processAlive, modified: modified, now: now),
              configurationValid != false, afterPID != pid, savedPreset == preset.rawValue else { return false }
        guard let d = preset.dimensions else { return !requested && ["fallback", "off", "original"].contains(mode) }
        return isActive(processAlive: processAlive, modified: modified, now: now) &&
            inputWidth == d.inputWidth && inputHeight == d.inputHeight &&
            outputWidth == d.outputWidth && outputHeight == d.outputHeight
    }
    var dimensionsDescription: String {
        guard dimensionsValid else { return "Waiting for actual render dimensions." }
        return "Render \(inputWidth) × \(inputHeight) → output \(outputWidth) × \(outputHeight)"
    }
}

#if !MODEL_CHECKS
import Combine

@MainActor
final class UpscalingModel: ObservableObject {
    @Published var selection: UpscalingPreset = .off
    @Published private(set) var savedPreset: UpscalingPreset?
    @Published private(set) var status: UpscalingRuntimeStatus?
    @Published private(set) var runtimeAvailable = false
    @Published private(set) var temporalActive = false
    @Published private(set) var waitingForRestart = false
    @Published private(set) var message = "Choose how BOTW scales its next game session."
    @Published private(set) var isError = false
    private var savedAfterPID: Int32?
    private var lastObservedPID: Int32?
    private var timer: Timer?
    let preferenceURL = FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/MacNativeGames/BotW/graphics-upscaler.txt")
    private let statusURLs = ["upscaling", "temporal"].map {
        FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Library/Application Support/BOTWEditor/\($0)/status.json")
    }

    init() {
        do {
            savedPreset = try UpscalingPreference.read(from: preferenceURL)
            selection = savedPreset ?? .off
        } catch { message = error.localizedDescription; isError = true }
        refresh()
    }
    func start() {
        guard timer == nil else { return }
        refresh()
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }
    }
    func stop() { timer?.invalidate(); timer = nil }
    func saveForNextLaunch() {
        do {
            refresh()
            try UpscalingPreference.write(selection, to: preferenceURL)
            savedAfterPID = lastObservedPID
            savedPreset = selection
            waitingForRestart = true
            isError = false
            message = "Saved. Quit and reopen BOTW to use \(selection.title)."
        } catch { message = error.localizedDescription; isError = true }
    }
    func refresh() {
        var records: [(value: UpscalingRuntimeStatus, modified: Date, alive: Bool)] = []
        for url in statusURLs {
            guard let data = try? Data(contentsOf: url),
                  let value = try? JSONDecoder().decode(UpscalingRuntimeStatus.self, from: data),
                  let attributes = try? FileManager.default.attributesOfItem(atPath: url.path),
                  let modified = attributes[.modificationDate] as? Date else { continue }
            records.append((value, modified, TrainerConnection.processExists(value.pid)))
        }
        // Prefer the installed backend. The earlier isolated prototype used the
        // second path; only a live, fresh process can supply an active fallback.
        guard let record = records.first(where: { $0.value.isFresh(processAlive: $0.alive, modified: $0.modified) }) ?? records.first(where: { $0.alive }) ?? records.first else {
            status = nil; runtimeAvailable = false; temporalActive = false
            return
        }
        let next = record.value, modified = record.modified, alive = record.alive
        if alive { lastObservedPID = next.pid }
        runtimeAvailable = next.isFresh(processAlive: alive, modified: modified)
        temporalActive = next.isActive(processAlive: alive, modified: modified)
        status = runtimeAvailable ? next : nil
        // Reopening the companion must not hide a saved boot preference that
        // differs from the still-running game's immutable boot token.
        if runtimeAvailable, !waitingForRestart, let savedPreset,
           let bootToken = next.savedPreset, bootToken != savedPreset.rawValue {
            savedAfterPID = next.pid
            waitingForRestart = true
            isError = false
            message = "Saved. Quit and reopen BOTW to use \(savedPreset.title)."
        }
        if waitingForRestart, let savedPreset,
           next.confirms(savedPreset, afterPID: savedAfterPID, processAlive: alive, modified: modified) {
            waitingForRestart = false
            message = savedPreset == .off ? "Original scaling confirmed in the new game session." : "\(savedPreset.title) confirmed in the new game session."
        }
    }
    var headline: String {
        if waitingForRestart { return "Saved — restart BOTW" }
        if temporalActive { return "MetalFX Temporal is active" }
        guard let status, runtimeAvailable else { return "Waiting for an updated BOTW session" }
        if !status.requested && status.mode != "temporal_upscale" { return "Original scaling is active" }
        if status.mode == "temporal_upscale", status.gpuCompleted == 0 { return "Waiting for completed temporal frames" }
        return "Original scaling fallback"
    }
    var detail: String {
        guard let status, runtimeAvailable else { return "Saved settings take effect when BOTW is restarted." }
        if waitingForRestart { return "Current session: \(status.dimensionsDescription.lowercased()). The new setting is not active yet." }
        if temporalActive { return status.dimensionsDescription }
        return status.fallbackReason.isEmpty ? status.dimensionsDescription : status.fallbackReason
    }
}
#endif
