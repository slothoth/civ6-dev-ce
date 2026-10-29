// SPDX-License-Identifier: AGPL-3.0-only
// Part of Dev CE, a fork of the Civilization VI Community Extension by Wild-W (https://github.com/Wild-W/CivilizationVI_CommunityExtension).
// Licensed under the GNU Affero General Public License v3.0 (see LICENSE.txt). Not endorsed by Wild-W, Firaxis or 2K.
#include "UnitUpgradeRules.h"
#include "UnitCommandActions.h"
#include "Runtime.h"
#include "DevBridge.h"
#include <cstring>
#include <unordered_map>

namespace UnitUpgradeRules {
	struct UnitVector { void** begin; void** end; };
	typedef int(__fastcall* GetAvailableUpgradeFn)(void* rules, void** def, void* player);
	typedef int(__fastcall* GetNextUpgradeFn)(void** def, void* player);
	typedef bool(__fastcall* HasPrerequisitesFn)(void* rules, void** def, void* player);
	typedef bool(__fastcall* CheckCycleFn)(void** def, void* player);
	typedef UnitVector* (__fastcall* GetUnitDefinitionsFn)();
	typedef int(__fastcall* ManagerUnitFn)(void* manager, void* unit);
	typedef bool(__fastcall* CanUpgradeFn)(void* manager, void* unit, bool testVisible, void* results);
	typedef void(__fastcall* AddStringFn)(void* results, uint32_t hash, const void* locString);
	typedef void* (__fastcall* LocLookupFn)(void* out, const char* key);
	typedef void(__fastcall* LocDtorFn)(void* self);
	typedef bool(__fastcall* LocPushIntFn)(void* self, int n);
	typedef bool(__fastcall* LocPushStrFn)(void* self, const char* s);
	typedef void* (__fastcall* UnitGetPlotFn)(void* unit);
	typedef void* (__fastcall* PlotCityAttributesFn)(void* plot);
	struct CityID { int16_t owner; int16_t kind; uint32_t id; };   // kind 2 = city
	typedef void* (__fastcall* GetCityFn)(const CityID* id);
	typedef bool(__fastcall* BuildingsTestFn)(void* buildings, int buildingType);

	static GetAvailableUpgradeFn base_GetAvailableUpgrade;
	static GetNextUpgradeFn GetNextUpgrade;
	static HasPrerequisitesFn HasPrerequisites;
	static CheckCycleFn CheckCycle;
	static GetUnitDefinitionsFn GetUnitDefinitions;
	static ManagerUnitFn base_GetUpgradeUnitType, base_GetUpgradeResourceCost;
	static CanUpgradeFn base_CanUpgrade;
	static AddStringFn ReportAddString;
	static LocLookupFn LocLookup;
	static LocDtorFn LocDtor;
	static LocPushIntFn LocPushInt;
	static LocPushStrFn LocPushStr;
	static UnitGetPlotFn UnitGetPlot;
	static GetCityFn GetCity;
	static BuildingsTestFn HasBuilding, IsPillaged;
	static bool gOk = false, gReportsOk = false, gBuildingRuleOk = false;

