import SwiftUI
import AppKit
import ImageIO

private let setupGuide = URL(string:"https://github.com/christianfurr/botw-editor/blob/v0.1.5/docs/INSTALLATION.md")!

#if !UI_SNAPSHOT
@main
struct BOTWEditorApp: App {
    @StateObject private var model = PlaygroundModel()
    var body: some Scene {
        WindowGroup("BOTW Editor") { EditorView(model: model) }
            .defaultSize(width: 1280, height: 820)
            .windowResizability(.contentMinSize)
            .commands {
                CommandGroup(replacing: .newItem) {}
                CommandGroup(after: .toolbar) {
                    Button("Refresh Connection") { model.refresh(); if model.connected { model.send(.status) } }
                        .keyboardShortcut("r", modifiers: .command)
                }
                CommandGroup(replacing: .help) {
                    Link("BOTW Editor Setup and Recovery", destination:setupGuide)
                }
            }
    }
}
#endif

enum EditorTab: String, CaseIterable {
    case inventory = "Inventory", player = "Player", world = "World"
    var icon: String { switch self { case .inventory: return "square.grid.2x2"; case .player: return "person.crop.circle"; case .world: return "map" } }
}

private struct ConfirmAction: Identifiable {
    let id = UUID()
    let title: String
    let detail: String
    let button: String
    let command: TrainerCommand
    var destructive = false
}

private enum Slate {
    static let background = Color(red: 0.035, green: 0.067, blue: 0.105)
    static let panel = Color(red: 0.055, green: 0.095, blue: 0.140)
    static let raised = Color(red: 0.080, green: 0.133, blue: 0.180)
    static let line = Color(red: 0.24, green: 0.42, blue: 0.49).opacity(0.38)
    static let cyan = Color(red: 0.34, green: 0.85, blue: 0.95)
    static let ivory = Color(red: 0.91, green: 0.90, blue: 0.82)
    static let muted = Color(red: 0.52, green: 0.62, blue: 0.67)
    static let gold = Color(red: 0.79, green: 0.65, blue: 0.37)
    static let orange = Color(red: 0.96, green: 0.61, blue: 0.30)
}

private struct CutCornerShape: Shape {
    var cut: CGFloat = 7
    func path(in r: CGRect) -> Path {
        Path { p in
            p.move(to:CGPoint(x:r.minX+cut,y:r.minY)); p.addLine(to:CGPoint(x:r.maxX-cut,y:r.minY))
            p.addLine(to:CGPoint(x:r.maxX,y:r.midY)); p.addLine(to:CGPoint(x:r.maxX-cut,y:r.maxY))
            p.addLine(to:CGPoint(x:r.minX+cut,y:r.maxY)); p.addLine(to:CGPoint(x:r.minX,y:r.midY)); p.closeSubpath()
        }
    }
}

private struct SlateButtonStyle: ButtonStyle {
    var primary = false
    var destructive = false
    @Environment(\.isEnabled) private var enabled
    func makeBody(configuration: Configuration) -> some View {
        configuration.label.font(.system(size:primary ? 15 : 13,weight:.medium,design:.serif)).italic()
            .foregroundStyle(!enabled ? Slate.ivory.opacity(0.48) : primary ? Slate.background : destructive ? Slate.gold : Slate.ivory)
            .padding(.horizontal,primary ? 19 : 14).frame(minHeight:primary ? 38 : 33)
            .background {
                CutCornerShape(cut:primary ? 8 : 4).fill(primary ? Slate.cyan.opacity(enabled ? (configuration.isPressed ? 0.75 : 1) : 0.09) : Slate.raised.opacity(configuration.isPressed ? 0.65 : 0.27))
            }
            .overlay { CutCornerShape(cut:primary ? 8 : 4).stroke(primary ? Slate.cyan.opacity(enabled ? 1 : 0.25) : Slate.ivory.opacity(enabled ? 0.42 : 0.13),lineWidth:0.7) }
            .shadow(color:primary && enabled ? Slate.cyan.opacity(0.14) : .clear,radius:8)
    }
}

private struct QuantityControl: View {
    @Binding var value: String
    var durability = false
    var maximum = 999
    private func step(_ direction: Int) {
        if durability {
            let fixture = InventoryEntry(slot:0,name:"",type:0,value:1,equipped:false)
            let raw = Int(InventoryValuePresentation.commandText(for:fixture,input:value)) ?? 100
            value = InventoryValuePresentation.durabilityText(rawValue:max(1,min(InventoryEntry.maximumRawDurability,raw+direction*100)))
        } else { value = String(max(1,min(maximum,(Int(value) ?? 1)+direction))) }
    }
    var body: some View {
        HStack(spacing:0) {
            Button { step(-1) } label: { Image(systemName:"minus").frame(width:31,height:31) }.accessibilityLabel("Decrease value")
            Rectangle().fill(Slate.line).frame(width:1)
            TextField("Value",text:$value).textFieldStyle(.plain).multilineTextAlignment(.center).font(.system(size:16,design:.serif)).frame(minWidth:42).padding(.horizontal,5).accessibilityLabel(durability ? "Durability in points" : "Quantity")
            Rectangle().fill(Slate.line).frame(width:1)
            Button { step(1) } label: { Image(systemName:"plus").frame(width:31,height:31) }.accessibilityLabel("Increase value")
        }.frame(height:33).buttonStyle(.plain).background(Slate.background.opacity(0.7),in:RoundedRectangle(cornerRadius:3)).overlay(RoundedRectangle(cornerRadius:3).stroke(Slate.ivory.opacity(0.45),lineWidth:0.7))
    }
}

private struct DestinationSymbol: View {
    var tower = false
    var color = Slate.cyan
    var body: some View {
        Canvas { c,s in
            let cx=s.width/2,cy=s.height/2
            var p=Path()
            if tower {
                p.move(to:CGPoint(x:cx,y:2)); p.addLine(to:CGPoint(x:cx-5,y:s.height-3)); p.addLine(to:CGPoint(x:cx+5,y:s.height-3)); p.closeSubpath()
                c.stroke(p,with:.color(color),lineWidth:1.2)
                for y in [cy-1,s.height-5] { var bar=Path();bar.move(to:CGPoint(x:cx-6,y:y));bar.addLine(to:CGPoint(x:cx+6,y:y));c.stroke(bar,with:.color(color),lineWidth:1.2) }
                c.stroke(Path(ellipseIn:CGRect(x:cx-2,y:0,width:4,height:4)),with:.color(color),lineWidth:1)
            } else {
                for radius:CGFloat in [8,4] {
                    var diamond=Path();diamond.move(to:CGPoint(x:cx,y:cy-radius));diamond.addLine(to:CGPoint(x:cx+radius,y:cy));diamond.addLine(to:CGPoint(x:cx,y:cy+radius));diamond.addLine(to:CGPoint(x:cx-radius,y:cy));diamond.closeSubpath();c.stroke(diamond,with:.color(color),lineWidth:1.2)
                }
                var stem=Path();stem.move(to:CGPoint(x:cx,y:cy+4));stem.addLine(to:CGPoint(x:cx,y:s.height));c.stroke(stem,with:.color(color),lineWidth:1.2)
            }
        }.frame(width:21,height:24).shadow(color:color.opacity(0.45),radius:4).accessibilityHidden(true)
    }
}

private struct ContourEngraving: View {
    var body: some View {
        Canvas { context,size in
            for center in [CGPoint(x:size.width*0.98,y:-28),CGPoint(x:-45,y:size.height*0.98)] {
                for index in 0..<17 {
                    let radius = CGFloat(42+index*11)
                    var path = Path()
                    for step in 0...150 {
                        let angle = Double(step)*Double.pi*2/150
                        let ripple = 1 + 0.09*sin(angle*3+Double(index)*0.3) + 0.04*cos(angle*7)
                        let point = CGPoint(x:center.x+cos(angle)*radius*ripple*1.7,y:center.y+sin(angle)*radius*ripple)
                        if step == 0 { path.move(to:point) } else { path.addLine(to:point) }
                    }
                    context.stroke(path,with:.color(Slate.cyan.opacity(0.07)),lineWidth:0.65)
                }
            }
        }.allowsHitTesting(false).accessibilityHidden(true)
    }
}

private struct CornerBrackets: Shape {
    func path(in r: CGRect) -> Path {
        Path { p in
            let d: CGFloat = 10
            for (x, dx) in [(r.minX,CGFloat(1)),(r.maxX,CGFloat(-1))] {
                for (y,dy) in [(r.minY,CGFloat(1)),(r.maxY,CGFloat(-1))] {
                    p.move(to:CGPoint(x:x,y:y+dy*d)); p.addLine(to:CGPoint(x:x,y:y)); p.addLine(to:CGPoint(x:x+dx*d,y:y))
                }
            }
        }
    }
}

private struct SelectionGlow: View {
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    var body: some View {
        TimelineView(.animation(minimumInterval:0.08,paused:reduceMotion)) { time in
            let glow = reduceMotion ? 0.95 : 0.82 + 0.16*sin(time.date.timeIntervalSinceReferenceDate*2)
            CornerBrackets().stroke(Slate.cyan.opacity(glow),lineWidth:1.5)
                .shadow(color:Slate.cyan.opacity(0.18),radius:4)
        }.allowsHitTesting(false).accessibilityHidden(true)
    }
}

private struct SlateHover: ViewModifier {
    @State private var hovered = false
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    func body(content:Content) -> some View {
        content.brightness(hovered ? 0.035 : 0)
            .animation(reduceMotion ? nil : .easeOut(duration:0.12),value:hovered)
            .onHover { hovered = $0 }
    }
}

private struct SlateEye: View {
    var body: some View {
        Canvas { c,s in
            let mid = CGPoint(x:s.width/2,y:s.height*0.42)
            var p = Path(); p.move(to:CGPoint(x:1,y:mid.y)); p.addQuadCurve(to:CGPoint(x:s.width-1,y:mid.y),control:CGPoint(x:mid.x,y:0)); p.addQuadCurve(to:CGPoint(x:1,y:mid.y),control:CGPoint(x:mid.x,y:s.height*0.76))
            c.stroke(p,with:.color(Slate.cyan),lineWidth:1.3)
            c.stroke(Path(ellipseIn:CGRect(x:mid.x-4,y:mid.y-4,width:8,height:8)),with:.color(Slate.cyan),lineWidth:1.3)
            var tear = Path(); tear.move(to:CGPoint(x:mid.x,y:mid.y+7)); tear.addLine(to:CGPoint(x:mid.x,y:s.height-1));
            c.stroke(tear,with:.color(Slate.gold),lineWidth:1.3)
        }.frame(width:27,height:25).accessibilityHidden(true)
    }
}

private func typeName(_ type: Int) -> String {
    let names = ["Weapons", "Bows", "Arrows", "Shields", "Head armor", "Body armor", "Leg armor", "Materials", "Food", "Key items"]
    return names.indices.contains(type) ? names[type] : "Other"
}
private func typeIcon(_ type: Int) -> String {
    let names = ["figure.fencing", "arrow.up.right", "arrow.up", "shield.lefthalf.filled", "crown", "tshirt", "figure.walk", "leaf", "carrot", "key"]
    return names.indices.contains(type) ? names[type] : "cube"
}

/// Artwork stays in the user's private cache. The app bundle contains no game art.
private enum GameArtwork {
    static let root = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("BOTWEditor/cache",isDirectory:true)
    static let icons = NSCache<NSString,NSImage>()
    static func icon(_ actor: String) -> NSImage? {
        guard actor.range(of:"^[A-Za-z0-9_]{1,80}$",options:.regularExpression) != nil else { return nil }
        if let cached = icons.object(forKey:actor as NSString) { return cached }
        guard let image = NSImage(contentsOf:root.appendingPathComponent("items/\(actor).png")) else { return nil }
        icons.setObject(image,forKey:actor as NSString)
        return image
    }
}

private struct ItemArtwork: View {
    let actor: String
    let type: Int
    var size: CGFloat = 60
    var body: some View {
        Group {
            if let icon = GameArtwork.icon(actor) { Image(nsImage:icon).resizable().interpolation(.high).scaledToFit() }
            else {
                Image(systemName:typeIcon(type)).resizable().scaledToFit().padding(size*0.22)
                    .foregroundStyle(Slate.ivory.opacity(0.45))
            }
        }.frame(width:size,height:size).accessibilityHidden(true)
    }
}

