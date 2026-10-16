/* AI/LLM-generated file. Do not treat as human-written source documentation. */
#include "TestHarness.h"
#include "core/world/Pathfinding.h"
#include "core/world/Vehicle.h"

using namespace fossynd::core;

FOSSYND_TEST(Vehicle, DefaultsAreACar)
{
    const Vehicle vehicle;
    FOSSYND_CHECK(vehicle.kind() == VehicleKind::Car);
    FOSSYND_CHECK(vehicle.isAlive());
    FOSSYND_CHECK(!vehicle.isOccupied());
    FOSSYND_CHECK_EQ(vehicle.driver(), -1);
    FOSSYND_CHECK_EQ(vehicle.tile(), (TilePos{0, 0}));
}

FOSSYND_TEST(Vehicle, FactoryBuildersDiffer)
{
    const Vehicle car = makeCar(1.0f, 2.0f);
    const Vehicle train = makeTrain(1.0f, 2.0f);

    FOSSYND_CHECK(car.kind() == VehicleKind::Car);
    FOSSYND_CHECK(train.kind() == VehicleKind::Train);
    FOSSYND_CHECK_EQ(std::string(car.kindName()), std::string("Car"));
    FOSSYND_CHECK_EQ(std::string(train.kindName()), std::string("Train"));

    // A train is the heavier, faster vehicle.
    FOSSYND_CHECK(train.maxHealth() > car.maxHealth());
    FOSSYND_CHECK(train.speed() > car.speed());

    // Both carry a mounted weapon.
    FOSSYND_CHECK(car.mountedWeapon().damage > 0.0f);
    FOSSYND_CHECK(train.mountedWeapon().damage > 0.0f);

    FOSSYND_CHECK_EQ(car.tile(), (TilePos{1, 2}));
}

FOSSYND_TEST(Vehicle, DriveToFindsPath)
{
    TileGrid grid(6, 6);
    Vehicle car = makeCar(0.0f, 0.0f);
    FOSSYND_CHECK(car.driveTo(grid, TilePos{5, 0}));
    FOSSYND_CHECK(car.hasPath());
    FOSSYND_CHECK(!car.path().empty());
    FOSSYND_CHECK_EQ(car.path().back(), (TilePos{5, 0}));
}

FOSSYND_TEST(Vehicle, DriveToUnreachableWalksAsCloseAsPossible)
{
    // A full wall at x=2 blocks the goal at (4,0). Like Agent::moveTo, driveTo
    // does not drop the order: the vehicle drives to the reachable tile nearest
    // the goal.
    TileGrid grid(5, 5);
    for (int y = 0; y < 5; ++y)
    {
        grid.setWalkable(2, y, false);
    }
    Vehicle car = makeCar(0.0f, 0.0f);
    FOSSYND_CHECK(car.driveTo(grid, TilePos{4, 0}));
    FOSSYND_CHECK(car.hasPath());
    FOSSYND_CHECK(!car.path().empty());
    if (car.path().empty())
    {
        return;
    }
    FOSSYND_CHECK_EQ(car.path().back(), (TilePos{1, 0}));
    for (const TilePos& tile : car.path())
    {
        FOSSYND_CHECK(grid.isWalkable(tile.x, tile.y));
    }
}

FOSSYND_TEST(Vehicle, DriveToReachablePathIsUnchanged)
{
    // When the goal is reachable, driveTo must be exactly findPath: the fallback
    // must not perturb a good route. Same route as an agent would walk.
    TileGrid grid(8, 8);
    Vehicle car = makeCar(0.0f, 0.0f);
    FOSSYND_CHECK(car.driveTo(grid, TilePos{5, 3}));

    const std::vector<TilePos> expected = findPath(grid, TilePos{0, 0}, TilePos{5, 3}, true);
    FOSSYND_CHECK(!expected.empty());
    FOSSYND_CHECK(car.path() == expected);
}

