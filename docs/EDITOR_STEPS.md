# Editor steps

Anything that cannot be done from C++/Python source and must be performed by hand in the
Unreal Editor, or run from a machine with the engine installed. Numbered, milestone by milestone.
Claude Code cannot author `.uasset`/`.umap` binaries or run the Windows toolchain, so these are
the handoff points.

> Environment: the project source is authored on the Linux host (daedalus). The engine
> (UE 5.7.4) and all compilation/editor work happen on your **Windows** machine. Sync the repo
> to Windows before running any step below.

---

## M1 — Project skeleton

1. On the Windows machine, right-click `Ninjago.uproject` → **Generate Visual Studio project
   files**. (If the "Generate…" entry is missing, run
   `"<UE_5.7>\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles -project="<repo>\Ninjago.uproject" -game -engine`.)
2. If prompted that modules are out of date / missing, choose **Yes** to rebuild — this compiles
   the empty `Ninjago` module.
3. Confirm the project opens in the editor with no missing-module errors.
   - Alternatively, build headless first:
     `"<UE_5.7>\Engine\Build\BatchFiles\Build.bat" NinjagoEditor Win64 Development -project="<repo>\Ninjago.uproject" -waitmutex`
4. **Acceptance check for M1:** project files generate and the empty module compiles clean
   (zero warnings in `Source/Ninjago`). Report the Build.bat output back.

> Note: `EngineAssociation` in `Ninjago.uproject` is set to `"5.7"`. If Windows reports a
> version mismatch, right-click the `.uproject` → **Switch Unreal Engine version** and pick the
> installed 5.7.4 build (this stamps the correct association / GUID for a source build).

---

## M2 — Data layer

Prerequisite: M1 compiled (the `Ninjago` module built, so the row structs exist as
`/Script/Ninjago.NinjagoUnitRow` etc.).

1. Open the project in the editor.
2. **Tools → Execute Python Script** → select `Tools/import_datatables.py`.
3. Watch the **Output Log**. Expect a summary block:
   ```
   ==================== Ninjago DataTable import ====================
     dt_units        209 / 209   OK
     dt_factions      17 / 17    OK
     dt_archetypes    16 / 16    OK
     dt_abilities     44 / 44    OK
   =================================================================
   All four DataTables imported; row counts match. M2 import OK.
   ```
   Four assets appear under `Content/Data/` (`/Game/Data/dt_*`). Re-running is safe —
   each table is emptied and refilled in place.
4. **Acceptance for M2:** four populated DataTables, `dt_units` = 209 rows. Report the counts back.

Watch-items (report if the log shows any):
- **Enum column `Tier`** (TROOP/CMD/ELITE/LORD) — if import warns it can't match an enum
  value, that's the one field with import risk; tell me and I'll switch `Tier` to `FName`.
- **Hex colours** import as plain `#RRGGBB` strings (converted to `FColor` at spawn in M4) —
  they should NOT be parsed as `FColor` here.
- **Mesh columns** are empty and import as null soft-pointers — expected.
- Verify Project Settings → **Ninjago** shows the eight tuning constants (CombatTickSeconds = 2.0,
  etc.); they serialise to `Config/DefaultGame.ini`.

## M3 — Combat resolver + formation

Pure C++ with headless automation tests. No editor asset work — just build and run tests.

1. Build the editor target (picks up the new `.cpp` files):
   `"<UE_5.7>\Engine\Build\BatchFiles\Build.bat" NinjagoEditor Win64 Development -project="<repo>\Ninjago.uproject" -waitmutex`
2. Run the tests headless and capture output:
   ```
   "<UE_5.7>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<repo>\Ninjago.uproject" ^
     -ExecCmds="Automation RunTests Ninjago; Quit" -unattended -nop4 -nosplash ^
     -testexit="Automation Test Queue Empty" -log -ReportOutputPath="<repo>\Saved\TestReport"
   ```
3. **Acceptance for M3:** all `Ninjago.Combat.*` and `Ninjago.Formation.*` tests pass. Paste the
   log tail (the `LogAutomationController` "... Test Completed. Result={Passed}" lines).

Expected tests (6 total):
- `Ninjago.Combat.HitChanceClamp`, `Ninjago.Combat.DamageFloor`,
  `Ninjago.Combat.ArmourPiercing`, `Ninjago.Combat.ResolveGuaranteedHitAndMiss`
- `Ninjago.Formation.SingleEntityAtOrigin`, `Ninjago.Formation.BlockSymmetricNoOverlap`

Watch-item: the tests use `EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter`.
If 5.7 renamed those enumerators and the build errors on them, tell me the exact name it expects
and I'll adjust the two spec files.

## M4 — Unit actor and rendering

1. Build the editor target (picks up the new Units/ code).
2. **Tools → Execute Python Script → `Tools/make_placeholder_meshes.py`.** Expect the log:
   ```
   ==================== Ninjago placeholder assets ====================
     meshes created this run: 6 (of 6)
     master material: built
     location: /Game/Placeholders
   ====================================================================
   ```
   Creates `/Game/Placeholders/SM_Minifig_*` (6) and `M_NinjagoMinifig`. Re-running is safe
   (existing assets are skipped; delete them to force a rebuild).
3. Make sure the DataTables exist (run M2's `import_datatables.py` if not).
4. Create/open a level with a floor and a PlayerStart. Drag a **Ninjago Unit** actor in.
5. In its Details:
   - **Unit Row Handle** → Data Table = `dt_units`, Row Name = e.g. `SKU_WARRIORS` (24 models)
     or `HRO_KAI` (single hero).
   - **Team** → Ninja or Skulkin.
6. **Play.** Acceptance: the right number of placeholder minifigs stand in a block (single
   entity for `UnitSize == 1`), in the row's faction colours (torso/arms = primary,
   hips/hat = secondary, head = flesh).

