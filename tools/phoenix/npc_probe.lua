-----------------------------------
-- Probe every ??? a player can see, inside the map server (live's commit and settings), with recording stand-ins.
-- tools/phoenix/headsup_probe runs it once every zone has loaded, with HEADSUP_PROBE_TARGETS set to the ??? (id,
-- zone, zoneName, script). Each ??? is clicked and traded through the server's own interaction framework
-- (InteractionGlobal), the path a real player's click takes, its own script and the zone's default actions, under a
-- grid of player states and each hour, weather, job, weekday and roll its script reads; each quest, mission and
-- battlefield step on it is run apart, its check skipped, so tools/phoenix/npcs.py can leave out the steps of content
-- live has off. What the handlers do is recorded instead of done: a stand-in player, trade and zone, and proxies of
-- the NPCs and mobs they touch. Each zone's clock, weather and mob deaths are run the same way, for the hidden ???
-- they show. Nothing a run reads changes between server runs (the time, server variables and entities' own state are
-- the probe's), so the output is the same each time: one JSON object a line in HEADSUP_PROBE_OUT.
-----------------------------------
local OUT     = assert(os.getenv('HEADSUP_PROBE_OUT'), 'HEADSUP_PROBE_OUT')
local targets = assert(HEADSUP_PROBE_TARGETS, 'HEADSUP_PROBE_TARGETS')
local unpack  = unpack or table.unpack
local out     = assert(io.open(OUT, 'w'))

-- A handler that loops on the stand-ins' answers is cut off after BUDGET instructions; the JIT is off so the count
-- hook sees every instruction.
local BUDGET = 20000000
if jit then
    jit.off()
end

local function guarded(fn, ...)
    debug.sethook(function()
        error('probe: instruction budget spent', 2)
    end, '', BUDGET)
    local result = { pcall(fn, ...) }
    debug.sethook()
    return unpack(result)
end

-----------------------------------
-- JSON
-----------------------------------
local function str(s)
    return '"' .. tostring(s):gsub('[%c"\\\128-\255]', function(c)
        return string.format('\\u%04x', c:byte())
    end) .. '"'
end

local isArray = function(t)
    local n = 0
    for _ in pairs(t) do
        n = n + 1
    end

    return n == #t
end

local json
json = function(v, depth)
    depth = depth or 6
    local t = type(v)
    if t == 'nil' then
        return 'null'
    elseif t == 'boolean' then
        return v and 'true' or 'false'
    elseif t == 'number' then
        if v ~= v or v == math.huge or v == -math.huge then
            return 'null'
        end

        return v == math.floor(v) and string.format('%d', v) or string.format('%.6g', v)
    elseif t == 'string' then
        return str(v)
    elseif t == 'table' and depth > 0 then
        local parts = {}
        if isArray(v) then
            for _, x in ipairs(v) do
                parts[#parts + 1] = json(x, depth - 1)
            end

            return '[' .. table.concat(parts, ',') .. ']'
        end

        local keys = {}
        for k in pairs(v) do
            keys[#keys + 1] = tostring(k)
        end

        table.sort(keys)
        for _, k in ipairs(keys) do
            local x = v[k]
            if x == nil then
                x = v[tonumber(k)]
            end

            parts[#parts + 1] = str(k) .. ':' .. json(x, depth - 1)
        end

        return '{' .. table.concat(parts, ',') .. '}'
    end

    return str(tostring(v))
end

-----------------------------------
-- Recording
-----------------------------------
local rec       = nil -- the current run's notes
local proxies   = setmetatable({}, { __mode = 'k' }) -- proxy -> the real entity
local scenario  = {}
local textNames = {} -- zone ID -> text ID -> its name in the zone's IDs.lua

-- A value as it goes in a note: entities by ID, tables shallow.
local describe
describe = function(v, depth)
    depth = depth or 2
    local t = type(v)
    if t == 'table' then
        if proxies[v] then
            return 'entity:' .. tostring(proxies[v]:getID())
        end

        if v.__probeName then
            return v.__probeName
        end

        if depth <= 0 then
            return 'table'
        end

        local copy = {}
        for k, x in pairs(v) do
            copy[type(k) == 'number' and k or tostring(k)] = describe(x, depth - 1)
        end

        return copy
    elseif t == 'userdata' then
        local ok, id = pcall(function()
            return v:getID()
        end)

        return ok and ('entity:' .. tostring(id)) or tostring(v)
    elseif t == 'function' then
        return 'function'
    end

    return v
end

local function note(kind, ...)
    if not rec then
        return
    end

    local entry = { kind }
    for i = 1, select('#', ...) do
        entry[#entry + 1] = describe((select(i, ...)))
    end

    rec[#rec + 1] = entry
end

local function unproxy(...)
    local args = { ... }
    for i = 1, select('#', ...) do
        if type(args[i]) == 'table' and proxies[args[i]] then
            args[i] = proxies[args[i]]
        end
    end

    return unpack(args, 1, select('#', ...))
end

local GETTERS = { '^get', '^is', '^has', '^check', '^query', '^can', '^at', '^was', '^did' }
local function isGetter(name)
    for _, pattern in ipairs(GETTERS) do
        if name:match(pattern) then
            return true
        end
    end

    return false
end

-- A proxy of a real entity: getters read the real one, everything else is noted and not done.
local function proxy(real, kind, overrides)
    if real == nil or proxies[real] then
        return real
    end

    local p      = {}
    local locals = {}
    setmetatable(p, {
        __index = function(_, name)
            if overrides and overrides[name] ~= nil then
                return overrides[name]
            end

            if name == 'getLocalVar' then
                return function(_, k)
                    if kind == 'npc' then
                        note('asks', 'npcVar', k)
                        if locals[k] == nil and scenario.npcVar then
                            return scenario.npcVar
                        end
                    end

                    return locals[k] or 0
                end
            elseif name == 'setLocalVar' then
                return function(_, k, v)
                    locals[k] = v
                    note(kind .. ':setLocalVar', real:getID(), k, v)
                end
            elseif name == 'getWeather' and scenario.weather then
                return function()
                    note('asks', 'weather')
                    return scenario.weather
                end
            elseif isGetter(name) then
                return function(_, ...)
                    local f = real[name]
                    if type(f) ~= 'function' then
                        error('no ' .. name .. ' on ' .. kind)
                    end

                    return f(real, unproxy(...))
                end
            end

            return function(_, ...)
                if name:match('Pos$') or name:match('^setSpawn') then
                    note(kind .. ':' .. name, real:getID())
                else
                    note(kind .. ':' .. name, real:getID(), ...)
                end
            end
        end,
    })
    proxies[p] = real
    return p
end

local function mobProxy(mob)
    if mob == nil then
        return nil
    end

    return proxy(mob, 'mob', {
        isSpawned = function()
            return false
        end,

        isAlive = function()
            return false
        end,

        isDead = function()
            return true
        end,

        spawn = function()
            note('pops', mob:getID())
        end,
    })
end

-----------------------------------
-- The world's globals, recorded
-----------------------------------
local real = {
    GetNPCByID = GetNPCByID, GetMobByID = GetMobByID, SpawnMob = SpawnMob, DespawnMob = DespawnMob, GetZone = GetZone,
    GetServerVariable = GetServerVariable, SetServerVariable = SetServerVariable,
    GetVolatileServerVariable = GetVolatileServerVariable, SetVolatileServerVariable = SetVolatileServerVariable,
    VanadielHour = VanadielHour, VanadielMinute = VanadielMinute, VanadielTOTD = VanadielTOTD,
    VanadielDayOfTheWeek = VanadielDayOfTheWeek, VanadielDayElement = VanadielDayElement,
    VanadielMoonPhase = VanadielMoonPhase, VanadielUniqueDay = VanadielUniqueDay, VanadielTime = VanadielTime,
    GetSystemTime = GetSystemTime, osTime = os.time, tradeHas = npcUtil.tradeHas, popFromQM = npcUtil.popFromQM,
    giveItem = npcUtil.giveItem, giveKeyItem = npcUtil.giveKeyItem, completeQuest = npcUtil.completeQuest,
    completeMission = npcUtil.completeMission, giveCurrency = npcUtil.giveCurrency,
}

local serverVars = {}

GetNPCByID = function(id, ...)
    return proxy(real.GetNPCByID(id, ...), 'npc')
end

GetMobByID = function(id, ...)
    return mobProxy(real.GetMobByID(id, ...))
end

SpawnMob = function(id, ...)
    note('pops', id)
    return mobProxy(real.GetMobByID(id))
end

DespawnMob = function(id)
    note('despawns', id)
end

local zoneProxy = function(zone)
    return proxy(zone, 'zone', {
        getID = function()
            return zone:getID()
        end,
    })
end

GetZone = function(id)
    return zoneProxy(real.GetZone(id))
end

GetServerVariable = function(k)
    note('asks', 'serverVar', k)
    if serverVars[k] ~= nil then
        return serverVars[k]
    end

    return 0
end

SetServerVariable = function(k, v)
    note('setsServerVar', k, v)
    serverVars[k] = v
end

GetVolatileServerVariable = function(k)
    note('asks', 'serverVar', k)
    if serverVars[k] ~= nil then
        return serverVars[k]
    end

    return 0
end

SetVolatileServerVariable = function(k, v)
    note('setsServerVar', k, v)
    serverVars[k] = v
end

-- The clock: a run's hour, as Phoenix's own Vana'diel time functions give it.
VanadielHour = function()
    note('asks', 'hour')
    return scenario.hour
end

-- Dice: every math.random* the server gives (random, randomInt, randomFloat, ...) rolls the run's lowest, middle or
-- highest value, so outcomes repeat.
real.dice = {}
for name, f in pairs(math) do
    if type(f) == 'function' and name:match('^random') and name ~= 'randomseed' then
        real.dice[name] = f
        math[name] = function(a, b)
            note('asks', 'random')
            local mode = scenario.random or 'low'
            if a == nil then
                return mode == 'low' and 0 or mode == 'high' and 0.999 or 0.5
            end

            local lo, hi = b and a or 1, b or a
            if mode == 'low' then
                return lo
            elseif mode == 'high' then
                return hi
            end

            local mid = (lo + hi) / 2
            return name == 'randomFloat' and mid or math.floor(mid)
        end
    end
end

-- 2026-01-01 00:00 UTC, the Earth time every run reads.
local PROBE_TIME = 1767225600
GetSystemTime = function()
    return PROBE_TIME
end

os.time = function(t)
    return t and real.osTime(t) or PROBE_TIME
end

VanadielUniqueDay = function()
    return 9000
end

VanadielTime = function()
    return PROBE_TIME - xi.time.VANADIEL_EPOCH
end

VanadielMinute = function()
    return 0
end

VanadielTOTD = function()
    note('asks', 'hour')
    -- The server's bands (common/vana_time.h get_totd).
    local h = scenario.hour
    return h < 4 and xi.time.MIDNIGHT or h < 6 and xi.time.NEW_DAY or h < 7 and xi.time.DAWN or
        h < 17 and xi.time.DAY or h < 18 and xi.time.DUSK or h < 20 and xi.time.EVENING or xi.time.NIGHT
end

VanadielDayOfTheWeek = function()
    note('asks', 'weekday')
    return scenario.weekday
end

VanadielDayElement = function()
    note('asks', 'weekday')
    return scenario.weekday
end

VanadielMoonPhase = function()
    note('asks', 'moon')
    return scenario.moon
end

-- npcUtil, recorded: what a trade check asks for, decided against the stand-in trade by Phoenix's own code.
local tradeChecks = 0
npcUtil.tradeHas = function(trade, items, exact)
    tradeChecks = tradeChecks + 1
    note('tradeCheck', tradeChecks, items, exact and 'exact' or 'has')
    trade.__inside = true
    local ok, res = pcall(real.tradeHas, trade, items, exact)
    trade.__inside = false
    if not ok then
        error(res)
    end

    return res
end

real.tradeMatches = npcUtil.tradeMatches
npcUtil.tradeMatches = function(trade, itemTable)
    tradeChecks = tradeChecks + 1
    note('tradeCheck', tradeChecks, itemTable, 'matches')
    trade.__inside = true
    local ok, res = pcall(real.tradeMatches, trade, itemTable)
    trade.__inside = false
    if not ok then
        error(res)
    end

    return res
end

npcUtil.popFromQM = function(player, qm, mobId, params)
    note('popFromQM', mobId, params)
    return true
end

npcUtil.giveItem = function(player, items, params)
    note('gives', items)
    return true
end

npcUtil.giveKeyItem = function(player, keyItems)
    note('givesKeyItem', keyItems)
    return true
end

npcUtil.giveCurrency = function(player, currency, amount)
    note('givesCurrency', currency, amount)
    return true
end

npcUtil.completeQuest = function(player, area, quest, params)
    note('completesQuest', area, quest, params)
    return true
end

npcUtil.completeMission = function(player, logId, missionId, params)
    note('completesMission', logId, missionId, params)
    return true
end

-----------------------------------
-- Stand-ins
-----------------------------------
local function makeTrade(contents)
    local slots = {}
    for item, qty in pairs(contents) do
        slots[#slots + 1] = { item, qty }
    end

    table.sort(slots, function(a, b)
        return a[1] < b[1]
    end)

    local t = { __probeName = 'trade' }
    local methods = {
        getSlotCount = function()
            return #slots
        end,

        getItemCount = function()
            local n = 0
            for _, s in ipairs(slots) do
                if s[1] ~= 65535 then
                    n = n + s[2]
                end
            end

            return n
        end,

        getItemId = function(_, slot)
            local s = slots[(slot or 0) + 1]
            return s and s[1] or 0
        end,

        getItemQty = function(self, item)
            if not self.__inside then
                tradeChecks = tradeChecks + 1
                note('tradeCheck', tradeChecks, { { item, 1 } }, 'qty')
            end

            return contents[item] or 0
        end,

        hasItemQty = function(_, item, qty)
            tradeChecks = tradeChecks + 1
            note('tradeCheck', tradeChecks, { { item, qty or 1 } }, 'hasItemQty')
            return (contents[item] or 0) >= (qty or 1)
        end,

        getGil = function()
            return contents[65535] or 0
        end,

        confirmItem = function(_, item, qty)
            note('confirms', item, qty)
        end,
    }
    setmetatable(t, {
        __index = function(_, name)
            if methods[name] then
                return methods[name]
            end

            return function(_, ...)
                note('trade:' .. name, ...)
                if isGetter(name) then
                    return 0
                end
            end
        end,
    })
    return t
end

local function makePlayer(npc, zoneId)
    local locals, inEvent, gotMessage = {}, false, false
    local p = { __probeName = 'player' }
    local message = function(kind)
        return function(_, ...)
            gotMessage = true
            local args = { ... }
            if kind == 'showText' or kind == 'messageText' or kind == 'messageName' then
                table.remove(args, 1) -- the entity that speaks
            end

            note(kind, unpack(args))
        end
    end

    local event = function(kind)
        return function(_, ...)
            inEvent = true
            note(kind, ...)
        end
    end

    local methods = {
        getID = function()
            return 1
        end,

        getName = function()
            return 'Probe'
        end,

        getZoneID = function()
            return zoneId
        end,

        getZone = function()
            return zoneProxy(real.GetZone(zoneId))
        end,

        getXPos = function()
            return npc:getXPos()
        end,

        getYPos = function()
            return npc:getYPos()
        end,

        getZPos = function()
            return npc:getZPos()
        end,

        getRotPos = function()
            return 0
        end,

        getPos = function()
            return { x = npc:getXPos(), y = npc:getYPos(), z = npc:getZPos(), rot = 0 }
        end,

        getLocalVar = function(_, k)
            if locals[k] == nil then
                note('asks', 'playerVar', k)
                if scenario.playerVar then
                    return scenario.playerVar
                end
            end

            return locals[k] or 0
        end,

        setLocalVar = function(_, k, v)
            locals[k] = v
        end,

        resetGotMessage = function()
            gotMessage = false
        end,

        didGetMessage = function()
            return gotMessage
        end,

        isInEvent = function()
            return inEvent
        end,

        getVar = function(_, k)
            note('asks', 'var', k)
            if type(scenario.vars) == 'table' then
                return scenario.vars[k] or 0
            end

            return scenario.var
        end,

        getCharVar = function(_, k)
            note('asks', 'var', k)
            if type(scenario.vars) == 'table' then
                return scenario.vars[k] or 0
            end

            return scenario.var
        end,

        hasKeyItem = function(_, ki)
            note('asks', 'keyItem', ki)
            if type(scenario.keyItems) == 'table' then
                return scenario.keyItems[ki] == true
            end

            return scenario.keyItems
        end,

        getQuestStatus = function(_, log, id)
            note('asks', 'quest', log, id)
            return scenario.quest
        end,

        hasCompletedQuest = function(_, log, id)
            note('asks', 'quest', log, id)
            return scenario.quest == 2
        end,

        getCurrentMission = function(_, log)
            note('asks', 'mission', log)
            return scenario.mission
        end,

        hasCompletedMission = function(_, log, id)
            note('asks', 'mission', log, id)
            return scenario.missionDone
        end,

        getMissionStatus = function(_, log)
            note('asks', 'mission', log)
            return scenario.var
        end,

        hasItem = function(_, item)
            note('asks', 'item', item)
            return scenario.items
        end,

        getMainJob = function()
            note('asks', 'job')
            return scenario.job
        end,

        getMainLvl = function()
            return 75
        end,

        getNation = function()
            note('asks', 'nation')
            return scenario.nation
        end,

        getRank = function()
            return 10
        end,

        getFameLevel = function()
            return 9
        end,

        getGil = function()
            return 1000000
        end,

        getFreeSlotsCount = function()
            return 30
        end,

        getWeather = function()
            note('asks', 'weather')
            return scenario.weather
        end,

        getStatus = function()
            return 0
        end,

        getParty = function()
            return { p }
        end,

        getAlliance = function()
            return { p }
        end,

        getPartySize = function()
            return 1
        end,

        checkDistance = function()
            return 1
        end,

        messageSpecial = message('message'),
        messageText = message('messageText'),
        showText = message('showText'),
        messageName = message('messageName'),
        messageBasic = message('messageBasic'),
        messageSystem = message('messageSystem'),
        PrintToPlayer = message('print'),
        startEvent = event('event'),
        startCutscene = event('event'),
        startOptionalCutscene = event('event'),

        addItem = function(_, ...)
            note('gives', ...)
            return true
        end,

        addKeyItem = function(_, ki)
            note('givesKeyItem', ki)
        end,

        delKeyItem = function(_, ki)
            note('takesKeyItem', ki)
        end,

        setCharVar = function(_, k, v)
            note('setsVar', k, v)
        end,

        confirmTrade = function()
            note('takesTrade')
        end,

        tradeComplete = function()
            note('takesTrade')
        end,
    }
    setmetatable(p, {
        __index = function(_, name)
            if methods[name] then
                return methods[name]
            end

            return function(_, ...)
                if isGetter(name) then
                    note('asks', name, ...)
                    return (name:match('^get') and 0) or false
                end

                note('player:' .. name, ...)
            end
        end,
    })
    return p
end

-----------------------------------
-- Runs
-----------------------------------
-- Every handler of the interaction framework, quest steps included (steps() reads it); clicks and trades see the
-- lookup without them, the NPC's own script and the zone's default actions.
local stepData = InteractionGlobal.lookup.data
local plain    = {}
for zoneId, entities in pairs(stepData) do
    plain[zoneId] = {}
    for key, handlers in pairs(entities) do
        plain[zoneId][key] = {}
        for handlerName, list in pairs(type(handlers) == 'table' and handlers or {}) do
            local kept = {}
            for _, entry in ipairs(type(list) == 'table' and list or {}) do
                if entry.container == nil then
                    kept[#kept + 1] = entry
                end
            end

            plain[zoneId][key][handlerName] = kept
        end
    end
end

InteractionGlobal.lookup.data = plain

local function sortedKeys(t)
    local keys = {}
    for k in pairs(t) do
        keys[#keys + 1] = k
    end

    table.sort(keys, function(a, b)
        if type(a) == 'number' and type(b) == 'number' then
            return a < b
        end

        return tostring(a) < tostring(b)
    end)
    return keys
end
local BASE = { keyItems = false, quest = 0, var = 0, items = false, hour = 12, weather = 0, job = 1, nation = 0,
    mission = 0, missionDone = false, weekday = 0, moon = 50 }

local function scenarioOf(changes)
    local s = {}
    for k, v in pairs(BASE) do
        s[k] = v
    end

    for k, v in pairs(changes or {}) do
        s[k] = v
    end

    return s
end

local function fileFunction(target, name)
    local zone   = xi.zones and xi.zones[target.zoneName]
    local script = zone and zone.npcs and zone.npcs[target.script]
    return type(script) == 'table' and script[name] or nil
end

local function zoneIdsOf(target)
    if not textNames[target.zone] then
        local names = {}
        local ids   = zones and zones[target.zone]
        for name, v in pairs(ids and ids.text or {}) do
            if type(v) == 'number' then
                names[v] = name
            end
        end

        textNames[target.zone] = names
    end

    return textNames[target.zone]
end

-- One run: fn(player, npc) under the scenario; its notes, the queries apart from what was done.
local function run(target, changes, fn, keep)
    scenario    = scenarioOf(changes)
    serverVars  = {}
    tradeChecks = 0
    rec         = {}
    local npc   = GetNPCByID(target.id)
    if npc == nil then
        rec = nil
        return { error = 'no NPC' }
    end

    local player = keep and keep.player or makePlayer(npc, target.zone)
    local ok, err = guarded(fn, player, npc)
    if not ok then
        note('error', tostring(err))
    end

    local done, asked = {}, {}
    for _, entry in ipairs(rec) do
        if entry[1] == 'asks' then
            asked[#asked + 1] = entry
        else
            if entry[1] == 'message' or entry[1] == 'showText' or entry[1] == 'messageText' then
                entry[#entry + 1] = zoneIdsOf(target)[entry[2]] or ''
            end

            done[#done + 1] = entry
        end
    end

    rec = nil
    return { done = done, asked = asked, player = player }
end

local function key(done)
    return json(done)
end

-- Each scenario's outcome, grouped: the outcomes in first-seen order with the scenarios that gave each.
local function group(results)
    local order, byKey = {}, {}
    for _, r in ipairs(results) do
        local k = key(r.done)
        if not byKey[k] then
            byKey[k] = { done = r.done, scenarios = {} }
            order[#order + 1] = byKey[k]
        end

        table.insert(byKey[k].scenarios, r.name)
    end

    return order
end

local function asksOf(results)
    local seen, list = {}, {}
    for _, r in ipairs(results) do
        for _, a in ipairs(r.asked or {}) do
            local k = json(a)
            if not seen[k] then
                seen[k] = true
                list[#list + 1] = a
            end
        end
    end

    return list
end

local function askedKind(results, kind)
    for _, r in ipairs(results) do
        for _, a in ipairs(r.asked or {}) do
            if a[2] == kind then
                return true
            end
        end
    end

    return false
end

local SCENARIOS = {
    { 'base', {} },
    { 'keyItems', { keyItems = true } },
    { 'questAccepted', { quest = 1 } },
    { 'questDone', { quest = 2, missionDone = true } },
    { 'var1', { var = 1 } },
    { 'var2', { var = 2 } },
    { 'var3', { var = 3 } },
    { 'accepted+var1', { quest = 1, var = 1 } },
    { 'accepted+var2', { quest = 2 - 1, var = 2 } },
    { 'accepted+keyItems', { quest = 1, keyItems = true } },
    { 'items', { items = true } },
    { 'var4', { var = 4 } },
    { 'var5', { var = 5 } },
    { 'accepted+var3', { quest = 1, var = 3 } },
    { 'accepted+var4', { quest = 1, var = 4 } },
    { 'accepted+var5', { quest = 1, var = 5 } },
    { 'accepted+var6', { quest = 1, var = 6 } },
}

-- The key items a script checks, as sets to hold: each alone, and every one but one.
local function keyItemSets(results)
    local asked, seen = {}, {}
    for _, r in ipairs(results) do
        for _, a in ipairs(r.asked or {}) do
            if a[2] == 'keyItem' and type(a[3]) == 'number' and not seen[a[3]] then
                seen[a[3]] = true
                asked[#asked + 1] = a[3]
            end
        end
    end

    local sets = {}
    if #asked < 2 or #asked > 8 then
        return sets
    end

    for _, ki in ipairs(asked) do
        sets[#sets + 1] = { name = 'only ' .. ki, set = { [ki] = true } }
        local rest = {}
        for _, other in ipairs(asked) do
            if other ~= ki then
                rest[other] = true
            end
        end

        sets[#sets + 1] = { name = 'all but ' .. ki, set = rest }
    end

    return sets
end

-- The variables a script reads, each alone at 1, 2 or 3 with the rest 0.
local function varSets(results)
    local asked, seen = {}, {}
    for _, r in ipairs(results) do
        for _, a in ipairs(r.asked or {}) do
            if a[2] == 'var' and type(a[3]) == 'string' and not seen[a[3]] then
                seen[a[3]] = true
                asked[#asked + 1] = a[3]
            end
        end
    end

    local sets = {}
    if #asked < 2 or #asked > 10 then
        return sets
    end

    for _, name in ipairs(asked) do
        for v = 1, 3 do
            sets[#sets + 1] = { name = name .. '=' .. v, vars = { [name] = v } }
        end
    end

    return sets
end

-- What a script may read besides the player's quests: each such dimension's values, as scenario changes.
local DIMENSIONS = {
    { 'weather', 0, 19, function(v) return { weather = v } end },
    { 'hour', 0, 23, function(v) return { hour = v } end },
    { 'job', 1, 22, function(v) return { job = v } end },
    { 'weekday', 0, 7, function(v) return { weekday = v } end },
    { 'nation', 0, 2, function(v) return { nation = v } end },
    { 'playerVar', 1, 1, function(v) return { playerVar = v } end },
    { 'npcVar', 1, 2, function(v) return { npcVar = v == 1 and 1 or 5 } end },
    { 'random', 1, 2, function(v) return { random = v == 1 and 'mid' or 'high' } end },
}

local function crossed(base, dimension, v)
    local changes = dimension[4](v)
    for k, x in pairs(base) do
        changes[k] = x
    end

    return changes
end

local function trigger(target)
    local onTrigger, onEventFinish = fileFunction(target, 'onTrigger'), fileFunction(target, 'onEventFinish')
    local results = {}
    local function go(name, changes, repeats)
        -- Three triggers in a row on one player: the framework takes turns between handlers.
        local keep = nil
        for n = 1, repeats or 3 do
            local r = run(target, changes, function(player, npc)
                return InteractionGlobal.onTrigger(player, npc, onTrigger)
            end, keep)
            keep = { player = r.player }
            r.name = n == 1 and name or (name .. '#' .. n)
            r.player = nil
            results[#results + 1] = r
            -- What an event it starts does when it ends, with options 0 and 1.
            for _, entry in ipairs(r.done or {}) do
                if entry[1] == 'event' and type(entry[2]) == 'number' then
                    for _, option in ipairs({ 0, 1 }) do
                        local f = run(target, changes, function(player, npc)
                            return InteractionGlobal.onEventFinish(player, entry[2], option, npc, onEventFinish)
                        end)
                        f.name = r.name .. '>finish ' .. entry[2] .. '/' .. option
                        f.player = nil
                        if #f.done > 0 then
                            results[#results + 1] = f
                        end
                    end
                end
            end
        end
    end

    for _, s in ipairs(SCENARIOS) do
        go(s[1], s[2])
    end

    for _, v in ipairs(varSets(results)) do
        local plainest = { { 'base', {} }, { 'questAccepted', { quest = 1 } }, { 'keyItems', { keyItems = true } } }
        for _, s in ipairs(plainest) do
            local changes = { vars = v.vars }
            for key, x in pairs(s[2]) do
                changes[key] = x
            end

            go(s[1] .. ', vars ' .. v.name, changes, 1)
        end
    end

    for _, k in ipairs(keyItemSets(results)) do
        for _, s in ipairs(SCENARIOS) do
            local changes = {}
            for key, v in pairs(s[2]) do
                changes[key] = v
            end

            changes.keyItems = k.set
            go(s[1] .. ', keyItems ' .. k.name, changes, 1)
        end
    end

    -- Each dimension the script reads, crossed with every scenario of the grid.
    for _, d in ipairs(DIMENSIONS) do
        if askedKind(results, d[1]) then
            for v = d[2], d[3] do
                for _, s in ipairs(SCENARIOS) do
                    go(s[1] .. ', ' .. d[1] .. ' ' .. v, crossed(s[2], d, v), 1)
                end
            end
        end
    end

    return { outcomes = group(results), asked = asksOf(results) }
end

-- Trades: an empty trade first, which records every check a handler makes; then one trade a check, holding exactly
-- what that check asks for, under the base scenario and with every key item.
local function normalize(items)
    local contents = {}
    if type(items) == 'number' then
        contents[items] = 1
    elseif type(items) == 'table' then
        for _, v in pairs(items) do
            if type(v) == 'number' then
                contents[v] = (contents[v] or 0) + 1
            elseif type(v) == 'table' and type(v[1]) == 'number' and type(v[2]) == 'number' then
                contents[v[1]] = (contents[v[1]] or 0) + v[2]
            elseif type(v) == 'table' and type(v[1]) == 'string' and type(v[2]) == 'number' then
                contents[65535] = (contents[65535] or 0) + v[2]
            end
        end
    end

    return contents
end

local function contentsKey(contents)
    local keys = {}
    for item, qty in pairs(contents) do
        keys[#keys + 1] = string.format('%d:%d', item, qty)
    end

    table.sort(keys)
    return table.concat(keys, ',')
end

local function holds(contents, wanted)
    for item, qty in pairs(wanted) do
        if (contents[item] or 0) < qty then
            return false
        end
    end

    return true
end

-- run: fn(player, npc, trade) for a trade's handlers. Each distinct check a run makes is tried as a trade of its own;
-- a check a run makes beyond what it holds (a script asking for item after item) is tried holding both.
local function tradesOf(target, call)
    local results, checks, seenCheck, tries, seenTry = {}, {}, {}, {}, {}
    local function try(contents)
        local k = contentsKey(contents)
        if seenTry[k] or #tries >= 60 then
            return
        end

        seenTry[k] = true
        tries[#tries + 1] = contents
    end

    local function go(name, contents, changes)
        local r = run(target, changes, function(player, npc)
            return call(player, npc, makeTrade(contents))
        end)
        r.name, r.player = name, nil
        results[#results + 1] = r
        for _, entry in ipairs(r.done or {}) do
            if entry[1] == 'tradeCheck' then
                local k = json(entry[3])
                if not seenCheck[k] then
                    seenCheck[k] = true
                    checks[#checks + 1] = entry[3]
                end

                local wanted = normalize(entry[3])
                try(wanted)
                if next(contents) and not holds(contents, wanted) then
                    local grown = {}
                    for item, qty in pairs(contents) do
                        grown[item] = qty
                    end

                    for item, qty in pairs(wanted) do
                        grown[item] = math.max(grown[item] or 0, qty)
                    end

                    try(grown)
                end
            end
        end
    end

    go('empty', {}, {})
    go('empty+keyItems', {}, { keyItems = true })
    go('empty+accepted', {}, { quest = 1 })
    local n = 1
    while n <= #tries do
        go('try ' .. n, tries[n], {})
        go('try ' .. n .. '+keyItems', tries[n], { keyItems = true })
        go('try ' .. n .. '+accepted', tries[n], { quest = 1, var = 1 })
        n = n + 1
    end

    for _, d in ipairs(DIMENSIONS) do
        if askedKind(results, d[1]) then
            for i = 1, #tries do
                for v = d[2], d[3] do
                    go('try ' .. i .. ', ' .. d[1] .. ' ' .. v, tries[i], crossed({}, d, v))
                    go('try ' .. i .. '+accepted, ' .. d[1] .. ' ' .. v, tries[i],
                        crossed({ quest = 1, var = 1 }, d, v))
                end
            end
        end
    end

    local listed = {}
    for i, contents in ipairs(tries) do
        local items = {}
        for item, qty in pairs(contents) do
            items[#items + 1] = { item, qty }
        end

        table.sort(items, function(a, b)
            return a[1] < b[1]
        end)
        listed[i] = items
    end

    return { outcomes = group(results), checks = checks, tries = listed }
end

local function trade(target)
    local onTrade = fileFunction(target, 'onTrade')
    return tradesOf(target, function(player, npc, t)
        return InteractionGlobal.onTrade(player, npc, t, onTrade)
    end)
end

-- Each quest, mission or battlefield step on this NPC, run directly with its check skipped: what the step does,
-- whatever state it needs. Events a step starts are followed into that container's own onEventFinish for them.
local function steps(target)
    local data    = stepData[target.zone]
    local entries = data and data[target.script]
    local found   = {}
    if type(entries) ~= 'table' then
        return found
    end

    -- A step's handler acts, or returns an action (an event, a message) for the framework to perform.
    local function perform(handler, player, ...)
        local result = handler
        if type(handler) == 'function' then
            result = handler(player, ...)
        end

        if type(result) == 'table' and result.perform then
            local args = { ... }
            local npc  = args[#args]
            return result:perform(player, type(npc) == 'table' and npc or nil)
        end

        return result
    end

    local STEP_SCENARIOS = { { 'step', {} }, { 'step+keyItems', { keyItems = true } } }
    for v = 1, 6 do
        STEP_SCENARIOS[#STEP_SCENARIOS + 1] = { 'step progress ' .. v, { quest = 1, var = v } }
        STEP_SCENARIOS[#STEP_SCENARIOS + 1] =
            { 'step progress ' .. v .. '+keyItems', { quest = 1, var = v, keyItems = true } }
    end

    for _, handlerName in ipairs(sortedKeys(entries)) do
        local list = entries[handlerName]
        for _, entry in ipairs(type(list) == 'table' and list or {}) do
            local c = entry.container
            if c and (handlerName == 'onTrigger' or handlerName == 'onTrade') then
                local label = { id = c.id, log = c.areaId, quest = c.questId, mission = c.missionId, name = c.name,
                    handler = handlerName }
                if handlerName == 'onTrigger' then
                    local results = {}
                    local function stepRun(name, changes)
                        local r = run(target, changes, function(player, npc)
                            return perform(entry.handler, player, npc)
                        end)
                        r.name, r.player = name, nil
                        results[#results + 1] = r
                        for _, e in ipairs(r.done or {}) do
                            local finish = e[1] == 'event' and type(e[2]) == 'number' and data.onEventFinish and
                                data.onEventFinish[e[2]]
                            for _, f in ipairs(type(finish) == 'table' and finish or {}) do
                                if f.container == c then
                                    for _, option in ipairs({ 0, 1 }) do
                                        local fr = run(target, changes, function(player, npc)
                                            return perform(f.handler, player, e[2], option, npc)
                                        end)
                                        fr.name, fr.player = name .. '>finish ' .. e[2] .. '/' .. option, nil
                                        if #fr.done > 0 then
                                            results[#results + 1] = fr
                                        end
                                    end
                                end
                            end
                        end
                    end

                    for _, s in ipairs(STEP_SCENARIOS) do
                        stepRun(s[1], s[2])
                    end

                    for _, k in ipairs(keyItemSets(results)) do
                        for progress = 0, 6 do
                            stepRun(string.format('step progress %d, keyItems %s', progress, k.name),
                                { quest = 1, var = progress, keyItems = k.set })
                        end
                    end

                    -- Each progress value crossed with what the step reads besides.
                    local DIMENSIONS = {
                        { 'weather', 'weather', function(v) return { weather = v } end, 0, 19 },
                        { 'hour', 'hour', function(v) return { hour = v } end, 0, 23 },
                        { 'job', 'job', function(v) return { job = v } end, 1, 22 },
                        { 'playerVar', 'playerVar', function(v) return { playerVar = v } end, 1, 1 },
                        { 'npcVar', 'npcVar', function(v) return { npcVar = v } end, 1, 1 },
                        { 'random', 'random', function(v) return { random = v == 1 and 'mid' or 'high' } end, 1, 2 },
                    }
                    for _, d in ipairs(DIMENSIONS) do
                        if askedKind(results, d[2]) then
                            for v = d[4], d[5] do
                                for progress = 0, 6 do
                                    for _, ki in ipairs({ false, true }) do
                                        local changes = d[3](v)
                                        changes.quest, changes.var, changes.keyItems = 1, progress, ki
                                        stepRun(string.format('step progress %d%s, %s %s', progress,
                                            ki and '+keyItems' or '', d[1], tostring(v)), changes)
                                    end
                                end
                            end
                        end
                    end

                    label.trigger = { outcomes = group(results) }
                else
                    label.trade = tradesOf(target, function(player, npc, t)
                        return perform(entry.handler, player, npc, t)
                    end)
                end

                found[#found + 1] = label
            end
        end
    end

    return found
end

-----------------------------------
-- The zones' clocks, weather and mob deaths, for the ??? they move or show
-----------------------------------
local function sweep(zoneId, zoneName, watched)
    local zone = xi.zones and xi.zones[zoneName]
    if type(zone) ~= 'table' then
        return nil
    end

    local zoneScript = zone.Zone or {}
    local results = {}
    local function go(name, changes, fn)
        scenario    = scenarioOf(changes)
        serverVars  = {}
        tradeChecks = 0
        rec         = {}
        local ok, err = guarded(fn)
        if not ok then
            note('error', tostring(err))
        end

        local touched = {}
        for _, entry in ipairs(rec) do
            if type(entry[1]) == 'string' and entry[1]:match('^npc:') and watched[entry[2]] then
                touched[#touched + 1] = entry
            end
        end

        rec = nil
        if #touched > 0 then
            results[#results + 1] = { name = name, done = touched }
        end
    end

    local zoneObject = zoneProxy(real.GetZone(zoneId))
    if zoneScript.onGameHour then
        for h = 0, 23 do
            go('hour ' .. h, { hour = h }, function()
                zoneScript.onGameHour(zoneObject)
            end)
        end
    end

    if zoneScript.onGameDay then
        for d = 0, 7 do
            go('day ' .. d, { weekday = d }, function()
                zoneScript.onGameDay(zoneObject)
            end)
        end
    end

    if zoneScript.onZoneWeatherChange then
        for w = 0, 19 do
            go('weather ' .. w, { weather = w }, function()
                zoneScript.onZoneWeatherChange(w)
            end)
        end
    end

    if zoneScript.onInitialize then
        go('initialize', {}, function()
            zoneScript.onInitialize(zoneObject)
        end)
    end

    -- Each mob script's death, despawn and spawn, on the first mob of its name.
    for _, name in ipairs(sortedKeys(zone.mobs or {})) do
        local script = zone.mobs[name]
        if type(script) == 'table' then
            local found = real.GetZone(zoneId) and real.GetZone(zoneId):queryEntitiesByName(name)
            local mob   = found and found[1]
            if mob then
                local mp     = mobProxy(mob)
                local player = makePlayer(mp, zoneId)
                for _, handler in ipairs({ 'onMobDeath', 'onMobDespawn', 'onMobSpawn' }) do
                    if type(script[handler]) == 'function' then
                        go(name .. ' ' .. handler, {}, function()
                            if handler == 'onMobDeath' then
                                InteractionGlobal.onMobDeath(mp, player, { isKiller = true, noKiller = false },
                                    script[handler])
                            else
                                script[handler](mp)
                            end
                        end)
                    end
                end
            end
        end
    end

    return results
end

-----------------------------------
-- Main
-----------------------------------
local byZone = {}
local count  = 0
table.sort(targets, function(a, b)
    return a.id < b.id
end)
for _, target in ipairs(targets) do
    local entry = { id = target.id, zone = target.zone, script = target.script }
    local npc   = real.GetNPCByID(target.id)
    if npc then
        entry.hasFile  = {
            onTrigger = fileFunction(target, 'onTrigger') ~= nil,
            onTrade = fileFunction(target, 'onTrade') ~= nil,
        }
        entry.trigger  = trigger(target)
        entry.trade    = trade(target)
        entry.steps    = steps(target)
    else
        entry.error = 'no NPC'
    end

    out:write('{"target":' .. json(entry, 12) .. '}\n')
    out:flush()
    byZone[target.zone] = byZone[target.zone] or { name = target.zoneName, ids = {} }
    byZone[target.zone].ids[target.id] = true
    count = count + 1
end

for _, zoneId in ipairs(sortedKeys(byZone)) do
    local z = byZone[zoneId]
    local ok, res = pcall(sweep, zoneId, z.name, z.ids)
    out:write('{"sweep":' .. json({ zone = zoneId, name = z.name, results = ok and res or nil,
        error = not ok and tostring(res) or nil }, 12) .. '}\n')
    out:flush()
end

out:close()
print(string.format('headsup_probe: %d ??? probed -> %s', count, OUT))
