# CLAUDE.md — Ninjago: Total War

Persistent context for Claude Code. Read this before any task in this repo.

---

## What this is

A Total War-style real-time army battler with LEGO Ninjago factions. Personal project, built for
my seven-year-old son. Not commercial, not shipping, no licensing concerns.

**The audience is a child.** Every design decision resolves in favour of "can a seven-year-old
hold this in his head?" If a feature is clever but needs explaining, it is wrong.

---

## Non-negotiable constraints

1. **One unit = one order.** Left-click selects, right-click moves. Right-click on an enemy
   attacks. That is the entire control scheme for v0.1. Do not add formations, stances, facing
   controls, or ability targeting UI.
2. **Lords get exactly one extra button.** Spacebar. Not an ability bar.
3. **Build v0.1 only.** Two factions, eight units, flat plane, no morale, no terrain, no campaign.
   The data tables contain 209 units — that is a menu, not a plan. Do not implement units,
   factions, or mechanics that are not in the v0.1 scope below.
4. **Data-driven, never hardcoded.** All stats come from `Content/Data/*.csv` via DataTables.
   No stat literals in C++ except in `UNinjagoSettings` defaults.
5. **No skeletal meshes.** See "Rendering approach".
6. **Ask before scope-adding.** If a task seems to require something outside v0.1, stop and ask.

---

## Rendering approach — read this before touching rendering

A LEGO minifigure **has no deformation**. It is ~6 rigid parts on a transform hierarchy:
hips, torso, left arm, right arm, head, headgear (+ a held weapon).

Therefore: **no skeletal meshes, no skinning, no anim blueprints, no Vertex Animation Textures.**

Each unit renders as a set of `UHierarchicalInstancedStaticMeshComponent`s — one per body part —
and animation is done by writing rigid transforms per instance per frame.

- 24 models × 6 parts = 144 instances per regiment.
- 8 regiments ≈ 1,150 instances. This is trivially cheap.
- Faction colours via `PerInstanceCustomData` (2 floats → packed colour index) read by a single
  master material. One material for every faction.
- Use `BatchUpdateInstancesTransforms` once per unit per frame, not per-instance calls.

**Do not reach for Mass Entity / Niagara-based crowds.** We are two orders of magnitude below
the point where that is justified. If profiling later says otherwise, we revisit then.

---

## Division of labour — important

Claude Code **can** create and edit:
- All C++ (`Source/**`)
- `.uproject`, `.Build.cs`, `.Target.cs`
- `Config/*.ini`
- `Content/Data/*.csv`
- Unreal Python editor scripts (`Tools/*.py`)
- Docs, tests, `.gitignore`

Claude Code **cannot** create `.uasset` / `.umap` binaries. That means Blueprints, DataTable
assets, materials, meshes, and levels are not directly authorable.

**The workaround, and the expected pattern:** write Unreal Python scripts under `Tools/` that I
run inside the editor (Tools → Execute Python Script) to generate those assets. Prefer this over
telling me to click through the editor manually. Anything that can be scripted, script.

If something genuinely must be done by hand in the editor, put it in `docs/EDITOR_STEPS.md` as a
numbered checklist — do not bury it in a chat message.

---

## v0.1 scope — the only thing being built right now

| | |
|---|---|
| Factions | `NINJA` and `SKULKIN` |
| Ninja units | `HRO_KAI`, `HRO_JAY`, `HRO_COLE`, `HRO_ZANE` (single-entity heroes) |
| Skulkin units | `SKU_MINERS`, `SKU_WARRIORS`, `SKU_WATCHMEN`, `LRD_SAMUKAI` |
| Map | Flat plane, no navmesh, no obstacles |
| Camera | Top-down/isometric, WASD pan + wheel zoom, no rotation |
| Controls | LMB select one unit · RMB move or attack · Spacebar Lord ability · Esc deselect |
| Combat | Melee only |
| Win condition | One side has zero living models |
| Explicitly out | Morale, routing, ammo, ranged, charge bonus, terrain, campaign, recruitment, unit XP, multi-select, box-select, formations, pathfinding |

---

## Architecture

```
Source/Ninjago/
  Ninjago.Build.cs
  NinjagoModule.cpp                    IMPLEMENT_PRIMARY_GAME_MODULE

  Data/
    NinjagoTypes.h                     enums: EUnitTier, EUnitState, EOrderType, ETeam
    NinjagoUnitRow.h                   FNinjagoUnitRow : FTableRowBase   -> dt_units.csv
    NinjagoFactionRow.h                FNinjagoFactionRow                -> dt_factions.csv
    NinjagoAbilityRow.h                FNinjagoAbilityRow                -> dt_abilities.csv
    NinjagoArchetypeRow.h              FNinjagoArchetypeRow              -> dt_archetypes.csv

  Units/
    NinjagoUnit.h/.cpp                 AActor. One regiment. Owns TArray<FNinjagoModel>
    NinjagoModelRenderer.h/.cpp        UActorComponent. Owns 6 HISM comps, writes transforms
    NinjagoFormation.h/.cpp            Pure static. SlotOffset(index, count, spacing)

  Combat/
    NinjagoCombatResolver.h/.cpp       Pure static. Hit chance + damage. No engine deps

  Player/
    NinjagoPlayerController.h/.cpp     Select + order
    NinjagoCameraPawn.h/.cpp           Pan + zoom

  Core/
    NinjagoGameMode.h/.cpp             Spawns armies, polls win condition
    NinjagoBattleSetup.h               UDataAsset: two army lists of FName row keys
    NinjagoSettings.h                  UDeveloperSettings: tuning constants

  Tests/
    CombatResolverSpec.cpp             Automation tests, headless-runnable
    FormationSpec.cpp

Content/Data/                          the four generated CSVs
Tools/
  import_datatables.py                 CSV -> DataTable assets
  make_placeholder_meshes.py           primitive minifig stand-ins
docs/EDITOR_STEPS.md                   anything that cannot be scripted
```