Watch-items:
- **Master material via Python** is the fragile part on 5.7. If the log warns the material
  wasn't found, or parts render grey, build it by hand instead (below) and re-Play.
- Placeholder proportions are rough by design; tweak `BuildPartLayout()` in
  `NinjagoModelRenderer.cpp` if the figure looks off. These are presentation constants, not stats.

### Manual fallback — build `M_NinjagoMinifig` by hand (only if the Python step fails)
1. Content Browser → `/Game/Placeholders` → **Add → Material**, name it `M_NinjagoMinifig`.
2. Open it. Add three **PerInstanceCustomData** nodes; set their Data Index to 0, 1, 2.
3. Add two **AppendVector** nodes. Wire: CustomData0→Append1.A, CustomData1→Append1.B,
   Append1→Append2.A, CustomData2→Append2.B.
4. Wire Append2 → **Base Color**. Save.
   (Result: instance RGB from PerInstanceCustomData drives Base Color.)

## M5 — Player control

Good news: **no input assets to author.** The Enhanced Input actions and mapping context are
built in C++ at runtime, and the game mode is wired as the global default in `DefaultEngine.ini`.

1. Build the editor target.
2. Open your test level (needs a floor with collision and a PlayerStart). Place a couple of
   `Ninjago Unit` actors (e.g. one `HRO_KAI` team Ninja, one `SKU_WARRIORS` team Skulkin).
3. **Play.** You should get the battle camera automatically (no manual GameMode override needed).
   - **WASD** pans, **mouse wheel** zooms, no rotation.
   - **LMB** on a unit selects it (a yellow ground ring appears under it).
   - **RMB** on empty ground moves the selected unit there (walks in formation); **RMB on an
     enemy unit** issues an attack-approach (it stops at engage range — actual fighting is M6).
   - **Esc** deselects.
4. **Acceptance:** select a unit, send it across the field, watch it walk in formation.

Notes / watch-items:
- **Esc in PIE** also stops Play-in-Editor. To test Esc-deselect specifically, use **New Editor
  Window (PIE)** with mouse control, or run Standalone. (Kept Esc per the control spec.)
- Selection highlight is a **debug-draw ring**, a deliberate stand-in for a decal (avoids a fragile
  decal-material asset). Swappable for a real decal later if you want.
- Floor must block the **Visibility** channel (default floors do) so move-orders can trace ground.
- If input does nothing, confirm `DefaultInput.ini` set the Enhanced Input default classes (M1) and
  the log has no "InputComponent is not an EnhancedInputComponent" error.

## M6 — Combat and win condition

1. Build the editor target.
2. (Optional) **Tools → Execute Python Script → `Tools/make_battle_setup.py`** to create
   `/Game/Data/DA_DefaultBattle` (editable rosters). Skippable — the game mode has a code default.
3. Two ways to run a battle:
   - **Empty level:** just Play. With no units placed and no BattleSetup assigned, the game mode
     spawns the default v0.1 battle (4 Ninja heroes vs the 4 Skulkin units) and they fight.
   - **Hand-placed:** drop `Ninjago Unit` actors of both teams in the level and Play; the game
     mode gathers whatever is there and runs combat on them (no auto-spawn when units exist).
   - To use a custom roster, put a `NinjagoGameMode` (Blueprint child) in World Settings and set
     its **Battle Setup** to your data asset — or edit `DA_DefaultBattle`.
4. **Acceptance:** the units advance to contact, fight on the 2.0s timer, and a winner is
   announced on screen ("Ninja win" / "Skulkin win").

How it behaves:
- Combat resolves every `CombatTickSeconds` (2.0). Each living model attacks the nearest living
  enemy model within `EngageRangeCm` (150) using the M3 resolver.
- Idle, un-ordered units advance to the nearest enemy automatically, so the battle always closes.
  Player orders (M5) override until they complete.
- Win is polled once per second; the result shows via an on-screen message (a minimal stand-in
  for a proper end screen).

Watch-items:
- Skulkin's "Already Dead" revive and morale are **not** in v0.1 (no morale system yet), so dead
  stays dead — expected.
- If nothing spawns in an empty level, confirm `import_datatables.py` ran (dt_units must exist).

## M7 — Lord ability

No new assets — Spacebar is wired via the same transient Enhanced Input, and abilities read from
the existing `dt_abilities` DataTable.

1. Build the editor target. Ensure `dt_abilities` exists (run `import_datatables.py` if not).
2. Play a battle. **Select** (LMB) an ability-bearer:
   - **Kai** (`HRO_KAI`) → **Fire Blast**: Space deals 45 to enemies within 700 cm, 30 s cooldown.
   - **Samukai** (`LRD_SAMUKAI`) → **Four-Armed Fury**: Space channels 40 dmg × 4/s for 6 s within
     400 cm, 45 s cooldown.
3. Press **Space**. A cooldown bar above the unit fills back to green as it recharges; pressing
   Space while on cooldown does nothing.
4. **Acceptance:** both abilities fire, respect cooldown, and visibly damage enemies in radius.

Notes:
- No aiming (one-button rule): both v0.1 abilities are centred on the Lord.
- Only these two abilities are implemented, per scope. Other units' `Ability` keys are ignored
  until their rows are implemented.
- Channel rate (4/s) is `UNinjagoSettings.AbilityChannelTicksPerSecond`; radius/duration/cooldown
  and the damage magnitude come from the ability row.
