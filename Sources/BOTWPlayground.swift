import SwiftUI
import AppKit
import ImageIO

#if !UI_SNAPSHOT
@main
struct BOTWEditorApp: App {
    @StateObject private var model = PlaygroundModel()
    var body: some Scene {
        WindowGroup("BOTW Editor") { EditorView(model: model) }
            .defaultSize(width: 1000, height: 760)
            .windowResizability(.contentMinSize)
            .commands {
                CommandGroup(replacing: .newItem) {}
                CommandGroup(after: .toolbar) {
                    Button("Refresh Connection") { model.refresh(); model.send(.status) }
                        .keyboardShortcut("r", modifiers: .command)
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

private struct SlateButtonStyle: ButtonStyle {
    var primary = false
    var destructive = false
    @Environment(\.isEnabled) private var enabled
    func makeBody(configuration: Configuration) -> some View {
        configuration.label.font(.system(size: 12, weight: .semibold))
            .foregroundStyle(!enabled ? Slate.ivory.opacity(0.7) : primary ? Slate.background : destructive ? Color(red: 0.96, green: 0.57, blue: 0.51) : Slate.ivory)
            .padding(.horizontal, 13).frame(minHeight: 31)
            .background(!enabled ? Slate.cyan.opacity(primary ? 0.13 : 0.035) : primary ? Slate.cyan.opacity(configuration.isPressed ? 0.7 : 1) : Slate.raised.opacity(configuration.isPressed ? 0.9 : 0.6), in: RoundedRectangle(cornerRadius: 4))
            .overlay(RoundedRectangle(cornerRadius: 4).stroke(primary ? Slate.cyan : Slate.line, lineWidth: 1))
            .opacity(enabled ? 1 : 0.9)
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
    @State private var selectedSlot: Int?
    @State private var itemValue = "1"
    @State private var rupees = "10000"
    @State private var maxHearts = "30"
    @State private var maxStamina = "3"
    @State private var showAdd = false
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
        inventory.filter { (category < 0 || $0.type == category) && (search.isEmpty || model.name(for:$0.name).localizedCaseInsensitiveContains(search) || $0.name.localizedCaseInsensitiveContains(search)) }
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
            navigationBar
            if !model.available { connectionNotice }
            Group {
                switch tab { case .inventory: inventoryTab; case .player: playerTab; case .world: worldTab }
            }.frame(maxWidth:.infinity,maxHeight:.infinity).transition(.opacity)
            feedbackBar
        }
        .frame(minWidth:1000,minHeight:760)
        .background { Slate.background; ContourEngraving() }.foregroundStyle(Slate.ivory).tint(Slate.cyan).preferredColorScheme(.dark)
        .font(.system(size:13))
        .onChange(of:selectedSlot) { _,_ in itemValue = selectedItem.map { $0.usesDurability ? InventoryValuePresentation.durabilityText(rawValue:$0.value) : String($0.value) } ?? "1" }
        .onAppear { selectFirstItemIfNeeded() }
        .onChange(of:inventory.map(\.slot)) { _,_ in selectFirstItemIfNeeded() }
        .sheet(isPresented:$showAdd) { AddItemView(model:model) }
        .alert(item:$confirmation) { action in
            let primary: Alert.Button = action.destructive ? .destructive(Text(action.button),action:{ model.send(action.command) }) : .default(Text(action.button),action:{ model.send(action.command) })
            return Alert(title:Text(action.title),message:Text(action.detail),primaryButton:primary,secondaryButton:.cancel())
        }
    }

    private var toolbar: some View {
        HStack(spacing:10) {
            SlateEye()
            Text("BOTW Editor").font(.system(size:18,weight:.medium,design:.serif)).italic()
            Rectangle().fill(Slate.line).frame(width:1,height:20).padding(.horizontal,5)
            HStack(spacing:6) {
                Circle().fill(model.available ? Slate.cyan : Slate.gold).frame(width:5,height:5)
                Text(model.available ? "Connected" : model.connected ? "Waiting for game" : "Game closed")
                    .font(.system(size:10,weight:.medium)).foregroundStyle(model.available ? Slate.cyan : Slate.muted)
            }.help(model.connectionMessage)
            Spacer(minLength:12)
            hud("heart.fill",value:heartHUD,color:Color(red:0.96,green:0.52,blue:0.44))
            hud("circle.dashed",value:staminaHUD,color:Color(red:0.57,green:0.87,blue:0.56))
            hud("diamond.fill",value:liveRupees,color:Slate.cyan)
            Button { model.refresh(); model.send(.status) } label: { Image(systemName:"arrow.clockwise").font(.system(size:12)) }
                .buttonStyle(.plain).padding(8).background(Slate.raised,in:RoundedRectangle(cornerRadius:4))
                .disabled(model.pending).accessibilityLabel("Refresh game connection").help("Refresh connection")
        }.padding(.horizontal,20).frame(height:48).background { Slate.panel; ContourEngraving().clipped() }
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
                    VStack(spacing:0) {
                        HStack(spacing:8) { Image(systemName:section.icon); Text(section.rawValue).font(.system(size:14,weight:.medium,design:.serif)).italic() }
                            .font(.system(size:12,weight:tab == section ? .semibold : .regular))
                            .foregroundStyle(tab == section ? Slate.ivory : Slate.muted)
                            .frame(width:125,height:43)
                        ZStack {
                            Color.clear.frame(height:2)
                            if tab == section { Slate.cyan.frame(height:2).matchedGeometryEffect(id:"navigation",in:navigation) }
                        }
                    }
                }.frame(width:125).buttonStyle(.plain).accessibilityAddTraits(tab == section ? .isSelected : [])
            }
            Spacer()
        }.background(Slate.panel.opacity(0.45)).overlay(alignment:.bottom) { Rectangle().fill(Slate.line).frame(height:1) }
    }
    private var connectionNotice: some View {
        HStack(spacing:8) {
            Image(systemName:model.state?.teleportBusy == true ? "arrow.trianglehead.2.clockwise.rotate.90" : "info.circle").foregroundStyle(Slate.gold)
            Text(model.connectionMessage).font(.system(size:11)).lineLimit(2)
            Spacer()
        }.padding(.horizontal,20).padding(.vertical,10).background(Slate.gold.opacity(0.06))
            .overlay(alignment:.bottom) { Rectangle().fill(Slate.gold.opacity(0.12)).frame(height:1) }
    }

