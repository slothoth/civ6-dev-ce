-- =====================================================================
--  DevCE_UnitUpgrades_GP.lua   (gameplay Lua context)
--
--  Reads DevCE_UnitUpgradeRequirements and hands it to the Dev CE native UnitUpgradeRules:
--    UnitUpgradeRules.SetMinLevel(fromUnitIndex, toUnitIndex, level)
--  Unique units (UnitReplaces) inherit the rule of the unit they replace, on either side, unless they have their own row.
--  Runs on every game start and load (the rules are forgotten with the old game).
-- =====================================================================

local LOG = "[DevCEUnitUpgrades] ";

local function PushRules()
	if UnitUpgradeRules == nil or UnitUpgradeRules.SetMinLevel == nil then
		print(LOG .. "Dev CE with UnitUpgradeRules is not loaded: minimum levels do nothing");
		return;
	end

	-- unit type -> { itself, the unique units that replace it }
	local versions = {};
	local function VersionsOf(unitType)
		if versions[unitType] == nil then
			local list = { unitType };
			for row in GameInfo.UnitReplaces() do
				if row.ReplacesUnitType == unitType then table.insert(list, row.CivUniqueUnitType); end
			end
			versions[unitType] = list;
		end
		return versions[unitType];
	end

	local rows = {};
	local exact = {};
	for row in GameInfo.DevCE_UnitUpgradeRequirements() do
		table.insert(rows, row);
		exact[row.Unit .. "|" .. row.UpgradeUnit] = true;
	end

	local nSet = 0;
	local function Set(fromType, toType, count)
		local from, to = GameInfo.Units[fromType], GameInfo.Units[toType];
		if from == nil or to == nil then
			print(LOG .. "unknown unit " .. tostring(fromType) .. " or " .. tostring(toType));
			return;
		end
		UnitUpgradeRules.SetMinLevel(from.Index, to.Index, count);
		nSet = nSet + 1;
	end

	for _, row in ipairs(rows) do
		local count = tonumber(row.MinLevel) or 1;
		for _, fromType in ipairs(VersionsOf(row.Unit)) do
			for _, toType in ipairs(VersionsOf(row.UpgradeUnit)) do
				local own = (fromType == row.Unit and toType == row.UpgradeUnit);
				if own or not exact[fromType .. "|" .. toType] then   -- a unique pair with its own row keeps that row
					Set(fromType, toType, count);
				end
			end
		end
	end
	print(LOG .. #rows .. " rows, " .. nSet .. " upgrade steps with a minimum level");
end

PushRules();