FOSSYND_TEST(Vehicle, DriveToEnclosedStartDropsOrder)
{
    TileGrid grid(3, 3);
    // Wall the start tile off completely, so there is nowhere at all to drive
    // and the order is genuinely dropped (unlike the near-miss case above).
    grid.setWalkable(0, 0, false);
    grid.setWalkable(0, 2, false);
    grid.setWalkable(1, 1, false);
    Vehicle car = makeCar(0.0f, 1.0f);
    FOSSYND_CHECK(!car.driveTo(grid, TilePos{2, 1}));
    FOSSYND_CHECK(!car.hasPath());
}

// The Vehicle mirror of `Agent.MoveToNeverEntersABlockedTileFromAnOffCentrePosition`
// (#396). `driveTo` keeps the vehicle's own tile centre as the first waypoint for
// the same reason `Agent::moveTo` does: the path is validated centre-to-centre, so
// a vehicle standing off-centre could otherwise walk straight at the second
// waypoint, cut the corner of a blocked tile the centre line never crossed, and
// end up inside a wall. Without the start waypoint this car clips (2,3) on its
// first step; with it, every tile visited is walkable and it reaches the goal.
FOSSYND_TEST(Vehicle, DriveToNeverEntersABlockedTileFromAnOffCentrePosition)
{
    const int width = 12;
    const char* rows[] = {
        ".....#.#....",
        ".##.#.......",
        "##..#.#..#..",
        "#.#....#...#",
    };
    TileGrid grid(width, 4);
    for (int y = 0; y < 4; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            grid.setWalkable(x, y, rows[y][x] == '.');
        }
    }

    // Off-centre inside walkable (2,2) — a fractional position like a mid-move
    // vehicle actually has.
    Vehicle car = makeCar(2.0f + 0.303f, 2.0f + 0.443f);
    FOSSYND_CHECK_EQ(car.tile(), (TilePos{2, 2}));
    FOSSYND_CHECK(car.driveTo(grid, TilePos{6, 3}));

    for (int tick = 0; tick < 900; ++tick)
    {
        car.update(1.0f / 60.0f);
        const TilePos at = car.tile();
        if (grid.inBounds(at.x, at.y))
        {
            FOSSYND_CHECK(grid.isWalkable(at.x, at.y));
        }
        if (!car.hasPath())
        {
            break;
        }
    }
    FOSSYND_CHECK_EQ(car.tile(), (TilePos{6, 3}));
}

// The Vehicle mirror of `Agent.MoveToReissuedSameRouteKeepsProgress` (#396): a
// per-tick re-issue of an unchanged route (how the mission AI drives) must not
// reset the vehicle to its tile centre, or it oscillates and never arrives.
FOSSYND_TEST(Vehicle, DriveToReissuedSameRouteKeepsProgress)
{
    TileGrid grid(12, 12);
    for (int y = 0; y < 12; ++y)
    {
        grid.setWalkable(9, y, false);
    }

    Vehicle car = makeCar(0.0f, 0.0f);
    car.setSpeed(2.0f);
    FOSSYND_CHECK(car.driveTo(grid, TilePos{5, 5}));

    bool reached = false;
    for (int tick = 0; tick < 3000; ++tick)
    {
        car.update(1.0f / 60.0f);
        if (car.tile() == (TilePos{5, 5}))
        {
            reached = true;
            break;
        }
        FOSSYND_CHECK(car.driveTo(grid, TilePos{5, 5})); // re-issue, unchanged
    }
    FOSSYND_CHECK(reached);
    FOSSYND_CHECK_EQ(car.tile(), (TilePos{5, 5}));
}

