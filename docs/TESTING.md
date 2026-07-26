# Ninjago v0.1 — complete testing runbook

The full path from a fresh sync to a verified v0.1, milestone by milestone. Every milestone has:
**commands to run**, **expected output**, and a **pass check**. Work top to bottom the first time;
after that you can jump to any milestone.

> The code is authored on Linux but **nothing here has been compiled or run yet** — this document
> is the acceptance gate. Do all of it on the **Windows** machine with UE 5.7.4 installed.
> Deeper asset-authoring detail lives in `docs/EDITOR_STEPS.md`; this file is the test path.

---

## 0. Setup (once per machine)

1. Sync the whole repo to the Windows box.
2. Set two variables at the top of every PowerShell session:

   ```powershell
   $UE   = "C:\Program Files\Epic Games\UE_5.7"     # your 5.7.4 install root
   $Proj = "C:\path\to\warjago\Ninjago.uproject"    # the synced .uproject
   ```

3. Handy derived paths:

   ```powershell
   $Build = "$UE\Engine\Build\BatchFiles\Build.bat"
   $Cmd   = "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
   $Editor= "$UE\Engine\Binaries\Win64\UnrealEditor.exe"
   ```

Checklist for the whole run:

- [ ] M1 compiles clean
- [ ] M2 four DataTables, dt_units = 209
- [ ] M3 six automation tests pass
- [ ] M4 minifigs render in formation, in faction colours
- [ ] M5 select + move works
- [ ] M6 battle fights to a winner
- [ ] M7 both abilities fire on cooldown

---

## M1 — Build (project files + compile)

```powershell
# 1) Generate Visual Studio project files
& $Build -projectfiles -project="$Proj" -game -engine -progress

# 2) Compile the editor target
& $Build NinjagoEditor Win64 Development -project="$Proj" -waitmutex
```

**Expected:** step 1 writes `Ninjago.sln`; step 2 ends with `Build succeeded`.

**Pass check:** step 2 succeeds with **zero warnings** mentioning `Source\Ninjago`.

- If a version-association error appears: right-click `Ninjago.uproject` → **Switch Unreal Engine
  version** → pick 5.7.4, re-run.
- If it errors on `BuildSettingsVersion.Latest` / `EngineIncludeOrderVersion.Latest`: note the exact
  message — one-line fix in the two `Source\*.Target.cs` files.

---

## M2 — Data import

Open the editor once, then run the importer from inside it:

```powershell
& $Editor "$Proj"
```

In the editor: **Tools → Execute Python Script → `Tools/import_datatables.py`**.
Watch the **Output Log** (Window → Output Log).

**Expected log:**

```
==================== Ninjago DataTable import ====================
  dt_units        209 / 209   OK
  dt_factions      17 / 17    OK
  dt_archetypes    16 / 16    OK
  dt_abilities     44 / 44    OK
=================================================================
All four DataTables imported; row counts match. M2 import OK.
```

**Pass check:** four assets under `Content/Data/` (`/Game/Data/dt_*`), `dt_units` = **209** rows.
Open `dt_units`, spot-check a row (e.g. `SKU_WARRIORS`: UnitSize 24, Damage 14) and confirm the
`Tier` column imported (TROOP/CMD/ELITE/LORD, not blank).

- If the log warns it can't match a `Tier` enum value → tell me; I switch `Tier` to `FName`.

Also verify **Project Settings → Ninjago** shows the eight tuning constants (CombatTickSeconds 2.0,
EngageRangeCm 150, …) plus AbilityChannelTicksPerSecond 4.

---

## M3 — Automation tests (headless)

Close the editor first (avoids a file lock), then:

```powershell
& $Cmd "$Proj" -ExecCmds="Automation RunTests Ninjago; Quit" -unattended -nop4 -nosplash `
  -TestExit="Automation Test Queue Empty" -log -ReportOutputPath="$(Split-Path $Proj)\Saved\TestReport"