    private var inventoryTab: some View {
        HStack(spacing:0) {
            inventoryCategories.frame(width:154)
            VStack(alignment:.leading,spacing:16) {
                HStack {
                    VStack(alignment:.leading,spacing:3) {
                        Text(category < 0 ? "Inventory" : typeName(category)).font(.system(size:23,weight:.medium,design:.serif)).italic()
                        Text("\(filteredInventory.count) items in your pouch").font(.system(size:11)).foregroundStyle(Slate.muted)
                    }
                    Spacer()
                    Button { showAdd = true } label: { Label("Add item",systemImage:"plus") }.buttonStyle(SlateButtonStyle(primary:true)).disabled(!model.canUse("items"))
                }
                SearchBox(placeholder:"Search your inventory",text:$search)
                if filteredInventory.isEmpty {
                    emptyState(search.isEmpty ? "Your inventory will appear here" : "No items found",icon:"backpack",detail:search.isEmpty ? "Load a gameplay session, then open the pouch in BOTW to inspect your items." : "Try another name or choose a different category.")
                        .frame(maxWidth:.infinity,maxHeight:.infinity)
                } else {
                    ScrollView {
                        LazyVGrid(columns:[GridItem(.adaptive(minimum:102,maximum:145),spacing:10)],spacing:10) {
                            ForEach(filteredInventory) { item in inventoryTile(item) }
                        }.padding(2)
                    }.scrollIndicators(.hidden)
                }
                HStack(spacing:5) { Image(systemName:"arrow.triangle.2.circlepath"); Text("Values refresh from the game. Save in BOTW to keep changes.") }
                    .font(.system(size:10)).foregroundStyle(Slate.muted)
            }.padding(20).frame(maxWidth:.infinity,maxHeight:.infinity,alignment:.top)
            line(vertical:true)
            inventoryInspector.frame(width:254)
        }
    }
    private var inventoryCategories: some View {
        VStack(alignment:.leading,spacing:4) {
            Text("Pouch").font(.system(size:11,weight:.semibold)).foregroundStyle(Slate.muted).padding(.horizontal,16).padding(.top,21).padding(.bottom,12)
            categoryButton(-1,"All items","square.grid.2x2")
            ForEach(0..<10,id:\.self) { type in categoryButton(type,typeName(type),typeIcon(type)) }
            Spacer()
            HStack(spacing:6) { Image(systemName:"square.stack.3d.up"); Text("\(inventory.count) entries") }.font(.system(size:10)).foregroundStyle(Slate.muted).padding(16)
        }.background(Slate.panel.opacity(0.45)).overlay(alignment:.trailing) { line(vertical:true) }
    }
    private func categoryButton(_ value:Int,_ name:String,_ icon:String) -> some View {
        Button {
            withAnimation(reduceMotion ? nil : .easeInOut(duration:0.15)) { category = value }
        } label: {
            HStack(spacing:10) {
                Image(systemName:icon).font(.system(size:12)).frame(width:17)
                Text(name).font(.system(size:12,weight:category == value ? .semibold : .regular,design:.serif)).italic()
                Spacer(minLength:0)
                if value >= 0 {
                    let count = inventory.filter { $0.type == value }.count
                    if count > 0 { Text("\(count)").font(.system(size:9)).foregroundStyle(Slate.muted) }
                }
            }.foregroundStyle(category == value ? Slate.cyan : Slate.muted).padding(.horizontal,15).frame(height:35)
                .background(category == value ? Slate.cyan.opacity(0.08) : Color.clear)
                .overlay(alignment:.leading) { if category == value { Slate.cyan.frame(width:2) } }
        }.buttonStyle(.plain).accessibilityAddTraits(category == value ? .isSelected : [])
    }
    private func inventoryTile(_ item:InventoryEntry) -> some View {
        Button {
            withAnimation(reduceMotion ? nil : .easeInOut(duration:0.16)) { selectedSlot = item.slot }
        } label: {
            VStack(alignment:.leading,spacing:5) {
                ZStack {
                    RadialGradient(colors:[Slate.cyan.opacity(0.06),.clear],center:.center,startRadius:0,endRadius:55)
                    ItemArtwork(actor:item.name,type:item.type,size:65)
                }.frame(maxWidth:.infinity).frame(height:79)
                Text(model.name(for:item.name)).font(.system(size:11,weight:.medium,design:.serif)).italic().lineLimit(2).frame(height:27,alignment:.topLeading)
                HStack {
                    Text(item.usesDurability ? "\(InventoryValuePresentation.durabilityText(rawValue:item.value)) dur." : item.maximumEditableValue != nil ? "× \(item.value.formatted())" : typeName(item.type)).font(.system(size:9)).foregroundStyle(Slate.muted).lineLimit(1)
                    Spacer(minLength:0)
                    if item.equipped { Image(systemName:"checkmark.circle.fill").font(.system(size:10)).foregroundStyle(Slate.cyan).help("Equipped") }
                }
            }.padding(9).frame(maxWidth:.infinity,alignment:.leading)
                .background(selectedSlot == item.slot ? Slate.cyan.opacity(0.08) : Slate.panel)
                .overlay(Rectangle().stroke(selectedSlot == item.slot ? Slate.cyan.opacity(0.5) : Slate.line.opacity(0.6),lineWidth:1))
                .overlay { if selectedSlot == item.slot { SelectionGlow() } }
        }.buttonStyle(.plain).modifier(SlateHover()).accessibilityLabel("\(model.name(for:item.name)), \(item.equipped ? "equipped, " : "")\(item.usesDurability ? "durability" : "value") \(item.value)")
            .accessibilityAddTraits(selectedSlot == item.slot ? .isSelected : [])
    }
    private var inventoryInspector: some View {
        VStack(alignment:.leading,spacing:0) {
            HStack { Text("Item details").font(.system(size:11,weight:.semibold)); Spacer(); Image(systemName:"sidebar.right").foregroundStyle(Slate.muted) }.padding(18)
            line()
            if let item = selectedItem {
                ScrollView {
                    VStack(alignment:.leading,spacing:16) {
                        ZStack {
                            RadialGradient(colors:[Slate.cyan.opacity(0.1),.clear],center:.center,startRadius:5,endRadius:100)
                            ItemArtwork(actor:item.name,type:item.type,size:115)
                        }.frame(maxWidth:.infinity).frame(height:137)
                        VStack(alignment:.leading,spacing:6) {
                            Text(model.name(for:item.name)).font(.system(size:22,weight:.medium,design:.serif)).italic().fixedSize(horizontal:false,vertical:true)
                            Text(typeName(item.type)).font(.system(size:11)).foregroundStyle(Slate.gold)
                        }
                        Text(item.name).font(.system(size:9,design:.monospaced)).foregroundStyle(Slate.muted).textSelection(.enabled).lineLimit(2)
                        HStack { Text("Slot \(item.slot)"); Spacer(); if item.equipped { Label("Equipped",systemImage:"checkmark.circle").foregroundStyle(Slate.cyan) } }.font(.system(size:10)).foregroundStyle(Slate.muted)
                        line()
                        VStack(alignment:.leading,spacing:8) {
                            Text(item.usesDurability ? "Durability points" : "Quantity").font(.system(size:12,weight:.semibold))
                            HStack { Text("Current").foregroundStyle(Slate.muted); Spacer(); Text(item.usesDurability ? InventoryValuePresentation.durabilityText(rawValue:item.value) : item.value.formatted()).monospacedDigit() }.font(.system(size:11))
                            TextField(item.usesDurability ? "Durability points" : "Quantity",text:$itemValue).textFieldStyle(.roundedBorder).font(.system(size:14,design:.monospaced)).accessibilityLabel(item.usesDurability ? "New durability in points" : "New quantity")
                            Text(item.usesDurability ? "0.01–1,000,000 points. Up to two decimal places." : item.maximumEditableValue != nil ? "Choose a stack quantity from 1 to 999." : "This item has a special value and cannot be edited here.")
                                .font(.system(size:10)).foregroundStyle(Slate.muted).fixedSize(horizontal:false,vertical:true)
                            Button("Apply \(item.usesDurability ? "durability" : "quantity")") { model.send(.quantity(item,commandValue(for:item))) }
                                .buttonStyle(SlateButtonStyle(primary:true)).disabled(!model.canUse("inventory_edit") || !item.canEditValue || commandValue(for:item).isEmpty)
                            if item.usesDurability && item.equipped { Text("Unequip this weapon in BOTW before editing durability.").font(.system(size:10)).foregroundStyle(Slate.gold).fixedSize(horizontal:false,vertical:true) }
                        }
                        line()
                        Button { confirmation = ConfirmAction(title:"Remove \(model.name(for:item.name))?",detail:"This removes the entire selected entry from your running game. Save in BOTW to keep the change.",button:"Remove item",command:.remove(item.slot),destructive:true) } label: { Label("Remove item…",systemImage:"trash") }
                            .buttonStyle(SlateButtonStyle(destructive:true)).disabled(!model.canUse("inventory_edit") || item.equipped)
                        if item.equipped { Text("Unequip this item in BOTW before removing it.").font(.system(size:10)).foregroundStyle(Slate.muted) }
                    }.padding(18)
                }
            } else {
                Spacer()
                emptyState("Select an item",icon:"cursorarrow",detail:"Choose something from your pouch to inspect or edit it.").padding(20)
                Spacer()
            }
        }.frame(maxHeight:.infinity).background(Slate.panel.opacity(0.6))
    }

