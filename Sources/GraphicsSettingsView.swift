import SwiftUI
import AppKit

struct GraphicsSettingsView: View {
    private enum Section: String, CaseIterable { case upscaling = "Upscaling", color = "Color & custom shaders" }
    @State private var section: Section = .upscaling
    @State private var showTechnical = false
    @StateObject private var model = GraphicsModel()
    @StateObject private var upscaling = UpscalingModel()
    @Environment(\.dismiss) private var dismiss
    private let ink = Color(red:0.035,green:0.067,blue:0.105)
    private let cyan = Color(red:0.34,green:0.85,blue:0.95)
    private let ivory = Color(red:0.91,green:0.90,blue:0.82)

    var body: some View {
        VStack(alignment:.leading,spacing:20) {
            HStack {
                VStack(alignment:.leading,spacing:5) {
                    Text("Graphics & shaders").font(.system(size:26,weight:.medium,design:.serif)).italic()
                    Text(section == .upscaling ? "Resolution reconstruction and render quality." : "Color, clarity and custom Metal filters.").font(.system(size:12)).foregroundStyle(.secondary)
                }
                Spacer()
                Button("Done") { dismiss() }.keyboardShortcut(.cancelAction)
            }
            Picker("Graphics section",selection:$section) {
                ForEach(Section.allCases,id:\.self) { Text($0.rawValue).tag($0) }
            }.pickerStyle(.segmented).labelsHidden()
            if section == .upscaling { upscalingPanel }
            else { colorPanel }
        }.padding(26).frame(width:720,height:section == .upscaling ? (showTechnical ? 650 : 570) : model.configuration.preset == .custom ? 700 : 530)
            .background(ink).foregroundStyle(ivory).tint(cyan).preferredColorScheme(.dark)
            .onAppear { model.start(); upscaling.start() }
            .onDisappear { model.stop(); upscaling.stop() }
    }

    private var upscalingPanel: some View {
        VStack(alignment:.leading,spacing:18) {
            HStack(spacing:9) {
                Image(systemName:"square.stack.3d.up").foregroundStyle(cyan)
                Text("MetalFX Temporal").font(.system(size:20,weight:.medium,design:.serif)).italic()
                Text("Experimental").font(.system(size:10,weight:.medium)).foregroundStyle(.orange).padding(.horizontal,7).padding(.vertical,4).background(Color.orange.opacity(0.08),in:Capsule())
                    .help("This implementation estimates camera motion. Moving objects and newly revealed surfaces may show reconstruction artifacts.")
                Spacer()
            }
            HStack(spacing:16) {
                Text("For next launch").font(.system(size:12)).foregroundStyle(.secondary)
                Picker("Upscaling preset for next launch",selection:$upscaling.selection) {
                    ForEach(UpscalingPreset.allCases) { Text($0.title).tag($0) }
                }.labelsHidden().frame(maxWidth:.infinity)
            }
            Text(upscaling.selection.nextLaunchDetail).font(.system(size:12)).foregroundStyle(.secondary).fixedSize(horizontal:false,vertical:true)
            VStack(alignment:.leading,spacing:10) {
                HStack(spacing:8) {
                    Circle().fill(upscaling.waitingForRestart ? Color.orange : upscaling.temporalActive ? cyan : ivory.opacity(0.4)).frame(width:6,height:6)
                    Text(upscaling.headline).font(.system(size:14,weight:.semibold))
                }
                Text(upscaling.detail).font(.system(size:12)).foregroundStyle(ivory.opacity(0.8)).fixedSize(horizontal:false,vertical:true)
                if let status = upscaling.status, upscaling.runtimeAvailable {
                    HStack(spacing:20) {
                        Label("\(status.gpuCompleted.formatted()) GPU frames completed",systemImage:"checkmark.circle")
                        Text("\(status.historyResets.formatted()) history reset\(status.historyResets == 1 ? "" : "s")")
                    }.font(.system(size:10)).foregroundStyle(.secondary)
                }
            }.padding(16).frame(maxWidth:.infinity,alignment:.leading).background(cyan.opacity(0.045)).overlay(Rectangle().stroke(cyan.opacity(0.22),lineWidth:1))
            DisclosureGroup("Runtime details",isExpanded:$showTechnical) {
                VStack(alignment:.leading,spacing:7) {
                    if let status = upscaling.status {
                        Text("Motion: \(status.motionSource.isEmpty ? "Not reported" : status.motionSource)")
                        Text("Encoded: \(status.encoded.formatted()) · GPU errors: \(status.gpuErrors.formatted()) · Missing inputs: \(status.missingInputs.formatted())")
                        if let preset = status.savedPreset { Text("Current process boot preset: \(preset)") }
                        if !status.fallbackReason.isEmpty { Text(status.fallbackReason).foregroundStyle(.orange) }
                    } else { Text("No fresh status from the temporal runtime.") }
                    Text("Experimental camera-derived motion can leave artifacts around moving objects. Completed-frame counts do not measure FPS or visual quality.").foregroundStyle(.secondary)
                }.font(.system(size:11)).fixedSize(horizontal:false,vertical:true).frame(maxWidth:.infinity,alignment:.leading).padding(.top,8)
            }.font(.system(size:11)).foregroundStyle(ivory.opacity(0.75))
            Spacer(minLength:0)
            Divider().overlay(cyan.opacity(0.2))
            HStack(alignment:.center,spacing:14) {
                Text(upscaling.message).font(.system(size:11)).foregroundStyle(upscaling.isError ? Color.orange : ivory.opacity(0.75)).lineLimit(4).fixedSize(horizontal:false,vertical:true)
                Spacer(minLength:0)
                Button("Use on next launch") { upscaling.saveForNextLaunch() }.buttonStyle(GraphicsActionStyle())
            }
            Text("Quit and reopen BOTW after saving. Color filters keep their own settings.").font(.system(size:10)).foregroundStyle(.secondary)
        }
    }

    private var colorPanel: some View {
        VStack(alignment:.leading,spacing:20) {
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
                    Text("These filters change the final picture’s color and sharpness, including the in-game HUD. Off restores the unfiltered picture; it does not change your upscaling preset.")
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
                    .buttonStyle(GraphicsActionStyle()).disabled(model.pending)
            }
        }
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

private struct GraphicsActionStyle: ButtonStyle {
    @Environment(\.isEnabled) private var enabled
    func makeBody(configuration: Configuration) -> some View {
        configuration.label.font(.system(size:12,weight:.semibold)).padding(.horizontal,15).frame(minHeight:33)
            .foregroundStyle(enabled ? Color(red:0.035,green:0.067,blue:0.105) : Color(red:0.91,green:0.90,blue:0.82).opacity(0.6))
            .background(Color(red:0.34,green:0.85,blue:0.95).opacity(enabled ? (configuration.isPressed ? 0.7 : 1) : 0.08),in:RoundedRectangle(cornerRadius:4))
            .overlay(RoundedRectangle(cornerRadius:4).stroke(Color(red:0.34,green:0.85,blue:0.95).opacity(enabled ? 1 : 0.2),lineWidth:0.7))
    }
}