	struct Check { const char* name; uintptr_t rva; uint8_t bytes[16]; size_t n; };
	static const Check kChecks[] = {
		{ "Rules::Units::GetAvailableUpgrade", GET_AVAILABLE_UPGRADE_OFFSET, { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48 }, 12 },
		{ "Rules::Units::GetNextUpgrade", GET_NEXT_UPGRADE_OFFSET, { 0x48, 0x8b, 0x09, 0x48, 0x8b, 0x81, 0xd8, 0x02, 0x00, 0x00, 0x48, 0x8b }, 12 },
		{ "Rules::Units::CheckForUnitUpgradeCycle", CHECK_UPGRADE_CYCLE_OFFSET, { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48 }, 12 },
		{ "Definitions::GetUnitDefinitions", GET_UNIT_DEFINITIONS_OFFSET, { 0x48, 0x83, 0xec, 0x28, 0xe8, 0xc7, 0x32, 0x87, 0xff, 0x48, 0x05, 0x30 }, 12 },
		{ "Unit::Upgrade::Manager::GetUpgradeUnitType", GET_UPGRADE_UNIT_TYPE_OFFSET, { 0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x48, 0x85, 0xd2, 0x74, 0x59, 0x48 }, 12 },
		{ "Unit::Upgrade::Manager::GetUpgradeResourceCost", GET_UPGRADE_RESOURCE_COST_OFFSET, { 0x40, 0x56, 0x57, 0x48, 0x83, 0xec, 0x48, 0x48, 0x8b, 0xb2, 0x98, 0x00 }, 12 },
		{ "Unit::Upgrade::Manager::CanUpgrade", CAN_UPGRADE_OFFSET, { 0x48, 0x89, 0x4c, 0x24, 0x08, 0x55, 0x53, 0x56, 0x57, 0x41, 0x56, 0x48 }, 12 },
		// GetAvailableUpgrade calls HasPrerequisites (call at 0x37dc3a: e8 + rel32 to 0x37e230)
		{ "GetAvailableUpgrade -> HasPrerequisites", 0x37dc3a, { 0xe8, 0xf1, 0x05, 0x00, 0x00 }, 5 },
	};
	// Rule 3 only (a mismatch switches off just that rule). The layout is checked where the game itself uses it.
	static const Check kBuildingChecks[] = {
		// CanUpgrade: unit->GetPlot(), plot->vt[0x30] CityAttributes, owner +0x1c, city id +0xc, GetCity(CityID)
		{ "CanUpgrade -> Unit::GetPlot", 0x3b79a3, { 0xe8, 0x98, 0x00, 0xcb, 0xff }, 5 },
		{ "CanUpgrade: plot CityAttributes owner / city id", 0x3b7a83, { 0xff, 0x50, 0x30, 0x0f, 0xbe, 0x48, 0x1c, 0x8b, 0x50, 0x0c }, 10 },
		{ "CanUpgrade -> GetCity(CityID)", 0x3b7ab8, { 0xe8, 0x83, 0xb3, 0xc8, 0xff }, 5 },
		{ "GetCity(CityID)", GET_CITY_OFFSET, { 0x40, 0x57, 0x48, 0x83, 0xec, 0x20, 0x0f, 0xb7, 0x01, 0x66, 0x83, 0xf8 }, 12 },
		// CanTrain: Unit_BuildingPrereqs vector (def +0x260 / +0x258), city Buildings +0xcb0 -> HasBuilding / IsPillaged
		{ "CanTrain: unit def building prereqs", 0xf8a18, { 0x48, 0x8b, 0xbe, 0x60, 0x02, 0x00, 0x00, 0x48, 0x8b, 0x9e, 0x58, 0x02, 0x00, 0x00 }, 14 },
		{ "CanTrain -> Buildings::HasBuilding", 0xf8a62, { 0x48, 0x81, 0xc1, 0xb0, 0x0c, 0x00, 0x00, 0xe8, 0xe2, 0x39, 0x01, 0x00 }, 12 },
		{ "CanTrain -> Buildings::IsPillaged", 0xf8a7f, { 0x48, 0x81, 0xc1, 0xb0, 0x0c, 0x00, 0x00, 0xe8, 0x25, 0x42, 0x01, 0x00 }, 12 },
		{ "City::Buildings::HasBuilding", CITY_BUILDINGS_HAS_BUILDING_OFFSET, { 0x83, 0xfa, 0xff, 0x74, 0x17, 0x3b, 0x51, 0x18, 0x73, 0x12, 0x48, 0x8b }, 12 },
		{ "City::Buildings::IsPillaged", CITY_BUILDINGS_IS_PILLAGED_OFFSET, { 0x85, 0xd2, 0x78, 0x11, 0x3b, 0x51, 0x30, 0x7d, 0x0c, 0x48, 0x8b, 0x41 }, 12 },
		// Buildings loader: column 30 (Name) -> building +0x68
		{ "Building definition Name at +0x68", 0x7b6494, { 0x48, 0x85, 0xc0, 0x74, 0x0e, 0x48, 0x8d, 0x4b, 0x68 }, 9 },
	};

