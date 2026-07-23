#!/usr/bin/env python3
"""
Ninjago Total-War-lite :: game data generator.

Emits Unreal-DataTable-ready CSV + JSON from a small hand-authored source of truth.
Stats are DERIVED from archetypes + faction modifiers, so you tune ~16 archetypes
instead of 250 characters. Re-run to regenerate everything.

    python3 build.py --out ../../outputs
"""
import csv, json, math, argparse, os

# ---------------------------------------------------------------------------
# 1. ARCHETYPES  --  tune the whole game from this table
# ---------------------------------------------------------------------------
# size    = models per regiment
# hp      = hp PER MODEL
# atk/def = 1..12 scale, feeds hit chance
# dmg     = damage per hit vs unarmoured
# ap      = damage per hit that ignores armour
# armour  = 0..8, subtracts from incoming dmg (not from ap)
# morale  = 0..100, unit routs at <=0
# speed   = cm/s (Unreal units)
# rng_atk = ranged accuracy 0..10 ; rng     = range in cm ; ammo = shots per model

ARCHETYPES = {
    #  key          role            size hp   atk def chrg dmg  ap arm mor  spd  ratk  rng  ammo scale
    "CHAFF":      ("Cheap swarm",     32,  40,  3,  2,  2,  10,  0,  0,  40, 400,   0,    0,  0, 1.0),
    "LINE":       ("Line infantry",   24,  60,  5,  4,  4,  14,  0,  2,  55, 380,   0,    0,  0, 1.0),
    "SHIELD":     ("Shield wall",     24,  70,  4,  7,  3,  12,  0,  4,  65, 340,   0,    0,  0, 1.0),
    "SPEAR":      ("Anti-large",      24,  60,  4,  6,  5,  13,  6,  2,  60, 360,   0,    0,  0, 1.0),
    "HEAVY":      ("Heavy infantry",  18,  90,  7,  6,  6,  20,  8,  5,  70, 300,   0,    0,  0, 1.1),
    "ELITE":      ("Elite infantry",  16, 110,  9,  8,  8,  24, 10,  5,  80, 400,   0,    0,  0, 1.1),
    "ARCHER":     ("Missile",         20,  50,  3,  2,  1,   9,  0,  0,  45, 400,   6, 1600, 14, 1.0),
    "SPITTER":    ("Area missile",    20,  50,  3,  3,  1,   9,  4,  0,  45, 380,   5,  900, 10, 1.0),
    "SKIRMISH":   ("Fast skirmisher", 20,  50,  4,  3,  3,  12,  0,  1,  50, 520,   4, 1100, 10, 1.0),
    "CAV":        ("Shock cavalry",   12, 130,  6,  5, 15,  18,  6,  3,  70, 650,   0,    0,  0, 1.4),
    "VEHICLE":    ("Vehicle",          4, 400,  6,  6, 14,  30, 12,  7,  99, 500,   0,    0,  0, 2.2),
    "MONSTER":    ("Monster",          1, 900, 10,  6, 12,  60, 25,  6,  85, 380,   0,    0,  0, 3.0),
    "GIANT":      ("Giant",            1,1600, 11,  5, 14,  90, 50,  7,  90, 300,   0,    0,  0, 4.5),
    "SWARM":      ("Expendable swarm", 48,  20,  2,  1,  1,   5,  0,  0,  30, 480,   0,    0,  0, 0.7),
    "HERO":       ("Hero (single)",    1, 700, 10,  8, 10,  35, 15,  4,  90, 440,   0,    0,  0, 1.15),
    "LORD":       ("Lord (single)",    1,1200, 12, 10, 12,  45, 25,  5, 100, 420,   0,    0,  0, 1.25),
}

# ---------------------------------------------------------------------------
# 2. FACTIONS  --  identity, colours, army-wide rule, stat modifiers
# ---------------------------------------------------------------------------
# mods are multiplicative on the named stat unless prefixed with '+' (additive)

FACTIONS = {
    "SKULKIN": dict(
        display="Skulkin", tier="MVP", colour_primary="#E8E3D3", colour_secondary="#8B1A1A",
        mechanic="Already Dead",
        mechanic_desc="Skulkin never rout. When a Skulkin model 'dies' it collapses into a bone pile and reassembles after 20s at 50% HP, once per battle.",
        playstyle="Cheap, fast, expendable. Wins by numbers and never breaking. Weak per-model.",
        mods={"hp": 0.85, "speed": 1.05, "cost": 0.80, "morale": 99},
    ),
    "SERPENTINE": dict(
        display="Serpentine", tier="MVP", colour_primary="#3C7A3C", colour_secondary="#C8A22A",
        mechanic="Cold-Blooded",
        mechanic_desc="+10% to all combat stats in sun/desert tiles, -15% in shade, snow or night. Plan your battlefield.",
        playstyle="Five tribes, five toolkits: hypnosis, conversion, burrowing, venom, and Anacondrai elites.",
        mods={"speed": 1.05, "morale": 0.95},
    ),
    "STONE_ARMY": dict(
        display="Stone Army", tier="MVP", colour_primary="#1C1C1C", colour_secondary="#B03030",
        mechanic="Helmet of Shadows",
        mechanic_desc="The army obeys whoever carries the Helmet. Immune to morale while the bearer lives. If the bearer is killed, ANY hero touching the dropped Helmet takes control of every Stone unit on the field.",
        playstyle="Slow, armoured, nearly unkillable. A single objective (the Helmet) is its glass jaw.",
        mods={"armour": "+3", "speed": 0.85, "cost": 1.20, "morale": 99},
    ),
    "NINJA": dict(
        display="Ninja & Ninjago", tier="MVP", colour_primary="#1E6FBF", colour_secondary="#D9B310",
        mechanic="Elemental Powers",
        mechanic_desc="Few units, all heroes. Each Elemental Master has a signature power on a short cooldown. Spinjitzu = short dash that damages everything passed through.",
        playstyle="The player's starter faction. Tiny armies of very strong individuals. Ability-driven.",
        mods={"cost": 1.30, "morale": 1.10},
    ),
    "NINDROID": dict(
        display="Nindroid Army", tier="WAVE2", colour_primary="#2B2B2B", colour_secondary="#00C2C7",
        mechanic="Networked",
        mechanic_desc="Nindroids never rout. Whenever General Cryptor is alive, all Nindroids gain +1 attack. If he dies, they gain +2 instead (no more backseat commanding).",
        playstyle="Disciplined, ranged-capable, expensive. No morale phase at all.",
        mods={"cost": 1.15, "rng_atk": "+2", "morale": 99},
    ),
    "CULTISTS": dict(
        display="Anacondrai Cultists", tier="WAVE2", colour_primary="#5B2C87", colour_secondary="#C8102E",
        mechanic="The Curse",
        mechanic_desc="Chen's spell transforms cultist units into Anacondrai mid-battle: pay the cost, and a LINE unit permanently upgrades to ELITE for the rest of the campaign.",
        playstyle="Starts weak, snowballs. Reward for surviving.",
        mods={"cost": 0.95},
    ),
    "GHOST": dict(
        display="Ghost Warriors", tier="WAVE2", colour_primary="#3FE0A0", colour_secondary="#1A1A2E",
        mechanic="Cursed",
        mechanic_desc="Ghosts ignore armour entirely and can pass through walls, but ANY water contact (rain, rivers, the Water Ninja) deals massive damage. Weather is a live threat.",
        playstyle="Terrifying on a dry map. Deletes itself on a wet one.",
        mods={"armour": 0.0, "ap": 1.6, "hp": 0.8},
    ),
    "SKY_PIRATES": dict(
        display="Sky Pirates", tier="WAVE2", colour_primary="#8B4513", colour_secondary="#2E8B57",
        mechanic="Twisted Wishes",
        mechanic_desc="Nadakhan can grant a wish to an enemy hero once per battle: they get a big buff, and Nadakhan takes control of them for 15s.",
        playstyle="Raiders. Strong ranged, weak line, high mobility, one huge trick.",
        mods={"speed": 1.10, "rng_atk": "+1", "hp": 0.9},
    ),
    "VERMILLION": dict(
        display="Vermillion", tier="WAVE2", colour_primary="#B22222", colour_secondary="#4A4A4A",
        mechanic="Reform",
        mechanic_desc="A destroyed Vermillion unit leaves a snake swarm. If the swarm survives 30s, the whole unit reforms at 60% strength. Kill the snakes, not the armour.",
        playstyle="Attrition nightmare. You must finish the job.",
        mods={"hp": 1.1, "speed": 0.95},
    ),
    "SONS_OF_GARMADON": dict(
        display="Sons of Garmadon", tier="WAVE2", colour_primary="#4B0082", colour_secondary="#E8E8E8",
        mechanic="Oni Masks",
        mechanic_desc="Three masks (Deception, Vengeance, Hatred) are equipment, not characters. Assign each to any hero before battle for a different ultimate.",
        playstyle="Bikers. Fast cavalry-heavy, hero-centric.",
        mods={"speed": 1.15, "charge": "+4", "armour": "-1"},
    ),
    "DRAGON_HUNTERS": dict(
        display="Dragon Hunters", tier="WAVE2", colour_primary="#8B7355", colour_secondary="#FF6A00",
        mechanic="Dragon Bait",
        mechanic_desc="Hunters can chain and capture any MONSTER/GIANT on the field, including the enemy's, and use it for the rest of the battle.",
        playstyle="Scrap-armoured skirmishers who steal your best unit.",
        mods={"cost": 0.9, "rng_atk": "+1"},
    ),
    "PYRO_VIPERS": dict(
        display="Pyro Vipers", tier="WAVE3", colour_primary="#FF8C00", colour_secondary="#8B0000",
        mechanic="Stolen Spinjitzu",
        mechanic_desc="Aspheera drains a defeated enemy hero's power and adds their ability to her own bar, permanently.",
        playstyle="Fire, area damage, a Lord that grows every battle.",
        mods={"dmg": 1.1, "armour": "-1"},
    ),
    "BLIZZARD_SAMURAI": dict(
        display="Blizzard Samurai", tier="WAVE3", colour_primary="#BFE8F5", colour_secondary="#2C5F7C",
        mechanic="The Long Winter",
        mechanic_desc="Every 60s the map freezes another step: enemy speed -10% (stacking to -40%), Blizzard units unaffected. Win slowly or lose.",
        playstyle="Defensive. Turns the clock into a weapon.",
        mods={"armour": "+1", "speed": 0.9, "morale": 1.05},
    ),
    "CRYSTAL_ARMY": dict(
        display="Crystal Army", tier="WAVE3", colour_primary="#7B2FBF", colour_secondary="#00E5FF",
        mechanic="Vengestone",
        mechanic_desc="Enemy heroes inside 800cm of any Vengestone unit cannot use abilities at all. Powers simply switch off.",
        playstyle="The anti-hero faction. Shuts down everything the player likes.",
        mods={"armour": "+2", "cost": 1.1},
    ),
    "IMPERIUM": dict(
        display="Imperium", tier="WAVE3", colour_primary="#D4AF37", colour_secondary="#14B8B8",
        mechanic="Dragon Cores",
        mechanic_desc="Imperium spends captured dragons as fuel: sacrifice a captured MONSTER to fully heal and re-ammo the whole army once per battle.",
        playstyle="Rich, high-tech, morally bankrupt. Good at everything, expensive.",
        mods={"cost": 1.25, "armour": "+1", "rng_atk": "+1"},
    ),
    "WOLF_CLAN": dict(
        display="Wolf Clan", tier="WAVE3", colour_primary="#5A5A5A", colour_secondary="#C8102E",
        mechanic="Pack Tactics",
        mechanic_desc="+2 attack for every friendly unit within 600cm, up to +6. They fight better in a mob.",
        playstyle="Cheap, aggressive, must stay clustered.",
        mods={"cost": 0.85, "charge": "+3"},
    ),
    "GREENBONES": dict(
        display="Greenbone Warriors", tier="WAVE3", colour_primary="#8FBF3F", colour_secondary="#2F2F2F",
        mechanic="Rise Again",
        mechanic_desc="Every enemy model that dies within 500cm of the Bone King rises as a Greenbone Warrior on your side.",
        playstyle="Snowballing undead horde. The longer the fight, the bigger they get.",
        mods={"hp": 0.9, "cost": 0.8, "morale": 99},
    ),
}

