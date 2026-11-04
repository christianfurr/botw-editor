import AppKit
import Foundation

// Original vector artwork. No game assets or external fonts are used.
let output = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
try FileManager.default.createDirectory(at: output, withIntermediateDirectories: true)

func drawIcon(size: Int) throws -> Data {
    let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: size, pixelsHigh: size,
        bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
        colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    let context = NSGraphicsContext(bitmapImageRep: bitmap)!
    NSGraphicsContext.saveGraphicsState()
    defer { NSGraphicsContext.restoreGraphicsState() }
    NSGraphicsContext.current = context
    context.cgContext.scaleBy(x: CGFloat(size) / 1024, y: CGFloat(size) / 1024)

    let cyan = NSColor(srgbRed: 0.33, green: 0.9, blue: 0.95, alpha: 1)
    let gold = NSColor(srgbRed: 0.68, green: 0.52, blue: 0.31, alpha: 1)
    let ivory = NSColor(srgbRed: 0.95, green: 0.89, blue: 0.73, alpha: 1)
    let plate = NSBezierPath(roundedRect: NSRect(x: 58,y: 58,width: 908,height: 908), xRadius: 190,yRadius: 190)
    let shadow = NSShadow()
    shadow.shadowColor = NSColor.black.withAlphaComponent(0.35)
    shadow.shadowBlurRadius = 30
    shadow.shadowOffset = NSSize(width: 0,height: -8)
    shadow.set()
    NSGradient(starting: NSColor(srgbRed: 0.14,green: 0.20,blue: 0.21,alpha: 1),
               ending: NSColor(srgbRed: 0.035,green: 0.07,blue: 0.08,alpha: 1))!
        .draw(in: plate, angle: -45)
    NSShadow().set()
    gold.withAlphaComponent(0.9).setStroke()
    plate.lineWidth = 14
    plate.stroke()
    let etched = NSBezierPath(roundedRect: NSRect(x: 95,y: 95,width: 834,height: 834),xRadius: 150,yRadius: 150)
    gold.withAlphaComponent(0.3).setStroke()
    etched.lineWidth = 3
    etched.stroke()

    let eye = NSBezierPath()
    eye.move(to: NSPoint(x: 200,y: 560))
    eye.curve(to: NSPoint(x: 824,y: 560),controlPoint1: NSPoint(x: 365,y: 760),controlPoint2: NSPoint(x: 660,y: 760))
    eye.curve(to: NSPoint(x: 200,y: 560),controlPoint1: NSPoint(x: 660,y: 355),controlPoint2: NSPoint(x: 365,y: 355))
    eye.close()
    eye.lineJoinStyle = .round
    for width in stride(from: 40.0,through: 20.0,by: -10) {
        cyan.withAlphaComponent(0.035).setStroke()
        eye.lineWidth = width
        eye.stroke()
    }
    cyan.setStroke()
    eye.lineWidth = 15
    eye.stroke()
    let iris = NSBezierPath(ovalIn: NSRect(x: 448,y: 496,width: 128,height: 128))
    iris.lineWidth = 11
    iris.stroke()
    ivory.setFill()
    NSBezierPath(ovalIn: NSRect(x: 490,y: 538,width: 44,height: 44)).fill()

    let glyph = NSBezierPath()
    glyph.move(to: NSPoint(x: 512,y: 422))
    glyph.line(to: NSPoint(x: 512,y: 230))
    glyph.line(to: NSPoint(x: 464,y: 290))
    for x in [370.0,512.0,654.0] {
        glyph.move(to: NSPoint(x: x,y: 755))
        glyph.line(to: NSPoint(x: x + (x-512)*0.12,y: 813))
    }
    cyan.setStroke()
    glyph.lineWidth = 12
    glyph.lineCapStyle = .round
    glyph.lineJoinStyle = .round
    glyph.stroke()

    let circuit = NSBezierPath()
    for sign in [-1.0,1.0] {
        let x = 512 + sign*335
        circuit.move(to: NSPoint(x: x,y: 720))
        circuit.line(to: NSPoint(x: x,y: 790))
        circuit.line(to: NSPoint(x: x-sign*60,y: 845))
        circuit.move(to: NSPoint(x: x,y: 350))
        circuit.line(to: NSPoint(x: x,y: 245))
        circuit.line(to: NSPoint(x: x-sign*60,y: 190))
    }
    gold.withAlphaComponent(0.8).setStroke()
    circuit.lineWidth = 7
    circuit.stroke()
    return bitmap.representation(using: .png,properties: [:])!
}

let sizes: [(String,Int)] = [
    ("icon_16x16.png",16),("icon_16x16@2x.png",32),
    ("icon_32x32.png",32),("icon_32x32@2x.png",64),
    ("icon_128x128.png",128),("icon_128x128@2x.png",256),
    ("icon_256x256.png",256),("icon_256x256@2x.png",512),
    ("icon_512x512.png",512),("icon_512x512@2x.png",1024)
]
for (name,size) in sizes { try drawIcon(size: size).write(to: output.appendingPathComponent(name)) }