private struct SearchBox: View {
    let placeholder: String
    @Binding var text: String
    var body: some View {
        HStack(spacing:8) {
            Image(systemName:"magnifyingglass").foregroundStyle(Slate.muted)
            TextField(placeholder,text:$text).textFieldStyle(.plain).font(.system(size:12))
            if !text.isEmpty { Button { text = "" } label: { Image(systemName:"xmark.circle.fill").foregroundStyle(Slate.muted) }.buttonStyle(.plain).accessibilityLabel("Clear search") }
        }.padding(.horizontal,11).frame(height:33).background(Slate.background.opacity(0.7),in:RoundedRectangle(cornerRadius:4))
            .overlay(RoundedRectangle(cornerRadius:4).stroke(Slate.line,lineWidth:1))
    }
}

/// Weapon durability is stored as integer hundredths. Keep display round trips exact.
enum InventoryValuePresentation {
    static func durabilityText(rawValue:Int) -> String {
        let whole = rawValue / 100
        let fraction = abs(rawValue % 100)
        guard fraction != 0 else { return String(whole) }
        var digits = String(format:"%02d",fraction)
        if digits.hasSuffix("0") { digits.removeLast() }
        let sign = rawValue < 0 && whole == 0 ? "-" : ""
        return "\(sign)\(whole).\(digits)"
    }

    static func commandText(for item:InventoryEntry,input:String) -> String {
        guard item.usesDurability else { return input }
        let text = input.trimmingCharacters(in:.whitespaces)
        guard text.range(of:"^[+]?[0-9]+(?:\\.[0-9]{1,2})?$",options:.regularExpression) != nil else { return "" }
        let parts = text.split(separator:".",omittingEmptySubsequences:false)
        guard let whole = Int(parts[0]), (0...1_000_000).contains(whole) else { return "" }
        let fractionText = parts.count == 2 ? String(parts[1]) : ""
        let fraction = Int(fractionText.padding(toLength:2,withPad:"0",startingAt:0)) ?? 0
        let rawValue = whole*100+fraction
        guard (1...InventoryEntry.maximumRawDurability).contains(rawValue) else { return "" }
        return String(rawValue)
    }
}

struct EditorView: View {
    @ObservedObject var model: PlaygroundModel
    @State private var tab: EditorTab = .inventory
    @State private var search = ""
    @State private var category = -1
    @State private var sortOrder = "Name (A–Z)"
    @State private var gridView = true
    @State private var selectedSlot: Int?
    @State private var itemValue = "1"
    @State private var rupees = "10000"
    @State private var maxHearts = "30"
    @State private var maxStamina = "3"
    @State private var showAdd = false
    @State private var showGraphics = false
    @State private var showPouchSlots = false
    @State private var worldSearch = ""
    @State private var worldKind = "All"
    @State private var selectedDestination: String?
    @State private var confirmation: ConfirmAction?
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    @Namespace private var navigation

    init(model: PlaygroundModel, initialTab: EditorTab = .inventory, initialDestination: String? = nil) {
        self.model = model
        _tab = State(initialValue:initialTab)
        _selectedDestination = State(initialValue:initialDestination)
    }
    private var inventory: [InventoryEntry] { model.state?.inventory ?? [] }
    private var selectedItem: InventoryEntry? { inventory.first { $0.slot == selectedSlot } }
    private var filteredInventory: [InventoryEntry] {
        inventory.filter { categoryMatches($0.type) && (search.isEmpty || model.name(for:$0.name).localizedCaseInsensitiveContains(search) || $0.name.localizedCaseInsensitiveContains(search)) }.sorted { a,b in
            if sortOrder == "Quantity" && a.value != b.value { return a.value > b.value }
            let order = model.name(for:a.name).localizedStandardCompare(model.name(for:b.name))
            return order == .orderedSame ? a.slot < b.slot : sortOrder == "Name (Z–A)" ? order == .orderedDescending : order == .orderedAscending
        }
    }
    private var destinations: [WorldDestination] { model.state?.destinations ?? [] }
    private var filteredDestinations: [WorldDestination] {
        destinations.filter {
            (worldKind == "All" || (worldKind == "Towers" ? $0.kind.localizedCaseInsensitiveContains("tower") : !$0.kind.localizedCaseInsensitiveContains("tower"))) &&
            (worldSearch.isEmpty || $0.name.localizedCaseInsensitiveContains(worldSearch) || $0.id.localizedCaseInsensitiveContains(worldSearch))
        }
    }
    private var destination: WorldDestination? { destinations.first { $0.id == selectedDestination } }
    private var maximumHearts: Double? { model.available ? model.state?.maxHeartsLive ?? model.state?.maxHearts : nil }
    private var maximumStamina: Double? { model.available ? model.state?.maxStaminaLive ?? model.state?.maxStamina : nil }

    var body: some View {
        VStack(spacing:0) {
            toolbar
            if !model.available { connectionNotice }
            Group {
                switch tab {
                case .inventory: inventoryTab
                case .player: playerTab
                case .world: worldTab
                }
            }.frame(maxWidth:.infinity,maxHeight:.infinity).transition(.opacity)
            feedbackBar
        }
        .frame(minWidth:1000,minHeight:680)
        .background { Slate.background; ContourEngraving() }.foregroundStyle(Slate.ivory).tint(Slate.cyan).preferredColorScheme(.dark)
        .font(.system(size:13))
        .onChange(of:selectedSlot) { _,_ in itemValue = selectedItem.map { $0.usesDurability ? InventoryValuePresentation.durabilityText(rawValue:$0.value) : String($0.value) } ?? "1" }
        .onAppear { selectFirstItemIfNeeded() }
        .onChange(of:selectedItem?.name) { _,_ in itemValue=selectedItem.map { $0.usesDurability ? InventoryValuePresentation.durabilityText(rawValue:$0.value) : String($0.value) } ?? "1" }
        .onChange(of:inventory.map(\.slot)) { _,_ in selectFirstItemIfNeeded() }
        .sheet(isPresented:$showAdd) { AddItemView(model:model) }
        .sheet(isPresented:$showGraphics) { GraphicsSettingsView() }
        .sheet(isPresented:$showPouchSlots) { PouchSlotsView(model:model) }
        .alert(item:$confirmation) { action in
            let primary: Alert.Button = action.destructive ? .destructive(Text(action.button),action:{ model.send(action.command) }) : .default(Text(action.button),action:{ model.send(action.command) })
            return Alert(title:Text(action.title),message:Text(action.detail),primaryButton:primary,secondaryButton:.cancel())
        }
    }

    private var toolbar: some View {
        HStack(spacing:18) {
            Text("BOTW Editor").font(.system(size:17,weight:.medium,design:.serif)).italic().padding(.trailing,8)
            navigationBar
            Spacer(minLength:8)
            if model.available {
                healthDisplay
                staminaDisplay
                Rectangle().fill(Slate.line).frame(width:1,height:20)
            }
            HStack(spacing:6) {
                Circle().fill(model.available ? Color.green : Slate.gold).frame(width:5,height:5)
                Text(model.available ? "Connected" : model.connected ? "Waiting" : "Disconnected").font(.system(size:11,design:.serif)).italic()
            }.help(model.connectionMessage)
            hud("diamond.fill",value:liveRupees,color:Slate.cyan)
            Button { showGraphics=true } label: { Image(systemName:"slider.horizontal.3").font(.system(size:12)) }.buttonStyle(.plain).accessibilityLabel("Graphics and shader settings").help("Graphics and shaders")
            Button { model.refresh(); if model.connected { model.send(.status) } } label: { Image(systemName:"arrow.clockwise").font(.system(size:12)) }
                .buttonStyle(.plain).disabled(model.pending).accessibilityLabel("Refresh game connection")
        }.padding(.horizontal,20).frame(height:48).background { Slate.panel.opacity(0.6); ContourEngraving().clipped() }
            .overlay(alignment:.bottom) { line() }
    }
    private var healthDisplay: some View {
        let current = Double(model.state?.health ?? 0)/4
        let maximum = max(1,min(30,Int(maximumHearts ?? 3)))
        return VStack(alignment:.trailing,spacing:2) {
            ForEach(0..<max(1,(maximum+14)/15),id:\.self) { row in
                HStack(spacing:2) {
                    ForEach(row*15..<min(maximum,(row+1)*15),id:\.self) { index in
                        Image(systemName:Double(index)<current ? "heart.fill" : "heart").font(.system(size:9)).foregroundStyle(Color(red:1,green:0.36,blue:0.35))
                    }
                }
            }
        }.accessibilityElement(children:.ignore).accessibilityLabel("Hearts, \(heartHUD)")
    }
    private var staminaDisplay: some View {
        let current = (model.state?.stamina ?? 0)/1000
        let maximum = max(1,min(3,Int(maximumStamina ?? 1)))
        return HStack(spacing:4) {
            ForEach(0..<maximum,id:\.self) { index in
                ZStack {
                    Circle().stroke(Color.green.opacity(0.2),lineWidth:2.5)
                    Circle().trim(from:0,to:max(0,min(1,current-Double(index)))).stroke(Color(red:0.38,green:0.9,blue:0.52),style:StrokeStyle(lineWidth:2.5,lineCap:.round)).rotationEffect(.degrees(-90))
                }.frame(width:15,height:15)
            }
        }.accessibilityElement(children:.ignore).accessibilityLabel("Stamina wheels, \(staminaHUD)")
    }
    private var heartHUD: String {
        guard model.available, let health = model.state?.health else { return "— / —" }
        return "\(number(Double(health)/4)) / \(maximumHearts.map(number) ?? "—")"
    }
    private var staminaHUD: String {
        guard model.available, let stamina = model.state?.stamina else { return "— / —" }
        return "\(number(stamina/1000)) / \(maximumStamina.map(number) ?? "—")"
    }
    private var liveRupees: String { model.available ? model.state?.rupees?.formatted() ?? "—" : "—" }
    private func number(_ value:Double) -> String { String(format:"%g",value) }
    private func hud(_ symbol:String,value:String,color:Color) -> some View {
        HStack(spacing:6) { Image(systemName:symbol).foregroundStyle(color); Text(value).monospacedDigit() }
            .font(.system(size:11,weight:.medium)).padding(.horizontal,8)
            .accessibilityElement(children:.combine)
            .accessibilityLabel(symbol == "heart.fill" ? "Hearts, \(value)" : symbol == "circle.dashed" ? "Stamina wheels, \(value)" : "Rupees, \(value)")
    }
    private var navigationBar: some View {
        HStack(spacing:0) {
            ForEach(EditorTab.allCases,id:\.self) { section in
                Button {
                    withAnimation(reduceMotion ? nil : .easeInOut(duration:0.18)) { tab = section }
                } label: {
                    Text(section.rawValue).font(.system(size:16,weight:.medium,design:.serif)).italic()
                        .foregroundStyle(tab == section ? Slate.cyan : Slate.ivory.opacity(0.8))
                        .frame(width:86,height:48)
                        .overlay(alignment:.bottom) {
                            if tab == section { Slate.cyan.frame(height:2).matchedGeometryEffect(id:"navigation",in:navigation).shadow(color:Slate.cyan.opacity(0.7),radius:5) }
                        }
                }.buttonStyle(.plain).accessibilityAddTraits(tab == section ? .isSelected : [])
            }
        }.fixedSize()
    }
    private var connectionNotice: some View {
        HStack(spacing:8) {
            Image(systemName:model.state?.teleportBusy == true ? "arrow.trianglehead.2.clockwise.rotate.90" : "info.circle").foregroundStyle(Slate.gold)
            Text(model.connectionMessage).font(.system(size:11)).lineLimit(2)
            Spacer()
            if !model.connected {
                Link("Setup guide",destination:setupGuide).font(.system(size:11,weight:.semibold)).foregroundStyle(Slate.cyan)
            }
        }.padding(.horizontal,20).padding(.vertical,10).background(Slate.gold.opacity(0.06))
            .overlay(alignment:.bottom) { Rectangle().fill(Slate.gold.opacity(0.12)).frame(height:1) }
    }