### Model representation

```cpp
struct FNinjagoModel
{
    FVector  Location   = FVector::ZeroVector;
    float    Yaw        = 0.f;
    int32    Hp         = 0;
    int32    SlotIndex  = INDEX_NONE;
    bool     bAlive     = true;
};
```

`ANinjagoUnit` holds `TArray<FNinjagoModel> Models`, a cached `FNinjagoUnitRow`, an `ETeam`, an
`EUnitState`, and a current order. Models steer independently toward
`UnitLocation + Rotate(SlotOffset(SlotIndex), UnitYaw)` at `SpeedCmS`. Flat plane, so no navmesh
— straight-line steering with simple separation is sufficient and correct for v0.1.

### Combat

Resolved on a **2.0s timer**, not in Tick.

```
hitChance = clamp(0.35 + 0.03 * (atk.MeleeAttack - def.MeleeDefence), 0.10, 0.90)
if (rand < hitChance)
    damage = atk.ArmourPiercing + max(1, atk.Damage - def.Armour * 4)
```

Note the `max(1, ...)`: without it, `Armour 5` vs `Damage 14, AP 0` floors to zero and the unit is
literally unkillable. Mathematically tidy, miserable for a child hammering an invincible wall.
Keep the floor at 1.

Pair each engaged model with the nearest living enemy model within `EngageRangeCm` (default 150).

### Stat semantics — do not get these wrong

- `Morale == 99` is a **flag meaning immune to morale**, not a value. v0.1 has no morale system,
  but the parser must not treat 99 as a number when morale lands in v0.2.
- `HpPerModel` is per model. Unit total HP is `UnitSize * HpPerModel`.
- `UnitSize == 1` means a single-entity Lord or Hero, not a one-man regiment. No formation.
- `SpeedCmS` is Unreal units (cm) per second.
- `Ability` is an FName row key into `dt_abilities`, or `NAME_None`.

---

## Data pipeline

`build.py` (repo root) is the **source of truth** for all game data. It derives every unit from
16 archetypes × 17 faction modifier tables. The four CSVs are generated output.

**Never hand-edit the CSVs.** Edit `build.py`, then:

```bash
python3 build.py --out Content/Data
```

Then re-run `Tools/import_datatables.py` in the editor to refresh the DataTable assets.

If a stat needs changing, change the archetype or the faction modifier, not the unit.

---

## Assets

There are no meshes yet. `Tools/make_placeholder_meshes.py` should build a minifig from engine
primitives (a cube for hips, a tapered cube for torso, cylinders for arms, a cylinder-plus-stud
for the head) so the game is playable and debuggable before any art exists.

Real geometry will come later from the LDraw open parts library, converted to static meshes.
Assume that pipeline exists but do not build it yet.

The `MeshHead / MeshTorso / MeshLegs / MeshHat / MeshWeapon` columns in `dt_units.csv` are
`TSoftObjectPtr<UStaticMesh>` and export empty. Resolve them at spawn with a fallback to the
placeholder when unset — never crash on a null mesh.

---

## Conventions

- **UE C++ style**: `F` structs, `U` UObjects, `A` Actors, `E` enums. `TObjectPtr` for UPROPERTY
  object refs. `check()` for invariants, `ensure()` for recoverable, `UE_LOG` under a
  `LogNinjago` category.
- **No magic numbers in gameplay code.** Put them in `UNinjagoSettings` (a `UDeveloperSettings`
  subclass) so they surface in Project Settings and serialise to ini.
- **C++ for systems, Blueprint for glue only.** No gameplay logic in Blueprint.
- **Commits**: one logical change each, imperative subject line. Do not batch a milestone into
  one commit.
- **Tests**: `Combat/` and `Units/NinjagoFormation` are pure and have no engine dependencies
  beyond core types. They must have automation tests that run headless.

---

## Verification

Before declaring any milestone done:

1. `Build.bat`/`RunUAT` compiles clean — zero warnings in `Source/Ninjago`.
2. Automation tests pass: `UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests Ninjago" -unattended -nop4 -testexit="Automation Test Queue Empty"`
3. Re-read the milestone's acceptance criteria and confirm each line item explicitly.
4. State plainly what was **not** done and what I need to do in the editor.

Do not report a milestone complete on the basis that the code looks right. If you could not run
something, say so.
