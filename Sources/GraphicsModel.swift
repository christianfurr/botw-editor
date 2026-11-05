import Foundation

enum GraphicsPreset: String, Codable, CaseIterable, Identifiable {
    case off, natural, vivid, cinematic, custom
    var id: String { rawValue }
    var title: String { rawValue.capitalized }
    var detail: String {
        switch self {
        case .off: return "The original game image."
        case .natural: return "A little more color and clarity, preserving BOTW’s look."
        case .vivid: return "Richer colors and stronger contrast."
        case .cinematic: return "A softer palette with warm highlights."
        case .custom: return "Use your own Metal color shader."
        }
    }
}

struct GraphicsConfiguration: Codable {
    var schema = 1
    var revision: UInt64 = 0
    var preset: GraphicsPreset = .off
    var strength = 0.65
    var sharpness = 0.15

    func validate() throws {
        guard schema == 1, strength.isFinite, sharpness.isFinite,
              (0...1).contains(strength), (0...1).contains(sharpness),
              revision <= 9_007_199_254_740_991 else {
            throw GraphicsError.invalid("Choose effect strength and sharpness between 0 and 100%.")
        }
    }
}

struct GraphicsRuntimeStatus: Decodable {
    let schema: Int
    let pid: Int32
    let supported: Bool
    let revision: UInt64
    let requestedPreset: String
    let appliedPreset: String
    let strength: Double
    let sharpness: Double
    let error: String?
    let stage: String?

    enum CodingKeys: String, CodingKey {
        case schema, pid, supported, revision, strength, sharpness, error, stage
        case requestedPreset = "requested_preset"
        case appliedPreset = "applied_preset"
    }

    func confirms(_ configuration: GraphicsConfiguration) -> Bool {
        schema == 1 && supported && revision == configuration.revision &&
        requestedPreset == configuration.preset.rawValue &&
        appliedPreset == configuration.preset.rawValue &&
        (stage == "applied" || (configuration.preset == .off && stage == "off")) &&
        (error?.isEmpty != false) && strength.isFinite && sharpness.isFinite &&
        abs(strength-configuration.strength) < 0.0001 &&
        abs(sharpness-configuration.sharpness) < 0.0001
    }
}

enum GraphicsError: LocalizedError {
    case invalid(String)
    var errorDescription: String? { switch self { case .invalid(let message): return message } }
}

#if !MODEL_CHECKS
import Combine

@MainActor
final class GraphicsModel: ObservableObject {
    static let sampleShader = """
    // Color is the original SDR image. Return RGB in the 0…1 range.
    float3 customGrade(float3 color, float2 uv) {
        float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
        float3 richer = mix(float3(luminance), color, 1.08);
        return clamp((richer - 0.5) * 1.03 + 0.5, 0.0, 1.0);
    }
    """
    let folder = FileManager.default.homeDirectoryForCurrentUser
        .appendingPathComponent("Library/Application Support/BOTWEditor/shaders", isDirectory:true)
    @Published var configuration = GraphicsConfiguration()
    @Published var source = GraphicsModel.sampleShader
    @Published private(set) var runtimeAvailable = false
    @Published private(set) var message = "Start the updated BOTW app to apply saved effects."
    @Published private(set) var isError = false
    @Published private(set) var pending = false
    private var waiting: (configuration: GraphicsConfiguration, started: Date)?
    private var timer: Timer?

    init() {
        if let data = try? Data(contentsOf:folder.appendingPathComponent("settings.json")),
           let saved = try? JSONDecoder().decode(GraphicsConfiguration.self,from:data),
           (try? saved.validate()) != nil { configuration = saved }
        if let savedSource = try? String(contentsOf:folder.appendingPathComponent("custom.metal"),encoding:.utf8) { source = savedSource }
        refresh()
    }

    func start() {
        guard timer == nil else { return }
        refresh()
        timer = Timer.scheduledTimer(withTimeInterval:1,repeats:true) { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }
    }
    func stop() { timer?.invalidate(); timer = nil }

    func apply(original: Bool = false) {
        do {
            var request = configuration
            if original { request.preset = .off }
            request.revision = max(request.revision+1,UInt64(Date().timeIntervalSince1970*1_000_000))
            try request.validate()
            if request.preset == .custom {
                guard !source.trimmingCharacters(in:.whitespacesAndNewlines).isEmpty, source.utf8.count <= 65_536 else {
                    throw GraphicsError.invalid("Enter a Metal shader smaller than 64 KB.")
                }
            }
            try FileManager.default.createDirectory(at:folder,withIntermediateDirectories:true,attributes:[.posixPermissions:0o700])
            if request.preset == .custom {
                try Data(source.utf8).write(to:folder.appendingPathComponent("custom.metal"),options:.atomic)
                try FileManager.default.setAttributes([.posixPermissions:0o600],ofItemAtPath:folder.appendingPathComponent("custom.metal").path)
            }
            let encoder = JSONEncoder(); encoder.outputFormatting = [.prettyPrinted,.sortedKeys]
            let settings = folder.appendingPathComponent("settings.json")
            try encoder.encode(request).write(to:settings,options:.atomic)
            try FileManager.default.setAttributes([.posixPermissions:0o600],ofItemAtPath:settings.path)
            configuration = request
            waiting = (request,Date())
            pending = runtimeAvailable
            isError = false
            message = runtimeAvailable ? "Waiting for the game to apply this look…" : "Saved. Open the updated BOTW app to apply this look."
        } catch { isError = true; message = error.localizedDescription; pending = false }
    }

    func refresh() {
        let url = folder.appendingPathComponent("status.json")
        guard let data = try? Data(contentsOf:url),
              let status = try? JSONDecoder().decode(GraphicsRuntimeStatus.self,from:data),
              let attributes = try? FileManager.default.attributesOfItem(atPath:url.path),
              let modified = attributes[.modificationDate] as? Date,
              TrainerConnection.processExists(status.pid),
              TrainerConnection.isFresh(modified), status.schema == 1, status.supported else {
            runtimeAvailable = false
            pending = false
            if waiting == nil { message = "Start the updated BOTW app to apply saved effects."; isError = false }
            return
        }
        runtimeAvailable = true
        if let request = waiting {
            if status.revision == request.configuration.revision {
                waiting = nil
                pending = false
                if let error = status.error, !error.isEmpty {
                    message = "Original image restored. \(error)"; isError = true
                } else if status.confirms(request.configuration) {
                    message = status.appliedPreset == "off" ? "Original game image restored." : "\(request.configuration.preset.title) applied by the game."
                    isError = false
                } else { message = "The game has not confirmed the requested look."; isError = true }
            } else if Date().timeIntervalSince(request.started) > 15 {
                pending = false; waiting = nil
                message = "No confirmation received. The requested look has not been verified."; isError = true
            }
        } else if let error = status.error, !error.isEmpty {
            message = "Original image restored. \(error)"; isError = true
        } else if status.stage == "ready" {
            message = "Waiting for the game to draw with the saved look…"; isError = false
        } else {
            message = status.appliedPreset == "off" ? "Original game image is active." : "\(status.appliedPreset.capitalized) is active in the game."
            isError = false
        }
    }
}
#endif
