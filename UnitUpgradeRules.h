// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <cstdint>
#include "HavokScript.h"

// Extra rules for upgrading units (gold upgrade, UnitCommandTypes.UPGRADE):
//   1. A unit cannot upgrade into a unit type its owner has build-disabled (MODIFIER_PLAYER_UNIT_BUILD_DISABLED)
//   2. Per upgrade step (unit -> upgrade unit) a minimum experience level of the unit (Lua unit:GetExperience():GetLevel():
//      1 for a new unit, +1 per promotion earned with experience; promotions given for free do not count). Set from Lua (the
//      DevCE_UnitUpgrades mod reads its table DevCE_UnitUpgradeRequirements, unique units included).
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

	void Create();
	void ResetForNewGame();
	int Register(hks::lua_State* L);   // gameplay state: UnitUpgradeRules.SetMinLevel / GetMinLevel / Clear
}