# ---------------------------------------------------------------------------
# 3. UNITS  --  (row, display, faction, archetype, size_mult, weapon, hat, notes)
# ---------------------------------------------------------------------------
UNITS = [
    # ---- SKULKIN -----------------------------------------------------------
    ("SKU_MINERS",     "Skulkin Miners",        "SKULKIN", "CHAFF",  1.0, "Pickaxe",       "None",       "Cheapest body on the field. Can dig a tunnel entrance."),
    ("SKU_WARRIORS",   "Skulkin Warriors",      "SKULKIN", "LINE",   1.0, "Bone Sword",    "None",       "The default Skulkin block. Elemental-coloured armour."),
    ("SKU_WATCHMEN",   "Skulkin Watchmen",      "SKULKIN", "SPEAR",  1.0, "Bone Spear",    "Conical",    "Anti-large. Holds a line against dragons and giants."),
    ("SKU_PITBOSSES",  "Skulkin Pit Bosses",    "SKULKIN", "HEAVY",  1.0, "Bone Axe",      "Helmet",     "Overseers. Nearby Skulkin reassemble twice instead of once."),
    ("SKU_ENGINEERS",  "Skulkin Engineers",     "SKULKIN", "SKIRMISH",1.0,"Wrench",        "Goggles",    "Repair vehicles. Throw bone bombs."),
    ("SKU_SKULLTRUCK", "Skull Truck",           "SKULKIN", "VEHICLE",1.0, "Ram",           "None",       "Chariot. Massive charge, terrible turning circle."),
    ("SKU_TREADASSAULT","Turbo Shredder",       "SKULKIN", "VEHICLE",1.0, "Saw Blades",    "None",       "Grinds through infantry. Useless against armour."),

    # ---- SERPENTINE --------------------------------------------------------
    ("SER_HYP_SOLDIERS","Hypnobrai Soldiers",   "SERPENTINE","LINE",  1.0, "Serpent Blade","None",       "Blue. Chance to briefly stun on hit."),
    ("SER_HYP_SCOUTS",  "Hypnobrai Scouts",     "SERPENTINE","SKIRMISH",1.0,"Dagger",      "None",       "Reveal hidden units. Fast."),
    ("SER_FAN_SOLDIERS","Fangpyre Soldiers",    "SERPENTINE","LINE",  1.0, "Fang Blade",   "None",       "Red. Killed enemy models have a 25% chance to raise as Fangpyre."),
    ("SER_FAN_SCOUTS",  "Fangpyre Scouts",      "SERPENTINE","CHAFF", 1.0, "Fangs",        "None",       "Snappa's lot. Reckless: +2 charge, -10 morale."),
    ("SER_CON_WARRIORS","Constrictai Warriors", "SERPENTINE","HEAVY", 1.0, "Bone Club",    "None",       "Black/orange. Can BURROW: deploy anywhere, ambush."),
    ("SER_CON_SOLDIERS","Constrictai Soldiers", "SERPENTINE","SHIELD",1.0, "Shield & Mace","None",       "Crushing grip: roots the unit it engages."),
    ("SER_VEN_SPITTERS","Venomari Spitters",    "SERPENTINE","SPITTER",1.0,"Venom Vials",  "None",       "Green. Hits cause hallucination: target attacks randomly for 5s."),
    ("SER_VEN_WARRIORS","Venomari Warriors",    "SERPENTINE","LINE",  1.0, "Twin Blades",  "None",       "Lizaru's line troops."),
    ("SER_ANA_WARRIORS","Anacondrai Warriors",  "SERPENTINE","ELITE", 1.0, "Anacondrai Blade","None",    "Purple. Bigger, smarter, natural leaders. Expensive."),
    ("SER_VIPER_FLYERS","Viper Flyers",         "SERPENTINE","SKIRMISH",1.0,"Glider Blades","None",      "Set-derived flyers. Harass and reposition."),
    ("SER_DEVOURER",    "The Great Devourer",   "SERPENTINE","GIANT", 1.0, "Jaws",         "None",       "Campaign boss. Grows every time it eats a unit."),

    # ---- STONE ARMY --------------------------------------------------------
    ("STO_SCOUTS",     "Stone Scouts",          "STONE_ARMY","ARCHER", 1.0, "Crossbow",    "None",       "Yellow markings, flaming bolts. No shoulder armour."),
    ("STO_SOLDIERS",   "Stone Soldiers",        "STONE_ARMY","SHIELD", 1.0, "Katana & Shield","Conical", "Conical red hat. The block that holds the line."),
    ("STO_SWORDSMEN",  "Stone Swordsmen",       "STONE_ARMY","LINE",   1.0, "Katana",      "Conical",    "Same body, no shield, more attack."),
    ("STO_WARRIORS",   "Stone Warriors",        "STONE_ARMY","HEAVY",  1.0, "Butterfly Swords","Warrior Helm","Black chest plate, samurai-style helm."),
    ("STO_GIANT",      "Giant Stone Warrior",   "STONE_ARMY","MONSTER",1.0, "Butterfly Swords","Black Helm","Single colossus. Cannot be killed by normal damage — only knocked down."),

    # ---- NINJA / NINJAGO ---------------------------------------------------
    ("NIN_POLICE",     "Ninjago Police",        "NINJA", "SHIELD",  1.0, "Baton & Shield", "Police Cap", "Cheap allied line. Arrives as reinforcements."),
    ("NIN_SOLDIERS",   "Ninjago Soldiers",      "NINJA", "SPEAR",   1.0, "Yari",           "Jingasa",    "Serpentine War era regulars."),
    ("NIN_ROYALGUARD", "Royal Guards",          "NINJA", "ELITE",   1.0, "Naginata",       "Guard Helm", "Hutchins' palace guard."),
    ("NIN_SHINTARO",   "Army of Shintaro",      "NINJA", "ARCHER",  1.0, "Longbow",        "Winged Helm","Sky Folk. Can be deployed on high ground only."),
    ("NIN_SAMURAIX",   "Samurai X Mech",        "NINJA", "MONSTER", 1.0, "Mech Blades",    "None",       "Nya's mech. Ranged + melee."),

    # ---- NINDROIDS ---------------------------------------------------------
    ("NDR_DRONES",     "Nindroid Drones",       "NINDROID", "CHAFF",   1.0, "Blade Arm",   "None",       "Expendable. Self-destruct on death for small AoE."),
    ("NDR_WARRIORS",   "Nindroid Warriors",     "NINDROID", "LINE",    1.0, "Techno Blade","None",       "Standard Nindroid infantry."),
    ("NDR_SENTRIES",   "Nindroid Sentries",     "NINDROID", "SHIELD",  1.0, "Stun Baton",  "None",       "Guards. Can be hacked and turned by Zane."),
    ("NDR_ELITE",      "Nindroid Elite Guard",  "NINDROID", "ELITE",   1.0, "Twin Blades", "None",       "Cryptor's personal guard."),
    ("NDR_MECHDRAGON", "Nindroid MechDragon",   "NINDROID", "GIANT",   1.0, "Missiles",    "None",       "Flying artillery platform."),

    # ---- ANACONDRAI CULTISTS ----------------------------------------------
    ("CUL_CULTISTS",   "Anacondrai Cultists",   "CULTISTS", "LINE",    1.0, "Condrai Blade","Bone Mask", "Bone armour over red robes. Upgrades via The Curse."),
    ("CUL_ARCHERS",    "Condrai Archers",       "CULTISTS", "ARCHER",  1.0, "Bow",          "Bone Mask", ""),
    ("CUL_CRUSHER",    "Anacondrai Crusher",    "CULTISTS", "VEHICLE", 1.0, "Ram",          "None",      ""),
    ("CUL_COPTER",     "Condrai Copter",        "CULTISTS", "VEHICLE", 1.0, "Guns",         "None",      "Air. Fast, fragile."),

    # ---- GHOST WARRIORS ----------------------------------------------------
    ("GHO_NINJA",      "Ghost Ninja",           "GHOST", "LINE",     1.0, "Spectral Blade","Hood",      "Attila, Hackler, Ming, Spyder, Howla, Yokai, Wooo."),
    ("GHO_WARRIORS",   "Ghost Warriors",        "GHOST", "CHAFF",    1.0, "Claws",         "None",      "Cowler, Cyrus, Ghurka, Pitch, Pyrrhus, Wail."),
    ("GHO_SKREEMERS",  "Skreemers",             "GHOST", "SWARM",    1.0, "Bite",          "None",      "Every 15s a surviving Skreemer spawns another Skreemer."),
    ("GHO_PREEMINENT", "The Preeminent",        "GHOST", "GIANT",    1.0, "Tentacles",     "None",      "Kaiju. One per campaign. Dies to a single large body of water."),

    # ---- SKY PIRATES -------------------------------------------------------
    ("SKY_CREW",       "Sky Pirate Crew",       "SKY_PIRATES", "LINE",   1.0, "Cutlass",   "Bandana",   ""),
    ("SKY_GUNNERS",    "Pirate Gunners",        "SKY_PIRATES", "ARCHER", 1.0, "Flintlock", "Tricorn",   "Flintlocke's shooters."),
    ("SKY_BOARDERS",   "Sky Boarders",          "SKY_PIRATES", "SKIRMISH",1.0,"Grapple",   "Bandana",   "Fly. Land behind enemy lines."),
    ("SKY_KEEP",       "Misfortune's Keep",     "SKY_PIRATES", "GIANT",  1.0, "Broadside", "None",      "Flying flagship. Artillery."),

    # ---- VERMILLION --------------------------------------------------------
    ("VER_WARRIORS",   "Vermillion Warriors",   "VERMILLION", "LINE",   1.0, "Serpent Axe","Vermillion Helm",""),
    ("VER_ARCHERS",    "Vermillion Archers",    "VERMILLION", "ARCHER", 1.0, "Bow",        "Vermillion Helm",""),
    ("VER_BUFFMILLION","Buffmillion",           "VERMILLION", "MONSTER",1.0, "Fists",      "Vermillion Helm","Giant. Reforms like the rest."),
    ("VER_SWARM",      "Vermillion Snakes",     "VERMILLION", "SWARM",  1.0, "Bite",       "None",      "What's left when armour breaks. Kill these to stop Reform."),

    # ---- SONS OF GARMADON --------------------------------------------------
    ("SOG_BIKERS",     "SoG Bikers",            "SONS_OF_GARMADON", "CAV",  1.0, "Chain",  "Helmet",    "Luke, Chopper Maroon, Mohawk, Nails, Skip Vicious."),
    ("SOG_THUGS",      "SoG Thugs",             "SONS_OF_GARMADON", "LINE", 1.0, "Bat",    "None",      "On foot."),
    ("SOG_COLOSSUS",   "Colossus",              "SONS_OF_GARMADON", "GIANT",1.0, "Fists",  "None",      "Garmadon's stone titan."),

    # ---- DRAGON HUNTERS ----------------------------------------------------
    ("DRH_HUNTERS",    "Dragon Hunters",        "DRAGON_HUNTERS", "LINE",    1.0, "Chain Weapon","Scrap Helm",""),
    ("DRH_CROSSBOWS",  "Hunter Crossbows",      "DRAGON_HUNTERS", "ARCHER",  1.0, "Crossbow",    "Scrap Helm",""),
    ("DRH_RIDERS",     "Hunter Bike Riders",    "DRAGON_HUNTERS", "CAV",     1.0, "Cleaver",     "Scrap Helm",""),
    ("DRH_FIRSTBOURNE","Firstbourne",           "DRAGON_HUNTERS", "GIANT",   1.0, "Fire Breath", "None",      "Mother of dragons. Neutral until captured — by either side."),

    # ---- PYRO VIPERS -------------------------------------------------------
    ("PYR_SLAYERS",    "Pyro Slayers",          "PYRO_VIPERS", "LINE",    1.0, "Flame Blade","None",     ""),
    ("PYR_WHIPPERS",   "Pyro Whippers",         "PYRO_VIPERS", "SKIRMISH",1.0, "Fire Whip",  "None",     "Long reach, pulls enemies out of formation."),
    ("PYR_DESTROYERS", "Pyro Destroyers",       "PYRO_VIPERS", "HEAVY",   1.0, "Flame Maul", "None",     ""),
    ("PYR_FIREFANG",   "Fire Fang",             "PYRO_VIPERS", "GIANT",   1.0, "Jaws",       "None",     ""),

    # ---- BLIZZARD SAMURAI --------------------------------------------------
    ("BLZ_WARRIORS",   "Blizzard Warriors",     "BLIZZARD_SAMURAI", "LINE",   1.0, "Ice Katana","Blizzard Helm",""),
    ("BLZ_SWORDMASTER","Blizzard Sword Masters","BLIZZARD_SAMURAI", "ELITE",  1.0, "Ice Odachi","Blizzard Helm",""),
    ("BLZ_ARCHERS",    "Blizzard Archers",      "BLIZZARD_SAMURAI", "ARCHER", 1.0, "Ice Bow",   "Blizzard Helm","Hits slow the target."),
    ("BLZ_BEHEMOTH",   "Ice Behemoth",          "BLIZZARD_SAMURAI", "MONSTER",1.0, "Fists",     "None",         ""),
    ("BLZ_BOREAL",     "Boreal",                "BLIZZARD_SAMURAI", "GIANT",  1.0, "Ice Breath","None",         ""),

    # ---- CRYSTAL ARMY ------------------------------------------------------
    ("CRY_VENGE_WAR",  "Vengestone Warriors",   "CRYSTAL_ARMY", "HEAVY",  1.0, "Vengestone Maul","None","Aura: enemy heroes within 800cm cannot use abilities."),
    ("CRY_VENGE_GUARD","Vengestone Guards",     "CRYSTAL_ARMY", "SHIELD", 1.0, "Shield & Blade","None", ""),
    ("CRY_VENGE_BRUTE","Vengestone Brutes",     "CRYSTAL_ARMY", "MONSTER",1.0, "Fists",         "None", ""),
    ("CRY_WARRIORS",   "Crystal Warriors",      "CRYSTAL_ARMY", "LINE",   1.0, "Crystal Blade", "None", ""),
    ("CRY_ZOMBIES",    "Crystal Zombies",       "CRYSTAL_ARMY", "CHAFF",  1.0, "Claws",         "None", "Converted civilians. Free — spawn from enemy casualties."),
    ("CRY_SPIDERS",    "Crystal Spiders",       "CRYSTAL_ARMY", "SWARM",  1.0, "Bite",          "None", ""),

    # ---- IMPERIUM ----------------------------------------------------------
    ("IMP_CLAWS",      "Claws of Imperium",     "IMPERIUM", "LINE",    1.0, "Shock Lance","Claw Helm",""),
    ("IMP_HUNTERS",    "Claw Hunters",          "IMPERIUM", "SKIRMISH",1.0, "Net Launcher","Claw Helm","Can capture MONSTER units."),
    ("IMP_GUARDS",     "Imperium Guards",       "IMPERIUM", "SHIELD",  1.0, "Spear & Shield","Gold Helm","Famously absent when needed."),
    ("IMP_TEENFORCE",  "Teen Protection Force", "IMPERIUM", "CHAFF",   1.0, "Baton",       "Cap",      ""),
    ("IMP_HOUND",      "Dragon Hunter Hound",   "IMPERIUM", "VEHICLE", 1.0, "Claws",       "None",     ""),
    ("IMP_EMPRESSMECH","Empress Mech",          "IMPERIUM", "GIANT",   1.0, "Sword Beam",  "None",     "Beatrix's ultimate. Immune to elemental damage."),

    # ---- WOLF CLAN ---------------------------------------------------------
    ("WLF_WARRIORS",   "Wolf Mask Warriors",    "WOLF_CLAN", "LINE",  1.0, "Wolf Blade","Wolf Mask",""),
    ("WLF_GUARDS",     "Wolf Mask Guards",      "WOLF_CLAN", "SHIELD",1.0, "Shield & Axe","Wolf Mask",""),
    ("WLF_RIDERS",     "Wolf Riders",           "WOLF_CLAN", "CAV",   1.0, "Spear",     "Wolf Mask",""),

    # ---- GREENBONES --------------------------------------------------------
    ("GRB_WARRIORS",   "Bone Warriors",         "GREENBONES", "LINE",   1.0, "Bone Sword","None",""),
    ("GRB_GUARDS",     "Bone Guards",           "GREENBONES", "SHIELD", 1.0, "Shield & Spear","None",""),
    ("GRB_HUNTERS",    "Bone Hunters",          "GREENBONES", "ARCHER", 1.0, "Bone Bow",  "None",""),
    ("GRB_KNIGHTS",    "Bone Knights",          "GREENBONES", "ELITE",  1.0, "Greatsword","Bone Helm",""),
    ("GRB_SCORPIOS",   "Bone Scorpios",         "GREENBONES", "CAV",    1.0, "Stinger",   "None","Mounts."),
]

