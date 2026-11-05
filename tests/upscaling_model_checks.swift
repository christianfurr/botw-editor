import Foundation

@main
struct UpscalingModelChecks {
    static func main() throws {
        var count = 0
        func check(_ value: Bool, _ message: String) { precondition(value, message); count += 1 }
        func reject(_ text: String) {
            do { _ = try UpscalingPreset.parse(text); preconditionFailure("Invalid token accepted: \(text)") }
            catch { count += 1 }
        }
        for preset in UpscalingPreset.allCases {
            check(try UpscalingPreset.parse(preset.rawValue) == preset, "Canonical token")
            check(try UpscalingPreset.parse(preset.rawValue + "\n") == preset, "File newline")
            check(try UpscalingPreset.parse(preset.rawValue + "\r\n") == preset, "CRLF newline")
            if let d = preset.dimensions {
                check(d.outputWidth > d.inputWidth && d.outputHeight > d.inputHeight, "Preset increases dimensions")
                check(d.outputWidth <= 3840 && d.outputHeight <= 2160, "Preset output bounds")
            }
        }
        for bad in ["", "quality", "QUALITY-1440", "quality-1440 extra", " quality-1440", "off\noff", "off\n\n", "balanced-4320", "quality-2160\t", String(repeating: "x", count: 100)] { reject(bad) }
        check(UpscalingPreset.quality1440.dimensions!.inputWidth == 1920, "Quality 1440 input")
        check(UpscalingPreset.balanced1440.dimensions!.inputWidth == 1280, "Balanced 1440 input")
        check(UpscalingPreset.quality2160.dimensions!.inputWidth == 2560, "Quality 2160 input")
        check(UpscalingPreset.balanced2160.dimensions!.inputWidth == 1920, "Balanced 2160 input")

        let now = Date(timeIntervalSince1970: 1000)
        let base: [String: Any] = [
            "schema": 1, "pid": 42, "version": "1.6.0", "requested": 1,
            "mode": "temporal_upscale", "encoded": 743, "gpu_completed": 742, "gpu_errors": 0,
            "input_width": 1920, "input_height": 1080, "output_width": 2560, "output_height": 1440,
            "history_resets": 1, "missing_inputs": 0, "fallback_reason": "",
            "motion_source": "camera reprojection plus depth-reactive mask", "updated_at": 1000,
            "experimental": true, "saved_preset": "quality-1440"
        ]
        func decode(_ changes: [String: Any] = [:]) throws -> UpscalingRuntimeStatus {
            var values = base
            for (key, value) in changes { values[key] = value }
            return try JSONDecoder().decode(UpscalingRuntimeStatus.self, from: JSONSerialization.data(withJSONObject: values))
        }
        let active = try decode()
        check(active.isActive(processAlive: true, modified: now, now: now), "Completed real upscale active")
        check(try decode(["requested": true]).requested, "Boolean request decoding")
        check(!active.isActive(processAlive: false, modified: now, now: now), "Dead process not active")
        check(!active.isActive(processAlive: true, modified: now.addingTimeInterval(-16), now: now), "Stale file not active")
        check(!active.isActive(processAlive: true, modified: now.addingTimeInterval(6), now: now), "Future file not active")
        for changes: [String: Any] in [
            ["requested": 0], ["mode": "fallback"], ["mode": "unknown"], ["gpu_completed": 0],
            ["gpu_errors": 1], ["encoded": 741], ["output_width": 1920], ["output_height": 1080],
            ["input_width": 0], ["output_width": 20000], ["fallback_reason": "missing motion"],
            ["updated_at": 984], ["updated_at": 1006], ["schema": 2], ["version": "1.9.0"], ["pid": 0],
            ["capable": false], ["config_valid": false], ["active": false]
        ] {
            let status = try decode(changes)
            check(!status.isActive(processAlive: true, modified: now, now: now), "Invalid active evidence: \(changes)")
        }
        for changes: [String: Any] in [["requested": 2], ["gpu_completed": -1], ["pid": "bad"]] {
            do { _ = try decode(changes); preconditionFailure("Invalid status decoded") }
            catch { count += 1 }
        }
        check(!active.confirms(.quality1440, afterPID: 42, processAlive: true, modified: now, now: now), "Same PID never confirms saved restart")
        check(active.confirms(.quality1440, afterPID: 41, processAlive: true, modified: now, now: now), "New PID with matching preset and actual dimensions")
        check(active.confirms(.quality1440, afterPID: nil, processAlive: true, modified: now, now: now), "Fresh first launch can confirm")
        check(!active.confirms(.balanced1440, afterPID: 41, processAlive: true, modified: now, now: now), "Different preset not confirmed")
        check(!active.confirms(.quality1440, afterPID: 41, processAlive: false, modified: now, now: now), "Dead new PID not confirmed")
        check(!(try decode(["saved_preset": "balanced-1440"])).confirms(.quality1440, afterPID: 41, processAlive: true, modified: now, now: now), "Boot token mismatch")
        check(!(try decode(["input_width": 1280])).confirms(.quality1440, afterPID: 41, processAlive: true, modified: now, now: now), "Dimensions must match saved mode")
        check(!(try decode(["gpu_completed": 0])).confirms(.quality1440, afterPID: 41, processAlive: true, modified: now, now: now), "Encoding alone does not confirm")
        let off = try decode(["requested": false, "mode": "fallback", "saved_preset": "off", "encoded": 0, "gpu_completed": 0])
        check(!off.isActive(processAlive: true, modified: now, now: now), "Off cannot look temporal")
        check(off.confirms(.off, afterPID: 41, processAlive: true, modified: now, now: now), "New Off launch confirmed without temporal work")
        check(!off.confirms(.off, afterPID: 42, processAlive: true, modified: now, now: now), "Off still requires restart")
        let invalidOff = try decode(["requested": false, "mode": "fallback", "saved_preset": "off", "config_valid": false])
        check(!invalidOff.confirms(.off, afterPID: 41, processAlive: true, modified: now, now: now), "Invalid config fallback does not confirm a selected mode")

        let folder = FileManager.default.temporaryDirectory.appendingPathComponent("botw-upscale-checks-" + UUID().uuidString, isDirectory: true)
        let file = folder.appendingPathComponent("graphics-upscaler.txt")
        defer { try? FileManager.default.removeItem(at: folder) }
        check(try UpscalingPreference.read(from: file) == nil, "Missing preference is read-only")
        check(!FileManager.default.fileExists(atPath: folder.path), "Reading does not create settings")
        for preset in UpscalingPreset.allCases {
            try UpscalingPreference.write(preset, to: file)
            check(try UpscalingPreference.read(from: file) == preset, "Atomic preference round trip")
            check(try String(contentsOf: file, encoding: .utf8) == preset.rawValue + "\n", "Canonical one-line save")
        }
        try Data("off\nquality-1440\n".utf8).write(to: file)
        do { _ = try UpscalingPreference.read(from: file); preconditionFailure("Injected preference accepted") }
        catch { count += 1 }
        try Data(repeating: 65, count: 100_000).write(to: file)
        do { _ = try UpscalingPreference.read(from: file); preconditionFailure("Oversized preference accepted") }
        catch { count += 1 }
        print("Upscaling model checks passed: \(count) assertions; strict presets, genuine active evidence, restart/PID confirmation, and atomic preference round trips.")
    }
}
