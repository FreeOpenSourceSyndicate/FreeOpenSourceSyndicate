/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include <algorithm>

#include "TestHarness.h"
#include "core/economy/Economy.h"
#include "core/economy/Equipment.h"
#include "core/economy/Research.h"
#include "core/sim/PlayerLoadout.h"

using namespace fossynd::core;

namespace
{

const char* kCatalogueJson = R"({
  "items": [
    { "id": "pistol", "kind": "weapon", "name": "Pistol", "cost": 0 },
    { "id": "rifle", "kind": "weapon", "name": "Rifle", "cost": 400,
      "requiresResearch": "a" },
    { "id": "shield", "kind": "gadget", "name": "Shield", "cost": 300 }
  ]
})";

const char* kTreeJson = R"({
  "nodes": [ { "id": "a", "name": "A", "cost": 100, "time": 10, "requires": [] } ]
})";

// A catalogue with two cybernetics and a weapon, for the equip same-kind rule.
const char* kCatalogueJsonWithCyber = R"({
  "items": [
    { "id": "pistol", "kind": "weapon", "name": "Pistol", "cost": 0 },
    { "id": "arms", "kind": "cybernetic", "name": "Arms", "cost": 500 },
    { "id": "legs", "kind": "cybernetic", "name": "Legs", "cost": 500 }
  ]
})";

EquipmentCatalogue catalogueOrFail(const char* json)
{
    std::string error;
    const Json root = Json::parse(json, error);
    FOSSYND_CHECK(error.empty());
    EquipmentCatalogue catalogue;
    FOSSYND_CHECK(EquipmentCatalogue::fromJson(root, catalogue, error));
    FOSSYND_CHECK(error.empty());
    return catalogue;
}

// A research state with node "a" completed.
Research researchWithA()
{
    std::string error;
    const Json root = Json::parse(kTreeJson, error);
    FOSSYND_CHECK(error.empty());
    ResearchTree tree;
    FOSSYND_CHECK(ResearchTree::fromJson(root, tree, error));
    Research research;
    Economy economy;
    economy.addFunds(0, 1000.0f);
    FOSSYND_CHECK(research.begin(0, "a", tree, economy));
    research.tick(10.0f);
    FOSSYND_CHECK(research.completed(0, "a"));
    return research;
}

} // namespace

FOSSYND_TEST(Equipment, ParsesCatalogue)
{
    const EquipmentCatalogue catalogue = catalogueOrFail(kCatalogueJson);

    FOSSYND_CHECK_EQ(catalogue.size(), std::size_t{3});
    FOSSYND_CHECK(catalogue.find("pistol") != nullptr);
    FOSSYND_CHECK(catalogue.find("rifle") != nullptr);
    FOSSYND_CHECK(catalogue.find("nope") == nullptr);
    FOSSYND_CHECK(catalogue.find("rifle")->kind == EquipmentKind::Weapon);
    FOSSYND_CHECK(catalogue.find("shield")->kind == EquipmentKind::Gadget);
    FOSSYND_CHECK_NEAR(catalogue.find("rifle")->cost, 400.0f);
    FOSSYND_CHECK_EQ(catalogue.find("rifle")->requiresResearch, std::string("a"));
    FOSSYND_CHECK_EQ(catalogue.find("pistol")->requiresResearch, std::string(""));
}

FOSSYND_TEST(Equipment, RejectsUnknownKind)
{
    std::string error;
    const Json root = Json::parse(R"({
      "items": [ { "id": "x", "kind": "spaceship", "name": "X" } ]
    })",
                                   error);
    FOSSYND_CHECK(error.empty());

    EquipmentCatalogue catalogue;
    FOSSYND_CHECK(!EquipmentCatalogue::fromJson(root, catalogue, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Equipment, RejectsDuplicateId)
{
    std::string error;
    const Json root = Json::parse(R"({
      "items": [
        { "id": "x", "kind": "weapon", "name": "X" },
        { "id": "x", "kind": "gadget", "name": "X2" }
      ]
    })",
                                   error);
    FOSSYND_CHECK(error.empty());

    EquipmentCatalogue catalogue;
    FOSSYND_CHECK(!EquipmentCatalogue::fromJson(root, catalogue, error));
    FOSSYND_CHECK(!error.empty());
}

