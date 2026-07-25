# Ninjago: Total War

A Total War-style real-time army battler with LEGO Ninjago factions. Left-click a unit,
right-click to move or attack, watch blocky minifig regiments march across a field and fight.

This is a personal project built for a seven-year-old. It is not commercial, not shipping, and has
no licensing intent. Every design decision resolves in favour of one question: **can a seven-year-old
hold this in his head?** If a feature is clever but needs explaining, it is wrong.

> **Status:** v0.1 is fully authored in C++ (milestones M1 through M7). The code was written on a
> Linux host with no engine installed, so it has **not yet been compiled or play-tested**. The first
> Windows build is the real acceptance gate. See [Status](#status) and
> [`docs/TESTING.md`](docs/TESTING.md).

Engine: **Unreal Engine 5.7.4** (Windows). Project and module name: **Ninjago**.

---

## What is in v0.1

| | |
|---|---|
| Factions | `NINJA` and `SKULKIN` |
| Ninja units | Kai, Jay, Cole, Zane (single-entity heroes) |
| Skulkin units | Miners, Warriors, Watchmen, Samukai (Lord) |
| Map | Flat plane, no navmesh, no obstacles |
| Camera | Top-down / isometric, WASD pan + wheel zoom, no rotation |
| Combat | Melee only, resolved on a 2.0s timer |
| Win | One side has zero living models |
| Lord ability | One button (Spacebar): Fire Blast (Kai), Four-Armed Fury (Samukai) |

Explicitly **out of scope** for v0.1: morale, routing, ranged, ammo, charge bonus, terrain,
campaign, recruitment, unit XP, multi-select, box-select, formations UI, and pathfinding. The data
tables describe 209 units across 17 factions; that is a menu for later versions, not the v0.1 plan.

---

## Controls

| Input | Action |
|---|---|
| Left-click | Select one unit |
| Right-click on ground | Move the selected unit there (walks in formation) |
| Right-click on an enemy | Attack (advance to contact) |
| WASD | Pan the camera |
| Mouse wheel | Zoom |
| Spacebar | Fire the selected Lord's ability (if off cooldown) |
| Esc | Deselect |

One unit, one order. That is the entire control scheme.

---

## Quick start

Requires UE 5.7.4 on Windows. Full step-by-step verification is in
[`docs/TESTING.md`](docs/TESTING.md); the short version:

```powershell
$UE   = "C:\Program Files\Epic Games\UE_5.7"
$Proj = "C:\path\to\warjago\Ninjago.uproject"
$Build = "$UE\Engine\Build\BatchFiles\Build.bat"

# 1. Generate project files + compile
& $Build -projectfiles -project="$Proj" -game -engine -progress
& $Build NinjagoEditor Win64 Development -project="$Proj" -waitmutex

# 2. Open the editor
& "$UE\Engine\Binaries\Win64\UnrealEditor.exe" "$Proj"
```

Then, inside the editor (Tools -> Execute Python Script):

1. `Tools/import_datatables.py`  builds the four DataTables from the CSVs.
2. `Tools/make_placeholder_meshes.py`  builds placeholder minifig parts + the master material.
3. (optional) `Tools/make_battle_setup.py`  creates an editable default battle.

Open an empty level with a floor and a PlayerStart and press Play: two armies spawn and fight.

---

## How it is built

### Data pipeline

`build.py` at the repo root is the single source of truth for all game data. It derives every unit
from 16 archetypes crossed with 17 faction modifier tables, so you tune ~16 archetypes instead of
250 characters. It emits four CSVs into `Content/Data/`:

```bash
python3 build.py --out Content/Data
```

The CSVs are generated output and are **never hand-edited**. To change a stat, change the archetype
or the faction modifier in `build.py`, regenerate, then re-run `Tools/import_datatables.py` in the
editor to refresh the DataTable assets.

| CSV | Rows | Struct |
|---|---|---|
| `dt_units.csv` | 209 | `FNinjagoUnitRow` |
| `dt_factions.csv` | 17 | `FNinjagoFactionRow` |
| `dt_archetypes.csv` | 16 | `FNinjagoArchetypeRow` |
| `dt_abilities.csv` | 44 | `FNinjagoAbilityRow` |

All gameplay numbers live in the DataTables. The only tuning constants in C++ are in
`UNinjagoSettings` (a `UDeveloperSettings` subclass, surfaced under Project Settings -> Ninjago).

### Rendering: no skeletal meshes

A LEGO minifigure has no deformation. It is about six rigid parts on a transform hierarchy: hips,
torso, two arms, head, and headgear. So there is no skinning, no anim blueprints, and no skeletal
meshes anywhere.

Each unit renders as six `UHierarchicalInstancedStaticMeshComponent`s, one per body part, with one
instance per model. Animation is writing rigid transforms per instance per frame via
`BatchUpdateInstancesTransforms`. Faction colour rides in `PerInstanceCustomData` and is read by a
single master material. A regiment of 24 is 144 instances; eight regiments is roughly 1,150. This is
trivially cheap, which is why there is deliberately no Mass Entity or Niagara crowd system.

### Combat

Resolved on a 2.0s timer, never in Tick. Each living model pairs with the nearest living enemy model
within engage range and rolls one attack:

```
hitChance = clamp(Base + PerPoint * (atk.MeleeAttack - def.MeleeDefence), Min, Max)
onHit:     damage = atk.ArmourPiercing + max(MinDamage, atk.Damage - def.Armour * 4)
```

The `max(MinDamage, ...)` floor matters: without it, heavy armour makes a unit literally unkillable,
which is mathematically tidy and miserable for a child hammering an invincible wall. The floor keeps
every landed hit at 1 or more. The combat math is a pure, engine-free static class
(`FNinjagoCombatResolver`) with headless automation tests.

---

## Project structure

```
Ninjago.uproject
build.py                         source of truth for all game data
Config/                          DefaultEngine / DefaultGame / DefaultInput ini
Content/Data/                    the four generated CSVs (+ DataTable assets, once imported)
Source/Ninjago/
  Data/        row structs + enums (NinjagoTypes, *Row.h)
  Core/        NinjagoSettings, NinjagoGameMode, NinjagoBattleSetup
  Combat/      NinjagoCombatResolver (pure, tested)
  Units/       NinjagoUnit, NinjagoModelRenderer, NinjagoFormation, NinjagoModel
  Player/      NinjagoCameraPawn, NinjagoPlayerController
  Tests/       CombatResolverSpec, FormationSpec (headless automation)
Tools/
  import_datatables.py           CSV -> DataTable assets
  make_placeholder_meshes.py     primitive minifig parts + master material
  make_battle_setup.py           default battle data asset
docs/
  TESTING.md                     complete milestone-by-milestone test runbook
  EDITOR_STEPS.md                editor/asset steps that cannot be scripted
```

---

## Division of labour

The C++, `.ini`, CSVs, and Python editor scripts are authored directly. Binary assets
(`.uasset` / `.umap`: Blueprints, DataTables, materials, meshes, levels) cannot be authored as text,
so anything that can be scripted is scripted as an Unreal Python tool under `Tools/`, and the few
things that must be done by hand are listed as numbered checklists in `docs/EDITOR_STEPS.md`.

This is why input uses transient Enhanced Input objects built in C++ (no input `.uasset`), why
meshes and the material are generated by a Python tool, and why the game mode can synthesise a
default battle in code when no data asset is present.

---

## Status

All seven v0.1 milestones are authored and self-verified as far as a compiler-less host allows
(brace/scope checks, Python mirrors of the pure math, schema cross-checks against the CSVs). What
remains is on Windows: compile, run the automation tests, run the three Python tools, and play-test.
`docs/TESTING.md` is the acceptance gate for each milestone.

A few implementation choices deviate from the original spec and are easy to revert:

- Faction colour is 3-float RGB (from each row's hex) rather than a 2-float packed palette index,
  because the data stores hex per row, not indices.
- The selection highlight is a debug-draw ring rather than a decal (avoids a fragile decal material).
- The win result is an on-screen debug message rather than a UMG end screen.

Known first-build watch-items: the `.Target.cs` build-settings enums, the Python-generated material
graph, the automation-test flag names, and the `Tier` enum DataTable import. Each has a documented
fix in `docs/TESTING.md`.

---

## Roadmap

v0.1 is the whole current scope. Later versions draw from the existing data (209 units, 44
abilities): morale and routing, ranged units and ammo, charge bonuses, terrain, more factions, and
eventually real LDraw-derived geometry to replace the placeholder minifig parts.

---

## Documentation

- [`docs/TESTING.md`](docs/TESTING.md)  complete build-and-test runbook (start here)
- [`docs/EDITOR_STEPS.md`](docs/EDITOR_STEPS.md)  per-milestone editor/asset steps
- [`CLAUDE.md`](CLAUDE.md)  architecture, constraints, and conventions (also the AI assistant guide)
