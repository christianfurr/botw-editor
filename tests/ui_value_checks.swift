import Foundation

@main
struct UIValueChecks {
    static func main() throws {
        let decoder = JSONDecoder()
        func item(type:Int) throws -> InventoryEntry {
            try decoder.decode(InventoryEntry.self,from:Data("{\"slot\":12,\"name\":\"Test_Actor\",\"type\":\(type),\"value\":2000,\"equipped\":false}".utf8))
        }
        let weapon = try item(type:0)
        let material = try item(type:7)
        func check(_ condition:Bool,_ message:String) { precondition(condition,message) }
        for raw in [1,9,10,29,99,100,101,2000,3000,12_345_678,99_999_999,100_000_000] {
            let displayed = InventoryValuePresentation.durabilityText(rawValue:raw)
            let command = InventoryValuePresentation.commandText(for:weapon,input:displayed)
            check(command == String(raw),"Durability round trip failed for \(raw): \(displayed) -> \(command)")
        }
        check(InventoryValuePresentation.durabilityText(rawValue:2000) == "20","Whole durability display")
        check(InventoryValuePresentation.durabilityText(rawValue:2999) == "29.99","Fractional durability display")
        for input in ["0","-1","0.001","1000000.01","nan","inf","hello","30.000","0.01000001","1e6"] {
            check(InventoryValuePresentation.commandText(for:weapon,input:input).isEmpty,"Invalid durability accepted: \(input)")
        }
        check(InventoryValuePresentation.commandText(for:weapon,input:"30") == "3000","Human points conversion")
        check(InventoryValuePresentation.commandText(for:weapon,input:" 0.29 ") == "29","Fractional conversion")
        check(InventoryValuePresentation.commandText(for:material,input:"999") == "999","Stack quantities must remain unchanged")
        let payload = try TrainerCommand.quantity(weapon,InventoryValuePresentation.commandText(for:weapon,input:"30")).payload()
        check(payload == "quantity 12 3000","Native command uses raw hundredths")
        print("UI value checks passed: exact durability round trips, fractional precision, bounds, stack preservation, and native payload.")
    }
}