# ---------------------------------------------------------------------------
# 4. ABILITIES  --  one button per Lord. Kid rule: 1 Lord = 1 special.
# ---------------------------------------------------------------------------
# (row, display, type, target, radius_cm, duration_s, cooldown_s, effect, magnitude)
ABILITIES = [
    ("AB_FOUR_ARMED_FURY","Four-Armed Fury","ACTIVE","SELF_AOE",400,6,45,"Damage burst, four hits per second in radius","dmg=40/hit"),
    ("AB_BONE_RALLY",     "Bone Rally",     "ACTIVE","ALLY_AOE",1200,20,60,"Nearby Skulkin instantly reassemble","heal=50%"),
    ("AB_LIGHTNING_RUSH", "Lightning Rush", "ACTIVE","SELF",0,8,40,"Speed +100% and charge damage doubled","speed=+100%"),
    ("AB_WATCHFUL_EYE",   "Watchful Eye",   "ACTIVE","ALLY_AOE",2000,15,50,"Reveals all hidden/burrowed units; allies +3 defence","def=+3"),
    ("AB_VANISH",         "Vanish",         "ACTIVE","SELF",0,10,45,"Invisible; the next attack is a guaranteed critical","crit=x3"),
    ("AB_HYPNOTIC_GAZE",  "Hypnotic Gaze",  "ACTIVE","ENEMY_UNIT",900,10,55,"Target enemy unit stops fighting and walks to you","control=partial"),
    ("AB_FANGPYRE_BITE",  "Fangpyre Bite",  "PASSIVE","AURA",700,0,0,"Enemy models killed nearby raise as Fangpyre Scouts","chance=25%"),
    ("AB_CONSTRICT",      "Constrict",      "ACTIVE","ENEMY_AOE",600,8,45,"Enemies in radius are rooted and take crush damage","dmg=15/s"),
    ("AB_VENOM_CLOUD",    "Venom Cloud",    "ACTIVE","GROUND_AOE",800,12,50,"Enemies hallucinate: attack random targets, friend or foe","confuse=100%"),
    ("AB_SLITHER_PIT",    "Slither Pit Duel","ACTIVE","ENEMY_HERO",500,0,90,"Challenge an enemy Lord to a 1v1. Winner's army gets +2 attack","buff=+2 atk"),
    ("AB_FOUR_BLADES",    "Four Blades",    "ACTIVE","SELF_AOE",500,8,50,"Spinning butterfly swords, heavy armour-piercing","ap=35/hit"),
    ("AB_HELMET_COMMAND", "Helmet of Shadows","ACTIVE","ENEMY_AOE",1500,0,0,"Take permanent control of every Stone Army unit in radius","control=full"),
    ("AB_DARKNESS_FALLS", "Darkness Falls", "ACTIVE","GLOBAL",0,20,120,"All enemies -3 attack, -20 morale. Map goes dark","debuff=-3 atk"),
    ("AB_MEGA_WEAPON",    "Mega Weapon",    "ACTIVE","GROUND_AOE",700,0,70,"Reshapes the ground: creates a wall or a chasm","terrain=modify"),
    ("AB_OVERCLOCK",      "Overclock",      "ACTIVE","ALLY_AOE",1200,15,50,"Nearby Nindroids attack twice as fast","atkspeed=+100%"),
    ("AB_ANACONDRAI_CURSE","Anacondrai Curse","ACTIVE","ALLY_UNIT",900,0,90,"Permanently upgrade one cultist unit to Anacondrai Warriors","upgrade=ELITE"),
    ("AB_DARK_SPELL",     "Dark Spell",     "ACTIVE","GROUND",1000,30,80,"Summons an Anacondrai Serpent to fight for you","summon=MONSTER"),
    ("AB_POSSESSION",     "Possession",     "ACTIVE","ENEMY_HERO",600,12,70,"Take full control of an enemy hero","control=full"),
    ("AB_WIND_BURST",     "Wind Burst",     "ACTIVE","SELF_AOE",700,0,35,"Knock every enemy in radius off their feet","knockback=800cm"),
    ("AB_TWISTED_WISH",   "Twisted Wish",   "ACTIVE","ENEMY_HERO",800,15,90,"Grant a wish: target is hugely buffed but you control them","control=full"),
    ("AB_REFORM",         "Reform",         "ACTIVE","ALLY_UNIT",1500,0,60,"Instantly rebuild one destroyed Vermillion unit at 60%","revive=60%"),
    ("AB_TIME_SLIP",      "Time Slip",      "ACTIVE","ENEMY_AOE",1000,10,60,"Enemies in radius move and attack at half speed","slow=50%"),
    ("AB_THE_QUIET_ONE",  "The Quiet One",  "PASSIVE","SELF",0,0,0,"Invisible to the enemy until she makes her first attack","stealth=true"),
    ("AB_MASK_DECEPTION", "Mask of Deception","ACTIVE","SELF",0,8,60,"Invulnerable and cannot be slowed","invuln=true"),
    ("AB_MASK_VENGEANCE", "Mask of Vengeance","ACTIVE","SELF",0,10,60,"Reflect 100% of damage taken back to the attacker","reflect=100%"),
    ("AB_MASK_HATRED",    "Mask of Hatred", "ACTIVE","SELF",0,12,60,"Double damage, but you cannot be ordered to retreat","dmg=x2"),
    ("AB_DRAGON_BAIT",    "Dragon Bait",    "ACTIVE","MONSTER",2000,0,90,"Chain and capture any monster on the field, including the enemy's","capture=true"),
    ("AB_STOLEN_SPINJITZU","Stolen Spinjitzu","ACTIVE","SELF_AOE",600,10,55,"Fire tornado. Permanently steals a defeated hero's ability","dmg=30/s"),
    ("AB_ASH_CLOUD",      "Ash Cloud",      "ACTIVE","GROUND_AOE",900,15,50,"Blinds enemies: ranged units cannot fire","blind=true"),
    ("AB_FROZEN_WASTES",  "Frozen Wastes",  "ACTIVE","GROUND_AOE",1200,0,80,"Freeze every enemy in radius solid for 6 seconds","freeze=6s"),
    ("AB_WHISPERS",       "Whispers",       "ACTIVE","ENEMY_HERO",1000,20,60,"Target enemy Lord's ability is disabled","silence=true"),
    ("AB_EMPRESS_MECH",   "Empress Mech",   "ACTIVE","SELF",0,0,0,"Transform into the Empress Mech: immune to elemental damage","transform=GIANT"),
    ("AB_RHINO_CHARGE",   "Rhino Charge",   "ACTIVE","DIRECTION",1500,0,35,"Charge in a straight line, flattening everything","dmg=60"),
    ("AB_PACK_HOWL",      "Pack Howl",      "ACTIVE","ALLY_AOE",1500,20,50,"All Wolf Clan units in radius gain max Pack Tactics bonus","atk=+6"),
    ("AB_RISE_AGAIN",     "Rise Again",     "PASSIVE","AURA",500,0,0,"Enemy models dying nearby rise as Bone Warriors","raise=100%"),
    ("AB_SPINJITZU",      "Spinjitzu",      "ACTIVE","DIRECTION",900,0,25,"Elemental tornado dash. Damages everything passed through","dmg=35"),
    ("AB_GOLDEN_DRAGON",  "Golden Dragon",  "ACTIVE","GLOBAL",0,20,120,"Summon the Golden Dragon. All allies +4 attack, +30 morale","buff=+4 atk"),
    ("AB_FIRE_BLAST",     "Fire Blast",     "ACTIVE","GROUND_AOE",700,0,30,"Cone of fire","dmg=45"),
    ("AB_LIGHTNING_BOLT", "Lightning Bolt", "ACTIVE","ENEMY_UNIT",1400,0,30,"Chains between up to 6 nearby enemies","dmg=40"),
    ("AB_EARTHQUAKE",     "Earthquake",     "ACTIVE","SELF_AOE",800,0,35,"Knocks down everything in radius","dmg=35"),
    ("AB_ICE_WALL",       "Ice Wall",       "ACTIVE","GROUND",1000,25,40,"Raise a wall of ice that blocks movement","terrain=wall"),
    ("AB_WATER_WAVE",     "Water Wave",     "ACTIVE","DIRECTION",1200,0,35,"A wave. Instantly destroys Ghost units","dmg=40"),
    ("AB_STAFF_OF_WISDOM","Staff of Wisdom","ACTIVE","ALLY_AOE",1500,20,50,"Allies in radius gain +3 attack and +3 defence","buff=+3/+3"),
    ("AB_BROWN_NINJA",    "The Brown Ninja","PASSIVE","SELF",0,0,0,"Can pick up the Helmet of Shadows. Otherwise unremarkable","special=helmet"),
]