FOSSYND_TEST(Vehicle, UpdateMovesAlongPath)
{
    TileGrid grid(10, 3);
    Vehicle car = makeCar(0.0f, 0.0f);
    car.setSpeed(2.0f);
    FOSSYND_CHECK(car.driveTo(grid, TilePos{4, 0}));

    // Two tiles per second: after one second the car is two tiles along.
    car.update(1.0f);
    FOSSYND_CHECK_NEAR(car.position().x, 2.0f);
    FOSSYND_CHECK_NEAR(car.position().y, 0.0f);

    // A non-positive step changes nothing.
    car.update(0.0f);
    FOSSYND_CHECK_NEAR(car.position().x, 2.0f);

    // Finish the route and stop cleanly at the goal.
    car.update(5.0f);
    FOSSYND_CHECK_NEAR(car.position().x, 4.0f);
    FOSSYND_CHECK(!car.hasPath());
}

FOSSYND_TEST(Vehicle, UpdateKeepsInterpolationConsistent)
{
    TileGrid grid(10, 3);
    Vehicle car = makeCar(0.0f, 0.0f);
    car.setSpeed(1.0f);
    FOSSYND_CHECK(car.driveTo(grid, TilePos{5, 0}));

    car.update(0.5f);
    // Half a tile travelled this step, so the previous step was at x=0 and the
    // blend at alpha 0.5 sits a quarter tile along.
    FOSSYND_CHECK_NEAR(car.position().x, 0.5f);
    FOSSYND_CHECK_NEAR(car.previousPosition().x, 0.0f);
    FOSSYND_CHECK_NEAR(car.interpolatedPosition(0.5f).x, 0.25f);
    // The blend is clamped to [0, 1].
    FOSSYND_CHECK_NEAR(car.interpolatedPosition(-1.0f).x, car.previousPosition().x);
    FOSSYND_CHECK_NEAR(car.interpolatedPosition(2.0f).x, car.position().x);
}

FOSSYND_TEST(Vehicle, HijackTakesAndReleasesTheWheel)
{
    Vehicle car = makeCar(0.0f, 0.0f);
    FOSSYND_CHECK(!car.isOccupied());

    FOSSYND_CHECK(car.hijack(3));
    FOSSYND_CHECK(car.isOccupied());
    FOSSYND_CHECK_EQ(car.driver(), 3);

    // Re-hijacking with the same agent is a harmless no-op.
    FOSSYND_CHECK(car.hijack(3));
    FOSSYND_CHECK_EQ(car.driver(), 3);

    // A second agent cannot take a car that is already being driven.
    FOSSYND_CHECK(!car.hijack(4));
    FOSSYND_CHECK_EQ(car.driver(), 3);

    car.eject();
    FOSSYND_CHECK(!car.isOccupied());
    FOSSYND_CHECK_EQ(car.driver(), -1);

    // After an eject the seat is free again.
    FOSSYND_CHECK(car.hijack(4));
    FOSSYND_CHECK_EQ(car.driver(), 4);
}

FOSSYND_TEST(Vehicle, HijackRejectsNegativeIndex)
{
    Vehicle car = makeCar(0.0f, 0.0f);
    FOSSYND_CHECK(!car.hijack(-1));
    FOSSYND_CHECK(!car.isOccupied());
}

FOSSYND_TEST(Vehicle, DamageReducesHealthAndClamps)
{
    Vehicle car = makeCar(0.0f, 0.0f);
    const float full = car.maxHealth();

    FOSSYND_CHECK_NEAR(car.applyDamage(30.0f), 30.0f);
    FOSSYND_CHECK_NEAR(car.health(), full - 30.0f);
    FOSSYND_CHECK_NEAR(car.healthFraction(), (full - 30.0f) / full);

    // Overkill only deals the health that was left.
    FOSSYND_CHECK_NEAR(car.applyDamage(full * 10.0f), full - 30.0f);
    FOSSYND_CHECK(!car.isAlive());
    FOSSYND_CHECK(car.isDestroyed());
    FOSSYND_CHECK_NEAR(car.healthFraction(), 0.0f);

    // A destroyed vehicle takes no further damage.
    FOSSYND_CHECK_NEAR(car.applyDamage(10.0f), 0.0f);
    // Non-positive damage is ignored.
    Vehicle other = makeCar(0.0f, 0.0f);
    FOSSYND_CHECK_NEAR(other.applyDamage(0.0f), 0.0f);
    FOSSYND_CHECK_NEAR(other.applyDamage(-5.0f), 0.0f);
    FOSSYND_CHECK_NEAR(other.health(), other.maxHealth());
}

