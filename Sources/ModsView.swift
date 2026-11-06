import AppKit
import SwiftUI

struct ModsView: View {
    @ObservedObject var model: ModsModel
    @State private var selectedID: String?
    @State private var removal: ManagedMod?
    @FocusState private var focusedID: String?

    private let bcmlGuide = URL(string: "https://github.com/NiceneNerd/BCML/blob/master/docs/export.md")!
    private var selected: ManagedMod? { model.mods.first { $0.id == selectedID } }

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            header
            HStack(spacing: 0) {
                library.frame(width: 282)
                Rectangle().fill(ModTint.line).frame(width: 1)
                inspector.frame(maxWidth: .infinity)
            }
            .frame(maxHeight: .infinity)
            .background(ModTint.panel)
            .overlay { Rectangle().stroke(ModTint.line, lineWidth: 1).allowsHitTesting(false) }

            if model.target.dataFolder == "BotW" {
                HStack(spacing: 10) {
                    Text("BCML: Export a merged Switch ZIP or 7z, then import it here.")
                        .foregroundStyle(ModTint.muted)
                    Link("BCML export guide", destination: bcmlGuide).foregroundStyle(ModTint.cyan)
                }.font(.system(size: 11))
            }
            footer
        }
        .padding(16)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .task { model.refresh(keepMessage: true) }
        .onChange(of: model.mods.map(\.id), initial: true) { old, new in
            if let imported = new.first(where: { !old.contains($0) }) {
                selectedID = imported
            } else if !new.contains(where: { $0 == selectedID }) {
                selectedID = new.first
            }
        }
        .onReceive(NSWorkspace.shared.notificationCenter.publisher(for: NSWorkspace.didLaunchApplicationNotification)) { refreshGameState($0) }
        .onReceive(NSWorkspace.shared.notificationCenter.publisher(for: NSWorkspace.didTerminateApplicationNotification)) { refreshGameState($0) }
        .alert(item: $removal) { mod in
            Alert(title: Text("Remove \(mod.name)?"),
                  message: Text("Remove the editor’s imported copy. Apply to prepare the updated mod set for the next launch."),
                  primaryButton: .destructive(Text("Remove copy")) { model.remove(mod) },
                  secondaryButton: .cancel())
        }
    }

    private var header: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 5) {
                Text(model.target.name).font(.system(size: 26, weight: .medium))
                Text("Version \(model.target.version) · Switch RomFS")
                    .font(.system(size: 12)).foregroundStyle(ModTint.muted)
            }
            Spacer()
            Label(gameLabel, systemImage: model.status?.gameRunning == true ? "play.circle" : "power")
                .font(.system(size: 11)).foregroundStyle(model.status?.gameRunning == true ? ModTint.gold : ModTint.muted)
            Button { model.refresh() } label: { Image(systemName: "arrow.clockwise") }
                .buttonStyle(.bordered).disabled(model.busy)
                .help("Refresh mod library and game status").accessibilityLabel("Refresh mod library")
            if model.status?.canMakeDemos == true {
                Button("Add demo mods") { model.addDemos() }
                    .buttonStyle(.bordered).disabled(model.busy)
            }
            Button { chooseImport() } label: { Label("Import…", systemImage: "tray.and.arrow.down") }
                .buttonStyle(.borderedProminent).disabled(model.busy)
                .help("Import a Switch RomFS folder, ZIP or 7z")
        }
    }

    private var gameLabel: String {
        guard model.current, let running = model.status?.gameRunning else { return "Game status unavailable" }
        return running ? "Game running" : "Game closed"
    }

    private var library: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack(alignment: .firstTextBaseline) {
                VStack(alignment: .leading, spacing: 5) {
                    Text("Load order").font(.system(size: 17, weight: .medium))
                    Text("Later enabled mods win shared files.")
                        .font(.system(size: 11)).foregroundStyle(ModTint.muted)
                }
                Spacer()
                Text("\(model.mods.filter(\.enabled).count) enabled")
                    .font(.system(size: 10)).monospacedDigit().foregroundStyle(ModTint.muted)
            }.padding(16)
            Rectangle().fill(ModTint.line).frame(height: 1)

            if model.mods.isEmpty {
                placeholder(model.current ? "No mods imported" : "Mod library",
                            detail: model.current ? "Import a folder or archive, or add the demos." : (model.activity ?? "Refresh to check your mod library."),
                            icon: "shippingbox")
            } else {
                ScrollView {
                    LazyVStack(spacing: 4) {
                        ForEach(Array(model.mods.enumerated()), id: \.element.id) { position, mod in
                            row(mod, position: position + 1)
                        }
                    }.padding(9)
                }
            }
        }.frame(maxHeight: .infinity, alignment: .topLeading)
    }

    private func row(_ mod: ManagedMod, position: Int) -> some View {
        let selected = selectedID == mod.id
        let focused = focusedID == mod.id
        return HStack(spacing: 9) {
            Button { selectedID = mod.id } label: {
                HStack(spacing: 9) {
                    Text("\(position)").font(.system(size: 10, weight: .medium)).monospacedDigit()
                        .foregroundStyle(ModTint.muted).frame(width: 17, alignment: .trailing)
                    Image(systemName: "shippingbox").foregroundStyle(selected ? ModTint.cyan : ModTint.muted)
                    VStack(alignment: .leading, spacing: 3) {
                        Text(mod.name).font(.system(size: 14, weight: .medium)).lineLimit(2)
                        Text(mod.valid ? (mod.enabled ? "Enabled" : "Disabled") : "Compatibility issue")
                            .font(.system(size: 10)).foregroundStyle(mod.valid ? ModTint.muted : ModTint.gold)
                    }
                    Spacer(minLength: 0)
                }.frame(maxWidth: .infinity, minHeight: 41, alignment: .leading).contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .focused($focusedID, equals: mod.id)
            .accessibilityLabel("\(mod.name), load order \(position)")
            .accessibilityAddTraits(selected ? .isSelected : [])

            Toggle("Enable \(mod.name)", isOn: Binding(get: { mod.enabled }, set: { model.toggle(mod, enabled: $0) }))
                .labelsHidden().toggleStyle(.switch).controlSize(.small)
                .disabled(!model.canEdit || (!mod.valid && !mod.enabled))
                .help(mod.valid ? "Include \(mod.name) in the next mod set" : mod.detail)
        }
        .padding(.horizontal, 8)
        .background(selected ? ModTint.raised : Color.clear, in: RoundedRectangle(cornerRadius: 4))
        .overlay {
            RoundedRectangle(cornerRadius: 4).stroke(focused ? ModTint.ivory : selected ? ModTint.cyan : Color.clear, lineWidth: 1)
                .allowsHitTesting(false)
        }
    }

    @ViewBuilder private var inspector: some View {
        if let mod = selected, let position = model.mods.firstIndex(where: { $0.id == mod.id }) {
            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    VStack(alignment: .leading, spacing: 7) {
                        Text(mod.name).font(.system(size: 24, weight: .medium))
                            .fixedSize(horizontal: false, vertical: true)
                        Label(mod.valid ? "RomFS files validated" : "Compatibility issue",
                              systemImage: mod.valid ? "checkmark.circle" : "exclamationmark.circle")
                            .font(.system(size: 11)).foregroundStyle(mod.valid ? ModTint.cyan : ModTint.gold)
                        if !mod.detail.isEmpty {
                            Text(mod.detail).font(.system(size: 12)).foregroundStyle(ModTint.muted)
                                .fixedSize(horizontal: false, vertical: true).textSelection(.enabled)
                        }
                    }
                    Rectangle().fill(ModTint.line).frame(height: 1)
                    HStack(spacing: 22) {
                        metric("Replacements", value: mod.replaced)
                        metric("Additions", value: mod.added)
                        metric("Overlaps", value: mod.conflicts.count)
                    }
                    HStack(spacing: 9) {
                        Button { model.move(mod, direction: .up) } label: { Label("Move up", systemImage: "arrow.up") }
                            .buttonStyle(.bordered).disabled(!model.canEdit || position == 0)
                        Button { model.move(mod, direction: .down) } label: { Label("Move down", systemImage: "arrow.down") }
                            .buttonStyle(.bordered).disabled(!model.canEdit || position == model.mods.count - 1)
                        Spacer(minLength: 0)
                        Button { removal = mod } label: { Label("Remove…", systemImage: "trash") }
                            .buttonStyle(.bordered).disabled(!model.canEdit)
                    }
                    if !mod.conflicts.isEmpty { overlaps(mod.conflicts) }
                }.padding(20).frame(maxWidth: .infinity, alignment: .leading)
            }.frame(maxHeight: .infinity, alignment: .topLeading)
        } else {
            placeholder("Mod details", detail: "Select a pack to check its files and change its priority.", icon: "doc.text.magnifyingglass")
        }
    }

    private func overlaps(_ paths: [String]) -> some View {
        var winners: [String: String] = [:]
        for mod in model.mods where mod.enabled {
            for path in mod.conflicts { winners[path] = mod.name }
        }
        return LazyVStack(alignment: .leading, spacing: 8) {
            Text("Shared files").font(.system(size: 14, weight: .medium))
            ForEach(Array(paths.enumerated()), id: \.offset) { _, path in
                VStack(alignment: .leading, spacing: 2) {
                    Text(path).font(.system(size: 10, design: .monospaced)).lineLimit(2)
                        .truncationMode(.middle).textSelection(.enabled)
                    if let winner = winners[path] {
                        Text("Wins: \(winner)").font(.system(size: 10)).foregroundStyle(ModTint.cyan)
                    }
                }
            }
        }.foregroundStyle(ModTint.muted)
    }

    private func metric(_ label: String, value: Int) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(value.formatted()).font(.system(size: 23, weight: .light)).monospacedDigit()
            Text(label).font(.system(size: 10)).foregroundStyle(ModTint.muted)
        }.accessibilityElement(children: .combine)
    }

    private func placeholder(_ title: String, detail: String, icon: String) -> some View {
        VStack(spacing: 9) {
            Image(systemName: icon).font(.system(size: 23, weight: .light)).foregroundStyle(ModTint.cyan)
                .accessibilityHidden(true)
            Text(title).font(.system(size: 17, weight: .medium))
            Text(detail).font(.system(size: 11)).foregroundStyle(ModTint.muted).multilineTextAlignment(.center)
        }.padding(20).frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private var footer: some View {
        HStack(alignment: .center, spacing: 12) {
            if model.busy { ProgressView().controlSize(.small).frame(width: 20) }
            else { Image(systemName: statusIcon).foregroundStyle(statusColor).frame(width: 20) }
            VStack(alignment: .leading, spacing: 4) {
                Text(statusTitle).font(.system(size: 13, weight: .medium)).foregroundStyle(statusColor)
                if !model.busy, let message = model.message, !message.isEmpty {
                    Text(message).font(.system(size: 11)).foregroundStyle(model.failed ? ModTint.orange : ModTint.muted)
                        .lineLimit(3).textSelection(.enabled)
                }
                if !model.busy, let blocker = model.blocker, blocker != model.message {
                    Text(blocker).font(.system(size: 11)).foregroundStyle(ModTint.gold).lineLimit(3)
                } else if !model.busy, !model.failed, let files = model.status?.appliedFiles, files > 0, model.status?.pending == false {
                    Text("\(files.formatted()) files prepared for startup.")
                        .font(.system(size: 11)).foregroundStyle(ModTint.muted)
                }
            }
            Spacer(minLength: 0)
            if let backup = model.backupURL {
                Button("Show backup") { NSWorkspace.shared.activateFileViewerSelecting([backup]) }
                    .buttonStyle(.bordered).help("Reveal the previous overlay in Finder")
            }
            Button("Apply for next launch") { model.apply() }
                .buttonStyle(.borderedProminent).disabled(!model.canApply)
                .help(model.blocker ?? "Prepare the enabled mods for the next game launch")
        }.padding(13).background(ModTint.panel).overlay { Rectangle().stroke(ModTint.line, lineWidth: 1).allowsHitTesting(false) }
    }

    private var statusTitle: String {
        if let activity = model.activity { return activity }
        if model.failed { return "Mods need attention" }
        if model.blocker != nil { return "Apply blocked" }
        if model.status?.pending == true { return "Changes ready to apply" }
        if (model.status?.appliedFiles ?? 0) > 0 { return "Prepared for next launch" }
        return model.current ? "No changes to apply" : "Mod library unavailable"
    }
    private var statusColor: Color {
        if model.busy { return ModTint.muted }
        if model.failed { return ModTint.orange }
        if model.blocker != nil { return ModTint.gold }
        return model.current ? ModTint.cyan : ModTint.muted
    }
    private var statusIcon: String {
        if model.failed || model.blocker != nil { return "exclamationmark.circle" }
        return (model.status?.appliedFiles ?? 0) > 0 && model.status?.pending == false ? "checkmark.circle" : "tray"
    }

    private func chooseImport() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = true
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.resolvesAliases = false
        panel.prompt = "Import"
        panel.message = "Choose a Switch RomFS folder, ZIP or 7z archive."
        panel.begin { response in
            if response == .OK, let url = panel.url { model.importPack(url) }
        }
    }

    private func refreshGameState(_ notification: Notification) {
        guard let app = notification.userInfo?[NSWorkspace.applicationUserInfoKey] as? NSRunningApplication,
              app.bundleIdentifier == model.target.bundleID else { return }
        model.refresh(keepMessage: true)
    }
}


private enum ModTint {
    static let line = Color.primary.opacity(0.13)
    static let panel = Color(nsColor: .controlBackgroundColor)
    static let raised = Color.accentColor.opacity(0.10)
    static let cyan = Color.accentColor
    static let muted = Color.secondary
    static let gold = Color.orange
    static let orange = Color.red
    static let ivory = Color.primary
}
