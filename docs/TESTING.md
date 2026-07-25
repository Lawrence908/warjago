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