    private func categoryMatches(_ type:Int) -> Bool {
        category < 0 || (category == 4 ? (4...6).contains(type) : category == 1 ? [1,2].contains(type) : category == type)
    }
    private var inventoryTab: some View {
        VStack(spacing:0) {
            HStack(spacing:0) {
                inventoryCategories.frame(width:148)
                VStack(alignment:.leading,spacing:19) {
                    HStack(spacing:10) {
                        SearchBox(placeholder:"Search items…",text:$search)
                        Menu {
                            Picker("Sort items",selection:$sortOrder) { Text("Name (A–Z)").tag("Name (A–Z)");Text("Name (Z–A)").tag("Name (Z–A)");Text("Quantity").tag("Quantity") }
                        } label: { HStack(spacing:10) { Text("Sort: \(sortOrder)").font(.system(size:11,design:.serif)).italic();Image(systemName:"chevron.down").font(.system(size:8)) } }
                            .menuStyle(.borderlessButton).padding(.horizontal,11).frame(width:153,height:33).overlay(RoundedRectangle(cornerRadius:3).stroke(Slate.ivory.opacity(0.35),lineWidth:0.7))
                        HStack(spacing:0) {
                            Button { gridView=true } label: { Image(systemName:"square.grid.2x2.fill").frame(width:27,height:31).foregroundStyle(gridView ? Slate.cyan : Slate.muted) }.accessibilityLabel("Grid view")
                            line(vertical:true)
                            Button { gridView=false } label: { Image(systemName:"list.bullet").frame(width:27,height:31).foregroundStyle(gridView ? Slate.muted : Slate.cyan) }.accessibilityLabel("List view")
                        }.buttonStyle(.plain).fixedSize().overlay(RoundedRectangle(cornerRadius:3).stroke(Slate.line,lineWidth:0.7))
                    }
                    line()
                    if let reason = model.inventoryBlockedReason {
                        inventoryNotice(reason)
                    }
                    if filteredInventory.isEmpty {
                        VStack(spacing:18) {
                            emptyState(search.isEmpty ? (model.available && model.inventoryBlockedReason == nil ? "Your pouch is empty" : "Waiting for your pouch") : "No items found",icon:"backpack",detail:search.isEmpty ? (model.available && model.inventoryBlockedReason == nil ? "Give yourself an item from the catalog to begin exploring." : "You can browse the item catalog while waiting for the game.") : "Try another name or choose a different category.")
                            if search.isEmpty { Button("Browse item catalog") { showAdd=true }.buttonStyle(SlateButtonStyle()) }
                        }.frame(maxWidth:.infinity,maxHeight:.infinity)
                    } else {
                        ScrollView {
                            if gridView {
                                LazyVGrid(columns:[GridItem(.adaptive(minimum:110,maximum:142),spacing:13)],spacing:15) {
                                    ForEach(filteredInventory) { item in inventoryTile(item) }
                                }.padding(3)
                            } else {
                                LazyVStack(spacing:2) { ForEach(filteredInventory) { item in inventoryListRow(item) } }
                            }
                        }.scrollIndicators(.hidden)
                    }
                }.padding(.horizontal,22).padding(.top,23).padding(.bottom,16).frame(maxWidth:.infinity,maxHeight:.infinity,alignment:.top)
                line(vertical:true)
                inventoryInspector.frame(width:248)
            }
            line()
            HStack(spacing:20) {
                Button { showAdd=true } label: { Label("Give item…",systemImage:"plus") }.buttonStyle(SlateButtonStyle()).foregroundStyle(Slate.gold)
                Button { showPouchSlots=true } label: { Label("Pouch slots…",systemImage:"backpack").foregroundStyle(Slate.cyan) }.buttonStyle(SlateButtonStyle())
                line(vertical:true).frame(height:20)
                Text("Changes appear in your game").font(.system(size:12,design:.serif)).italic().foregroundStyle(Slate.muted)
                Spacer()
                Text("\(inventory.count) \(inventory.count == 1 ? "item" : "items")").font(.system(size:10)).foregroundStyle(Slate.muted)
            }.padding(.horizontal,25).frame(height:55).background { Slate.panel.opacity(0.65);ContourEngraving().clipped() }
        }
    }
    private func inventoryNotice(_ text:String) -> some View {
        Label(text,systemImage:"info.circle").font(.system(size:11)).foregroundStyle(Slate.gold).fixedSize(horizontal:false,vertical:true)
            .padding(10).frame(maxWidth:.infinity,alignment:.leading).background(Slate.gold.opacity(0.05)).overlay(Rectangle().stroke(Slate.gold.opacity(0.18),lineWidth:0.6))
    }
    private var inventoryCategories: some View {
        VStack(alignment:.leading,spacing:7) {
            Text("Inventory").font(.system(size:18,weight:.medium,design:.serif)).italic().padding(.horizontal,17).padding(.top,23).padding(.bottom,3)
            line().padding(.horizontal,13).padding(.bottom,8)
            categoryButton(-1,"All items","square.grid.2x2")
            categoryButton(0,"Weapons","figure.fencing")
            categoryButton(1,"Bows & arrows","arrow.up.right")
            categoryButton(3,"Shields","shield.lefthalf.filled")
            categoryButton(4,"Armor","tshirt")
            categoryButton(7,"Materials","leaf")
            categoryButton(8,"Food","carrot")
            categoryButton(9,"Key items","key")
            Spacer()
        }.background { Slate.panel.opacity(0.25);ContourEngraving().clipped() }.overlay(alignment:.trailing) { line(vertical:true) }
    }
    private func categoryButton(_ value:Int,_ name:String,_ icon:String) -> some View {
        Button {
            withAnimation(reduceMotion ? nil : .easeInOut(duration:0.15)) { category=value }
        } label: {
            HStack(spacing:10) {
                Image(systemName:icon).font(.system(size:14)).frame(width:18)
                Text(name).font(.system(size:14,weight:.medium,design:.serif)).italic().lineLimit(1).minimumScaleFactor(0.8)
                Spacer(minLength:0)
            }.foregroundStyle(category == value ? Slate.cyan : Slate.ivory.opacity(0.8)).padding(.horizontal,14).frame(height:35)
                .background { if category == value { CutCornerShape(cut:8).fill(Slate.cyan.opacity(0.10)) } }
                .overlay { if category == value { CutCornerShape(cut:8).stroke(Slate.cyan.opacity(0.75),lineWidth:0.8) } }
        }.buttonStyle(.plain).accessibilityAddTraits(category == value ? .isSelected : [])
    }
    private func inventoryTile(_ item:InventoryEntry) -> some View {
        Button {
            withAnimation(reduceMotion ? nil : .easeInOut(duration:0.16)) { selectedSlot=item.slot }
        } label: {
            VStack(spacing:6) {
                ZStack(alignment:.bottomTrailing) {
                    ZStack {
                        RadialGradient(colors:[Slate.cyan.opacity(selectedSlot == item.slot ? 0.11 : 0.035),.clear],center:.center,startRadius:0,endRadius:65)
                        ItemArtwork(actor:item.name,type:item.type,size:77)
                    }.frame(maxWidth:.infinity).frame(height:88)
                    Text(item.usesDurability ? InventoryValuePresentation.durabilityText(rawValue:item.value) : item.maximumEditableValue != nil ? "×\(item.value)" : "")
                        .font(.system(size:13,design:.serif)).italic().foregroundStyle(Slate.ivory).shadow(color:.black,radius:3).padding(6)
                    if item.equipped { Image(systemName:"checkmark.circle.fill").font(.system(size:10)).foregroundStyle(Slate.cyan).frame(maxWidth:.infinity,maxHeight:.infinity,alignment:.topLeading).padding(5) }
                }.background(Slate.panel.opacity(0.25)).overlay(RoundedRectangle(cornerRadius:3).stroke(Slate.ivory.opacity(selectedSlot == item.slot ? 0.22 : 0.28),lineWidth:0.55))
                    .overlay { if selectedSlot == item.slot { SelectionGlow() } }
                Text(model.name(for:item.name)).font(.system(size:12,weight:.medium,design:.serif)).italic().lineLimit(2).multilineTextAlignment(.center).frame(height:30,alignment:.top).foregroundStyle(Slate.ivory)
            }.frame(maxWidth:.infinity)
        }.buttonStyle(.plain).modifier(SlateHover()).accessibilityLabel("\(model.name(for:item.name)), \(item.equipped ? "equipped, " : "")\(item.usesDurability ? "durability" : "quantity") \(item.usesDurability ? InventoryValuePresentation.durabilityText(rawValue:item.value) : String(item.value))")
            .accessibilityAddTraits(selectedSlot == item.slot ? .isSelected : [])
    }
    private func inventoryListRow(_ item:InventoryEntry) -> some View {
        Button { selectedSlot=item.slot } label: {
            HStack(spacing:12) {
                ItemArtwork(actor:item.name,type:item.type,size:35)
                Text(model.name(for:item.name)).font(.system(size:13,design:.serif)).italic()
                Spacer()
                Text(item.usesDurability ? InventoryValuePresentation.durabilityText(rawValue:item.value) : item.maximumEditableValue != nil ? "×\(item.value)" : item.equipped ? "Equipped" : (4...6).contains(item.type) ? "1 piece" : "").font(.system(size:12)).foregroundStyle(Slate.muted)
            }.padding(.horizontal,10).padding(.vertical,4).background(selectedSlot == item.slot ? Slate.cyan.opacity(0.1) : Color.clear)
        }.buttonStyle(.plain)
    }
    private var inventoryInspector: some View {
        VStack(alignment:.leading,spacing:0) {
            if let item=selectedItem {
                ScrollView {
                    VStack(alignment:.leading,spacing:18) {
                        ZStack {
                            RadialGradient(colors:[Slate.cyan.opacity(0.07),.clear],center:.center,startRadius:5,endRadius:125)
                            ItemArtwork(actor:item.name,type:item.type,size:158)
                        }.frame(maxWidth:.infinity).frame(height:185)
                        Text(model.name(for:item.name)).font(.system(size:28,weight:.medium,design:.serif)).italic().fixedSize(horizontal:false,vertical:true)
                        Label(typeName(item.type),systemImage:typeIcon(item.type)).font(.system(size:14,design:.serif)).italic().foregroundStyle(Slate.ivory.opacity(0.8))
                        line()
                        if item.name == ItemGrantRules.masterSword {
                            MasterSwordLevelPanel(model:model,item:item).id(item.slot)
                            line()
                        }
                        HStack {
                            Text(item.usesDurability ? "Durability" : item.maximumEditableValue != nil ? "Quantity" : "Item").font(.system(size:14,design:.serif)).italic()
                            Spacer(minLength:8)
                            if item.maximumEditableValue != nil { QuantityControl(value:$itemValue,durability:item.usesDurability).frame(width:116) }
                            else { Text((4...6).contains(item.type) ? "1 piece" : "Read only").font(.system(size:12,design:.serif)).italic().foregroundStyle(Slate.muted) }
                        }
                        if item.usesDurability { Text("Shown in durability points.").font(.system(size:10)).foregroundStyle(Slate.muted) }
                        Button { confirmation=ConfirmAction(title:"Remove \(model.name(for:item.name))?",detail:"This removes the entire selected entry from your running game. Save in BOTW to keep the change.",button:"Remove item",command:.remove(item.slot),destructive:true) } label: { Label("Remove item",systemImage:"trash").font(.system(size:13,design:.serif)).italic().underline() }
                            .buttonStyle(.plain).foregroundStyle(Slate.gold).disabled(!model.canEditInventory(feature:"inventory_edit") || item.equipped)
                        Button { model.send(.quantity(item,commandValue(for:item))) } label: { Text("Apply \(item.usesDurability ? "durability" : "quantity")").frame(maxWidth:.infinity) }
                            .buttonStyle(SlateButtonStyle(primary:true)).frame(maxWidth:.infinity).disabled(!model.canEditInventory(feature:"inventory_edit") || !item.canEditValue || commandValue(for:item).isEmpty)
                        if item.equipped { inventoryNotice("Unequip this item in BOTW before changing or removing it.") }
                        DisclosureGroup("Item details") {
                            VStack(alignment:.leading,spacing:5) { Text(item.name).textSelection(.enabled);Text("Slot \(item.slot)") }.font(.system(size:10,design:.monospaced)).foregroundStyle(Slate.muted).frame(maxWidth:.infinity,alignment:.leading).padding(.top,6)
                        }.font(.system(size:10)).foregroundStyle(Slate.muted)
                    }.padding(.horizontal,22).padding(.top,25).padding(.bottom,20)
                }.scrollIndicators(.hidden)
            } else {
                Spacer()
                emptyState("Select an item",icon:"cursorarrow",detail:"Choose something from your pouch to inspect or edit it.").padding(20)
                Spacer()
            }
        }.frame(maxHeight:.infinity).background(Slate.panel.opacity(0.25))
    }

