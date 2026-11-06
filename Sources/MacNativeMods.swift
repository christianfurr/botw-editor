import AppKit
import SwiftUI
import UniformTypeIdentifiers

struct ModTarget: Hashable, Identifiable, Sendable {
    let url: URL
    let bundleID: String
    let name: String
    let dataFolder: String
    let version: String

    var id: URL { url }

    init?(_ url: URL) {
        let path = url.standardizedFileURL.resolvingSymlinksInPath()
        guard let bundle = Bundle(url: path), let id = bundle.bundleIdentifier,
              let version = bundle.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String else { return nil }
        switch id {
        case "org.macnativegames.botw": (name, dataFolder) = ("Breath of the Wild", "BotW")
        case "org.macnativegames.totk": (name, dataFolder) = ("Tears of the Kingdom", "TOTK")
        case "org.macnativegames.ot0": (name, dataFolder) = ("Octopath Traveler 0", "OT0")
        default: return nil
        }
        self.url = path
        self.bundleID = id
        self.version = version
    }
}

@main
struct MacNativeModsApp: App {
    var body: some Scene {
        WindowGroup("MacNative Mods") { ModManagerWindow() }
            .defaultSize(width: 1100, height: 680)
            .windowResizability(.contentMinSize)
    }
}

private struct ModManagerWindow: View {
    @AppStorage("checkout") private var checkout = ""
    @AppStorage("selectedGame") private var selectedPath = ""
    @AppStorage("addedGames") private var addedGames = ""
    @State private var games: [ModTarget] = []
    @State private var selection: URL?
    @State private var selectionError = false

    var body: some View {
        NavigationSplitView {
            List(selection: $selection) {
                Section("Games") {
                    ForEach(games) { game in
                        VStack(alignment: .leading, spacing: 3) {
                            Text(game.name)
                            Text(game.version).font(.caption).foregroundStyle(.secondary)
                        }
                        .tag(game.id)
                    }
                }
            }
            .navigationSplitViewColumnWidth(min: 200, ideal: 220)
            .safeAreaInset(edge: .bottom) {
                Button("Add Game…", systemImage: "plus") { chooseGame() }
                    .padding(10)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
        } detail: {
            if let game = games.first(where: { $0.id == selection }) {
                GameLibrary(game: game, checkout: checkout)
                    .id([game.url.path, checkout])
            } else {
                ContentUnavailableView("Select a game", systemImage: "shippingbox",
                                       description: Text("Add a MacNativeGames app to manage its RomFS mods."))
            }
        }
        .frame(minWidth: 940, minHeight: 530)
        .toolbar {
            Button("MacNativeGames Folder…", systemImage: "folder") { chooseCheckout() }
                .help(checkout.isEmpty ? "Use the checkout from BOTW setup or select a folder." : checkout)
        }
        .task { scan() }
        .onChange(of: selection) { _, url in if let url { selectedPath = url.path } }
        .alert("Choose a supported game", isPresented: $selectionError) {
            Button("OK", role: .cancel) {}
        } message: {
            Text("Select a MacNativeGames build of Breath of the Wild, Tears of the Kingdom, or Octopath Traveler 0.")
        }
    }

    private func scan() {
        let folders = [URL(fileURLWithPath: "/Applications"),
                       FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent("Applications")]
        let localApps = folders.flatMap {
            (try? FileManager.default.contentsOfDirectory(at: $0, includingPropertiesForKeys: nil)) ?? []
        }
        let chosen = addedGames.split(separator: "\n").map { URL(fileURLWithPath: String($0)) }
        games = Array(Set((localApps.filter { $0.pathExtension == "app" } + chosen).compactMap(ModTarget.init)))
            .sorted { ($0.name, $0.version, $0.url.path) < ($1.name, $1.version, $1.url.path) }
        selection = games.first { $0.url.path == selectedPath }?.id ?? games.first?.id
    }

    private func chooseGame() {
        let panel = NSOpenPanel()
        panel.allowedContentTypes = [.application]
        panel.prompt = "Add Game"
        guard panel.runModal() == .OK, let url = panel.url else { return }
        guard let game = ModTarget(url), !game.url.path.contains("\n") else {
            selectionError = true
            return
        }
        if !addedGames.split(separator: "\n").contains(Substring(game.url.path)) {
            addedGames += game.url.path + "\n"
        }
        selectedPath = game.url.path
        scan()
    }

    private func chooseCheckout() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.prompt = "Choose Folder"
        if panel.runModal() == .OK, let url = panel.url { checkout = url.path }
    }
}

private struct GameLibrary: View {
    @StateObject private var model: ModsModel

    init(game: ModTarget, checkout: String) {
        _model = StateObject(wrappedValue: ModsModel(target: game, checkout: checkout))
    }

    var body: some View { ModsView(model: model) }
}
