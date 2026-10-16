-- Live Phoenix's settings where they differ from the defaults of its settings/default/main.lua, which no branch of
-- the Phoenix repository changes. refresh.sh copies this file to the server's settings/main.lua. Each key is set over
-- the defaults one by one: a file that sets a whole table keeps only its own keys for the XI_ variables to apply to.
xi = xi or {}
xi.settings = xi.settings or {}

-- Content up to and including Treasures of Aht Urhgan.
xi.settings.main.RESTRICT_CONTENT = 1
xi.settings.main.ENABLE_ROTZ      = 1
xi.settings.main.ENABLE_COP       = 1
xi.settings.main.ENABLE_TOAU      = 1
xi.settings.main.ENABLE_WOTG      = 0
xi.settings.main.ENABLE_ACP       = 0
xi.settings.main.ENABLE_AMK       = 0
xi.settings.main.ENABLE_ASA       = 0
xi.settings.main.ENABLE_ABYSSEA   = 0
xi.settings.main.ENABLE_SOA       = 0
xi.settings.main.ENABLE_ROV       = 0
xi.settings.main.ENABLE_TVR       = 0
xi.settings.main.ENABLE_VOIDWATCH = 0

-- Every zone is level corrected, as before Adoulin.
xi.settings.main.USE_ADOULIN_WEAPON_SKILL_CHANGES = false