    private var playerTab: some View {
        ScrollView {
          VStack(alignment:.leading,spacing:22) {
            HStack {
                VStack(alignment:.leading,spacing:5) {
                    Text("Player").font(.system(size:26,weight:.medium,design:.serif)).italic()
                    Text("Health, stamina and abilities").font(.system(size:12)).foregroundStyle(Slate.muted)
                }
                Spacer()
                Label(model.available ? "Live session" : "Session required",systemImage:"waveform.path").font(.system(size:11)).foregroundStyle(model.available ? Slate.cyan : Slate.muted)
            }
            HStack(alignment:.top,spacing:22) {
                VStack(alignment:.leading,spacing:0) {
                    panelTitle("Hearts & stamina",icon:"heart")
                    maximumRow(title:"Maximum hearts",detail:maximumHearts.map { "Current maximum: \(number($0)) hearts" } ?? "Current maximum unavailable",symbol:"heart.fill",color:Color(red:0.96,green:0.52,blue:0.44),value:$maxHearts,range:"1–30",enabled:model.canSetMaximum("health")) { model.send(.maxHearts(maxHearts)) }
                    line().padding(.horizontal,17)
                    maximumRow(title:"Stamina wheels",detail:maximumStamina.map { "Current maximum: \(number($0)) wheels" } ?? "Current maximum unavailable",symbol:"circle.dashed",color:Color(red:0.57,green:0.87,blue:0.56),value:$maxStamina,range:"1–3",enabled:model.canSetMaximum("stamina")) { model.send(.maxStamina(maxStamina)) }
                    if model.state?.playerCacheReady == false {
                        Label("Player stats are still loading.",systemImage:"clock").font(.system(size:10)).foregroundStyle(Slate.gold).padding(.horizontal,18).padding(.bottom,16)
                    }
                }.background(Slate.panel).overlay(Rectangle().stroke(Slate.line,lineWidth:1))
                VStack(alignment:.leading,spacing:0) {
                    panelTitle("Rupees",icon:"diamond")
                    VStack(alignment:.leading,spacing:16) {
                        HStack(alignment:.firstTextBaseline,spacing:8) {
                            Image(systemName:"diamond.fill").foregroundStyle(Slate.cyan)
                            Text(liveRupees).font(.system(size:29,weight:.light)).monospacedDigit()
                            Text("in your wallet").font(.system(size:10)).foregroundStyle(Slate.muted)
                        }
                        HStack(spacing:10) {
                            TextField("Amount",text:$rupees).textFieldStyle(.roundedBorder).accessibilityLabel("New rupee amount, 0 to 999999")
                            Button("Set rupees") { model.send(.rupees(rupees)) }.buttonStyle(SlateButtonStyle(primary:true)).disabled(!model.canUse("rupees"))
                        }
                        Text("Choose an amount from 0 to 999,999.").font(.system(size:10)).foregroundStyle(Slate.muted)
                    }.padding(18)
                }.background(Slate.panel).overlay(Rectangle().stroke(Slate.line,lineWidth:1))
            }
            HStack(alignment:.top,spacing:22) {
                VStack(alignment:.leading,spacing:0) {
                    panelTitle("Exploration assists",icon:"figure.walk")
                    VStack(spacing:17) {
                        assistToggle("Lock current health",detail:"Hold the health value you have now.",icon:"heart",isOn:Binding(get:{ model.state?.healthLock ?? false },set:{ model.send(.health($0)) }),enabled:model.canUse("health"))
                        line()
                        assistToggle("Keep stamina full",detail:"Refill stamina while you explore.",icon:"bolt",isOn:Binding(get:{ model.state?.staminaLock ?? false },set:{ model.send(.stamina($0)) }),enabled:model.canUse("stamina"))
                    }.padding(18)
                }.background(Slate.panel).overlay(Rectangle().stroke(Slate.line,lineWidth:1))
                VStack(alignment:.leading,spacing:0) {
                    panelTitle("Abilities",icon:"sparkle")
                    VStack(alignment:.leading,spacing:14) {
                        Text("Make more of Hyrule accessible.").font(.system(size:12)).foregroundStyle(Slate.muted)
                        HStack {
                            Button("Unlock runes…") { confirmRunes() }.buttonStyle(SlateButtonStyle()).disabled(!model.canUse("runes"))
                            Button("Get paraglider…") { confirmParaglider() }.buttonStyle(SlateButtonStyle()).disabled(!model.canUse("runes"))
                        }
                        Text("Unlocks apply to your current gameplay session.").font(.system(size:10)).foregroundStyle(Slate.muted)
                    }.padding(18)
                }.background(Slate.panel).overlay(Rectangle().stroke(Slate.line,lineWidth:1))
            }
            PlayerHorseSpeedPanel(model:model)
            Label("Save in BOTW to keep stat and ability changes. Horse speed applies only to this session.",systemImage:"info.circle").font(.system(size:11)).foregroundStyle(Slate.muted)
          }.padding(26)
        }.scrollIndicators(.hidden)
    }
    private func maximumRow(title:String,detail:String,symbol:String,color:Color,value:Binding<String>,range:String,enabled:Bool,action:@escaping ()->Void) -> some View {
        HStack(spacing:13) {
            Image(systemName:symbol).font(.system(size:19)).foregroundStyle(color).frame(width:24)
            VStack(alignment:.leading,spacing:5) { Text(title).font(.system(size:12,weight:.medium)); Text(detail).font(.system(size:10)).foregroundStyle(Slate.muted) }
            Spacer(minLength:5)
            VStack(spacing:4) { TextField(range,text:value).textFieldStyle(.roundedBorder).frame(width:47).accessibilityLabel("\(title), \(range)"); Text(range).font(.system(size:9)).foregroundStyle(Slate.muted) }
            Button("Apply",action:action).buttonStyle(SlateButtonStyle(primary:true)).disabled(!enabled)
        }.padding(17)
    }
    private func assistToggle(_ title:String,detail:String,icon:String,isOn:Binding<Bool>,enabled:Bool) -> some View {
        HStack(spacing:12) {
            Image(systemName:icon).font(.system(size:17)).foregroundStyle(Slate.cyan).frame(width:23)
            VStack(alignment:.leading,spacing:4) { Text(title).font(.system(size:12,weight:.medium)); Text(detail).font(.system(size:10)).foregroundStyle(Slate.muted) }
            Spacer()
            Toggle(title,isOn:isOn).labelsHidden().toggleStyle(.switch).controlSize(.small).disabled(!enabled).accessibilityLabel(title)
        }
    }

    private var worldTab: some View {
        HStack(spacing:0) {
            VStack(alignment:.leading,spacing:12) {
                SearchBox(placeholder:"Find a shrine or tower",text:$worldSearch)
                HStack(spacing:6) {
                    ForEach(["All","Shrines","Towers"],id:\.self) { kind in
                        Button { worldKind=kind } label: { Text(kind).font(.system(size:12,design:.serif)).italic().frame(maxWidth:.infinity).frame(height:29).foregroundStyle(worldKind == kind ? Slate.cyan : Slate.ivory) }
                            .buttonStyle(.plain).background(worldKind == kind ? Slate.cyan.opacity(0.08) : Slate.raised.opacity(0.2),in:RoundedRectangle(cornerRadius:3)).overlay(RoundedRectangle(cornerRadius:3).stroke(worldKind == kind ? Slate.cyan : Slate.ivory.opacity(0.3),lineWidth:0.7))
                    }
                }
                line().padding(.top,3)
                if filteredDestinations.isEmpty {
                    emptyState("No destinations",icon:"mappin",detail:destinations.isEmpty ? "Load your game to see local travel points." : "Try a different search.").frame(maxHeight:.infinity)
                } else {
                    ScrollViewReader { proxy in
                        ScrollView {
                            LazyVStack(spacing:2) { ForEach(filteredDestinations) { point in destinationRow(point).id(point.id) } }
                        }.scrollIndicators(.hidden).onChange(of:selectedDestination) { _,id in if let id { withAnimation(reduceMotion ? nil : .easeInOut(duration:0.15)) { proxy.scrollTo(id,anchor:.center) } } }
                    }
                }
                line()
                Menu {
                    Button("Unlock runes…") { confirmRunes() }.disabled(!model.canUse("runes"))
                    Button("Get paraglider…") { confirmParaglider() }.disabled(!model.canUse("runes"))
                    Divider()
                    Button("Full access…") { confirmFullAccess() }.disabled(!model.canUse("runes"))
                    Divider()
                    Button("Mark all 120 base shrines cleared…") { confirmAllShrinesCleared() }.disabled(!model.canUse("shrine_completion"))
                } label: { HStack { Image(systemName:"chevron.down").font(.system(size:9));Text("World access").font(.system(size:14,design:.serif)).italic();Spacer() } }
                    .menuStyle(.borderlessButton).padding(.vertical,6)
                mapAccessControl
                Button { confirmMap() } label: { Label("Reveal map",systemImage:"map").frame(maxWidth:.infinity,alignment:.leading) }.buttonStyle(SlateButtonStyle()).disabled(!model.canUse("map"))
                Button { confirmAllShrines() } label: { Label("Activate shrine travel…",systemImage:"diamond").frame(maxWidth:.infinity,alignment:.leading) }.buttonStyle(SlateButtonStyle()).disabled(!model.canUse("shrines"))
                if model.available, let count=model.state?.shrineClearCount {
                    Text("Shrines cleared: \(count)").font(.system(size:10)).foregroundStyle(Slate.muted).padding(.horizontal,4)
                }
            }.padding(.horizontal,13).padding(.top,22).padding(.bottom,20).frame(width:224).background { Slate.panel.opacity(0.35);ContourEngraving().clipped() }
            line(vertical:true)
            HyruleMapView(points:destinations,visibleIDs:Set(filteredDestinations.map(\.id)),selected:$selectedDestination,playerPosition:model.available ? model.state?.position : nil)
                .overlay(alignment:.bottomTrailing) { destinationInspector.padding(.trailing,18).padding(.bottom,18) }
                .frame(maxWidth:.infinity,maxHeight:.infinity)
        }
    }
    private var mapAccessControl: some View {
        let unlocked=model.available && model.state?.mapAccessUnlocked == true
        let supported=model.supports("map_access")
        return VStack(alignment:.leading,spacing:6) {
            Button { confirmMapAccess() } label: {
                Label(unlocked ? "Map access enabled" : "Enable in-game map",systemImage:unlocked ? "checkmark.circle" : "lock.open")
                    .frame(maxWidth:.infinity,alignment:.leading)
            }.buttonStyle(SlateButtonStyle()).disabled(unlocked || !model.canUse("map_access"))
                .help(supported ? "Open the map with Tab or M in BOTW." : "Runtime update required for in-game map access.")
            Text(supported ? "Tab or M in BOTW" : "Runtime update required")
                .font(.system(size:10)).foregroundStyle(supported ? Slate.muted : Slate.gold).padding(.horizontal,4)
        }
    }
    private func destinationRow(_ point:WorldDestination) -> some View {
        let tower=point.kind.localizedCaseInsensitiveContains("tower")
        return Button {
            withAnimation(reduceMotion ? nil : .easeInOut(duration:0.15)) { selectedDestination=point.id }
        } label: {
            HStack(spacing:9) {
                DestinationSymbol(tower:tower,color:tower ? Slate.gold : point.unlocked == true ? Slate.cyan : Slate.orange)
                Text(point.name).font(.system(size:13,weight:.medium,design:.serif)).italic().foregroundStyle(Slate.ivory.opacity(selectedDestination == point.id ? 1 : 0.86)).lineLimit(2)
                Spacer(minLength:0)
                if point.unlocked == true { Image(systemName:"checkmark").font(.system(size:10)).foregroundStyle(tower ? Slate.gold : Slate.cyan) }
            }.padding(.horizontal,7).frame(minHeight:43).background(selectedDestination == point.id ? Slate.cyan.opacity(0.10) : Color.clear,in:RoundedRectangle(cornerRadius:3))
                .overlay(RoundedRectangle(cornerRadius:3).stroke(selectedDestination == point.id ? Slate.cyan : .clear,lineWidth:0.8))
        }.buttonStyle(.plain).accessibilityAddTraits(selectedDestination == point.id ? .isSelected : [])
    }
    private var destinationInspector: some View {
        VStack(alignment:.leading,spacing:13) {
            if let point=destination {
                HStack(spacing:14) {
                    DestinationSymbol(tower:point.kind.localizedCaseInsensitiveContains("tower"),color:point.kind.localizedCaseInsensitiveContains("tower") ? Slate.gold : Slate.cyan).scaleEffect(1.5).frame(width:32,height:38)
                    VStack(alignment:.leading,spacing:6) {
                        Text(point.name).font(.system(size:22,weight:.medium,design:.serif)).italic().fixedSize(horizontal:false,vertical:true)
                        HStack(spacing:5) {
                            Image(systemName:point.unlocked == true ? "checkmark" : "lock")
                            Text(point.unlocked == true ? "Activated" : "Not activated")
                        }.font(.system(size:12,design:.serif)).italic().foregroundStyle(point.unlocked == true ? Slate.cyan : Slate.gold)
                        if !point.kind.localizedCaseInsensitiveContains("tower"), let completed=point.completed {
                            Label(completed ? "Cleared" : "Not cleared",systemImage:completed ? "checkmark.seal" : "circle")
                                .font(.system(size:10)).foregroundStyle(completed ? Slate.cyan : Slate.muted)
                        }
                    }
                    Spacer(minLength:0)
                }
                HStack(spacing:10) {
                    Button { confirmation=ConfirmAction(title:"Travel to \(point.name)?",detail:"BOTW will load this destination using its normal travel system. Wait for loading to finish before sending another edit.",button:"Travel",command:.teleport(point.id)) } label: { Label("Teleport here",systemImage:"scope").frame(maxWidth:.infinity) }
                        .buttonStyle(SlateButtonStyle(primary:true)).disabled(!model.canUse("teleport") || !point.permitsTeleport)
                    if !point.kind.localizedCaseInsensitiveContains("tower") {
                        Button("Activate shrine") { confirmation=ConfirmAction(title:"Activate \(point.name)?",detail:"Enable this shrine's travel point. This does not mark the shrine completed. Save in BOTW to keep it.",button:"Activate shrine",command:.unlockShrine(point.id)) }.buttonStyle(SlateButtonStyle()).disabled(!model.canUse("shrines"))
                    }
                }
                if !point.kind.localizedCaseInsensitiveContains("tower") {
                    Button { confirmShrineCleared(point) } label: {
                        Label(point.completed == true ? "Shrine marked cleared" : "Mark cleared…",systemImage:"checkmark.seal")
                            .frame(maxWidth:.infinity)
                    }.buttonStyle(SlateButtonStyle()).disabled(point.completed == true || !model.canUse("shrine_completion"))
                        .help(model.supports("shrine_completion") ? "Marks completion without granting Spirit Orbs, quests, chests or rewards." : "Runtime update required to mark shrines cleared.")
                }
                if !point.permitsTeleport { Text("Activate the first tower in BOTW or use Full Access before tower travel.").font(.system(size:10)).foregroundStyle(Slate.gold).fixedSize(horizontal:false,vertical:true) }
            } else {
                HStack(spacing:10) { DestinationSymbol();Text("Select a destination to travel").font(.system(size:15,design:.serif)).italic() }
            }
        }.padding(18).frame(width:364,alignment:.leading).background { Slate.background.opacity(0.94);ContourEngraving().clipped() }
            .overlay(RoundedRectangle(cornerRadius:5).stroke(Slate.gold.opacity(0.65),lineWidth:0.7)).clipShape(RoundedRectangle(cornerRadius:5))
    }

