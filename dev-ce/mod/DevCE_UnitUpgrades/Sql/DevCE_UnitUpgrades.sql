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

-- Example (not active):
-- INSERT INTO DevCE_UnitUpgradeRequirements (Unit, UpgradeUnit, MinLevel) VALUES
--   ('UNIT_SLTH_ADEPT', 'UNIT_SLTH_MAGE', 5),      -- 4 earned promotions
--   ('UNIT_SLTH_MAGE', 'UNIT_SLTH_ARCHMAGE', 7);   -- 6 earned promotions
