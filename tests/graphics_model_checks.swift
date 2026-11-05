import Foundation

@main
struct GraphicsChecks {
    static func main() throws {
        let decoder = JSONDecoder()
        var configuration = GraphicsConfiguration()
        precondition(configuration.preset == .off)
        try configuration.validate()
        configuration.preset = .natural
        configuration.revision = 17
        let encoded = try JSONEncoder().encode(configuration)
        let decoded = try decoder.decode(GraphicsConfiguration.self,from:encoded)
        precondition(decoded.preset == .natural && decoded.revision == 17)
        for strength in [-0.01,1.01,Double.nan,Double.infinity] {
            var invalid = configuration; invalid.strength = strength
            do { try invalid.validate(); fatalError("Invalid effect strength accepted") } catch is GraphicsError { }
        }
        for sharpness in [-1.0,1.001,Double.nan] {
            var invalid = configuration; invalid.sharpness = sharpness
            do { try invalid.validate(); fatalError("Invalid sharpness accepted") } catch is GraphicsError { }
        }
        func status(revision:UInt64 = 17,applied:String = "natural",error:String? = nil) -> GraphicsRuntimeStatus {
            GraphicsRuntimeStatus(schema:1,pid:123,supported:true,revision:revision,requestedPreset:"natural",appliedPreset:applied,strength:0.65,sharpness:0.15,error:error,stage:"applied")
        }
        precondition(status().confirms(configuration))
        precondition(!status(revision:16).confirms(configuration),"Stale shader acknowledgements must not confirm a change")
        precondition(!status(applied:"off",error:"Compiler error").confirms(configuration))
        precondition(!status(applied:"vivid").confirms(configuration))
        precondition(!status(error:"Compiler error").confirms(configuration))
        let badSchema = GraphicsRuntimeStatus(schema:2,pid:123,supported:true,revision:17,requestedPreset:"natural",appliedPreset:"natural",strength:0.65,sharpness:0.15,error:nil,stage:"applied")
        precondition(!badSchema.confirms(configuration))
        let notDrawn = GraphicsRuntimeStatus(schema:1,pid:123,supported:true,revision:17,requestedPreset:"natural",appliedPreset:"natural",strength:0.65,sharpness:0.15,error:nil,stage:"ready")
        precondition(!notDrawn.confirms(configuration),"Parsed settings alone must not confirm a rendered effect")
        var hugeRevision = configuration; hugeRevision.revision = 9_007_199_254_740_992
        do { try hugeRevision.validate(); fatalError("Inexact JSON revision accepted") } catch is GraphicsError { }
        print("Graphics model checks passed: neutral default, bounded effects, configuration round trip, and matching runtime acknowledgements.")
    }
}