    private var feedbackBar: some View {
        HStack(spacing:8) {
            if model.pending { ProgressView().controlSize(.mini).frame(width:12,height:12) }
            else { Image(systemName:model.feedbackIsError ? "exclamationmark.circle" : "waveform.path").font(.system(size:11)).foregroundStyle(model.feedbackIsError ? Slate.orange : Slate.muted) }
            Text(model.feedback).font(.system(size:10)).foregroundStyle(model.feedbackIsError ? Slate.orange : Slate.muted).lineLimit(2).textSelection(.enabled)
            Spacer()
            Text("BOTW 1.6.0").font(.system(size:9)).foregroundStyle(Slate.muted.opacity(0.65))
        }.padding(.horizontal,18).frame(minHeight:31).background(Slate.panel.opacity(0.7)).overlay(alignment:.top) { line() }.accessibilityElement(children:.combine)
    }
    private func panelTitle(_ text:String,icon:String) -> some View {
        HStack(spacing:9) { Image(systemName:icon).foregroundStyle(Slate.cyan); Text(text).font(.system(size:12,weight:.semibold)); Spacer() }.padding(.horizontal,18).frame(height:43).overlay(alignment:.bottom) { line() }
    }
    private func line(vertical:Bool=false) -> some View { Rectangle().fill(Slate.line).frame(width:vertical ? 1 : nil,height:vertical ? nil : 1) }
    private func emptyState(_ title:String,icon:String,detail:String) -> some View {
        VStack(spacing:12) { Image(systemName:icon).font(.system(size:27,weight:.ultraLight)).foregroundStyle(Slate.cyan.opacity(0.65)); Text(title).font(.system(size:18,weight:.medium,design:.serif)).italic(); Text(detail).font(.system(size:11)).foregroundStyle(Slate.muted).multilineTextAlignment(.center).fixedSize(horizontal:false,vertical:true).frame(maxWidth:260) }.frame(maxWidth:.infinity)
    }
    private func commandValue(for item:InventoryEntry) -> String {
        InventoryValuePresentation.commandText(for:item,input:itemValue)
    }
    private func selectFirstItemIfNeeded() {
        if selectedItem == nil { selectedSlot = filteredInventory.first?.slot ?? inventory.first?.slot }
    }
    private func confirmAllShrines() { confirmation = ConfirmAction(title:"Activate all shrine travel?",detail:"Activate all 120 base shrine travel points. This does not mark shrines completed. Save in BOTW to keep the changes.",button:"Activate travel points",command:.unlockAllShrines) }
    private func confirmShrineCleared(_ point:WorldDestination) { confirmation = ConfirmAction(title:"Mark \(point.name) cleared?",detail:"Mark this shrine cleared, discover it and activate its travel point. The shrine clear count increases once. This does not grant Spirit Orbs, complete quests, empty chests or grant other rewards. Save in BOTW to keep the change.",button:"Mark cleared",command:.markShrineCleared(point.id)) }
    private func confirmAllShrinesCleared() { confirmation = ConfirmAction(title:"Mark all 120 base shrines cleared?",detail:"Mark the 120 base-game shrines cleared, discover them and activate their travel points. The clear count increases only for newly cleared shrines. This does not grant Spirit Orbs, complete quests, empty chests or grant other rewards. Save in BOTW to keep these changes.",button:"Mark all cleared",command:.markAllShrinesCleared) }
    private func confirmMapAccess() { confirmation = ConfirmAction(title:"Enable the in-game map?",detail:"Enable the Slate’s map screen, then open it with Tab or M in BOTW. Map regions are managed separately by Reveal map.",button:"Enable map access",command:.enableMapAccess) }
    private func confirmMap() { confirmation = ConfirmAction(title:"Reveal all map regions?",detail:"Reveal the map regions in your current game. Save in BOTW to keep the change.",button:"Reveal map",command:.revealMap) }
    private func confirmRunes() { confirmation = ConfirmAction(title:"Unlock runes?",detail:"Acquire the base runes and enable Sheikah Slate access. In BOTW, hold 3 to choose a rune, then release 3. Press F to use the selected rune. Save in BOTW to keep the change.",button:"Unlock runes",command:.runes) }
    private func confirmParaglider() { confirmation = ConfirmAction(title:"Get the paraglider?",detail:"Grant the paraglider in your current game. Save in BOTW to keep it.",button:"Get paraglider",command:.paraglider) }
    private func confirmFullAccess() { confirmation = ConfirmAction(title:"Unlock exploration access?",detail:"Activate shrine travel, reveal the map, raise every tower, unlock runes, and grant the paraglider. This skips the opening tower activation sequence. It does not directly mark shrines or story quests complete. Save in BOTW to keep these changes.",button:"Unlock full access",command:.fullAccess) }
}

private struct PlayerHorseSpeedPanel: View {
    @ObservedObject var model: PlaygroundModel
    @State private var chosenPercent: Double = 100

    private var supported: Bool { model.supports("horse_speed") }
    private var currentPercent: Int? {
        guard model.available, model.state?.horseSpeedAvailable == true,
              let value=model.state?.horseSpeedMultiplier, value.isFinite, (1...2).contains(value),
              let active=model.state?.horseSpeedActive else { return nil }
        let percent=Int((value*100).rounded())
        return active == (percent > 100) ? percent : nil
    }
    private var canApply: Bool { model.canUse("horse_speed") && currentPercent != nil }
    private var currentLabel: String {
        guard let percent=currentPercent else { return "Awaiting live values" }
        return percent == 100 ? "Off · 1.00×" : String(format:"%.2f× active",Double(percent)/100)
    }
    private var unavailableReason: String? {
        if !supported { return "Runtime update required" }
        if !model.available { return "Load your gameplay session to change horse speed." }
        if currentPercent == nil { return model.state?.horseSpeedReason ?? "Waiting for verified horse movement data." }
        return nil
    }
    var body: some View {
        VStack(alignment:.leading,spacing:14) {
            HStack {
                Label("Horse speed",systemImage:"figure.equestrian.sports")
                    .font(.system(size:16,weight:.medium,design:.serif)).italic().foregroundStyle(Slate.cyan)
                Spacer()
                Text(currentLabel).font(.system(size:11)).monospacedDigit()
                    .foregroundStyle(currentPercent.map { $0 > 100 } == true ? Slate.cyan : Slate.muted)
            }
            HStack(spacing:14) {
                Text("1.00×").font(.system(size:10)).foregroundStyle(Slate.muted)
                Slider(value:$chosenPercent,in:100...200,step:5)
                    .disabled(!canApply).accessibilityLabel("Standard horse speed multiplier")
                    .accessibilityValue(String(format:"%.2f times normal",chosenPercent/100))
                Text("2.00×").font(.system(size:10)).foregroundStyle(Slate.muted)
                Text(String(format:"%.2f×",chosenPercent/100)).font(.system(size:18,weight:.light)).monospacedDigit().frame(width:65)
                Button("Apply") { model.send(.horseSpeed(Int(chosenPercent.rounded()))) }
                    .buttonStyle(SlateButtonStyle(primary:true)).disabled(!canApply)
                Button("Off") { model.send(.horseSpeed(nil)) }
                    .buttonStyle(SlateButtonStyle()).disabled(!canApply || model.state?.horseSpeedActive != true)
            }
            if let unavailableReason {
                Label(unavailableReason,systemImage:supported ? "info.circle" : "arrow.down.square")
                    .font(.system(size:10)).foregroundStyle(Slate.gold).fixedSize(horizontal:false,vertical:true)
            }
            Text("Applies to all standard horses this session. Special mounts are unaffected. Restarts at Off; this setting is not saved.")
                .font(.system(size:10)).foregroundStyle(Slate.muted).fixedSize(horizontal:false,vertical:true)
        }.padding(18).background(Slate.panel).overlay(Rectangle().stroke(Slate.line,lineWidth:1))
            .onAppear { synchronizeSelection() }
            .onChange(of:model.state?.horseSpeedMultiplier) { _,_ in synchronizeSelection() }
            .onChange(of:model.available) { _,_ in synchronizeSelection() }
    }
    private func synchronizeSelection() {
        if let currentPercent { chosenPercent=Double(currentPercent) }
    }
}

private struct PouchSlotsView: View {
    @ObservedObject var model: PlaygroundModel
    @Environment(\.dismiss) private var dismiss
    private var supported: Bool { model.supports("pouch_slots") }

    var body: some View {
        VStack(alignment:.leading,spacing:16) {
            HStack {
                VStack(alignment:.leading,spacing:5) {
                    Text("Pouch slots").font(.system(size:26,weight:.medium,design:.serif)).italic()
                    Text("Adds capacity; does not grant items.").font(.system(size:12)).foregroundStyle(Slate.muted)
                }
                Spacer()
                Image(systemName:"backpack").font(.system(size:27,weight:.light)).foregroundStyle(Slate.cyan)
            }
            Rectangle().fill(Slate.line).frame(height:1)
            if !supported {
                Label("Runtime update required. Install the updated editor bridge to change pouch slots.",systemImage:"arrow.down.square")
                    .font(.system(size:11)).foregroundStyle(Slate.gold).fixedSize(horizontal:false,vertical:true)
            } else if !model.available {
                Label(model.connectionMessage,systemImage:"info.circle").font(.system(size:11)).foregroundStyle(Slate.gold)
            } else if let reason=model.inventoryBlockedReason {
                Label(reason,systemImage:"info.circle").font(.system(size:11)).foregroundStyle(Slate.gold).fixedSize(horizontal:false,vertical:true)
            }
            VStack(spacing:0) {
                ForEach(PouchSlotKind.allCases) { kind in
                    PouchSlotCapacityRow(model:model,kind:kind)
                    if kind != PouchSlotKind.allCases.last { Rectangle().fill(Slate.line).frame(height:1) }
                }
            }.background(Slate.panel.opacity(0.5)).overlay(Rectangle().stroke(Slate.line,lineWidth:1))
            Text("Capacity can only increase. Each Apply button changes its own pouch.")
                .font(.system(size:11)).foregroundStyle(Slate.muted)
            Spacer(minLength:0)
            Rectangle().fill(Slate.line).frame(height:1)
            HStack(spacing:10) {
                if model.pending { ProgressView().controlSize(.small) }
                Text(model.feedback).font(.system(size:11)).foregroundStyle(model.feedbackIsError ? Slate.orange : Slate.muted).lineLimit(3).fixedSize(horizontal:false,vertical:true)
                Spacer(minLength:4)
                Button("Done") { dismiss() }.buttonStyle(SlateButtonStyle()).keyboardShortcut(.cancelAction)
            }
        }.padding(25).frame(width:620,height:480).background { Slate.background;ContourEngraving() }
            .foregroundStyle(Slate.ivory).tint(Slate.cyan).preferredColorScheme(.dark)
    }
}

