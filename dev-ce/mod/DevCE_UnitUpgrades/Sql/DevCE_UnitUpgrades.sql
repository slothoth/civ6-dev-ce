-- Dev CE unit upgrade rules: a minimum experience level for one upgrade step.
--
-- One row per step, as in UnitUpgrades (Unit -> UpgradeUnit). The unit needs at least experience level MinLevel to upgrade
-- (Lua unit:GetExperience():GetLevel(): 1 for a new unit, +1 per promotion earned with experience; promotions given for free,
-- e.g. by a modifier or Lua SetPromotion, do not raise it). 4 earned promotions = level 5. Unique units that replace Unit or UpgradeUnit (UnitReplaces) get the same rule,
-- unless they have a row of their own.
-- When several steps are unlocked at once (Adept -> Mage -> Archmage), the unit goes as far as it is allowed:
-- with the rows below, level 5 reaches the Mage, level 7 the Archmage.
CREATE TABLE IF NOT EXISTS DevCE_UnitUpgradeRequirements (
	Unit          TEXT NOT NULL,
	UpgradeUnit   TEXT NOT NULL,
	MinLevel      INTEGER NOT NULL DEFAULT 1,
	PRIMARY KEY (Unit, UpgradeUnit),
	FOREIGN KEY (Unit) REFERENCES Units (UnitType) ON DELETE CASCADE ON UPDATE CASCADE,
	FOREIGN KEY (UpgradeUnit) REFERENCES Units (UnitType) ON DELETE CASCADE ON UPDATE CASCADE
);

-- Building prerequisites: an upgrade unit with Unit_BuildingPrereqs can only be upgraded to while the unit stands inside the borders
-- of one of its owner's cities that has one of those buildings (built, not pillaged). 1 = on (default with this mod), 0 = off:
--   UPDATE GlobalParameters SET Value = '0' WHERE Name = 'DEVCE_UPGRADE_NEEDS_PREREQ_BUILDING';
INSERT OR IGNORE INTO GlobalParameters (Name, Value) VALUES ('DEVCE_UPGRADE_NEEDS_PREREQ_BUILDING', '1');

-- Alternative upgrades: more upgrade choices besides the UnitUpgrades row (which stays the default and the only one that makes
-- the unit obsolete for training). Each row adds one choice; the player gets one Upgrade button per choice (DevCE_UnitUpgradesUI),
-- the AI takes the allowed choice its production AI rates best. Every choice follows the rules above (level, limits, buildings),
-- and goes further along the UpgradeUnit's own UnitUpgrades chain when that is unlocked too.
-- Unique units that replace Unit get the same choices unless they have rows of their own; an UpgradeUnit is replaced by the
-- player's own unique version, as for UnitUpgrades.
CREATE TABLE IF NOT EXISTS DevCE_UnitAltUpgrades (
	Unit          TEXT NOT NULL,
	UpgradeUnit   TEXT NOT NULL,
	PRIMARY KEY (Unit, UpgradeUnit),
	FOREIGN KEY (Unit) REFERENCES Units (UnitType) ON DELETE CASCADE ON UPDATE CASCADE,
	FOREIGN KEY (UpgradeUnit) REFERENCES Units (UnitType) ON DELETE CASCADE ON UPDATE CASCADE
);

-- Example (not active):
-- INSERT INTO DevCE_UnitUpgradeRequirements (Unit, UpgradeUnit, MinLevel) VALUES
--   ('UNIT_SLTH_ADEPT', 'UNIT_SLTH_MAGE', 5),      -- 4 earned promotions
--   ('UNIT_SLTH_MAGE', 'UNIT_SLTH_ARCHMAGE', 7);   -- 6 earned promotions
-- INSERT INTO DevCE_UnitAltUpgrades (Unit, UpgradeUnit) VALUES
--   ('UNIT_WARRIOR', 'UNIT_ARCHER');                -- Warrior: Spearman (UnitUpgrades) or Archer