    private var playerTab: some View {
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
            Spacer(minLength:0)
            Label("Save in BOTW to keep your changes. Switches reflect the state confirmed by the game.",systemImage:"info.circle").font(.system(size:11)).foregroundStyle(Slate.muted)
        }.padding(26)
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
                HStack { Text("Destinations").font(.system(size:20,weight:.medium,design:.serif)).italic(); Spacer(); Text("\(filteredDestinations.count)").font(.system(size:10)).foregroundStyle(Slate.muted) }
                SearchBox(placeholder:"Search Hyrule",text:$worldSearch)
                Picker("Destination type",selection:$worldKind) { Text("All").tag("All"); Text("Shrines").tag("Shrines"); Text("Towers").tag("Towers") }.pickerStyle(.segmented).labelsHidden().controlSize(.small)
                if filteredDestinations.isEmpty {
                    emptyState("No destinations",icon:"mappin",detail:destinations.isEmpty ? "Your local destination catalog appears when the editor runtime is ready." : "Try a different search.").frame(maxHeight:.infinity)
                } else {
                    ScrollViewReader { proxy in
                        ScrollView {
                            LazyVStack(spacing:2) {
                                ForEach(filteredDestinations) { point in destinationRow(point).id(point.id) }
                            }
                        }.scrollIndicators(.hidden)
                            .onChange(of:selectedDestination) { _,id in if let id { withAnimation(reduceMotion ? nil : .easeInOut(duration:0.15)) { proxy.scrollTo(id,anchor:.center) } } }
                    }
                }
                Menu {
                    Button("Unlock all shrine travel…") { confirmAllShrines() }.disabled(!model.canUse("shrines"))
                    Button("Reveal map regions…") { confirmMap() }.disabled(!model.canUse("map"))
                    Divider()
                    Button("Unlock runes…") { confirmRunes() }.disabled(!model.canUse("runes"))
                    Button("Get paraglider…") { confirmParaglider() }.disabled(!model.canUse("runes"))
                    Divider()
                    Button("Full access…") { confirmFullAccess() }.disabled(!model.canUse("runes"))
                } label: { HStack { Image(systemName:"lock.open"); Text("World access"); Spacer(); Image(systemName:"chevron.down") } }
                    .menuStyle(.borderlessButton).font(.system(size:11,weight:.medium)).padding(10).background(Slate.raised,in:RoundedRectangle(cornerRadius:4))
            }.padding(15).frame(width:235).background(Slate.panel.opacity(0.55))
            line(vertical:true)
            VStack(spacing:0) {
                HyruleMapView(points:destinations,visibleIDs:Set(filteredDestinations.map(\.id)),selected:$selectedDestination,playerPosition:model.available ? model.state?.position : nil)
                line()
                destinationInspector
            }.frame(maxWidth:.infinity,maxHeight:.infinity)
        }
    }
    private func destinationRow(_ point:WorldDestination) -> some View {
        Button {
            withAnimation(reduceMotion ? nil : .easeInOut(duration:0.15)) { selectedDestination = point.id }
        } label: {
            HStack(spacing:10) {
                Image(systemName:point.kind.localizedCaseInsensitiveContains("tower") ? "antenna.radiowaves.left.and.right" : "diamond").font(.system(size:13)).foregroundStyle(point.unlocked == true ? Slate.cyan : Slate.gold).frame(width:18)
                VStack(alignment:.leading,spacing:4) {
                    Text(point.name).font(.system(size:12,weight:.medium,design:.serif)).italic().foregroundStyle(selectedDestination == point.id ? Slate.ivory : Slate.ivory.opacity(0.85)).lineLimit(1)
                    Text("\(point.kind.capitalized)\(point.unlocked == true ? " · Active" : "")").font(.system(size:9)).foregroundStyle(Slate.muted)
                }
                Spacer(minLength:0)
            }.padding(.horizontal,8).padding(.vertical,9).background(selectedDestination == point.id ? Slate.cyan.opacity(0.09) : Color.clear)
                .overlay(alignment:.leading) { if selectedDestination == point.id { Slate.cyan.frame(width:2) } }
        }.buttonStyle(.plain).accessibilityAddTraits(selectedDestination == point.id ? .isSelected : [])
    }
    private var destinationInspector: some View {
        VStack(alignment:.leading,spacing:11) {
            if let point = destination {
                HStack(alignment:.center,spacing:16) {
                    VStack(alignment:.leading,spacing:5) {
                        HStack(spacing:8) { Text(point.name).font(.system(size:21,weight:.medium,design:.serif)).italic(); if point.unlocked == true { Text("Active").font(.system(size:9,weight:.medium)).foregroundStyle(Slate.cyan).padding(.horizontal,6).padding(.vertical,3).background(Slate.cyan.opacity(0.08),in:Capsule()) } }
                        Text("\(point.kind.capitalized)  ·  X \(Int(point.x))  Y \(Int(point.y))  Z \(Int(point.z))").font(.system(size:10)).foregroundStyle(Slate.muted).monospacedDigit()
                    }
                    Spacer(minLength:0)
                    if !point.kind.localizedCaseInsensitiveContains("tower") {
                        Button("Activate shrine…") { confirmation = ConfirmAction(title:"Activate \(point.name)?",detail:"Enable this shrine's travel point. This does not mark the shrine completed. Save in BOTW to keep it.",button:"Activate shrine",command:.unlockShrine(point.id)) }.buttonStyle(SlateButtonStyle()).disabled(!model.canUse("shrines"))
                    }
                    Button { confirmation = ConfirmAction(title:"Travel to \(point.name)?",detail:"BOTW will load this destination using its normal travel system. Wait for loading to finish before sending another edit.",button:"Travel",command:.teleport(point.id)) } label: { Label("Teleport here…",systemImage:"location.north.fill") }
                        .buttonStyle(SlateButtonStyle(primary:true)).disabled(!model.canUse("teleport") || !point.permitsTeleport)
                }
                if !point.permitsTeleport { Text("Raise towers with Full Access, or activate the first tower in BOTW.").font(.system(size:10)).foregroundStyle(Slate.gold) }
            } else {
                HStack(spacing:13) {
                    Image(systemName:"mappin.and.ellipse").font(.system(size:24,weight:.light)).foregroundStyle(Slate.cyan)
                    VStack(alignment:.leading,spacing:5) { Text("Choose your next destination").font(.system(size:19,weight:.medium,design:.serif)).italic(); Text("Select a shrine or tower on the map, or search the list.").font(.system(size:11)).foregroundStyle(Slate.muted) }
                    Spacer()
                }
            }
        }.padding(.horizontal,23).padding(.vertical,18).frame(minHeight:100,maxHeight:122,alignment:.center).background(Slate.panel)
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
    private func confirmMap() { confirmation = ConfirmAction(title:"Reveal all map regions?",detail:"Reveal the map regions in your current game. Save in BOTW to keep the change.",button:"Reveal map",command:.revealMap) }
    private func confirmRunes() { confirmation = ConfirmAction(title:"Unlock runes?",detail:"Enable rune abilities in your current game. Save in BOTW to keep the change.",button:"Unlock runes",command:.runes) }
    private func confirmParaglider() { confirmation = ConfirmAction(title:"Get the paraglider?",detail:"Grant the paraglider in your current game. Save in BOTW to keep it.",button:"Get paraglider",command:.paraglider) }
    private func confirmFullAccess() { confirmation = ConfirmAction(title:"Unlock exploration access?",detail:"Activate shrine travel, reveal the map, raise every tower, unlock runes, and grant the paraglider. This skips the opening tower activation sequence. It does not directly mark shrines or story quests complete. Save in BOTW to keep these changes.",button:"Unlock full access",command:.fullAccess) }
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
    var body: some View {
        GeometryReader { geo in
            let rect = mapRect(geo.size)
            ZStack(alignment:.topLeading) {
                Slate.background
                if let terrain {
                    Image(nsImage:terrain.image).resizable().interpolation(.high).frame(width:rect.width,height:rect.height).position(x:rect.midX,y:rect.midY)
                        .saturation(0.8).brightness(-0.05)
                }
                Canvas { c,size in
                    if terrain == nil {
                        var grid = Path()
                        for i in 0...12 { let x = rect.minX+rect.width*CGFloat(i)/12; grid.move(to:CGPoint(x:x,y:rect.minY)); grid.addLine(to:CGPoint(x:x,y:rect.maxY)) }
                        for i in 0...10 { let y = rect.minY+rect.height*CGFloat(i)/10; grid.move(to:CGPoint(x:rect.minX,y:y)); grid.addLine(to:CGPoint(x:rect.maxX,y:y)) }
                        c.stroke(grid,with:.color(Slate.line),lineWidth:0.5)
                    }
                    for point in points where point.x.isFinite && point.z.isFinite {
                        let p = position(point,in:rect)
                        guard p.x >= -15,p.y >= -15,p.x <= size.width+15,p.y <= size.height+15 else { continue }
                        let active = point.unlocked == true
                        let color = active ? Slate.cyan : Slate.orange
                        let highlighted = selected == point.id || hover == point.id
                        let opacity = visibleIDs.contains(point.id) ? 1.0 : 0.15
                        let tower = point.kind.localizedCaseInsensitiveContains("tower")
                        let radius:CGFloat = highlighted ? 7 : tower ? 5 : 4
                        var path = Path()
                        if tower {
                            path.move(to:CGPoint(x:p.x,y:p.y-radius-2)); path.addLine(to:CGPoint(x:p.x+radius,y:p.y+radius)); path.addLine(to:CGPoint(x:p.x-radius,y:p.y+radius)); path.closeSubpath()
                            c.fill(path,with:.color(Slate.background.opacity(0.9)))
                            c.stroke(path,with:.color(color.opacity(opacity)),lineWidth:highlighted ? 2 : 1.5)
                            var bar = Path(); bar.move(to:CGPoint(x:p.x-3,y:p.y+1)); bar.addLine(to:CGPoint(x:p.x+3,y:p.y+1)); c.stroke(bar,with:.color(color.opacity(opacity)),lineWidth:1)
                        } else {
                            path.move(to:CGPoint(x:p.x,y:p.y-radius));path.addLine(to:CGPoint(x:p.x+radius,y:p.y));path.addLine(to:CGPoint(x:p.x,y:p.y+radius));path.addLine(to:CGPoint(x:p.x-radius,y:p.y));path.closeSubpath()
                            c.fill(path,with:.color(Slate.background.opacity(0.9)))
                            c.stroke(path,with:.color(color.opacity(opacity)),lineWidth:highlighted ? 2 : 1)
                            if active { c.fill(Path(ellipseIn:CGRect(x:p.x-1.3,y:p.y-1.3,width:2.6,height:2.6)),with:.color(color.opacity(opacity))) }
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
        VStack(spacing:0) {
            HStack(alignment:.top) {
                VStack(alignment:.leading,spacing:5) {
                    Text("Hyrule").font(.system(size:25,weight:.medium,design:.serif)).italic().foregroundStyle(Slate.ivory)
                    Text(terrain == nil ? "Coordinate view · Terrain not installed" : "Surface map").font(.system(size:10)).foregroundStyle(Slate.ivory.opacity(0.7))
                }.padding(.horizontal,14).padding(.vertical,11).background(Slate.background.opacity(0.8),in:RoundedRectangle(cornerRadius:4))
                Spacer()
                VStack(spacing:2) { Text("N").font(.system(size:10,weight:.semibold)); Image(systemName:"location.north.fill").font(.system(size:20,weight:.light)); Rectangle().fill(Slate.ivory.opacity(0.45)).frame(width:1,height:15) }.foregroundStyle(Slate.ivory).padding(10)
            }
            Spacer()
            if terrain == nil {
                Text("Install the editor bridge to extract terrain from your own game files.").font(.system(size:11)).foregroundStyle(Slate.ivory).padding(10).background(Slate.background.opacity(0.9),in:RoundedRectangle(cornerRadius:4)).padding(.bottom,10)
            }
            HStack(alignment:.bottom) {
                HStack(spacing:13) {
                    if let playerPosition, playerPosition.isFinite { Label("Link",systemImage:"circle.inset.filled").foregroundStyle(Slate.ivory) }
                    Label("Shrine",systemImage:"diamond").foregroundStyle(Slate.ivory.opacity(0.85))
                    Label("Tower",systemImage:"triangle").foregroundStyle(Slate.ivory.opacity(0.85))
                    HStack(spacing:4) { Circle().fill(Slate.cyan).frame(width:4,height:4); Text("Active") }
                    HStack(spacing:4) { Circle().fill(Slate.orange).frame(width:4,height:4); Text("Inactive") }
                }.font(.system(size:9)).padding(9).background(Slate.background.opacity(0.85),in:RoundedRectangle(cornerRadius:4))
                Spacer(minLength:6)
                VStack(spacing:0) {
                    Button { changeZoom(min(6,zoom*1.4),size:size) } label: { Image(systemName:"plus").frame(width:30,height:29) }.accessibilityLabel("Zoom in")
                    Rectangle().fill(Slate.line).frame(height:1)
                    Button { changeZoom(max(1,zoom/1.4),size:size) } label: { Image(systemName:"minus").frame(width:30,height:29) }.accessibilityLabel("Zoom out")
                    Rectangle().fill(Slate.line).frame(height:1)
                    Button { withAnimation(reduceMotion ? nil : .easeInOut(duration:0.18)) { zoom=1;previousZoom=1;offset = .zero } } label: { Image(systemName:"arrow.up.left.and.arrow.down.right").font(.system(size:10)).frame(width:30,height:29) }.accessibilityLabel("Show whole map")
                }.frame(width:32).buttonStyle(.plain).font(.system(size:12)).foregroundStyle(Slate.ivory).background(Slate.background.opacity(0.9),in:RoundedRectangle(cornerRadius:4)).overlay(RoundedRectangle(cornerRadius:4).stroke(Slate.line,lineWidth:1))
            }
        }.padding(17)
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

private struct AddItemView: View {
    @ObservedObject var model:PlaygroundModel
    @Environment(\.dismiss) private var dismiss
    @State private var search = ""
    @State private var category = "all"
    @State private var selected:String?
    @State private var customID = ""
    @State private var quantity = "1"
    private var filtered:[CatalogItem] { model.catalog.filter { (category == "all" || $0.category == category) && (search.isEmpty || $0.name.localizedCaseInsensitiveContains(search) || $0.id.localizedCaseInsensitiveContains(search)) } }
    private var chosen:String { customID.isEmpty ? selected ?? "" : customID.trimmingCharacters(in:.whitespaces) }
    private func iconType(_ category:String) -> Int {
        switch category { case "weapons":return 0;case "bows":return 1;case "shields":return 3;case "armors":return 5;case "materials":return 7;case "food":return 8;default:return 9 }
    }
    var body:some View {
        VStack(alignment:.leading,spacing:17) {
            HStack { VStack(alignment:.leading,spacing:5) { Text("Add an item").font(.system(size:26,weight:.medium,design:.serif)).italic(); Text("Choose from the catalog or enter an actor ID.").font(.system(size:11)).foregroundStyle(Slate.muted) }; Spacer(); Button { dismiss() } label: { Image(systemName:"xmark") }.buttonStyle(.plain).accessibilityLabel("Close item catalog") }
            HStack { SearchBox(placeholder:"Search 1,012 items",text:$search); Picker("Category",selection:$category) { Text("All categories").tag("all"); ForEach(Array(Set(model.catalog.map(\.category))).sorted(),id:\.self) { Text($0.capitalized).tag($0) } }.frame(width:160).controlSize(.small) }
            HStack(spacing:0) {
                List(selection:$selected) {
                    ForEach(filtered) { item in
                        HStack(spacing:12) { ItemArtwork(actor:item.id,type:iconType(item.category),size:38); VStack(alignment:.leading,spacing:4) { Text(item.name).font(.system(size:12)); Text(item.id).font(.system(size:9)).foregroundStyle(Slate.muted) }; Spacer() }.padding(.vertical,3).tag(item.id)
                    }
                }.listStyle(.plain).scrollContentBackground(.hidden).background(Slate.background).onChange(of:selected) { _,_ in customID="" }
                Rectangle().fill(Slate.line).frame(width:1)
                VStack(alignment:.leading,spacing:15) {
                    ItemArtwork(actor:chosen,type:iconType(model.catalog.first { $0.id == chosen }?.category ?? ""),size:100).frame(maxWidth:.infinity)
                    Text(model.catalog.first { $0.id == chosen }?.name ?? (chosen.isEmpty ? "Select an item" : chosen)).font(.system(size:21,weight:.medium,design:.serif)).italic()
                    Text("Quantity").font(.system(size:11,weight:.semibold))
                    TextField("1–999",text:$quantity).textFieldStyle(.roundedBorder).accessibilityLabel("Item quantity, 1 to 999")
                    Text("The game checks pouch capacity and item restrictions.").font(.system(size:10)).foregroundStyle(Slate.muted).fixedSize(horizontal:false,vertical:true)
                    Spacer()
                    Button("Give item") { model.send(.give(chosen,quantity)) }.buttonStyle(SlateButtonStyle(primary:true)).disabled(chosen.isEmpty || !model.canUse("items"))
                }.padding(20).frame(width:220)
            }.overlay(Rectangle().stroke(Slate.line,lineWidth:1))
            HStack { Text("Custom actor ID").font(.system(size:11)).foregroundStyle(Slate.muted); TextField("e.g. Item_Fruit_A",text:$customID).textFieldStyle(.roundedBorder).font(.system(size:11,design:.monospaced)) }
            HStack { if model.pending { ProgressView().controlSize(.mini) }; Text(model.feedback).font(.system(size:10)).foregroundStyle(model.feedbackIsError ? Slate.orange : Slate.muted); Spacer(); Button("Done") { dismiss() }.buttonStyle(SlateButtonStyle()).keyboardShortcut(.cancelAction) }
        }.padding(24).frame(width:760,height:620).background(Slate.panel).foregroundStyle(Slate.ivory).tint(Slate.cyan).preferredColorScheme(.dark)
    }
}