private struct PouchSlotCapacityRow: View {
    @ObservedObject var model: PlaygroundModel
    let kind: PouchSlotKind
    @State private var target: Int

    init(model: PlaygroundModel, kind: PouchSlotKind) {
        self.model=model
        self.kind=kind
        let value=model.available ? model.state?.slots(for:kind) : nil
        _target=State(initialValue:value.flatMap { (kind.minimum...kind.maximum).contains($0) ? $0 : nil } ?? kind.minimum)
    }
    private var current: Int? {
        guard model.available, let value=model.state?.slots(for:kind), (kind.minimum...kind.maximum).contains(value) else { return nil }
        return value
    }
    private var lowerBound: Int { current ?? kind.minimum }
    private var icon: String {
        switch kind { case .weapons:return "figure.fencing";case .bows:return "arrow.up.right";case .shields:return "shield.lefthalf.filled" }
    }
    private var canApply: Bool {
        guard let current else { return false }
        return model.supports("pouch_slots") && model.canEditInventory(feature:"pouch_slots") &&
            target > current && target <= kind.maximum
    }
    var body: some View {
        HStack(spacing:15) {
            Image(systemName:icon).font(.system(size:19)).foregroundStyle(Slate.cyan).frame(width:24)
            VStack(alignment:.leading,spacing:5) {
                Text(kind.title).font(.system(size:16,weight:.medium,design:.serif)).italic()
                Text(current.map { "\($0) / \(kind.maximum) slots" } ?? "Current capacity unavailable")
                    .font(.system(size:10)).foregroundStyle(Slate.muted)
            }
            Spacer(minLength:5)
            HStack(spacing:10) {
                Text("\(target)").font(.system(size:17,design:.serif)).monospacedDigit().frame(width:24)
                Stepper("New \(kind.title.lowercased()) capacity",value:$target,in:lowerBound...kind.maximum).labelsHidden().fixedSize()
                    .disabled(current == nil || !model.supports("pouch_slots") || model.pending)
            }
            Button("Apply") { model.send(.pouchSlots(kind,String(target))) }
                .buttonStyle(SlateButtonStyle(primary:true)).disabled(!canApply)
        }.padding(.horizontal,17).frame(height:70)
            .onAppear { synchronizeTarget() }
            .onChange(of:current) { _,_ in synchronizeTarget() }
    }
    private func synchronizeTarget() {
        target=max(lowerBound,min(kind.maximum,target))
    }
}

private struct MasterSwordLevelPanel: View {
    @ObservedObject var model: PlaygroundModel
    let item: InventoryEntry
    @State private var chosenLevel: MasterSwordLevel?

    private var supported: Bool { model.supports("master_sword") }
    private var currentLevel: MasterSwordLevel? {
        guard model.available, let value = model.state?.masterSwordLevel else { return nil }
        return MasterSwordLevel(rawValue:value)
    }
    private var hasLiveValues: Bool {
        guard model.available, let attack=model.state?.masterSwordAttack,
              model.state?.masterSwordFullPower != nil else { return false }
        return (30...60).contains(attack)
    }
    private var canApply: Bool {
        supported && hasLiveValues && chosenLevel != nil && !item.equipped &&
            model.canEditInventory(feature:"master_sword")
    }
    var body: some View {
        VStack(alignment:.leading,spacing:11) {
            Label("Awakening level",systemImage:"sparkles")
                .font(.system(size:15,weight:.medium,design:.serif)).italic().foregroundStyle(Slate.cyan)
            if !supported {
                Label("Runtime update required",systemImage:"arrow.down.square")
                    .font(.system(size:11,weight:.medium)).foregroundStyle(Slate.gold)
                Text("Install the updated editor bridge to change this level.")
                    .font(.system(size:10)).foregroundStyle(Slate.muted).fixedSize(horizontal:false,vertical:true)
            } else {
                currentReadback
            }
            Picker("Master Sword awakening level",selection:$chosenLevel) {
                Text("Choose level").tag(nil as MasterSwordLevel?)
                ForEach(MasterSwordLevel.allCases) { level in
                    Text(level.title).tag(level as MasterSwordLevel?)
                }
            }.labelsHidden().pickerStyle(.menu).frame(maxWidth:.infinity)
                .disabled(!supported || !model.available || item.equipped)
            Button {
                if let chosenLevel { model.send(.masterSwordLevel(chosenLevel)) }
            } label: { Text("Apply level").frame(maxWidth:.infinity) }
                .buttonStyle(SlateButtonStyle(primary:true)).disabled(!canApply)
            Text("Normal attack; the game can awaken it near enemies.")
                .font(.system(size:10)).foregroundStyle(Slate.muted).fixedSize(horizontal:false,vertical:true)
            if supported && item.equipped {
                Text("Unequip Master Sword in BOTW first.").font(.system(size:10)).foregroundStyle(Slate.gold)
            }
        }
        .onAppear { chosenLevel=currentLevel }
        .onChange(of:item.slot) { _,_ in chosenLevel=currentLevel }
        .onChange(of:model.state?.masterSwordLevel) { _,_ in chosenLevel=currentLevel }
        .onChange(of:model.available) { _,_ in chosenLevel=currentLevel }
    }
    private var currentReadback: some View {
        VStack(alignment:.leading,spacing:5) {
            if hasLiveValues {
                Text(currentLevel.map { "Current: \($0.title)" } ?? "Current: Custom configuration")
                    .font(.system(size:11)).foregroundStyle(Slate.ivory)
                if let attack=model.state?.masterSwordAttack {
                    Text("Normal attack: \(attack)").font(.system(size:10)).foregroundStyle(Slate.muted)
                }
                if model.state?.masterSwordFullPower == true {
                    Label("Full power enabled",systemImage:"checkmark.circle").font(.system(size:10)).foregroundStyle(Slate.cyan)
                }
            } else {
                Text("Waiting for live sword values.").font(.system(size:10)).foregroundStyle(Slate.muted)
            }
        }
    }
}

private struct TerrainManifest: Decodable {
    struct Bounds: Decodable {
        let minX: Double, maxX: Double, minZ: Double, maxZ: Double
        enum CodingKeys:String,CodingKey { case minX="min_x", maxX="max_x", minZ="min_z", maxZ="max_z" }
    }
    let schema:Int
    let image:String
    let width:Int
    let height:Int
    let orientation:String
    let worldBounds:Bounds
    enum CodingKeys:String,CodingKey { case schema,image,width,height,orientation; case worldBounds="world_bounds" }
}

private struct TerrainArtwork {
    let image:NSImage
    let bounds:TerrainManifest.Bounds
    static let local:TerrainArtwork? = {
        let root = GameArtwork.root.appendingPathComponent("map",isDirectory:true)
        guard let data = try? Data(contentsOf:root.appendingPathComponent("map.json")),
              let manifest = try? JSONDecoder().decode(TerrainManifest.self,from:data),
              manifest.schema == 1, manifest.orientation == "north-up", manifest.image == "map.png",
              manifest.width > 0,manifest.height > 0,
              manifest.worldBounds.minX.isFinite,manifest.worldBounds.maxX.isFinite,manifest.worldBounds.minZ.isFinite,manifest.worldBounds.maxZ.isFinite,
              manifest.worldBounds.maxX > manifest.worldBounds.minX,manifest.worldBounds.maxZ > manifest.worldBounds.minZ,
              let source = CGImageSourceCreateWithURL(root.appendingPathComponent(manifest.image) as CFURL,nil),
              let cg = CGImageSourceCreateThumbnailAtIndex(source,0,[kCGImageSourceCreateThumbnailFromImageAlways:true,kCGImageSourceThumbnailMaxPixelSize:4096,kCGImageSourceCreateThumbnailWithTransform:true] as CFDictionary)
        else { return nil }
        return TerrainArtwork(image:NSImage(cgImage:cg,size:NSSize(width:manifest.width,height:manifest.height)),bounds:manifest.worldBounds)
    }()
}

private struct TerrainLabel: Decodable {
    let id: String
    let name: String
    let x: Double
    let z: Double
    let level: Int
    let kind: String

    func isVisible(at zoom: CGFloat) -> Bool {
        switch kind {
        case "region": return zoom < 1.7
        case "area": return zoom >= 1.35 && zoom < 3.5
        case "settlement": return zoom >= 1.8
        default: return zoom >= (level == 8 ? 4.5 : 2.8)
        }
    }
    var fontSize: CGFloat {
        switch kind { case "region":return 16;case "area":return 14;case "settlement":return 12;default:return 11 }
    }
}

private enum TerrainLabels {
    private struct Manifest: Decodable {
        let schema: Int
        let labels: [TerrainLabel]
    }
    static let local: [TerrainLabel] = {
        let url=GameArtwork.root.appendingPathComponent("map/labels.json")
        guard let size=try? url.resourceValues(forKeys:[.fileSizeKey]).fileSize,size <= 512*1024,
              let data=try? Data(contentsOf:url),let manifest=try? JSONDecoder().decode(Manifest.self,from:data),
              manifest.schema == 1,!manifest.labels.isEmpty,manifest.labels.count <= 1000,
              Set(manifest.labels.map(\.id)).count == manifest.labels.count,
              manifest.labels.allSatisfy({ label in
                  !label.id.isEmpty && label.id.count <= 96 && !label.name.isEmpty && label.name.count <= 96 &&
                  label.name.unicodeScalars.allSatisfy { !CharacterSet.controlCharacters.contains($0) } &&
                  label.x.isFinite && label.z.isFinite && abs(label.x) <= 6000 && abs(label.z) <= 5000 &&
                  [1,2,4,8].contains(label.level) && ["region","area","settlement","landmark"].contains(label.kind)
              }) else { return [] }
        return manifest.labels
    }()
}

/// A stable north-up world projection. Search changes emphasis, never map bounds.
private struct HyruleMapView: View {
    let points:[WorldDestination]
    let visibleIDs:Set<String>
    @Binding var selected:String?
    let playerPosition:WorldPosition?
    @State private var zoom:CGFloat = 1
    @State private var previousZoom:CGFloat = 1
    @State private var offset:CGSize = .zero
    @GestureState private var drag:CGSize = .zero
    @State private var hover:String?
    @Environment(\.accessibilityReduceMotion) private var reduceMotion
    private let terrain = TerrainArtwork.local
    private let labels = TerrainLabels.local
    private var bounds:TerrainManifest.Bounds { terrain?.bounds ?? TerrainManifest.Bounds(minX:-6000,maxX:6000,minZ:-5000,maxZ:5000) }