# ---------------------------------------------------------------------------
# 5. LORDS & HEROES  --  (row, display, faction, tier, archetype, ability, notes)
# ---------------------------------------------------------------------------
LORDS = [
    # SKULKIN
    ("LRD_SAMUKAI",  "Samukai",       "SKULKIN","LORD","LORD","AB_FOUR_ARMED_FURY","Four arms, four bone daggers. King of the Underworld."),
    ("LRD_GARMADON_U","Lord Garmadon (Underworld)","SKULKIN","LORD","LORD","AB_MEGA_WEAPON","Four-armed. Commands the Skulkin from the throne."),
    ("HRO_KRUNCHA",  "Kruncha",       "SKULKIN","CMD","HERO","AB_BONE_RALLY","General of Earth. Helmeted, permanently furious."),
    ("HRO_NUCKAL",   "Nuckal",        "SKULKIN","CMD","HERO","AB_LIGHTNING_RUSH","General of Lightning. Spiked skull, no impulse control."),
    ("HRO_WYPLASH",  "Wyplash",       "SKULKIN","CMD","HERO","AB_WATCHFUL_EYE","General of Ice. Straw hat, deeply paranoid."),
    ("HRO_FRAKJAW",  "Frakjaw",       "SKULKIN","ELITE","HERO","AB_FIRE_BLAST","Fire warrior. Loud."),
    ("HRO_BONEZAI",  "Bonezai",       "SKULKIN","ELITE","HERO","AB_ICE_WALL","Ice warrior. Builds the vehicles."),
    ("HRO_CHOPOV",   "Chopov",        "SKULKIN","ELITE","HERO","AB_EARTHQUAKE","Earth warrior. Mechanic."),
    ("HRO_KRAZI",    "Krazi",         "SKULKIN","ELITE","HERO","AB_LIGHTNING_BOLT","Lightning warrior. Jester hat, unhinged."),

    # SERPENTINE
    ("LRD_PYTHOR",   "Pythor P. Chumsworth","SERPENTINE","LORD","LORD","AB_VANISH","Last Anacondrai. Snake King. Turns invisible."),
    ("LRD_SKALES",   "Skales",        "SERPENTINE","LORD","LORD","AB_HYPNOTIC_GAZE","Hypnobrai General, later Snake King."),
    ("HRO_SLITHRAA", "Slithraa",      "SERPENTINE","CMD","HERO","AB_SLITHER_PIT","Deposed Hypnobrai General. Wants his tail back."),
    ("HRO_FANGTOM",  "Fangtom",       "SERPENTINE","CMD","HERO","AB_FANGPYRE_BITE","Two-headed Fangpyre General."),
    ("HRO_SKALIDOR", "Skalidor",      "SERPENTINE","CMD","HERO","AB_CONSTRICT","Constrictai General. Huge and slow."),
    ("HRO_ACIDICUS", "Acidicus",      "SERPENTINE","CMD","HERO","AB_VENOM_CLOUD","Venomari General. Invented the venom vials."),
    ("HRO_ARCTURUS", "Arcturus",      "SERPENTINE","CMD","HERO","AB_VANISH","Ancient Anacondrai General."),
    ("HRO_MEZMO",    "Mezmo",         "SERPENTINE","ELITE","HERO","AB_HYPNOTIC_GAZE","Hypnobrai Warrior."),
    ("HRO_RATTLA",   "Rattla",        "SERPENTINE","ELITE","HERO","AB_HYPNOTIC_GAZE","Hypnobrai Soldier."),
    ("HRO_FANGDAM",  "Fangdam",       "SERPENTINE","ELITE","HERO","AB_FANGPYRE_BITE","Fangtom's two-headed brother."),
    ("HRO_FANGSUEI", "Fang-Suei",     "SERPENTINE","ELITE","HERO","AB_FANGPYRE_BITE","Fangpyre Soldier."),
    ("HRO_SNAPPA",   "Snappa",        "SERPENTINE","ELITE","HERO","AB_FANGPYRE_BITE","Fangpyre Scout. Reckless."),
    ("HRO_BYTAR",    "Bytar",         "SERPENTINE","ELITE","HERO","AB_CONSTRICT","Constrictai Warrior."),
    ("HRO_CHOKUN",   "Chokun",        "SERPENTINE","ELITE","HERO","AB_CONSTRICT","Constrictai Soldier."),
    ("HRO_SNIKE",    "Snike",         "SERPENTINE","ELITE","HERO","AB_CONSTRICT","Constrictai Scout."),
    ("HRO_LIZARU",   "Lizaru",        "SERPENTINE","ELITE","HERO","AB_VENOM_CLOUD","Venomari Warrior."),
    ("HRO_SPITTA",   "Spitta",        "SERPENTINE","ELITE","HERO","AB_VENOM_CLOUD","Venomari Soldier. Leaks constantly."),
    ("HRO_ZOLTAR",   "Zoltar",        "SERPENTINE","ELITE","HERO","AB_VENOM_CLOUD","Venomari Scout."),

    # STONE ARMY
    ("LRD_OVERLORD", "The Overlord",  "STONE_ARMY","LORD","LORD","AB_DARKNESS_FALLS","Creator of the Stone Army. The first evil."),
    ("LRD_KOZU",     "General Kozu",  "STONE_ARMY","LORD","LORD","AB_FOUR_BLADES","Four arms, twin butterfly swords. Only Stone soldier who speaks."),
    ("LRD_GARMADON_S","Lord Garmadon (Helmet)","STONE_ARMY","LORD","LORD","AB_MEGA_WEAPON","Commands the Stone Army via the Helmet of Shadows."),
    ("HRO_DARETH",   "Dareth",        "STONE_ARMY","LORD","HERO","AB_HELMET_COMMAND","The Brown Ninja. Genuinely canonically commands this army."),

    # NINJA
    ("LRD_LLOYD",    "Lloyd Garmadon","NINJA","LORD","LORD","AB_GOLDEN_DRAGON","Green/Golden Ninja. Master of Energy."),
    ("LRD_WU",       "Master Wu",     "NINJA","LORD","LORD","AB_STAFF_OF_WISDOM","Sensei. Creation Spinjitzu."),
    ("HRO_KAI",      "Kai",           "NINJA","CMD","HERO","AB_FIRE_BLAST","Master of Fire."),
    ("HRO_JAY",      "Jay Walker",    "NINJA","CMD","HERO","AB_LIGHTNING_BOLT","Master of Lightning."),
    ("HRO_COLE",     "Cole",          "NINJA","CMD","HERO","AB_EARTHQUAKE","Master of Earth."),
    ("HRO_ZANE",     "Zane",          "NINJA","CMD","HERO","AB_ICE_WALL","Master of Ice. Nindroid."),
    ("HRO_NYA",      "Nya",           "NINJA","CMD","HERO","AB_WATER_WAVE","Master of Water. Hard-counters Ghosts."),
    ("HRO_PIXAL",    "P.I.X.A.L.",    "NINJA","CMD","HERO","AB_OVERCLOCK","Samurai X."),
    ("HRO_MORRO_G",  "Morro (redeemed)","NINJA","CMD","HERO","AB_WIND_BURST","Master of Wind."),
    ("HRO_HUTCHINS", "Hutchins",      "NINJA","CMD","HERO","AB_STAFF_OF_WISDOM","Head of the Royal Guard."),
    ("HRO_HAILMAR",  "Captain Hailmar","NINJA","CMD","HERO","AB_STAFF_OF_WISDOM","Shintaro Royal Guard."),
    ("HRO_SKYLOR",   "Skylor Chen",   "NINJA","CMD","HERO","AB_ANACONDRAI_CURSE","Master of Amber: copies the ability of whoever she last touched."),
    ("HRO_RONIN",    "Ronin",         "NINJA","ELITE","HERO","AB_VANISH","Mercenary. Available to the highest bidder."),

    # NINDROIDS
    ("LRD_OVERLORD_D","The Golden Master","NINDROID","LORD","LORD","AB_DARKNESS_FALLS","The Overlord, digitised and rebuilt."),
    ("LRD_CRYPTOR",  "General Cryptor","NINDROID","LORD","LORD","AB_OVERCLOCK","Modelled on Zane. Insufferable about it."),
    ("HRO_MINDROID", "Min-Droid",     "NINDROID","ELITE","HERO","AB_LIGHTNING_RUSH","Built short. Furious about it."),
    ("HRO_SENTRYGEN","Sentry General","NINDROID","CMD","HERO","AB_OVERCLOCK","Commands the Sentries."),

    # CULTISTS
    ("LRD_CHEN",     "Master Chen",   "CULTISTS","LORD","LORD","AB_ANACONDRAI_CURSE","Cult leader. Runs a noodle empire as a front."),
    ("LRD_CLOUSE",   "Clouse",        "CULTISTS","LORD","LORD","AB_DARK_SPELL","Chen's sorcerer and right hand."),
    ("HRO_EYEZOR",   "Eyezor",        "CULTISTS","CMD","HERO","AB_BONE_RALLY","Chief enforcer. Mohawk, eye scar."),
    ("HRO_ZUGU",     "Zugu",          "CULTISTS","CMD","HERO","AB_EARTHQUAKE","General. Former sumo."),
    ("HRO_KAPAU",    "Kapau",         "CULTISTS","ELITE","HERO","AB_ANACONDRAI_CURSE","Later Kapau'rai."),
    ("HRO_CHOPE",    "Chope",         "CULTISTS","ELITE","HERO","AB_ANACONDRAI_CURSE","Later Chope'rai."),
    ("HRO_KRAIT",    "Krait",         "CULTISTS","ELITE","HERO","AB_VENOM_CLOUD","Cultist."),
    ("HRO_SLEVEN",   "Sleven",        "CULTISTS","ELITE","HERO","AB_VENOM_CLOUD","Cultist."),

    # GHOSTS
    ("LRD_MORRO",    "Morro",         "GHOST","LORD","LORD","AB_POSSESSION","Wu's first student. Master of Wind. Possesses Lloyd."),
    ("HRO_SOULARCHER","Bow Master Soul Archer","GHOST","CMD","HERO","AB_VANISH","Cursed arrows."),
    ("HRO_BANSHA",   "Blade Master Bansha","GHOST","CMD","HERO","AB_POSSESSION","Telepath."),
    ("HRO_GHOULTAR", "Scythe Master Ghoultar","GHOST","CMD","HERO","AB_EARTHQUAKE","All strength, no plan."),
    ("HRO_WRAYTH",   "Chain Master Wrayth","GHOST","CMD","HERO","AB_CONSTRICT","Chain whip and Soul Cycle."),

    # SKY PIRATES
    ("LRD_NADAKHAN", "Nadakhan",      "SKY_PIRATES","LORD","LORD","AB_TWISTED_WISH","Djinn captain. Four arms. Grants wishes, badly."),
    ("HRO_FLINTLOCKE","Flintlocke",   "SKY_PIRATES","CMD","HERO","AB_FIRE_BLAST","First mate. Two pistols, never misses."),
    ("HRO_DOGSHANK", "Dogshank",      "SKY_PIRATES","CMD","HERO","AB_CONSTRICT","Giant. Anchor and chain."),
    ("HRO_DOUBLOON", "Doubloon",      "SKY_PIRATES","ELITE","HERO","AB_VANISH","Two-faced. Silent."),
    ("HRO_CLANCEE",  "Clancee",       "SKY_PIRATES","ELITE","HERO","AB_VENOM_CLOUD","Serpentine swabbie. Peg leg. Seasick."),
    ("HRO_SQIFFY",   "Sqiffy",        "SKY_PIRATES","ELITE","HERO","AB_LIGHTNING_RUSH","Youngest of the crew."),
    ("HRO_BUCKO",    "Bucko",         "SKY_PIRATES","ELITE","HERO","AB_FIRE_BLAST","Pirate."),
    ("HRO_CYREN",    "Cyren",         "SKY_PIRATES","ELITE","HERO","AB_HYPNOTIC_GAZE","Her singing voice stops an army."),
    ("HRO_MONKEYWR", "Monkey Wretch", "SKY_PIRATES","ELITE","HERO","AB_OVERCLOCK","Cybernetic monkey mechanic."),

    # VERMILLION
    ("LRD_KRUX",     "Krux",          "VERMILLION","LORD","LORD","AB_TIME_SLIP","Time Twin. Poses as Dr. Saunders."),
    ("LRD_ACRONIX",  "Acronix",       "VERMILLION","LORD","LORD","AB_TIME_SLIP","Time Twin. The one who skipped 40 years."),
    ("HRO_MACHIA",   "Supreme Commander Machia","VERMILLION","CMD","HERO","AB_REFORM","Actually competent. Replaces the other two."),
    ("HRO_RAGGMUNK", "Commander Raggmunk","VERMILLION","CMD","HERO","AB_BONE_RALLY","Ego-driven."),
    ("HRO_BLUNCK",   "Commander Blunck","VERMILLION","CMD","HERO","AB_EARTHQUAKE","Brutish."),
    ("HRO_SLACKJAW", "Slackjaw",      "VERMILLION","ELITE","HERO","AB_REFORM","Named warrior."),
    ("HRO_RIVETT",   "Rivett",        "VERMILLION","ELITE","HERO","AB_REFORM","Named warrior."),
    ("HRO_TANNIN",   "Tannin",        "VERMILLION","ELITE","HERO","AB_REFORM","Named warrior."),
    ("HRO_VERMIN",   "Vermin",        "VERMILLION","ELITE","HERO","AB_REFORM","Named warrior."),

    # SONS OF GARMADON
    ("LRD_HARUMI",   "Harumi",        "SONS_OF_GARMADON","LORD","LORD","AB_THE_QUIET_ONE","The Quiet One. Princess of Ninjago. The actual leader."),
    ("LRD_GARMADON_R","Lord Garmadon (Reborn)","SONS_OF_GARMADON","LORD","LORD","AB_MEGA_WEAPON","Resurrected. Four arms, no restraint."),
    ("HRO_KILLOW",   "Killow",        "SONS_OF_GARMADON","CMD","HERO","AB_MASK_DECEPTION","Fist of the Quiet One. Enormous."),
    ("HRO_MRE",      "Mr. E",         "SONS_OF_GARMADON","CMD","HERO","AB_MASK_VENGEANCE","Fist of the Quiet One. Nindroid."),
    ("HRO_ULTRAVIOLET","Ultra Violet","SONS_OF_GARMADON","CMD","HERO","AB_MASK_HATRED","Fist of the Quiet One. Manic."),
    ("HRO_LUKE",     "Luke Cunningham","SONS_OF_GARMADON","ELITE","HERO","AB_LIGHTNING_RUSH","Biker."),
    ("HRO_CHOPPER",  "Chopper Maroon","SONS_OF_GARMADON","ELITE","HERO","AB_LIGHTNING_RUSH","Biker."),
    ("HRO_MOHAWK",   "Mohawk",        "SONS_OF_GARMADON","ELITE","HERO","AB_LIGHTNING_RUSH","Biker."),
    ("HRO_NAILS",    "Nails",         "SONS_OF_GARMADON","ELITE","HERO","AB_LIGHTNING_RUSH","Biker."),
    ("HRO_SKIPVIC",  "Skip Vicious",  "SONS_OF_GARMADON","ELITE","HERO","AB_LIGHTNING_RUSH","Biker."),

    # DRAGON HUNTERS
    ("LRD_IRONBARON","Iron Baron",    "DRAGON_HUNTERS","LORD","LORD","AB_DRAGON_BAIT","Mechanical leg, claw arm, elaborate lie."),
    ("HRO_HEAVYMETAL","Heavy Metal (Faith)","DRAGON_HUNTERS","CMD","HERO","AB_VANISH","Second-in-command. Defects."),
    ("HRO_JETJACK",  "Jet Jack",      "DRAGON_HUNTERS","CMD","HERO","AB_LIGHTNING_RUSH","Jetpack. Sharpshooter."),
    ("HRO_DADDYNOLEGS","Daddy No Legs","DRAGON_HUNTERS","CMD","HERO","AB_CONSTRICT","Mechanical spider legs."),
    ("HRO_MUZZLE",   "Muzzle",        "DRAGON_HUNTERS","ELITE","HERO","AB_EARTHQUAKE","Muzzled brute."),
    ("HRO_CHEWTOY",  "Chew Toy",      "DRAGON_HUNTERS","ELITE","HERO","AB_EARTHQUAKE","Hunter."),
    ("HRO_ARKADE",   "Arkade",        "DRAGON_HUNTERS","ELITE","HERO","AB_FIRE_BLAST","Hunter."),
    ("HRO_SCAR",     "Scar the Skullbreaker","DRAGON_HUNTERS","ELITE","HERO","AB_EARTHQUAKE","Hunter."),
    ("HRO_OTTOPILOT","Otto Pilot",    "DRAGON_HUNTERS","ELITE","HERO","AB_LIGHTNING_RUSH","Pilot."),
    ("HRO_DANGERBUFF","Stalwart Dangerbuff","DRAGON_HUNTERS","ELITE","HERO","AB_BONE_RALLY","Dangerbuff clan."),

    # ONI (attach to Dragon Hunters' realm for now)
    ("LRD_OMEGA",    "The Omega",     "DRAGON_HUNTERS","LORD","LORD","AB_DARKNESS_FALLS","Oni warlord supreme. Turns everything to dust."),

    # PYRO VIPERS
    ("LRD_ASPHEERA", "Aspheera",      "PYRO_VIPERS","LORD","LORD","AB_STOLEN_SPINJITZU","Stole Wu's Spinjitzu. Holds a 1000-year grudge."),
    ("HRO_CHAR",     "Char",          "PYRO_VIPERS","CMD","HERO","AB_ASH_CLOUD","General. Charred survivor."),
    ("HRO_MAMBO_V",  "Mambo V",       "PYRO_VIPERS","CMD","HERO","AB_STAFF_OF_WISDOM","Serpentine King. Aspheera's era."),

    # BLIZZARD SAMURAI
    ("LRD_ICEEMPEROR","The Ice Emperor","BLIZZARD_SAMURAI","LORD","LORD","AB_FROZEN_WASTES","Zane, amnesiac, holding the Scroll of Forbidden Spinjitzu."),
    ("LRD_VEX",      "General Vex",   "BLIZZARD_SAMURAI","LORD","LORD","AB_WHISPERS","The advisor. The actual villain."),
    ("HRO_GRIMFAX",  "Grimfax",       "BLIZZARD_SAMURAI","CMD","HERO","AB_ICE_WALL","Former Blizzard general and King of the Never-Realm. Defects."),

    # CRYSTAL ARMY
    ("LRD_CRYSTALKING","The Crystal King","CRYSTAL_ARMY","LORD","LORD","AB_DARKNESS_FALLS","The Overlord's final form."),
    ("LRD_HARUMI_C", "Harumi (Crystal)","CRYSTAL_ARMY","LORD","LORD","AB_THE_QUIET_ONE","Field commander of the Crystal Army."),

    # IMPERIUM
    ("LRD_BEATRIX",  "Empress Beatrix Vespasian-Orus","IMPERIUM","LORD","LORD","AB_EMPRESS_MECH","Usurper. Killed her father, imprisoned her sister."),
    ("LRD_RAS_I",    "Lord Ras (Imperium)","IMPERIUM","LORD","LORD","AB_RHINO_CHARGE","Beatrix's second-in-command. Rhino mask."),
    ("HRO_RAPTON",   "Rapton",        "IMPERIUM","CMD","HERO","AB_DRAGON_BAIT","Claw commander."),
    ("HRO_DORAMA",   "Dorama",        "IMPERIUM","CMD","HERO","AB_VANISH","Theatrical master of disguise."),
    ("HRO_JORDANA_I","Jordana",       "IMPERIUM","CMD","HERO","AB_LIGHTNING_BOLT","Sora's rival. Ruthlessly competitive."),
    ("HRO_LAROW",    "Dr. LaRow",     "IMPERIUM","CMD","HERO","AB_OVERCLOCK","Chief scientist. Built the MergeQuake weapon."),
    ("HRO_MELVIN",   "Melvin",        "IMPERIUM","ELITE","HERO","AB_BONE_RALLY","Guard. Doing his best."),
    ("HRO_CLAWGEN",  "Claw General",  "IMPERIUM","CMD","HERO","AB_RHINO_CHARGE","Generic general — clone for variety."),

    # WOLF CLAN
    ("LRD_RAS_W",    "Lord Ras (Wolf Clan)","WOLF_CLAN","LORD","LORD","AB_PACK_HOWL","Same rhino, own army now."),
    ("HRO_JORDANA_W","Jordana (Wolf Clan)","WOLF_CLAN","CMD","HERO","AB_LIGHTNING_BOLT","Lieutenant."),
    ("HRO_ASA",      "Asa",           "WOLF_CLAN","ELITE","HERO","AB_PACK_HOWL","Children of the Wolf."),
    ("HRO_BALUN",    "Balun",         "WOLF_CLAN","ELITE","HERO","AB_PACK_HOWL","Children of the Wolf."),
    ("HRO_IRIEL",    "Iriel",         "WOLF_CLAN","ELITE","HERO","AB_PACK_HOWL","Children of the Wolf."),
    ("HRO_WARBY",    "Warby",         "WOLF_CLAN","ELITE","HERO","AB_PACK_HOWL","Children of the Wolf."),
    ("HRO_ZURI",     "Zuri",          "WOLF_CLAN","ELITE","HERO","AB_PACK_HOWL","Children of the Wolf."),

    # GREENBONES
    ("LRD_BONEKING", "Bone King",     "GREENBONES","LORD","LORD","AB_RISE_AGAIN","Every enemy that dies near him joins him."),
]

