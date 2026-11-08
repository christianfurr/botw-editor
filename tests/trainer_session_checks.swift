import Foundation
import Darwin

@main
struct TrainerSessionChecks {
    @MainActor
    static func main() throws {
        let fm = FileManager.default
        let folder = fm.temporaryDirectory.appendingPathComponent(UUID().uuidString, isDirectory: true)
        try fm.createDirectory(at: folder, withIntermediateDirectories: false)
        defer { try? fm.removeItem(at: folder) }
        let input = folder.appendingPathComponent("input.txt")
        let state = folder.appendingPathComponent("trainer-state.json")
        try Data().write(to: input)

        func publish(pid: Int32, request: UInt64 = 0, stale: Bool = false) throws {
            let json: [String: Any] = [
                "schema": 1, "pid": pid, "version": "1.6.0", "ready": true, "message": "",
                "health_lock": false, "stamina_lock": false, "last_request": request,
                "last_ok": true, "last_result": "Verified rupees", "features": ["rupees": true],
            ]
            try JSONSerialization.data(withJSONObject: json).write(to: state, options: .atomic)
            if stale {
                try fm.setAttributes([.modificationDate: Date(timeIntervalSinceNow: -60)], ofItemAtPath: state.path)
            }
        }

        func commands() throws -> [Substring] {
            try String(contentsOf: input, encoding: .utf8).split(separator: "\n")
        }

        func lastRequest() throws -> UInt64 {
            UInt64(try commands().last!.split(separator: " ")[1])!
        }

        let first = getpid()
        try publish(pid: first)
        let model = PlaygroundModel(folder: folder)
        let initialCommands = try commands()
        precondition(model.available && initialCommands.isEmpty, "Connecting should be read-only")
        model.send(.rupees("20"))
        precondition(model.pending)
        let firstRequest = try lastRequest()

        let other = Process()
        other.executableURL = URL(fileURLWithPath: "/bin/sleep")
        other.arguments = ["30"]
        try other.run()
        defer { if other.isRunning { other.terminate() } }
        let second = other.processIdentifier
        try publish(pid: second, request: firstRequest)
        model.refresh()
        precondition(!model.pending && model.feedbackIsError && model.feedback.contains("restarted"))
        try publish(pid: second)
        model.refresh()
        let afterReconnect = try commands()
        precondition(model.available && model.feedbackIsError && afterReconnect.count == 1)

        model.send(.rupees("30"))
        let secondRequest = try lastRequest()
        try publish(pid: second, request: secondRequest, stale: true)
        model.refresh()
        precondition(model.pending && !model.connected, "A stale matching request cannot confirm a change")
        try publish(pid: second, request: secondRequest)
        model.refresh()
        precondition(!model.pending && !model.feedbackIsError && model.feedback == "Verified rupees")

        model.send(.rupees("40"))
        other.terminate()
        other.waitUntilExit()
        try fm.removeItem(at: state)
        model.refresh()
        precondition(!model.pending && !model.connected && model.feedbackIsError && model.feedback.contains("closed"))
        print("Trainer session checks passed: read-only connect, PID-bound acknowledgement, stale reply, fresh reply, and game exit.")
    }
}
