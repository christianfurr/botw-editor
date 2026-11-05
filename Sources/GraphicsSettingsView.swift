import SwiftUI
import AppKit

struct GraphicsSettingsView: View {
    @StateObject private var model = GraphicsModel()
    @Environment(\.dismiss) private var dismiss
    private let ink = Color(red:0.035,green:0.067,blue:0.105)
    private let cyan = Color(red:0.34,green:0.85,blue:0.95)
    private let ivory = Color(red:0.91,green:0.90,blue:0.82)

    var body: some View {
        VStack(alignment:.leading,spacing:20) {
            HStack {
                VStack(alignment:.leading,spacing:5) {
                    Text("Graphics & shaders").font(.system(size:26,weight:.medium,design:.serif)).italic()
                    Text("Change the game’s color and clarity.").font(.system(size:12)).foregroundStyle(.secondary)
                }
                Spacer()
                Button("Done") { dismiss() }.keyboardShortcut(.cancelAction)
            }
            Picker("Look",selection:$model.configuration.preset) {
                ForEach(GraphicsPreset.allCases) { preset in Text(preset.title).tag(preset) }
            }.pickerStyle(.segmented)
            Text(model.configuration.preset.detail).font(.system(size:13)).frame(minHeight:20,alignment:.leading)
            VStack(spacing:18) {
                slider("Effect strength",value:$model.configuration.strength)
                slider("Sharpness",value:$model.configuration.sharpness)
            }.disabled(model.configuration.preset == .off)
            if model.configuration.preset == .custom {
                HStack {
                    Text("Custom Metal shader").font(.system(size:12,weight:.semibold))
                    Spacer()
                    Button("Use sample") { model.source = GraphicsModel.sampleShader }
                    Button("Open shader folder") { revealShaderFolder() }
                }
                TextEditor(text:$model.source).font(.system(size:12,design:.monospaced))
                    .scrollContentBackground(.hidden).padding(8).frame(minHeight:190,maxHeight:.infinity)
                    .background(.black.opacity(0.2)).overlay(Rectangle().stroke(cyan.opacity(0.3),lineWidth:1))
                    .accessibilityLabel("Metal shader source")
                Text("Keep the customGrade(color, uv) function. Shader errors restore the original image.")
                    .font(.system(size:11)).foregroundStyle(.secondary)
            } else {
                Spacer(minLength:10)
                HStack(alignment:.top,spacing:10) {
                    Image(systemName:"sparkles").foregroundStyle(cyan)
                    Text("These lightweight effects keep BOTW’s lighting and art. They also affect the in-game HUD. Choose Off to restore the original image.")
                        .font(.system(size:12)).foregroundStyle(.secondary).fixedSize(horizontal:false,vertical:true)
                }.padding(16).background(cyan.opacity(0.045))
                Spacer(minLength:10)
            }
            Divider().overlay(cyan.opacity(0.2))
            HStack(alignment:.center,spacing:12) {
                if model.pending { ProgressView().controlSize(.small) }
                Text(model.message).font(.system(size:11)).foregroundStyle(model.isError ? Color.orange : ivory.opacity(0.75))
                    .lineLimit(5).fixedSize(horizontal:false,vertical:true).help(model.message).accessibilityLabel(model.message)
                Spacer(minLength:5)
                Button("Restore original") { model.apply(original:true) }.disabled(model.pending)
                Button(model.runtimeAvailable ? "Apply to game" : "Save look") { model.apply() }
                    .buttonStyle(.borderedProminent).tint(cyan).foregroundStyle(ink).disabled(model.pending)
            }
        }.padding(26).frame(width:700,height:model.configuration.preset == .custom ? 650 : 470)
            .background(ink).foregroundStyle(ivory).tint(cyan).preferredColorScheme(.dark)
            .onAppear { model.start() }.onDisappear { model.stop() }
    }

    private func slider(_ title:String,value:Binding<Double>) -> some View {
        HStack(spacing:18) {
            Text(title).font(.system(size:12)).frame(width:100,alignment:.leading)
            Slider(value:value,in:0...1).accessibilityLabel(title)
            Text(value.wrappedValue.formatted(.percent.precision(.fractionLength(0))))
                .font(.system(size:12)).monospacedDigit().frame(width:40,alignment:.trailing)
        }
    }
    private func revealShaderFolder() {
        do {
            try FileManager.default.createDirectory(at:model.folder,withIntermediateDirectories:true)
            NSWorkspace.shared.open(model.folder)
        } catch { }
    }
}
