// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstdint>
#include "HavokScript.h"

// Extra rules for upgrading units (gold upgrade, UnitCommandTypes.UPGRADE):
//   1. A unit cannot upgrade into a unit type its owner has build-disabled (MODIFIER_PLAYER_UNIT_BUILD_DISABLED)
//   2. Per upgrade step (unit -> upgrade unit) a minimum experience level of the unit (Lua unit:GetExperience():GetLevel():
//      1 for a new unit, +1 per promotion earned with experience; promotions given for free do not count). Set from Lua (the
//      DevCE_UnitUpgrades mod reads its table DevCE_UnitUpgradeRequirements, unique units included).
//   3. (switched on from Lua) An upgrade unit with Unit_BuildingPrereqs needs the unit to stand inside the borders of one of
//      its owner's cities that has one of those buildings (built, not pillaged: the same test as training it there).
//
// The game picks the upgrade target in Rules::Units::GetAvailableUpgrade: the next UnitUpgrades unit (the player's unique
// version), and further along the chain while the next one is unlocked too (Adept -> Archmage when both are). Dev CE walks
// the same chain for a unit and stops before a step the rules block. If the very first step is blocked, the target stays that
// unit, so the unit panel still shows "Upgrade to X" with the reason in red, and CanUpgrade says no.
//
// Hooks: GetAvailableUpgrade (our walk while a unit is known: called through GetUpgradeUnitType, GetUpgradeResourceCost or
// CanUpgrade), and those three to know the unit. Other callers (build queue, start state) keep the game's walk.
namespace UnitUpgradeRules {
	// Addresses in GameCore_XP2_FinalRelease.dll, Steam build 15038592 (same DLL in 15296837).
	constexpr uintptr_t GET_AVAILABLE_UPGRADE_OFFSET = 0x37dbe0;        // Rules::Units::Instance::GetAvailableUpgrade(rules, const Unit** def, player) -> unit or -1 (hooked)
	constexpr uintptr_t GET_NEXT_UPGRADE_OFFSET = 0x37dca0;             // Rules::Units::GetNextUpgrade(const Unit** def, player) -> first UnitUpgrades unit, player's version
	constexpr uintptr_t HAS_PREREQUISITES_OFFSET = 0x37e230;            // Rules::Units::Instance::HasPrerequisites(rules, const Unit** def, player)
	constexpr uintptr_t CHECK_UPGRADE_CYCLE_OFFSET = 0x37d920;          // Rules::Units::CheckForUnitUpgradeCycle(const Unit** def, player)
	constexpr uintptr_t GET_UNIT_DEFINITIONS_OFFSET = 0x7d1b00;         // Definitions::GetUnitDefinitions() -> vector<Unit*>
	constexpr uintptr_t GET_UPGRADE_UNIT_TYPE_OFFSET = 0x3b8010;        // Unit::Upgrade::Manager::GetUpgradeUnitType(manager, unit) (hooked)
	constexpr uintptr_t GET_UPGRADE_RESOURCE_COST_OFFSET = 0x3b7e10;    // Unit::Upgrade::Manager::GetUpgradeResourceCost(manager, unit) (hooked)
	constexpr uintptr_t CAN_UPGRADE_OFFSET = 0x3b7870;                  // Unit::Upgrade::Manager::CanUpgrade(manager, unit, testVisible, results) (hooked)

	// Layout (same build)
	constexpr uintptr_t UNIT_DEF_INDEX = 0x14;              // Definition::Unit: its UnitType index
	constexpr uintptr_t PLAYER_UNITS = 0x6c8;               // Player::Instance -> Player::Units*
	constexpr uintptr_t PLAYER_UNITS_BUILD_DISABLED = 0x198;  // Player::Units: uint8_t[unit type] (Lua PlayerUnits:IsBuildDisabled)
	constexpr uintptr_t UNIT_EXPERIENCE = 0x970;            // Unit::Instance: Experience (value of the FAutoVariable at +0x960)
	constexpr uintptr_t EXPERIENCE_LEVEL = 0x10;            // Experience: int level (Reset sets 1, Promote adds 1; SetLevel writes it)

	// Rule 3 (building prerequisites), same build
	constexpr uintptr_t GET_CITY_OFFSET = 0x42e40;                // City::Instance* GetCity(const CityID*) (Lua Cities.GetPlotPurchaseCity uses it)
	constexpr uintptr_t CITY_BUILDINGS_HAS_BUILDING_OFFSET = 0x10c450;  // City::Buildings::HasBuilding(Buildings*, BuildingTypes)
	constexpr uintptr_t CITY_BUILDINGS_IS_PILLAGED_OFFSET = 0x10ccb0;   // City::Buildings::IsPillaged(Buildings*, BuildingTypes)
	constexpr uintptr_t UNIT_OWNER = 0x128;                 // Unit::Instance: PlayerTypes owner
	constexpr uintptr_t PLOT_VT_CITY_ATTRIBUTES = 0x30;     // Map::Plot vtable: const CityAttributes* (plot)
	constexpr uintptr_t CITY_ATTR_CITY_ID = 0xc;            // CityAttributes: uint32 id of the city that owns the plot (-1: none)
	constexpr uintptr_t CITY_ATTR_OWNER = 0x1c;             // CityAttributes: int8 owner player (-1: none)
	constexpr uintptr_t CITY_BUILDINGS = 0xcb0;             // City::Instance: City::Buildings
	constexpr uintptr_t UNIT_DEF_PREREQ_BUILDINGS = 0x258;  // Definition::Unit: vector<const Building*> (Unit_BuildingPrereqs), begin/end
	constexpr uintptr_t BUILDING_DEF_INDEX = 0x14;          // Definition::Building: BuildingTypes index
	constexpr uintptr_t BUILDING_DEF_NAME = 0x68;           // Definition::Building: Name (LOC key, char*)

	void Create();
	void ResetForNewGame();
	int Register(hks::lua_State* L);   // gameplay state: UnitUpgradeRules.SetMinLevel / GetMinLevel / Clear / SetBuildingRule / GetBuildingRule
}