FOSSYND_TEST(Equipment, BuyRespectsResearchGate)
{
    const EquipmentCatalogue catalogue = catalogueOrFail(kCatalogueJson);
    Equipment equipment;
    Economy economy;
    economy.addFunds(0, 1000.0f);

    // No research done: the gated item cannot be bought, and no funds are spent.
    Research none;
    FOSSYND_CHECK(!equipment.buy(0, "rifle", catalogue, none, economy));
    FOSSYND_CHECK_NEAR(economy.funds(0), 1000.0f);

    // With "a" completed, it can be bought.
    const Research done = researchWithA();
    FOSSYND_CHECK(equipment.buy(0, "rifle", catalogue, done, economy));
    FOSSYND_CHECK_NEAR(economy.funds(0), 600.0f);
}

FOSSYND_TEST(Equipment, BuyRespectsFunds)
{
    const EquipmentCatalogue catalogue = catalogueOrFail(kCatalogueJson);
    Equipment equipment;
    Economy economy;
    economy.addFunds(0, 100.0f); // less than the 400 rifle

    const Research done = researchWithA();
    FOSSYND_CHECK(!equipment.buy(0, "rifle", catalogue, done, economy));
    FOSSYND_CHECK_NEAR(economy.funds(0), 100.0f);
}

FOSSYND_TEST(Equipment, BuysUngatedItemAndRejectsUnknown)
{
    const EquipmentCatalogue catalogue = catalogueOrFail(kCatalogueJson);
    Equipment equipment;
    Economy economy;
    economy.addFunds(0, 1000.0f);
    Research none;

    // Free, ungated item.
    FOSSYND_CHECK(equipment.buy(0, "pistol", catalogue, none, economy));
    FOSSYND_CHECK_NEAR(economy.funds(0), 1000.0f);

    // Unknown item: no change.
    FOSSYND_CHECK(!equipment.buy(0, "ghost", catalogue, none, economy));
    FOSSYND_CHECK_NEAR(economy.funds(0), 1000.0f);
}

FOSSYND_TEST(Equipment, AssignAndClearLoadout)
{
    Equipment equipment;

    equipment.assign(0, 0, "pistol");
    equipment.assign(0, 0, "shield");
    equipment.assign(0, 0, "pistol"); // duplicate: ignored

    const std::vector<std::string> slot = equipment.loadout(0, 0);
    FOSSYND_CHECK_EQ(slot.size(), std::size_t{2});
    FOSSYND_CHECK_EQ(slot[0], std::string("pistol"));
    FOSSYND_CHECK_EQ(slot[1], std::string("shield"));

    equipment.clear(0, 0);
    FOSSYND_CHECK_EQ(equipment.loadout(0, 0).size(), std::size_t{0});

    // Out-of-range slot / unknown syndicate: no-op, no crash.
    equipment.assign(0, 99, "pistol");
    equipment.assign(0, -1, "pistol");
    FOSSYND_CHECK_EQ(equipment.loadout(0, 99).size(), std::size_t{0});
    FOSSYND_CHECK_EQ(equipment.loadout(5, 0).size(), std::size_t{0});
}

FOSSYND_TEST(Equipment, EquipReplacesSameKindAndKeepsOtherKind)
{
    const EquipmentCatalogue catalogue = catalogueOrFail(kCatalogueJson);
    Equipment equipment;

    // A weapon replaces the slot's weapon; the gadget is kept (a slot deploys one
    // of each kind, so both fit).
    FOSSYND_CHECK(equipment.equip(0, 0, "pistol", catalogue));
    FOSSYND_CHECK(equipment.equip(0, 0, "shield", catalogue));
    FOSSYND_CHECK(equipment.equip(0, 0, "rifle", catalogue)); // replaces pistol
    const std::vector<std::string> slot = equipment.loadout(0, 0);
    FOSSYND_CHECK_EQ(slot.size(), std::size_t{2});
    FOSSYND_CHECK(std::find(slot.begin(), slot.end(), "rifle") != slot.end());
    FOSSYND_CHECK(std::find(slot.begin(), slot.end(), "pistol") == slot.end());
    FOSSYND_CHECK(std::find(slot.begin(), slot.end(), "shield") != slot.end());

    // Equipping the item already in the slot is a no-op (not a duplicate).
    FOSSYND_CHECK(equipment.equip(0, 0, "rifle", catalogue));
    FOSSYND_CHECK_EQ(equipment.loadout(0, 0).size(), std::size_t{2});

    // An id that is not in the catalogue is refused, and nothing changes.
    FOSSYND_CHECK(!equipment.equip(0, 0, "ghost", catalogue));
    FOSSYND_CHECK(!equipment.equip(0, 99, "pistol", catalogue));
    FOSSYND_CHECK_EQ(equipment.loadout(0, 0).size(), std::size_t{2});
}