	// ---- rules (written by the gameplay script at game start, read from any thread afterwards) ----------------------------
	static std::unordered_map<uint64_t, int> gMinLevel;   // (from << 32 | to) -> minimum experience level
	static uint64_t Key(int from, int to) { return ((uint64_t)(uint32_t)from << 32) | (uint32_t)to; }
	static int MinLevel(int from, int to) {
		if (gMinLevel.empty()) return 0;
		auto it = gMinLevel.find(Key(from, to));
		return it != gMinLevel.end() ? it->second : 0;
	}

	static bool gBuildingRule = false;   // rule 3, switched on from Lua

	// ---- the walk ---------------------------------------------------------------------------------------------------------
	enum BlockKind { BLOCK_NONE = 0, BLOCK_UNAVAILABLE = 1, BLOCK_LEVEL = 2, BLOCK_BUILDING = 4 };   // bits: a step can fail several
	struct Block { int kind; int need; int have; int to; };

	// The unit whose upgrade the game is asking about on this thread (set by our GetUpgradeUnitType / GetUpgradeResourceCost /
	// CanUpgrade hooks), and what blocked its first upgrade step last time (read by our CanUpgrade hook).
	static thread_local void* tUnit = nullptr;
	static thread_local Block tFirstBlock = {};

	static bool IsBuildDisabled(void* player, int unitType) {
		__try {
			char* units = *(char**)((char*)player + PLAYER_UNITS);
			if (units == nullptr) return false;
			const uint8_t* flags = *(const uint8_t**)(units + PLAYER_UNITS_BUILD_DISABLED);
			return flags != nullptr && flags[unitType] != 0;
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}
	static int UnitLevel(void* unit) {
		__try {
			return *(const int*)((const char*)unit + UNIT_EXPERIENCE + EXPERIENCE_LEVEL);
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return 0;
		}
	}
	static void* UnitDefinition(int index) {
		UnitVector* v = GetUnitDefinitions();
		if (index < 0 || v == nullptr || (size_t)index >= (size_t)(v->end - v->begin)) return nullptr;
		return v->begin[index];
	}

	// Unit_BuildingPrereqs of a unit definition (null entries skipped by the callers, as in CanTrain)
	struct BuildingVector { void** begin; void** end; };
	static const BuildingVector* PrereqBuildings(void* unitDef) {
		return (const BuildingVector*)((char*)unitDef + UNIT_DEF_PREREQ_BUILDINGS);
	}

	// The city whose borders the unit stands in, if it belongs to the unit's owner (an enemy, allied or city-state city never counts).
	static void* OwnCityAtUnit(void* unit) {
		void* plot = UnitGetPlot(unit);
		if (plot == nullptr) return nullptr;
		const char* attrs = (const char*)(*(PlotCityAttributesFn*)(*(char**)plot + PLOT_VT_CITY_ATTRIBUTES))(plot);
		if (attrs == nullptr) return nullptr;
		const int owner = *(const int8_t*)(attrs + CITY_ATTR_OWNER);
		const uint32_t id = *(const uint32_t*)(attrs + CITY_ATTR_CITY_ID);
		if (owner < 0 || id == 0xffffffffu || owner != *(const int*)((const char*)unit + UNIT_OWNER)) return nullptr;
		const CityID cid = { (int16_t)owner, 2, id };
		return GetCity(&cid);
	}

	// Rule 3: true when the upgrade unit has no Unit_BuildingPrereqs, or the unit is in one of its owner's cities with one of them
	// (built and not pillaged, like City::BuildQueue::CanTrain; its NumSupported limit is not applied to upgrades).
	static bool HasUpgradeBuilding(void* unit, int to) {
		void* toDef = UnitDefinition(to);
		if (toDef == nullptr) return true;
		__try {
			const BuildingVector* v = PrereqBuildings(toDef);
			bool any = false;
			for (void** p = v->begin; p != v->end; ++p) if (*p != nullptr) { any = true; break; }
			if (!any) return true;
			void* city = OwnCityAtUnit(unit);
			if (city == nullptr) return false;
			void* buildings = (char*)city + CITY_BUILDINGS;
			for (void** p = v->begin; p != v->end; ++p) {
				if (*p == nullptr) continue;
				const int b = *(const int*)((const char*)*p + BUILDING_DEF_INDEX);
				if (HasBuilding(buildings, b) && !IsPillaged(buildings, b)) return true;
			}
			return false;
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return true;   // unreadable: leave the game's decision alone
		}
	}

	static Block Gate(void* unit, void* player, int from, int to) {
		Block b = { BLOCK_NONE, 0, 0, to };
		if (IsBuildDisabled(player, to)) b.kind |= BLOCK_UNAVAILABLE;
		const int need = MinLevel(from, to);
		if (need > 1) {
			const int have = UnitLevel(unit);
			if (have < need) { b.kind |= BLOCK_LEVEL; b.need = need; b.have = have; }
		}
		if (gBuildingRule && gBuildingRuleOk && !HasUpgradeBuilding(unit, to)) b.kind |= BLOCK_BUILDING;
		return b;
	}

	// GetAvailableUpgrade (0x37dbe0) with the rules: the furthest unlocked step that the rules allow. A blocked first step is
	// still returned (with *firstBlock set) so the panel can name it; a blocked later step ends the walk before it.
	static int Walk(void* rules, void* def, void* player, void* unit, int depth, Block* firstBlock) {
		if (def == nullptr) return -1;
		void* from = def;
		const int next = GetNextUpgrade(&from, player);
		if (next < 0) return -1;
		void* nextDef = UnitDefinition(next);
		if (nextDef == nullptr || !HasPrerequisites(rules, &nextDef, player)) return -1;
		const Block b = Gate(unit, player, *(int*)((char*)def + UNIT_DEF_INDEX), next);
		if (b.kind != BLOCK_NONE) {
			if (depth == 0) { *firstBlock = b; return next; }
			return -1;
		}
		if (depth < 16 && !CheckCycle(&nextDef, player)) {
			Block ignored = {};
			const int deeper = Walk(rules, nextDef, player, unit, depth + 1, &ignored);
			if (deeper != -1) return deeper;
		}
		return next;
	}

	static int __fastcall Hook_GetAvailableUpgrade(void* rules, void** def, void* player) {
		void* unit = tUnit;
		if (unit == nullptr || def == nullptr) return base_GetAvailableUpgrade(rules, def, player);
		tUnit = nullptr;   // the game's own calls below us (none today) see the plain walk
		Block b = {};
		const int r = Walk(rules, *def, player, unit, 0, &b);
		tUnit = unit;
		tFirstBlock = b;
		return r;
	}

	static int __fastcall Hook_GetUpgradeUnitType(void* manager, void* unit) {
		void* saved = tUnit;
		tUnit = unit;
		const int r = base_GetUpgradeUnitType(manager, unit);
		tUnit = saved;
		return r;
	}
	static int __fastcall Hook_GetUpgradeResourceCost(void* manager, void* unit) {
		void* saved = tUnit;
		tUnit = unit;
		const int r = base_GetUpgradeResourceCost(manager, unit);
		tUnit = saved;
		return r;
	}

	// Names of the upgrade unit's prerequisite buildings (LOC keys, the game localizes string arguments), at most 2.
	static int PrereqBuildingNames(int to, const char* names[2]) {
		void* toDef = UnitDefinition(to);
		if (toDef == nullptr) return 0;
		int n = 0;
		__try {
			const BuildingVector* v = PrereqBuildings(toDef);
			for (void** p = v->begin; p != v->end; ++p) {
				if (*p == nullptr) continue;
				if (n == 2) return 3;   // more than 2: the text without names
				const char* name = *(const char* const*)((const char*)*p + BUILDING_DEF_NAME);
				if (name == nullptr || *name == '\0') return 3;
				names[n++] = name;
			}
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return 3;
		}
		return n;
	}

	static void AddText(void* results, const char* key, int i1, int i2, const char* s1, const char* s2) {
		alignas(16) uint8_t s[0x100];   // Localization::String (smaller than this)
		LocLookup(s, key);
		if (i1 >= 0) LocPushInt(s, i1);
		if (i2 >= 0) LocPushInt(s, i2);
		if (s1 != nullptr) LocPushStr(s, s1);
		if (s2 != nullptr) LocPushStr(s, s2);
		ReportAddString(results, UnitCommandActions::RESULT_FAILURE_REASONS, s);
		LocDtor(s);
	}

	static void AddReason(void* results, const Block& b) {
		if (!gReportsOk || results == nullptr) return;
		if (b.kind & BLOCK_UNAVAILABLE) AddText(results, "LOC_DEVCE_UPGRADE_UNIT_UNAVAILABLE", -1, -1, nullptr, nullptr);
		if (b.kind & BLOCK_LEVEL) AddText(results, "LOC_DEVCE_UPGRADE_NEEDS_LEVEL", b.need, b.have, nullptr, nullptr);
		if (b.kind & BLOCK_BUILDING) {
			const char* names[2] = {};
			const int n = PrereqBuildingNames(b.to, names);
			if (n == 1) AddText(results, "LOC_DEVCE_UPGRADE_NEEDS_BUILDING", -1, -1, names[0], nullptr);
			else if (n == 2) AddText(results, "LOC_DEVCE_UPGRADE_NEEDS_BUILDING_2", -1, -1, names[0], names[1]);
			else AddText(results, "LOC_DEVCE_UPGRADE_NEEDS_BUILDING_ANY", -1, -1, nullptr, nullptr);
		}
	}

	// The game's CanUpgrade (its GetUpgradeUnitType call runs our walk), then: a blocked first step keeps the button (testVisible)
	// but the real check fails, with the reason.
	static bool __fastcall Hook_CanUpgrade(void* manager, void* unit, bool testVisible, void* results) {
		void* saved = tUnit;
		tUnit = unit;
		tFirstBlock = {};
		bool r = base_CanUpgrade(manager, unit, testVisible, results);
		const Block b = tFirstBlock;
		tUnit = saved;
		if (b.kind != BLOCK_NONE && !testVisible) {
			AddReason(results, b);
			r = false;
		}
		return r;
	}

	// ---- Lua (gameplay state) ---------------------------------------------------------------------------------------------
	// UnitUpgradeRules.SetMinLevel(fromUnitIndex, toUnitIndex, level): level <= 1 removes the rule
	static int lSetMinLevel(hks::lua_State* L) {
		const int from = hks::checkinteger(L, 1), to = hks::checkinteger(L, 2), n = hks::checkinteger(L, 3);
		if (from < 0 || to < 0 || UnitDefinition(from) == nullptr || UnitDefinition(to) == nullptr) {
			hks::error(L, "UnitUpgradeRules.SetMinLevel: invalid unit index %d or %d", from, to);
			return 0;
		}
		if (n > 1) gMinLevel[Key(from, to)] = n; else gMinLevel.erase(Key(from, to));
		hks::pushboolean(L, true);
		return 1;
	}
	// UnitUpgradeRules.GetMinLevel(fromUnitIndex, toUnitIndex) -> level (0 = no rule)
	static int lGetMinLevel(hks::lua_State* L) {
		hks::pushinteger(L, MinLevel(hks::checkinteger(L, 1), hks::checkinteger(L, 2)));
		return 1;
	}
	// UnitUpgradeRules.Clear(): the minimum levels
	static int lClear(hks::lua_State* L) {
		gMinLevel.clear();
		return 0;
	}
	// UnitUpgradeRules.SetBuildingRule(on) -> true if the rule is available in this build (rule 3: Unit_BuildingPrereqs)
	static int lSetBuildingRule(hks::lua_State* L) {
		gBuildingRule = hks::toboolean(L, 1) != 0;
		hks::pushboolean(L, gBuildingRuleOk);
		return 1;
	}
	// UnitUpgradeRules.GetBuildingRule() -> on (and available)
	static int lGetBuildingRule(hks::lua_State* L) {
		hks::pushboolean(L, gBuildingRule && gBuildingRuleOk);
		return 1;
	}

	int Register(hks::lua_State* L) {
		if (!gOk) return 0;
		hks::createtable(L, 0, 5);
		PushLuaMethod(L, lSetMinLevel, "lUnitUpgradeRulesSetMinLevel", -2, "SetMinLevel");
		PushLuaMethod(L, lGetMinLevel, "lUnitUpgradeRulesGetMinLevel", -2, "GetMinLevel");
		PushLuaMethod(L, lClear, "lUnitUpgradeRulesClear", -2, "Clear");
		PushLuaMethod(L, lSetBuildingRule, "lUnitUpgradeRulesSetBuildingRule", -2, "SetBuildingRule");
		PushLuaMethod(L, lGetBuildingRule, "lUnitUpgradeRulesGetBuildingRule", -2, "GetBuildingRule");
		hks::setfield(L, hks::LUA_GLOBAL, "UnitUpgradeRules");
		return 0;
	}

	void ResetForNewGame() { gMinLevel.clear(); gBuildingRule = false; }

	// ---- setup -------------------------------------------------------------------------------------------------------------
	void Create() {
		if (gOk) return;   // hooks stay installed when Dev CE starts again in the same process
		const uintptr_t b = Runtime::GameCoreAddress;
		for (const Check& c : kChecks) {
			if (memcmp((void*)(b + c.rva), c.bytes, c.n) != 0) {
				DevBridge::Log("[UnitUpgradeRules] %s has unexpected bytes at 0x%llx: upgrade rules are not available", c.name, (unsigned long long)c.rva);
				return;
			}
		}
		GetNextUpgrade = (GetNextUpgradeFn)(b + GET_NEXT_UPGRADE_OFFSET);
		HasPrerequisites = (HasPrerequisitesFn)(b + HAS_PREREQUISITES_OFFSET);
		CheckCycle = (CheckCycleFn)(b + CHECK_UPGRADE_CYCLE_OFFSET);
		GetUnitDefinitions = (GetUnitDefinitionsFn)(b + GET_UNIT_DEFINITIONS_OFFSET);
		ReportAddString = (AddStringFn)(b + UnitCommandActions::REPORTING_ADD_STRING_OFFSET);
		if (HMODULE loc = GetModuleHandleW(L"Localization_FinalRelease.dll")) {
			LocDtor = (LocDtorFn)GetProcAddress(loc, "??1String@Localization@@UEAA@XZ");
			LocLookup = (LocLookupFn)GetProcAddress(loc, "?Lookup@Localization@@YA?AVString@1@PEBD@Z");
			LocPushInt = (LocPushIntFn)GetProcAddress(loc, "?PushArgument@String@Localization@@QEAA_NH@Z");
			LocPushStr = (LocPushStrFn)GetProcAddress(loc, "?PushArgument@String@Localization@@QEAA_NPEBD@Z");
			gReportsOk = LocDtor && LocLookup && LocPushInt && LocPushStr;
		}
		gBuildingRuleOk = true;
		for (const Check& c : kBuildingChecks) {
			if (memcmp((void*)(b + c.rva), c.bytes, c.n) != 0) {
				DevBridge::Log("[UnitUpgradeRules] %s has unexpected bytes at 0x%llx: the building prerequisite rule is not available", c.name, (unsigned long long)c.rva);
				gBuildingRuleOk = false;
				break;
			}
		}
		UnitGetPlot = (UnitGetPlotFn)(b + UnitCommandActions::UNIT_GET_PLOT_OFFSET);
		GetCity = (GetCityFn)(b + GET_CITY_OFFSET);
		HasBuilding = (BuildingsTestFn)(b + CITY_BUILDINGS_HAS_BUILDING_OFFSET);
		IsPillaged = (BuildingsTestFn)(b + CITY_BUILDINGS_IS_PILLAGED_OFFSET);
		Runtime::CreateHook((LPVOID)(b + GET_AVAILABLE_UPGRADE_OFFSET), (LPVOID)&Hook_GetAvailableUpgrade, &base_GetAvailableUpgrade);
		Runtime::CreateHook((LPVOID)(b + GET_UPGRADE_UNIT_TYPE_OFFSET), (LPVOID)&Hook_GetUpgradeUnitType, &base_GetUpgradeUnitType);
		Runtime::CreateHook((LPVOID)(b + GET_UPGRADE_RESOURCE_COST_OFFSET), (LPVOID)&Hook_GetUpgradeResourceCost, &base_GetUpgradeResourceCost);
		Runtime::CreateHook((LPVOID)(b + CAN_UPGRADE_OFFSET), (LPVOID)&Hook_CanUpgrade, &base_CanUpgrade);
		gOk = true;
		DevBridge::Log("[UnitUpgradeRules] hooks installed (no upgrading into build-disabled units; minimum levels from Lua; building prerequisite rule %s)%s",
			gBuildingRuleOk ? "available" : "NOT available",
			gReportsOk ? "" : "; Localization exports not found: no failure reasons in the unit panel");
	}
}