    private func mapRect(_ size:CGSize) -> CGRect {
        let aspect = (bounds.maxX-bounds.minX)/(bounds.maxZ-bounds.minZ)
        let width = min(size.width,size.height*aspect)*zoom
        let height = width/aspect
        let pan = clamped(CGSize(width:offset.width+drag.width,height:offset.height+drag.height),size:size,width:width,height:height)
        return CGRect(x:(size.width-width)/2+pan.width,y:(size.height-height)/2+pan.height,width:width,height:height)
    }
    private func clamped(_ pan:CGSize,size:CGSize,width:CGFloat,height:CGFloat) -> CGSize {
        CGSize(width:max(-max((width-size.width)/2,0),min(max((width-size.width)/2,0),pan.width)),height:max(-max((height-size.height)/2,0),min(max((height-size.height)/2,0),pan.height)))
    }
    private func position(_ point:WorldDestination,in rect:CGRect) -> CGPoint {
        CGPoint(x:rect.minX+(point.x-bounds.minX)/(bounds.maxX-bounds.minX)*rect.width,y:rect.minY+(point.z-bounds.minZ)/(bounds.maxZ-bounds.minZ)*rect.height)
    }
    private func nearest(_ location:CGPoint,rect:CGRect) -> WorldDestination? {
        let eligible = points.filter { visibleIDs.contains($0.id) && $0.x.isFinite && $0.z.isFinite }
        guard let point = eligible.min(by:{ let a=position($0,in:rect),b=position($1,in:rect); return hypot(a.x-location.x,a.y-location.y) < hypot(b.x-location.x,b.y-location.y) }) else { return nil }
        let p = position(point,in:rect)
        return hypot(p.x-location.x,p.y-location.y) <= 15 ? point : nil
    }
    private func drawPlaceLabels(_ context: inout GraphicsContext, size: CGSize, rect: CGRect) {
        var occupied=[CGRect(x:size.width-80,y:0,width:80,height:230),CGRect(x:0,y:size.height-54,width:175,height:54)]
        if selected != nil { occupied.append(CGRect(x:max(0,size.width-330),y:max(0,size.height-220),width:330,height:220)) }
        for point in points where point.id == selected || point.id == hover {
            let p=position(point,in:rect)
            occupied.append(CGRect(x:p.x-15,y:p.y-15,width:30,height:30))
        }
        for label in labels where label.isVisible(at:zoom) {
            let p=CGPoint(x:rect.minX+(label.x-bounds.minX)/(bounds.maxX-bounds.minX)*rect.width,
                          y:rect.minY+(label.z-bounds.minZ)/(bounds.maxZ-bounds.minZ)*rect.height)
            guard p.x >= 5,p.y >= 5,p.x <= size.width-5,p.y <= size.height-5 else { continue }
            let text=Text(label.name).font(.system(size:label.fontSize,weight:label.kind == "settlement" ? .medium : .regular,design:.serif))
                .italic().tracking(label.kind == "region" ? 1.1 : 0.3)
            let resolved=context.resolve(text.foregroundStyle(Slate.ivory.opacity(label.kind == "landmark" ? 0.8 : 0.94)))
            let measured=resolved.measure(in:CGSize(width:220,height:65))
            let box=CGRect(x:p.x-measured.width/2-5,y:p.y-measured.height/2-3,width:measured.width+10,height:measured.height+6)
            guard box.minX >= 2,box.maxX <= size.width-2,box.minY >= 2,box.maxY <= size.height-2,
                  !occupied.contains(where: { $0.intersects(box) }) else { continue }
            occupied.append(box)
            let shadow=context.resolve(text.foregroundStyle(Slate.background.opacity(0.95)))
            for shift in [CGPoint(x:-1,y:0),CGPoint(x:1,y:0),CGPoint(x:0,y:-1),CGPoint(x:0,y:1)] {
                context.draw(shadow,at:CGPoint(x:p.x+shift.x,y:p.y+shift.y))
            }
            context.draw(resolved,at:p)
        }
    }
    var body: some View {
        GeometryReader { geo in
            let rect = mapRect(geo.size)
            ZStack(alignment:.topLeading) {
                Slate.background
                if let terrain {
                    Image(nsImage:terrain.image).resizable().interpolation(.high).frame(width:rect.width,height:rect.height).position(x:rect.midX,y:rect.midY)
                        .saturation(1.02).brightness(0.015)
                }
                Canvas { c,size in
                    if terrain == nil {
                        var grid = Path()
                        for i in 0...12 { let x = rect.minX+rect.width*CGFloat(i)/12; grid.move(to:CGPoint(x:x,y:rect.minY)); grid.addLine(to:CGPoint(x:x,y:rect.maxY)) }
                        for i in 0...10 { let y = rect.minY+rect.height*CGFloat(i)/10; grid.move(to:CGPoint(x:rect.minX,y:y)); grid.addLine(to:CGPoint(x:rect.maxX,y:y)) }
                        c.stroke(grid,with:.color(Slate.line),lineWidth:0.5)
                    }
                    drawPlaceLabels(&c,size:size,rect:rect)
                    for point in points where point.x.isFinite && point.z.isFinite {
                        let p = position(point,in:rect)
                        guard p.x >= -15,p.y >= -15,p.x <= size.width+15,p.y <= size.height+15 else { continue }
                        let active = point.unlocked == true
                        let tower = point.kind.localizedCaseInsensitiveContains("tower")
                        let color = tower ? Slate.gold : active ? Slate.cyan : Slate.orange
                        let highlighted = selected == point.id || hover == point.id
                        let opacity = visibleIDs.contains(point.id) ? 1.0 : 0.15
                        let radius:CGFloat = highlighted ? 7 : tower ? 5 : 4
                        var path = Path()
                        if tower {
                            path.move(to:CGPoint(x:p.x,y:p.y-radius-3));path.addLine(to:CGPoint(x:p.x+radius*0.65,y:p.y+radius));path.addLine(to:CGPoint(x:p.x-radius*0.65,y:p.y+radius));path.closeSubpath()
                            c.fill(path,with:.color(Slate.background.opacity(0.85)));c.stroke(path,with:.color(color.opacity(opacity)),lineWidth:highlighted ? 1.7 : 1.1)
                            for dy in [CGFloat(-2),radius] { var bar=Path();bar.move(to:CGPoint(x:p.x-radius,y:p.y+dy));bar.addLine(to:CGPoint(x:p.x+radius,y:p.y+dy));c.stroke(bar,with:.color(color.opacity(opacity)),lineWidth:1) }
                            c.stroke(Path(ellipseIn:CGRect(x:p.x-1.6,y:p.y-radius-5,width:3.2,height:3.2)),with:.color(color.opacity(opacity)),lineWidth:1)
                        } else {
                            for inset:CGFloat in [0,2.8] {
                                let r=max(1.3,radius-inset)
                                var diamond=Path();diamond.move(to:CGPoint(x:p.x,y:p.y-r));diamond.addLine(to:CGPoint(x:p.x+r,y:p.y));diamond.addLine(to:CGPoint(x:p.x,y:p.y+r));diamond.addLine(to:CGPoint(x:p.x-r,y:p.y));diamond.closeSubpath()
                                if inset == 0 { c.fill(diamond,with:.color(Slate.background.opacity(0.85))) }
                                c.stroke(diamond,with:.color(color.opacity(opacity)),lineWidth:highlighted ? 1.5 : 0.9)
                            }
                            var stem=Path();stem.move(to:CGPoint(x:p.x,y:p.y+2));stem.addLine(to:CGPoint(x:p.x,y:p.y+radius+2));c.stroke(stem,with:.color(color.opacity(opacity)),lineWidth:0.9)
                        }
                        if selected == point.id {
                            c.stroke(Path(ellipseIn:CGRect(x:p.x-13,y:p.y-13,width:26,height:26)),with:.color(Slate.cyan.opacity(0.8)),lineWidth:1)
                        }
                    }
                    if let player = playerPosition, player.isFinite {
                        let p = CGPoint(x:rect.minX+(player.x-bounds.minX)/(bounds.maxX-bounds.minX)*rect.width,y:rect.minY+(player.z-bounds.minZ)/(bounds.maxZ-bounds.minZ)*rect.height)
                        if p.x >= 0,p.x <= size.width,p.y >= 0,p.y <= size.height {
                            c.fill(Path(ellipseIn:CGRect(x:p.x-7,y:p.y-7,width:14,height:14)),with:.color(Slate.background.opacity(0.9)))
                            c.stroke(Path(ellipseIn:CGRect(x:p.x-9,y:p.y-9,width:18,height:18)),with:.color(Slate.cyan),lineWidth:1.5)
                            c.fill(Path(ellipseIn:CGRect(x:p.x-4,y:p.y-4,width:8,height:8)),with:.color(Slate.ivory))
                        }
                    }
                }
                if let point = points.first(where:{ $0.id == (hover ?? selected) }) {
                    let p = position(point,in:rect)
                    if p.x >= 0,p.x <= geo.size.width,p.y >= 0,p.y <= geo.size.height {
                        Text(point.name).font(.system(size:12,weight:.medium,design:.serif)).italic().foregroundStyle(Slate.ivory).lineLimit(1)
                            .padding(.horizontal,10).padding(.vertical,7).background(Slate.background.opacity(0.92),in:RoundedRectangle(cornerRadius:3))
                            .overlay(RoundedRectangle(cornerRadius:3).stroke(Slate.cyan.opacity(0.45),lineWidth:1))
                            .position(x:max(95,min(geo.size.width-95,p.x)),y:max(55,min(geo.size.height-22,p.y-29)))
                            .allowsHitTesting(false)
                    }
                }
                mapChrome(size:geo.size)
            }
            .contentShape(Rectangle()).clipped()
            .gesture(DragGesture(minimumDistance:3).updating($drag) { value,state,_ in state=value.translation }.onEnded { value in
                offset = clamped(CGSize(width:offset.width+value.translation.width,height:offset.height+value.translation.height),size:geo.size,width:rect.width,height:rect.height)
            })
            .simultaneousGesture(MagnifyGesture().onChanged { value in zoom = max(1,min(6,previousZoom*value.magnification)) }.onEnded { _ in previousZoom=zoom; offset=clamped(offset,size:geo.size,width:mapRect(geo.size).width,height:mapRect(geo.size).height) })
            .onTapGesture { location in if let point = nearest(location,rect:rect) { withAnimation(reduceMotion ? nil : .easeInOut(duration:0.15)) { selected=point.id } } }
            .onContinuousHover { phase in switch phase { case .active(let p): hover=nearest(p,rect:rect)?.id; case .ended: hover=nil } }
            .accessibilityLabel("Hyrule map. Drag to pan, pinch or use the plus and minus buttons to zoom. Select destinations using the adjacent list.")
        }
    }
    private func mapChrome(size:CGSize) -> some View {
        HStack(alignment:.top) {
            VStack(alignment:.leading) {
                if terrain == nil { Text("Install the bridge to load your game’s terrain map.").font(.system(size:11)).foregroundStyle(Slate.ivory).padding(10).background(Slate.background.opacity(0.9)) }
                Spacer()
                HStack(spacing:8) {
                    DestinationSymbol().scaleEffect(0.55).frame(width:12,height:14);Text("Shrine")
                    DestinationSymbol(tower:true,color:Slate.gold).scaleEffect(0.55).frame(width:12,height:14);Text("Tower")
                }.font(.system(size:10,design:.serif)).italic().foregroundStyle(Slate.ivory.opacity(0.8)).padding(8).background(Slate.background.opacity(0.8),in:RoundedRectangle(cornerRadius:3))
            }
            Spacer()
            VStack(spacing:14) {
                VStack(spacing:4) { Text("N").font(.system(size:12,design:.serif)).italic();Image(systemName:"location.north.fill").font(.system(size:26,weight:.ultraLight));Rectangle().fill(Slate.gold.opacity(0.5)).frame(width:1,height:20) }.foregroundStyle(Slate.gold).padding(.bottom,4)
                Button { centerOnPlayer(size:size) } label: { Image(systemName:"location.fill").frame(width:31,height:31) }.buttonStyle(.plain).background(Slate.background.opacity(0.85),in:RoundedRectangle(cornerRadius:4)).overlay(RoundedRectangle(cornerRadius:4).stroke(Slate.ivory.opacity(0.35),lineWidth:0.7)).disabled(playerPosition?.isFinite != true).accessibilityLabel("Center on Link")
                VStack(spacing:0) {
                    Button { changeZoom(min(6,zoom*1.4),size:size) } label: { Image(systemName:"plus").frame(width:31,height:31) }.accessibilityLabel("Zoom in")
                    Rectangle().fill(Slate.line).frame(height:1)
                    Button { changeZoom(max(1,zoom/1.4),size:size) } label: { Image(systemName:"minus").frame(width:31,height:31) }.accessibilityLabel("Zoom out")
                    Rectangle().fill(Slate.line).frame(height:1)
                    Button { withAnimation(reduceMotion ? nil : .easeInOut(duration:0.18)) { zoom=1;previousZoom=1;offset = .zero } } label: { Image(systemName:"arrow.up.left.and.arrow.down.right").font(.system(size:10)).frame(width:31,height:31) }.accessibilityLabel("Show whole map")
                }.frame(width:33).buttonStyle(.plain).font(.system(size:13)).background(Slate.background.opacity(0.9),in:RoundedRectangle(cornerRadius:4)).overlay(RoundedRectangle(cornerRadius:4).stroke(Slate.ivory.opacity(0.35),lineWidth:0.7))
                Spacer()
            }.foregroundStyle(Slate.ivory)
        }.padding(18)
    }
    private func centerOnPlayer(size:CGSize) {
        guard let player=playerPosition,player.isFinite else { return }
        let nextZoom=max(zoom,2.2)
        let aspect=(bounds.maxX-bounds.minX)/(bounds.maxZ-bounds.minZ)
        let width=min(size.width,size.height*aspect)*nextZoom,height=width/aspect
        let desired=CGSize(width:(0.5-(player.x-bounds.minX)/(bounds.maxX-bounds.minX))*width,height:(0.5-(player.z-bounds.minZ)/(bounds.maxZ-bounds.minZ))*height)
        withAnimation(reduceMotion ? nil : .easeInOut(duration:0.2)) { zoom=nextZoom;previousZoom=nextZoom;offset=clamped(desired,size:size,width:width,height:height) }
    }
    private func changeZoom(_ value:CGFloat,size:CGSize) {
        let aspect = (bounds.maxX-bounds.minX)/(bounds.maxZ-bounds.minZ)
        let width = min(size.width,size.height*aspect)*value
        withAnimation(reduceMotion ? nil : .easeInOut(duration:0.18)) {
            zoom=value
            previousZoom=value
            offset=clamped(offset,size:size,width:width,height:width/aspect)
        }
    }
}