FOSSYND_TEST(Equipment, EquipReplacesACyberneticToo)
{
    // A LoadoutSlot carries one cybernetic, so a second cybernetic replaces the
    // first (the same rule as weapon/gadget), keeping the slot's other kinds.
    const EquipmentCatalogue catalogue = catalogueOrFail(kCatalogueJsonWithCyber);
    Equipment equipment;
    FOSSYND_CHECK(equipment.equip(0, 0, "pistol", catalogue));
    FOSSYND_CHECK(equipment.equip(0, 0, "arms", catalogue));
    FOSSYND_CHECK(equipment.equip(0, 0, "legs", catalogue)); // replaces arms

    const std::vector<std::string> slot = equipment.loadout(0, 0);
    FOSSYND_CHECK_EQ(slot.size(), std::size_t{2});
    FOSSYND_CHECK(std::find(slot.begin(), slot.end(), "legs") != slot.end());
    FOSSYND_CHECK(std::find(slot.begin(), slot.end(), "arms") == slot.end());
    FOSSYND_CHECK(std::find(slot.begin(), slot.end(), "pistol") != slot.end());
}

FOSSYND_TEST(Equipment, EquipUpgradeDeploysTheNewWeapon)
{
    // The bug this guards: buying a second weapon into a slot used to spend the
    // funds but deploy the *first* weapon, so an upgrade was silently lost.
    const EquipmentCatalogue catalogue = catalogueOrFail(kCatalogueJson);
    Equipment equipment;
    FOSSYND_CHECK(equipment.equip(0, 0, "pistol", catalogue));
    FOSSYND_CHECK(equipment.equip(0, 0, "rifle", catalogue));

    // A slot holds exactly one weapon; the upgrade is the one deployed.
    const std::vector<std::string> slot = equipment.loadout(0, 0);
    int weapons = 0;
    for (const std::string& id : slot)
    {
        const EquipmentItem* item = catalogue.find(id);
        if (item != nullptr && item->kind == EquipmentKind::Weapon)
        {
            ++weapons;
            FOSSYND_CHECK_EQ(id, std::string("rifle"));
        }
    }
    FOSSYND_CHECK_EQ(weapons, 1);
}

FOSSYND_TEST(Equipment, LoadoutsAreIndependentPerSyndicate)
{
    Equipment equipment;

    equipment.assign(0, 0, "pistol");
    equipment.assign(1, 0, "shield");

    FOSSYND_CHECK_EQ(equipment.loadout(0, 0)[0], std::string("pistol"));
    FOSSYND_CHECK_EQ(equipment.loadout(1, 0)[0], std::string("shield"));
}

FOSSYND_TEST(Equipment, LoadsShippedCatalogueFile)
{
    std::string error;
    EquipmentCatalogue catalogue;
    const std::string path = std::string(FOSSYND_SOURCE_DIR) + "/assets/data/world/equipment.json";
    FOSSYND_CHECK(EquipmentCatalogue::loadFromFile(path, catalogue, error));
    FOSSYND_CHECK(error.empty());
    FOSSYND_CHECK(catalogue.size() >= std::size_t{4});
    FOSSYND_CHECK(catalogue.find("pistol") != nullptr);

    // A gadget the catalogue sells must be in the deploy vocabulary, or it can be
    // bought and equipped yet silently grants nothing when the squad deploys
    // (gadgetChargesForId rejects an unknown id). The scanner and drugs landed in
    // core with #314; guard that the catalogue keeps up.
    FOSSYND_CHECK(catalogue.find("scanner") != nullptr);
    FOSSYND_CHECK(catalogue.find("drugs") != nullptr);
    for (const EquipmentItem& item : catalogue.items())
    {
        if (item.kind == EquipmentKind::Gadget)
        {
            FOSSYND_CHECK(gadgetChargesForId(item.id) != kUnknownGadget);
        }
    }
}
