# Kickoff prompt — paste this into Claude Code

> Put `CLAUDE.md`, `build.py`, and the four `dt_*.csv` files in the repo root first.
> Then paste everything below the line as your first message.

---

## Task

Scaffold an Unreal Engine C++ project for a Total War-style LEGO Ninjago army battler, and
implement **v0.1 only** as defined in `CLAUDE.md`.

Read `CLAUDE.md` fully before writing anything. It contains the architecture, the rendering
approach, the stat semantics, and the scope boundary. Do not exceed that scope.

## Before you write code

Ask me these, then wait:

1. Unreal Engine version and engine install path.
2. Windows or Linux for the editor.
3. Project name — default `Ninjago`, module name `Ninjago`.
4. Anything in `CLAUDE.md` you think is wrong, contradictory, or under-specified.

Do not guess the engine version. The API surface differs enough to matter.

## Constraints

- v0.1 scope only. Two factions, eight units, flat plane. No morale, no ranged, no terrain, no
  campaign, no multi-select.
- No skeletal meshes. HISM-based rigid part rendering, per `CLAUDE.md`.
- All stats from `Content/Data/*.csv` via DataTables. No stat literals outside `UNinjagoSettings`.
- You cannot author `.uasset` files. Script asset creation with Unreal Python under `Tools/`;
  anything unscriptable goes in `docs/EDITOR_STEPS.md` as a numbered checklist.
- Never hand-edit the generated CSVs. `build.py` is the source of truth.

## Ordered process

Work milestone by milestone. **Stop after each and report** — do not chain them.

### M1 — Project skeleton
- `.uproject`, `Ninjago.Build.cs`, `Ninjago.Target.cs`, `NinjagoEditor.Target.cs`, module bootstrap
- `.gitignore` for Unreal (`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`)
- `Config/DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini`
- `LogNinjago` log category
- Copy the four CSVs into `Content/Data/`

**Accept when:** project generates project files and compiles clean with an empty module.

### M2 — Data layer
- The four row structs, exactly matching the CSV headers
- `UNinjagoSettings : UDeveloperSettings` holding: `CombatTickSeconds` (2.0), `EngageRangeCm`
  (150), `ModelSpacingCm` (90), `HitChanceBase` (0.35), `HitChancePerPoint` (0.03),
  `HitChanceMin` (0.10), `HitChanceMax` (0.90), `MinDamage` (1)
- `Tools/import_datatables.py` — creates/refreshes the four DataTable assets from the CSVs,
  idempotent, safe to re-run

**Accept when:** running the Python script in-editor produces four populated DataTables and
`dt_units` has 209 rows. Report the row counts back to me.

### M3 — Combat resolver + formation, with tests
- `NinjagoCombatResolver` — pure static, no engine deps beyond core types. Implements the hit
  chance and damage formula in `CLAUDE.md`, including the `max(1, ...)` damage floor.
- `NinjagoFormation::SlotOffset(index, count, spacing)` — pure static, rows/columns block
- `Tests/CombatResolverSpec.cpp` and `Tests/FormationSpec.cpp`

Cover at minimum: hit chance clamps at both ends; the damage floor (Armour 5 vs Damage 14 / AP 0
must deal 1, not 0); armour-piercing bypasses armour; `SlotOffset` for count 1, 24, and 32 is
symmetric about the origin and never overlaps.

**Accept when:** tests pass headless. Paste the test output.

### M4 — Unit actor and rendering
- `ANinjagoUnit` with `TArray<FNinjagoModel>`, cached row, team, state, current order
- `UNinjagoModelRenderer` owning six HISM components, updating via
  `BatchUpdateInstancesTransforms` once per unit per frame
- `PerInstanceCustomData` carrying the faction colour index; a placeholder master material
- `Tools/make_placeholder_meshes.py` — builds the primitive minifig parts
- Model steering toward formation slots, straight-line, with simple separation
- Dead models hide their instances

**Accept when:** I can place a `ANinjagoUnit` in a level, set a row key, hit Play, and see the
right number of placeholder minifigs standing in formation in faction colours.

### M5 — Player control
- `ANinjagoCameraPawn` — WASD pan, wheel zoom, no rotation, clamped to map bounds
- `ANinjagoPlayerController` — LMB single-select with a selection decal, RMB order (trace: enemy
  unit → attack, else → move), Esc deselect
- Enhanced Input assets created via Python if scriptable, else documented in `EDITOR_STEPS.md`

**Accept when:** I can select a unit, send it across the field, and watch it walk in formation.

### M6 — Combat and win condition
- Units transition Idle → Moving → Fighting on contact
- 2.0s combat timer pairs engaged models with nearest living enemy within `EngageRangeCm`
- `ANinjagoGameMode` spawns both armies from a `UNinjagoBattleSetup` data asset and polls the
  win condition once per second
- Minimal on-screen result: "Ninja win" / "Skulkin win"

**Accept when:** the eight v0.1 units fight to a conclusion and a winner is declared.

### M7 — Lord ability, one button
- Spacebar fires the selected unit's `Ability` row if it has one and is off cooldown
- Implement exactly two: `AB_FOUR_ARMED_FURY` (Samukai) and `AB_FIRE_BLAST` (Kai)
- Cooldown UI: a simple radial or bar on the selected unit

**Accept when:** both abilities fire, respect cooldown, and visibly damage enemies in radius.

## Output format

For each milestone, report in this shape:

```
## M<n> — <name>

Files added/changed:
  <path>  — <one line>

Verification run:
  <the actual command and its actual output, or "not run: <reason>">

Editor steps needed from me:
  <numbered, or "none">

Not done / deferred:
  <anything cut, and why>
```

## Verification

Before reporting any milestone done:

1. Compile clean, zero warnings in `Source/Ninjago`.
2. Run the automation tests where they exist and paste real output.
3. Re-read that milestone's acceptance criteria and confirm each line explicitly.
4. Re-read `CLAUDE.md` and check for anything you contradicted, omitted, or silently expanded.

If you could not run something, say so plainly. Do not infer success from code that looks correct.