# ---------------------------------------------------------------------------
# derivation
# ---------------------------------------------------------------------------
STATS = ["size","hp","atk","def","charge","dmg","ap","armour","morale","speed","rng_atk","rng","ammo","scale"]

def apply_mod(value, mod):
    if isinstance(mod, str) and mod.startswith(("+","-")):
        return value + float(mod)
    return value * float(mod)

def derive(archetype_key, faction_key, size_mult=1.0):
    a = ARCHETYPES[archetype_key]
    s = dict(zip(STATS, a[1:]))
    s["size"] = max(1, round(s["size"] * size_mult))
    mods = FACTIONS[faction_key]["mods"]
    for stat, mod in mods.items():
        if stat == "cost":
            continue
        if stat == "morale" and mod == 99:
            s["morale"] = 99   # 99 = flag: immune to morale
            continue
        if stat in s:
            s[stat] = apply_mod(s[stat], mod)
    for k in ("hp","atk","def","charge","dmg","ap","armour","morale","speed","rng_atk","rng","ammo"):
        s[k] = max(0, round(s[k]))
    s["scale"] = round(s["scale"], 2)
    return s

def cost_of(s, faction_key):
    power = (s["hp"]*0.5 + s["atk"]*8 + s["def"]*8 + s["dmg"]*4 + s["ap"]*3
             + s["armour"]*10 + s["charge"]*2 + s["rng_atk"]*6 + s["rng"]*0.01 + s["speed"]*0.05)
    # single entities (Lords, Monsters, Giants, Vehicles) get a floor so that a
    # 1-model Lord isn't cheaper than a 24-model line regiment
    effective_size = max(s["size"], 8)
    raw = power * effective_size / 10.0
    raw *= float(FACTIONS[faction_key]["mods"].get("cost", 1.0))
    return int(round(raw / 5.0) * 5)