struct AddItemView: View {
    @ObservedObject var model:PlaygroundModel
    @Environment(\.dismiss) private var dismiss
    @State private var search = ""
    @State private var category = "all"
    @State private var selected:String?
    @State private var customID = ""
    @State private var quantity = "1"
    @State private var showCustom = false
    @State private var confirmOutfits = false
    init(model:PlaygroundModel,initialSelection:String? = nil,initialCategory:String = "all") {
        self.model=model
        _selected=State(initialValue:initialSelection)
        _category=State(initialValue:initialCategory)
    }
    private var filtered:[CatalogItem] { model.catalog.filter { (category == "all" || $0.category == category) && (search.isEmpty || $0.name.localizedCaseInsensitiveContains(search) || $0.id.localizedCaseInsensitiveContains(search)) } }
    private var chosen:String { customID.isEmpty ? selected ?? "" : customID.trimmingCharacters(in:.whitespaces) }
    private var chosenItem:CatalogItem? { model.catalog.first { $0.id == chosen } }
    private var internalMasterSword:Bool { ItemGrantRules.isInternalMasterSwordForm(chosen) }
    private var giveBlockedReason:String? { model.giveBlockedReason(for:chosen) }
    private var validQuantity:Bool { (1...999).contains(Int(quantity) ?? 0) }
    private func iconType(_ category:String) -> Int {
        switch category { case "weapons":return 0;case "bows":return 1;case "shields":return 3;case "clothes":return 5;case "materials":return 7;case "food":return 8;default:return 9 }
    }
    private func categoryName(_ category:String) -> String {
        switch category { case "all":return "All items";case "clothes":return "Armor";case "other":return "Key items";default:return category.capitalized }
    }
    var body:some View {
        VStack(spacing:0) {
            HStack {
                Text("Give item").font(.system(size:22,weight:.medium,design:.serif)).italic()
                Spacer()
                Text("Item catalog").font(.system(size:12,design:.serif)).italic().foregroundStyle(Slate.muted)
                Button { dismiss() } label: { Image(systemName:"xmark").font(.system(size:13)).padding(7) }.buttonStyle(.plain).accessibilityLabel("Close item catalog")
            }.padding(.horizontal,22).frame(height:57).background(Slate.panel.opacity(0.6))
            Rectangle().fill(Slate.line).frame(height:1)
            HStack(spacing:0) {
                VStack(alignment:.leading,spacing:7) {
                    ForEach(["all","weapons","bows","shields","clothes","materials","food","other"],id:\.self) { itemCategory in
                        Button { category=itemCategory } label: {
                            HStack(spacing:8) {
                                Image(systemName:itemCategory == "all" ? "square.grid.2x2" : typeIcon(iconType(itemCategory))).font(.system(size:13)).frame(width:17)
                                Text(categoryName(itemCategory)).font(.system(size:13,design:.serif)).italic()
                                Spacer(minLength:0)
                            }.padding(.horizontal,12).frame(height:33).foregroundStyle(category == itemCategory ? Slate.cyan : Slate.ivory.opacity(0.8))
                                .background { if category == itemCategory { CutCornerShape(cut:6).fill(Slate.cyan.opacity(0.1)) } }
                                .overlay { if category == itemCategory { CutCornerShape(cut:6).stroke(Slate.cyan.opacity(0.65),lineWidth:0.7) } }
                        }.buttonStyle(.plain)
                    }
                    Spacer()
                    Text("\(model.catalog.count.formatted()) items").font(.system(size:10)).foregroundStyle(Slate.muted).padding(15)
                }.padding(.top,19).frame(width:145).background(Slate.panel.opacity(0.25))
                Rectangle().fill(Slate.line).frame(width:1)
                VStack(alignment:.leading,spacing:16) {
                    HStack(spacing:10) {
                        SearchBox(placeholder:"Search items…",text:$search)
                        if model.givingOutfits {
                            Button("Stop giving outfits") { model.stopGivingOutfits() }
                                .buttonStyle(SlateButtonStyle())
                        } else if category == "clothes" {
                            Button("Give all outfits…") { confirmOutfits=true }
                                .buttonStyle(SlateButtonStyle())
                                .disabled(model.missingOutfits.isEmpty || model.freeArmorSlots == 0 || !model.canEditInventory(feature:"items"))
                        }
                    }
                    if category == "clothes", model.available, let slots = model.freeArmorSlots {
                        Text(slots == 0 ? "Armor pouch full (\(OutfitGrantRules.armorCapacity) pieces)." : "\(slots) of \(OutfitGrantRules.armorCapacity) armor slots free")
                            .font(.system(size:11)).foregroundStyle(slots == 0 ? Slate.gold : Slate.muted)
                    }
                    ScrollView {
                        LazyVGrid(columns:[GridItem(.adaptive(minimum:88,maximum:122),spacing:12)],spacing:13) {
                            ForEach(filtered) { item in
                                Button { selected=item.id;customID="" } label: {
                                    VStack(spacing:5) {
                                        ItemArtwork(actor:item.id,type:iconType(item.category),size:64).frame(maxWidth:.infinity).frame(height:80)
                                            .background(Slate.panel.opacity(0.3)).overlay(RoundedRectangle(cornerRadius:3).stroke(Slate.ivory.opacity(0.25),lineWidth:0.6))
                                            .overlay { if chosen == item.id { SelectionGlow() } }
                                        Text(item.name).font(.system(size:12,design:.serif)).italic().multilineTextAlignment(.center).lineLimit(2).frame(height:29,alignment:.top)
                                        if ItemGrantRules.isInternalMasterSwordForm(item.id) {
                                            Text("Internal form").font(.system(size:9)).foregroundStyle(Slate.gold)
                                        }
                                    }.frame(maxWidth:.infinity).foregroundStyle(Slate.ivory)
                                }.buttonStyle(.plain).modifier(SlateHover()).accessibilityAddTraits(chosen == item.id ? .isSelected : [])
                            }
                        }.padding(3)
                    }.scrollIndicators(.hidden)
                    if filtered.isEmpty { Text("No items match this search.").font(.system(size:12)).foregroundStyle(Slate.muted) }
                }.padding(19).frame(maxWidth:.infinity,maxHeight:.infinity)
                Rectangle().fill(Slate.line).frame(width:1)
                VStack(spacing:0) {
                    ScrollView {
                        VStack(alignment:.leading,spacing:15) {
                            ItemArtwork(actor:chosen,type:iconType(chosenItem?.category ?? "other"),size:145).frame(maxWidth:.infinity).frame(height:158)
                            Text(chosenItem?.name ?? (chosen.isEmpty ? "Select an item" : chosen)).font(.system(size:26,weight:.medium,design:.serif)).italic().fixedSize(horizontal:false,vertical:true)
                            if let chosenItem { Text(categoryName(chosenItem.category)).font(.system(size:13,design:.serif)).italic().foregroundStyle(Slate.muted) }
                            if internalMasterSword {
                                Text("The game uses this model for the Master Sword's world or appearance state. It cannot be collected as a separate weapon.")
                                    .font(.system(size:12)).foregroundStyle(Slate.muted).fixedSize(horizontal:false,vertical:true)
                                Button("Select usable Master Sword") { selected=ItemGrantRules.masterSword;customID="";quantity="1" }
                                    .buttonStyle(SlateButtonStyle())
                            }
                            DisclosureGroup("Custom item ID",isExpanded:$showCustom) { TextField("e.g. Item_Fruit_A",text:$customID).textFieldStyle(.plain).font(.system(size:10,design:.monospaced)).padding(7).overlay(Rectangle().stroke(Slate.line,lineWidth:1)).padding(.top,6) }.font(.system(size:10)).foregroundStyle(Slate.muted)
                        }.padding(21).frame(maxWidth:.infinity,alignment:.leading)
                    }.scrollIndicators(.hidden)
                    Rectangle().fill(Slate.line).frame(height:1)
                    VStack(alignment:.leading,spacing:13) {
                        HStack {
                            Text("Quantity").font(.system(size:14,design:.serif)).italic()
                            Spacer()
                            QuantityControl(value:$quantity).frame(width:113)
                                .disabled(chosen == ItemGrantRules.masterSword || internalMasterSword)
                        }
                        if !model.available {
                            Label(model.connectionMessage,systemImage:"info.circle").font(.system(size:11)).foregroundStyle(Slate.gold).fixedSize(horizontal:false,vertical:true)
                        } else if let reason=model.inventoryBlockedReason {
                            Label(reason,systemImage:"info.circle").font(.system(size:11)).foregroundStyle(Slate.gold).fixedSize(horizontal:false,vertical:true)
                        }
                        if let reason=giveBlockedReason {
                            Label(reason,systemImage:"info.circle").font(.system(size:11)).foregroundStyle(Slate.gold).fixedSize(horizontal:false,vertical:true)
                        }
                        if !validQuantity { Text("Choose a quantity from 1 to 999.").font(.system(size:11)).foregroundStyle(Slate.orange) }
                        Text("The game checks pouch capacity and item restrictions.").font(.system(size:10)).foregroundStyle(Slate.muted).fixedSize(horizontal:false,vertical:true)
                        Button { model.send(.give(chosen,quantity)) } label: { Text(model.pending ? "Waiting for game…" : "Give item").frame(maxWidth:.infinity) }
                            .buttonStyle(SlateButtonStyle(primary:true)).disabled(chosen.isEmpty || !validQuantity || giveBlockedReason != nil || !model.canEditInventory(feature:"items"))
                        if model.pending { HStack { ProgressView().controlSize(.small);Text("Waiting for confirmation").font(.system(size:10)) } }
                    }.padding(19)
                }.frame(width:247).frame(maxHeight:.infinity).background(Slate.panel.opacity(0.25))
            }
            Rectangle().fill(Slate.line).frame(height:1)
            HStack(spacing:8) {
                Image(systemName:model.feedbackIsError ? "exclamationmark.circle" : "info.circle").foregroundStyle(model.feedbackIsError ? Slate.orange : Slate.cyan)
                Text(model.feedback).font(.system(size:11)).foregroundStyle(model.feedbackIsError ? Slate.orange : Slate.muted).lineLimit(2).textSelection(.enabled)
                Spacer()
                Button("Done") { dismiss() }.buttonStyle(SlateButtonStyle()).keyboardShortcut(.cancelAction)
            }.padding(.horizontal,21).frame(height:55).background(Slate.panel.opacity(0.6))
        }.frame(width:940,height:650).background { Slate.background;ContourEngraving() }.foregroundStyle(Slate.ivory).tint(Slate.cyan).preferredColorScheme(.dark)
            .confirmationDialog("Give all outfits?",isPresented:$confirmOutfits,titleVisibility:.visible) {
                Button("Give missing pieces") { model.giveAllOutfits() }
            } message: {
                Text("Add up to \(min(model.freeArmorSlots ?? 0, model.missingOutfits.count)) of \(model.missingOutfits.count) missing base pieces. Owned and upgraded pieces are kept.")
            }
            .onAppear { if selected == nil { selected=filtered.first?.id } }
            .onChange(of:chosen) { _,actor in if actor == ItemGrantRules.masterSword || ItemGrantRules.isInternalMasterSwordForm(actor) { quantity="1" } }
    }
}