FOSSYND_TEST(Vehicle, DestructionStopsAndEjects)
{
    TileGrid grid(10, 3);
    Vehicle car = makeCar(0.0f, 0.0f);
    FOSSYND_CHECK(car.driveTo(grid, TilePos{5, 0}));
    FOSSYND_CHECK(car.hijack(1));

    car.applyDamage(car.maxHealth());

    FOSSYND_CHECK(car.isDestroyed());
    FOSSYND_CHECK(!car.hasPath());
    FOSSYND_CHECK(!car.isOccupied());
    FOSSYND_CHECK_EQ(car.driver(), -1);

    // A wreck cannot be hijacked and does not move.
    FOSSYND_CHECK(!car.hijack(2));
    car.update(1.0f);
    FOSSYND_CHECK_NEAR(car.position().x, 0.0f);
}

FOSSYND_TEST(Vehicle, MountedFireNeedsDriverAndHealth)
{
    Vehicle car = makeCar(0.0f, 0.0f);

    // Unoccupied: no gunner.
    FOSSYND_CHECK(!car.canFire());

    FOSSYND_CHECK(car.hijack(0));
    FOSSYND_CHECK(car.canFire());

    // Firing starts the weapon's cooldown.
    car.beginFireCooldown();
    FOSSYND_CHECK(car.fireCooldown() > 0.0f);
    FOSSYND_CHECK(!car.canFire());

    // The cooldown decays with the fixed timestep.
    car.update(car.mountedWeapon().cooldown());
    FOSSYND_CHECK(car.canFire());

    // A wreck has no gunner even if a driver index lingers.
    car.applyDamage(car.maxHealth());
    FOSSYND_CHECK(!car.canFire());
}

FOSSYND_TEST(Vehicle, SetMaxHealthRefillsAndClamps)
{
    Vehicle car = makeCar(0.0f, 0.0f);
    car.applyDamage(40.0f);
    car.setMaxHealth(200.0f);
    FOSSYND_CHECK_NEAR(car.maxHealth(), 200.0f);
    FOSSYND_CHECK_NEAR(car.health(), 200.0f);
    FOSSYND_CHECK_NEAR(car.healthFraction(), 1.0f);

    // A degenerate max is clamped to at least 1.
    car.setMaxHealth(0.0f);
    FOSSYND_CHECK_NEAR(car.maxHealth(), 1.0f);
}

FOSSYND_TEST(Vehicle, SetPositionSnapsInterpolation)
{
    Vehicle car = makeCar(0.0f, 0.0f);
    car.setPosition(4.0f, 7.0f);
    FOSSYND_CHECK_EQ(car.tile(), (TilePos{4, 7}));
    // A teleport must not interpolate across the map.
    FOSSYND_CHECK_NEAR(car.interpolatedPosition(0.5f).x, 4.0f);
    FOSSYND_CHECK_NEAR(car.interpolatedPosition(0.5f).y, 7.0f);
}

FOSSYND_TEST(Vehicle, MovementIsDeterministic)
{
    TileGrid grid(12, 12);
    for (int y = 0; y < 12; ++y)
    {
        if (y != 6)
        {
            grid.setWalkable(6, y, false);
        }
    }

    Vehicle a = makeCar(0.0f, 0.0f);
    Vehicle b = makeCar(0.0f, 0.0f);
    FOSSYND_CHECK(a.driveTo(grid, TilePos{11, 11}));
    FOSSYND_CHECK(b.driveTo(grid, TilePos{11, 11}));

    for (int i = 0; i < 200; ++i)
    {
        a.update(0.05f);
        b.update(0.05f);
    }
    FOSSYND_CHECK(a.position() == b.position());
    FOSSYND_CHECK(a.path() == b.path());
}