```

**Expected:** the log lists six tests completing, each `Result={Passed}`:

```
Ninjago.Combat.HitChanceClamp ................. Passed
Ninjago.Combat.DamageFloor .................... Passed
Ninjago.Combat.ArmourPiercing ................. Passed
Ninjago.Combat.ResolveGuaranteedHitAndMiss .... Passed
Ninjago.Formation.SingleEntityAtOrigin ........ Passed
Ninjago.Formation.BlockSymmetricNoOverlap ..... Passed
```

**Pass check:** 6 run, 6 passed, 0 failed. (Open `Saved\TestReport\index.json` or read the log tail.)

Alternative (visual): in the editor, **Tools → Test Automation** → Session Frontend → Automation
tab → filter `Ninjago` → Start Tests.

- If it errors on `EAutomationTestFlags::EditorContext` / `EngineFilter` at *compile* time → tell me
  the expected name; two-file fix in `Source\Ninjago\Tests\*`.

---

## M4 — Rendering

1. Build the master material + placeholder meshes: in the editor,
   **Tools → Execute Python Script → `Tools/make_placeholder_meshes.py`**.

   **Expected:**
   ```
     meshes created this run: 6 (of 6)
     master material: built
     location: /Game/Placeholders
   ```

2. Create a test level (or open an existing one) with a **floor** (with collision) and a
   **PlayerStart**. Save it. Set it as the editor/startup map if you like.
3. Drag a **Ninjago Unit** actor into the level. In Details:
   - **Unit Row Handle** → Data Table `dt_units`, Row `SKU_WARRIORS` (a 24-model block).
   - **Team** → Skulkin.
   Add a second one: `HRO_KAI`, Team Ninja (a single hero).
4. **Play** (PIE).

**Pass check:** the Skulkin unit shows **24** placeholder minifigs standing in a rectangular block;
Kai shows **one**. Torso/arms are the primary colour, hips/hat the secondary, head is flesh.

- If parts render grey → the material didn't build; hand-make it (see EDITOR_STEPS M4 fallback).
- If proportions look wrong → tweak `BuildPartLayout()` in `NinjagoModelRenderer.cpp` (presentation
  only, no gameplay impact).

---

## M5 — Player control

Use the same level (units placed). **Play**.

Run through each control and tick it off:

- [ ] **WASD** pans the camera; **mouse wheel** zooms; the camera never rotates.
- [ ] **LMB** on a unit selects it — a **yellow ring** appears under it.
- [ ] **RMB on empty ground** moves the selected unit there; it **walks in formation** (block stays
      together), not teleporting.
- [ ] **RMB on an enemy unit** issues attack-approach (it advances and stops at engage range).
- [ ] **Esc** deselects (ring disappears).

**Pass check:** select a unit, send it across the field, watch it walk in formation.

- Esc also stops PIE. To test Esc-deselect specifically, launch **New Editor Window (PIE)** with
  mouse control, or run **Standalone Game**.
- If nothing responds to input: confirm no `InputComponent is not an EnhancedInputComponent` error
  in the log.

---

## M6 — Combat and win condition

Two ways to run a full battle:

**A. Auto-battle (simplest):** open an **empty** level (floor + PlayerStart, no units) and Play.
The game mode spawns the default v0.1 battle (4 Ninja heroes vs the 4 Skulkin units).

**B. Hand-placed:** put units of both teams in the level and Play; the game mode fights whatever
is present.

(Optional: **Tools → Execute Python Script → `Tools/make_battle_setup.py`** to create an editable
`DA_DefaultBattle`.)

**Expected:** the two armies advance to contact, fight (models drop over time), and a message
appears on screen: **"Ninja win"** or **"Skulkin win"**.

**Pass check:** the battle reaches a conclusion and a winner is announced. Watch the Output Log for
`Battle start: N units …` and `Battle resolved: …`.

- Skulkin models do **not** revive and there is no morale/routing — that is correct for v0.1.
- If nothing spawns in an empty level → dt_units missing; re-run M2.

---

## M7 — Lord ability (Spacebar)

In a running battle (M6), **LMB-select** an ability-bearer:

- **Kai** (`HRO_KAI`) → **Fire Blast**: instant 45 damage to enemies within 700 cm, 30 s cooldown.
- **Samukai** (`LRD_SAMUKAI`) → **Four-Armed Fury**: 40 damage × 4/s for 6 s within 400 cm,
  45 s cooldown.

Press **Space**.

**Pass check:**
- [ ] Enemies inside the radius take visible damage (models drop).
- [ ] A **cooldown bar** above the unit drains then refills to green.
- [ ] Pressing Space again while the bar is filling does nothing (cooldown respected).
- [ ] The Output Log shows `… fired Fire Blast (radius 700, dmg 45, cd 30s)` (or Four-Armed Fury).

---

## Full smoke test (end to end)

1. Empty level → Play → auto-battle starts.
2. Pan/zoom around the fight.
3. Select Kai, RMB an enemy to send him in, Space to Fire Blast.
4. Let the battle run to a winner; confirm the result message.

If all seven milestones and this smoke test pass, **v0.1 is verified**.

---

## v0.2 — Ranged combat (branch `v0.2`)

Ranged is additive: the default battle now includes an archer per side (`NIN_SHINTARO` for Ninja,
`SKU_ENGINEERS` for Skulkin), spawned a rank behind the melee line.

### RM-tests — new automation tests

Two ranged tests join the M3 suite (8 total now). Run the same command as M3 and confirm:

```
Ninjago.Combat.RangedHitChanceClamp .......... Passed
Ninjago.Combat.RangedResolveHitAndMiss ....... Passed
```

### Ranged battle (visual)

1. Build; ensure `dt_units` is imported and `make_placeholder_meshes.py` has run.
2. Play an **empty** level (the default battle now has archers), or run
   `Tools/make_battle_setup.py` and use `DA_DefaultBattle`.

**Pass checks:**

- [ ] The two armies each have a unit that **starts behind** the others.
- [ ] As the lines close, archers **fire from a distance** — you see **tracer bolts** arc between
      the ranks (volleys on the 2s combat cadence).
- [ ] Archers **hold at range** while shooting, then **charge into melee** once ammo runs out
      (each archer model has a fixed number of shots).
- [ ] Enemies struck by arrows take damage (models drop before the lines even meet).
- [ ] The battle still resolves to a winner.

Notes:
- Ranged accuracy comes from the `RangedAttack` stat; damage reuses the melee floor + AP model.
- Tracers are cosmetic (damage applies instantly), so a missed shot still shows a bolt.
- Stand-off distance = `RangeCm * RangedStandoffFraction` (Project Settings → Ninjago → Combat|Ranged).

---

## v0.3 — Morale and routing (branch `v0.3`)

Units now have morale (the row `Morale`; **exactly 99 means immune**). The default battle gains a
breakable Ninja block (`NIN_SOLDIERS`, morale 66); Skulkin are all immune and never rout.

### MO-tests — new automation tests

Four morale tests join the suite (12 total now). Run the M3 command and confirm:

```
Ninjago.Morale.CasualtiesLowerMorale ......... Passed
Ninjago.Morale.UnderStrengthPenalty .......... Passed
Ninjago.Morale.RecoversWhenSafe .............. Passed
Ninjago.Morale.BreakAndRallyHysteresis ....... Passed
```

### Routing battle (visual)

1. Build; ensure `dt_units` is imported and placeholders exist.
2. Play an **empty** level (the default battle now includes the breakable block).

**Pass checks:**

- [ ] As the Ninja soldier block takes casualties, at some point it **breaks and flees** away from
      the enemy (a **red flag** appears above a routing unit).
- [ ] A routing unit **stops fighting** while it runs.
- [ ] Skulkin units **never rout** no matter how many they lose (immune by faction).
- [ ] If a routed unit escapes and survives, it may **rally** (red flag disappears) and re-engage.
- [ ] The battle still resolves to a winner.

Tuning lives in Project Settings → Ninjago → Morale (casualty shock, break/rally fractions, rout
speed). A unit whose row `Morale` is exactly 99 is immune; 100+ are high-but-breakable.

---

## v0.4 — Charge impact (branch `v0.4`)

Every unit has a `ChargeBonus`. On its **first melee contact** after advancing, a unit hits harder
and delivers a **morale shock** to whatever it struck (charger's `ChargeBonus` ×
`MoraleChargeShockScale`). Cavalry/monsters/Lords have big charge values; archers almost none.

### CH-tests — new automation test

One charge test joins the suite (13 total now). Run the M3 command and confirm:

```
Ninjago.Combat.ChargeAddsBonusDamage ......... Passed
```

### Charge battle (visual)

Play the default battle (empty level). Watch the first moment the lines meet.

**Pass checks:**

- [ ] The **first hit** as units make contact is bigger than the steady melee that follows
      (a spike of casualties on impact).
- [ ] A charge into the breakable Ninja block (e.g. Samukai, ChargeBonus 12) makes it **break
      sooner** than it would from attrition alone — the charge shock plus casualties tips it.
- [ ] Immune units (Skulkin) still take charge **damage** but ignore the morale **shock**.
- [ ] A unit that disengages and re-advances can **charge again** on the next contact (not a
      one-time-per-battle thing).

Tuning: `MoraleChargeShockScale` in Project Settings → Ninjago → Morale. Charge damage is the
row's `ChargeBonus` added to base damage through the usual floor/armour model.

---

## v0.5 — Spears and anti-large (branch `v0.5`)

Spear units (`SPEAR` archetype: `SKU_WATCHMEN`, `NIN_SOLDIERS`) counter large units (cavalry,
monsters, giants, vehicles, classified by `ModelScale >= 1.4`). The default battle adds the Ninja
monster `NIN_SAMURAIX` so the Skulkin spears have something to brace against.

### SP-tests — new automation test

One spear test joins the suite (14 total now). Run the M3 command and confirm:

```
Ninjago.Combat.SpearsBraceAndAntiLarge ....... Passed
```

### Spear battle (visual)

Play the default battle (empty level). Watch the Ninja monster (a much larger minifig) hit the line.

**Pass checks:**

- [ ] When the large monster charges the Skulkin **spear** block (Watchmen), it does **not** get
      its big charge spike — the spears brace (compare to it charging a non-spear block).
- [ ] The spear block **out-damages** the monster relative to how a normal block would (anti-large
      bonus), so a lone monster into braced spears is a bad trade.
- [ ] Non-spear units are still hammered by the charge as before.

Tuning: `LargeModelScaleThreshold` and `SpearAntiLargeBonus` in Project Settings → Ninjago →
Combat|Melee.

---

## v0.6 — Ability variety (branch `v0.6`)

Abilities now dispatch on their `Magnitude` verb and `Target`, so more of the roster works. The
four ninja heroes each have a distinct Space power:

| Hero | Ability | Effect |
|---|---|---|
| Kai | Fire Blast | instant AoE damage (700 cm) |
| Cole | Earthquake | instant AoE damage around Cole (800 cm) |
| Jay | Lightning Bolt | chains damage to the nearest enemies (up to 6, 1400 cm) |
| Zane | Ice Wall | terrain effect — **not implemented yet** (logs a warning) |

Heal powers (`heal=` on ally-targeted abilities) also work now, restoring HP to nearby friendly
models, though no default-battle unit uses one.

### AB-tests — new automation tests

Two ability tests join the suite (16 total now). Run the M3 command and confirm:

```
Ninjago.Ability.ParseMagnitude ............... Passed
Ninjago.Ability.SelectNearest ................ Passed
```

### Ability battle (visual)

Play the default battle. Select each ninja hero (LMB) and press Space:

- [ ] **Kai / Cole**: a burst of damage drops nearby enemies.
- [ ] **Jay**: damage lands on a handful of the **nearest** enemies (the chain), out to a long range.
- [ ] **Zane**: nothing happens yet; the Output Log shows "ability effect ... not yet implemented".
- [ ] All respect their cooldowns (the bar over the selected hero).

Chain size: `AbilityChainMaxTargets` in Project Settings → Ninjago → Abilities.

---

## v0.7 — Buffs and debuffs (branch `v0.7`)

Units now carry timed stat modifiers, and buff/debuff abilities apply them. Effective attack,
defence, damage, and speed all flow through the modifier stack into combat and movement. The
default battle adds Skulkin `HRO_WYPLASH`, whose ability grants a `def=+3` buff to nearby allies.

Supported ability verbs: `atk` / `def` (flat), `speed` / `slow` / `atkspeed` (percent), and
`buff` (compound attack + defence). Duration comes from the ability, else
`BuffDefaultDurationS`.

### BU-tests — new automation tests

Two modifier tests join the suite (18 total now). Run the M3 command and confirm:

```
Ninjago.Modifiers.FlatAndPercent ............. Passed
Ninjago.Modifiers.Expiry ..................... Passed
```

### Buff battle (visual)

Play the default battle. Select the Skulkin buffer (`HRO_WYPLASH`) and press Space.

**Pass checks:**

- [ ] Nearby Skulkin allies become **harder to kill** for a while (their defence is buffed), then
      the effect wears off.
- [ ] A speed buff visibly makes a unit **move faster** (and a slow debuff, slower) for its duration.
- [ ] Buffs stack with everything else — a buffed line holds a charge better; a slowed enemy
      reaches you later.

Tuning: `BuffDefaultDurationS` in Project Settings → Ninjago → Abilities.

---

## v0.8 — Skulkin "Already Dead" revive (branch `v0.8`)

The Skulkin faction mechanic: a slain Skulkin model collapses into a bone pile and **reassembles
after 20 s at 50% HP, once per battle**. Combined with their morale immunity, Skulkin are a
relentless horde; the Ninja heroes have to actually finish them off (twice).

No new automation tests (this is stateful integration, verified by trace). Suite stays at 18.

### Revive battle (visual)

Play the default battle (it already fields Skulkin) and watch a Skulkin block after it takes losses.

**Pass checks:**

- [ ] Skulkin casualties **come back** about 20 seconds later (models reappear where they fell),
      at roughly half health.
- [ ] Each Skulkin model revives **at most once** — the second time it dies, it stays dead.
- [ ] The battle is **not** declared over while a Skulkin unit still has models reassembling
      (a unit at zero living but pending revives is not yet beaten).
- [ ] Ninja units do **not** revive (no faction mechanic).
- [ ] The battle still reaches a winner (total model-lives are finite: at most twice per Skulkin).

Tuning: `ReviveDelaySeconds` and `ReviveHpFraction` in Project Settings → Ninjago → Revive.

---

## v0.9 — Crowd control: freeze (branch `v0.9`)

Freeze abilities (`freeze=Ns`) now stun the target: a frozen unit cannot move, fight, or use its
ability for the duration. The default battle adds a **guest** freeze-caster, `LRD_ICEEMPEROR`, on
the Ninja side (thematically a crossover, functionally a demo of freeze). Mind-control (`control=`)
remains unimplemented.

The freeze duration parse (`freeze=6s` → 6) is covered by the ability parser test; the stun state
itself is stateful integration, verified by trace.

### Freeze battle (visual)

Play the default battle. Select `LRD_ICEEMPEROR` (LMB) and press Space near the Skulkin.

**Pass checks:**

- [ ] Skulkin units in range **freeze** — a **cyan sphere** appears above them and they stop moving
      and fighting for ~6 seconds, then resume.
- [ ] Frozen units can still be attacked (they are sitting ducks).
- [ ] A frozen unit cannot fire its own ability while frozen (select one and try — nothing happens).
- [ ] Freeze is a great counter to the Skulkin horde: it buys time against the relentless revive.

Tuning: freeze length comes from the ability's magnitude (`freeze=6s`).

---

## v0.10 - Mind control (branch `v0.10`)

Control abilities (`control=`) convert an enemy unit to fight for the caster's team. Duration comes
from the ability; a duration of 0 (e.g. Helmet Command) is **permanent**. `ENEMY_AOE` converts every
enemy in radius; `ENEMY_UNIT` / `ENEMY_HERO` takes the nearest one. The demo adds a **guest**
hypnotist, `LRD_SKALES`, on the Ninja side.

Because the whole game keys off a unit's team, a converted unit immediately turns on its former
allies. (Partial control is simplified to a full convert for the duration.)

Stateful integration, verified by trace; suite stays at 18.

### Mind-control battle (visual)

Play the default battle. Select `LRD_SKALES` (LMB) and press Space near a Skulkin unit.

**Pass checks:**

- [ ] A Skulkin unit in range turns and **fights for your side** (a **magenta sphere** marks it).
- [ ] It attacks its former Skulkin allies while controlled.
- [ ] After the duration it **reverts** to Skulkin (marker clears) and fights you again.
- [ ] Permanent control (a `dur=0` ability) never reverts. Controlling an enemy also shifts the win
      count: convert enough and their side runs out.

---

## v0.11 - AI ability usage (branch `v0.11`)

Units now cast their own abilities during the battle, so both armies use their full kit without the
player. Each combat tick, any unit whose ability is off cooldown and has a worthwhile target fires
it: offensive abilities (damage, freeze, control, slow) when an enemy is within reach; support
abilities (heal, buffs) once the battle is joined. Routing and stunned units do not cast.

The player's Spacebar still works and simply lets you pre-empt with better timing.

Stateful integration, verified by trace; suite stays at 18.

### AI ability battle (visual)

Play the default battle and just watch (no need to select anything).

**Pass checks:**

- [ ] As the lines meet, abilities go off on **both sides** on their own: Fire Blasts, chained
      lightning, freezes (cyan markers), buffs, mind-control (magenta markers), Samukai's Fury.
- [ ] Abilities respect cooldowns (they do not spam - each fires roughly once per its cooldown).
- [ ] You can still select a hero and press Space to fire early yourself.
- [ ] Casters do not fire at spawn before anyone is in range, nor while routing/frozen.

---

## v0.12 - Battle flow: restart and replay (branch `v0.12`)

The game loop closes: when a battle ends it freezes under the result banner, and you can fight
again. Press **R** at any time to restart the battle, and it **auto-restarts** a few seconds after
a result so battles loop for a watching child.

Restart reloads the level, which re-runs the spawn and gives a fresh battle. Pure C++, no assets.

### Restart battle (visual)

Play the default battle.

**Pass checks:**

- [ ] Press **R** mid-battle: the battle restarts from the beginning.
- [ ] Let a battle finish: the result shows ("... press R to fight again"), units freeze, and after
      a few seconds the battle **auto-restarts**.
- [ ] Pressing R after a result restarts immediately (before the auto-restart fires).

Tuning: `AutoRestartSeconds` in Project Settings -> Ninjago -> Battle (0 disables auto-restart).

---

## v0.13 - Battle readability (branch `v0.13`)

Each unit shows a health bar (green when full, red when nearly dead) reflecting its remaining
strength (living models' HP over the unit maximum), and the screen shows a running Ninja vs Skulkin
model tally. Both are debug-draw, no assets.

### Readability battle (visual)

Play the default battle.

**Pass checks:**

- [ ] A **health bar** floats over each unit and shrinks / reddens as the unit takes losses.
- [ ] The on-screen **tally** ("Ninja: N / Skulkin: M") updates as models fall, so you can see which
      side is winning at a glance.
- [ ] The tally counts a mind-controlled unit for its true side (it uses allegiance), and counts a
      reassembling Skulkin unit while it is down.

---

## v0.14 - Preset battle scenarios (branch `v0.14`)

Number keys pick a curated matchup (in the empty-level auto-battle). The chosen scenario persists
across restarts within the session. Pure C++, no menu.

| Key | Scenario | What |
|---|---|---|
| 1 | Heroes vs Horde | 4 ninja heroes vs a swarm of Skulkin |
| 2 | Giant Brawl | monsters and giants smashing (guest big units) |
| 3 | Skirmish (ranged) | archers and skirmishers trading fire |
| 4 (or default) | Grand Battle | the full mixed roster with every mechanic |

### Scenarios (visual)

Play an empty level.

**Pass checks:**

- [ ] The on-screen header shows the scenario name and "keys 1-4 to switch battles, R to restart".
- [ ] Press **2**: the battle reloads as a clash of giant units.
- [ ] Press **1**: a few heroes face a Skulkin horde; **3**: a ranged skirmish.
- [ ] The choice sticks across R restarts until you press another number.

Note: scenarios drive the code-default (empty-level) battle. A level with hand-placed units or an
assigned Battle Setup uses those instead.

---

## v0.15 - Resurrection (branch `v0.15`)

Reform abilities (`revive=N%`) rebuild a **destroyed** friendly unit: every model comes back at N%
HP with fresh morale and ammo, snapped back into formation. No new unit is spawned (the destroyed
unit is already tracked), so it is safe and simple. The demo adds a guest resurrector, `HRO_MACHIA`,
on the Skulkin side.

Stateful integration, verified by trace; suite stays at 18.

### Resurrection battle (visual)

Play the default battle and let a Skulkin unit be wiped out.

**Pass checks:**

- [ ] After a Skulkin regiment is destroyed, it can **reform** whole (at ~60% HP) and rejoin the
      fight (the resurrector `HRO_MACHIA` casts it, or select him and press Space).
- [ ] The reformed unit fights normally again (fresh morale, ammo, formation).
- [ ] The AI only casts it when there is actually a destroyed ally to rebuild.
- [ ] It cannot un-lose a beaten battle: the caster must be alive to cast, and a living caster means
      that side was not beaten.

---

## v0.16 - Battle MVP and kill stats (branch `v0.16`)

Each unit tracks how many enemy models it kills (melee, ranged, and abilities), and the result
banner crowns the battle MVP: "Ninja win  -  MVP: Kai (37 kills)". Pure counters, no assets.

### MVP (visual)

Play the default battle to a conclusion.

**Pass checks:**

- [ ] The result banner names an **MVP** and a kill count.
- [ ] The MVP is a unit that did a lot of work (a hero or a big block usually); a Lord ability that
      wipes a crowd (e.g. Samukai's Fury) credits its caster.
- [ ] Kills come from melee, ranged, and ability damage alike.

---

## v0.17 - Vanish (stealth strike) (branch `v0.17`)

Vanish (`crit=xN`) makes a unit invisible for its duration and turns the next attack into a
critical of that multiplier (x3). While cloaked the unit cannot be targeted or sought by enemies;
its models are hidden and a faint shimmer shows the player where it is. The strike reveals it. The
demo adds a Ninja with Vanish, `HRO_RONIN`.

The `crit=x3` multiplier parse is covered by the ability parser test; the stealth state is stateful
integration, verified by trace.

### Vanish battle (visual)

Play the default battle. Select `HRO_RONIN` (LMB) and press Space near the enemy.

**Pass checks:**

- [ ] Ronin **vanishes**: his minifig disappears, replaced by a **shimmer** sphere.
- [ ] Enemies stop targeting/advancing on him while cloaked (they cannot see him).
- [ ] His next hit is a **guaranteed critical** (a big damage spike) and he **reappears**.
- [ ] If ~10s pass without attacking, he reappears anyway (the crit is lost).
- [ ] While cloaked, enemy **abilities** (Fire Blast, freeze, hypnosis, etc.) cannot hit him either,
      and the enemy AI does not react to him.

---

## Troubleshooting quick reference

| Symptom | Likely cause | Fix |
|---|---|---|
| Version mismatch opening project | `EngineAssociation "5.7"` | Right-click uproject → Switch UE version → 5.7.4 |
| Compile error on `*.Target.cs` enums | 5.7 renamed BuildSettings/IncludeOrder enum | Send exact message; one-line fix |
| Test compile error on `EAutomationTestFlags` | 5.7 renamed enumerators | Send name; fix `Tests\*.cpp` |
| `Tier` import warning | enum-by-name CSV import | Switch `Tier` to `FName` |
| Minifig parts grey | master material missing/failed | Re-run `make_placeholder_meshes.py` or hand-build (EDITOR_STEPS M4) |
| Input does nothing | Enhanced Input defaults | Check `DefaultInput.ini`; look for the EnhancedInputComponent error |
| Esc closes PIE instead of deselecting | PIE reserves Esc | Use New-Editor-Window PIE or Standalone |
| Empty level spawns nothing | dt_units not imported | Re-run `import_datatables.py` |

Report any failure with the exact log lines and I'll fix the source.