# ---------------------------------------------------------------------------
# emit
# ---------------------------------------------------------------------------
def write(path, rows, fields):
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader(); w.writerows(rows)
    with open(path.replace(".csv", ".json"), "w") as f:
        json.dump(rows, f, indent=2)

def main(out):
    os.makedirs(out, exist_ok=True)

    # factions
    frows = []
    for k, v in FACTIONS.items():
        frows.append(dict(Name=k, DisplayName=v["display"], Wave=v["tier"],
                          ColourPrimary=v["colour_primary"], ColourSecondary=v["colour_secondary"],
                          Mechanic=v["mechanic"], MechanicDesc=v["mechanic_desc"],
                          Playstyle=v["playstyle"],
                          StatMods="; ".join(f"{a}{b}" if isinstance(b,str) else f"{a}x{b}" for a,b in v["mods"].items())))
    write(os.path.join(out, "dt_factions.csv"), frows,
          ["Name","DisplayName","Wave","ColourPrimary","ColourSecondary","Mechanic","MechanicDesc","Playstyle","StatMods"])

    # archetypes
    arows = []
    for k, a in ARCHETYPES.items():
        d = dict(zip(STATS, a[1:]))
        arows.append(dict(Name=k, Role=a[0], **{x.capitalize(): d[x] for x in STATS}))
    write(os.path.join(out, "dt_archetypes.csv"), arows,
          ["Name","Role"] + [x.capitalize() for x in STATS])

    # abilities
    abrows = [dict(Name=r[0], DisplayName=r[1], Type=r[2], Target=r[3], RadiusCm=r[4],
                   DurationS=r[5], CooldownS=r[6], Effect=r[7], Magnitude=r[8]) for r in ABILITIES]
    write(os.path.join(out, "dt_abilities.csv"), abrows,
          ["Name","DisplayName","Type","Target","RadiusCm","DurationS","CooldownS","Effect","Magnitude"])

    # units
    urows = []
    for row, disp, fac, arch, sm, weapon, hat, notes in UNITS:
        s = derive(arch, fac, sm)
        urows.append(dict(
            Name=row, DisplayName=disp, Faction=fac, Archetype=arch, Tier="TROOP",
            UnitSize=s["size"], HpPerModel=s["hp"], MeleeAttack=s["atk"], MeleeDefence=s["def"],
            ChargeBonus=s["charge"], Damage=s["dmg"], ArmourPiercing=s["ap"], Armour=s["armour"],
            Morale=s["morale"], SpeedCmS=s["speed"], RangedAttack=s["rng_atk"], RangeCm=s["rng"],
            Ammo=s["ammo"], ModelScale=s["scale"], Cost=cost_of(s, fac), Upkeep=max(1, cost_of(s, fac)//10),
            Ability="", Weapon=weapon, Headgear=hat,
            ColourPrimary=FACTIONS[fac]["colour_primary"], ColourSecondary=FACTIONS[fac]["colour_secondary"],
            MeshHead="", MeshTorso="", MeshLegs="", MeshHat="", MeshWeapon="",
            Image="", AssetStatus="todo", Notes=notes))
    for row, disp, fac, tier, arch, ability, notes in LORDS:
        s = derive(arch, fac, 1.0)
        urows.append(dict(
            Name=row, DisplayName=disp, Faction=fac, Archetype=arch, Tier=tier,
            UnitSize=1, HpPerModel=s["hp"], MeleeAttack=s["atk"], MeleeDefence=s["def"],
            ChargeBonus=s["charge"], Damage=s["dmg"], ArmourPiercing=s["ap"], Armour=s["armour"],
            Morale=s["morale"], SpeedCmS=s["speed"], RangedAttack=s["rng_atk"], RangeCm=s["rng"],
            Ammo=s["ammo"], ModelScale=s["scale"], Cost=cost_of(s, fac), Upkeep=max(1, cost_of(s, fac)//10),
            Ability=ability, Weapon="", Headgear="",
            ColourPrimary=FACTIONS[fac]["colour_primary"], ColourSecondary=FACTIONS[fac]["colour_secondary"],
            MeshHead="", MeshTorso="", MeshLegs="", MeshHat="", MeshWeapon="",
            Image="", AssetStatus="todo", Notes=notes))
    ufields = ["Name","DisplayName","Faction","Archetype","Tier","UnitSize","HpPerModel","MeleeAttack",
               "MeleeDefence","ChargeBonus","Damage","ArmourPiercing","Armour","Morale","SpeedCmS",
               "RangedAttack","RangeCm","Ammo","ModelScale","Cost","Upkeep","Ability","Weapon","Headgear",
               "ColourPrimary","ColourSecondary","MeshHead","MeshTorso","MeshLegs","MeshHat","MeshWeapon",
               "Image","AssetStatus","Notes"]
    write(os.path.join(out, "dt_units.csv"), urows, ufields)

    print(f"factions   {len(frows)}")
    print(f"archetypes {len(arows)}")
    print(f"abilities  {len(abrows)}")
    print(f"units+lords {len(urows)}  ({len(UNITS)} regiments, {len(LORDS)} single-entity)")

if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--out", default="out")
    main(p.parse_args().out)
