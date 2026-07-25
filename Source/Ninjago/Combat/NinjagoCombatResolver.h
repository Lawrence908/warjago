// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"

struct FNinjagoUnitRow;

/**
 * Tuning constants for the combat math, passed in by the caller (filled from UNinjagoSettings).
 * Kept as a plain struct so the resolver has no dependency on the settings/engine layer and
 * stays trivially unit-testable. Defaults mirror UNinjagoSettings.
 */
struct FNinjagoCombatParams
{
	float HitChanceBase = 0.35f;
	float HitChancePerPoint = 0.03f;
	float HitChanceMin = 0.10f;
	float HitChanceMax = 0.90f;
	int32 MinDamage = 1;
};

/**
 * Pure, static melee combat math. No engine dependencies beyond core types (FMath, FRandomStream).
 * All numbers come from the caller via FNinjagoCombatParams; there are no literals here.
 *
 *   hitChance = clamp(Base + PerPoint * (atk.MeleeAttack - def.MeleeDefence), Min, Max)
 *   onHit:     damage = atk.ArmourPiercing + max(MinDamage, atk.Damage - def.Armour * 4)
 *
 * The MinDamage floor is applied to the armour-reduced term only, so armour-piercing is
 * always added on top and a landed hit can never deal zero (an unkillable wall is miserable
 * for a seven-year-old). See CLAUDE.md.
 */
class NINJAGO_API FNinjagoCombatResolver
{
public:
	/** Probability in [Min, Max] that an attack of this matchup lands. */
	static float HitChance(int32 AttackerMeleeAttack, int32 DefenderMeleeDefence, const FNinjagoCombatParams& Params);

	/** Damage dealt by a landed hit. Always >= AttackerArmourPiercing + MinDamage. */
	static int32 DamageOnHit(int32 AttackerDamage, int32 AttackerArmourPiercing, int32 DefenderArmour, const FNinjagoCombatParams& Params);

	/** Roll one attack. Returns damage on a hit, 0 on a miss. RNG is injected for determinism. */
	static int32 ResolveAttack(
		int32 AttackerMeleeAttack, int32 AttackerDamage, int32 AttackerArmourPiercing,
		int32 DefenderMeleeDefence, int32 DefenderArmour,
		const FNinjagoCombatParams& Params, FRandomStream& Rng);

	/** Convenience overload reading stats straight off two unit rows. */
	static int32 ResolveAttack(const FNinjagoUnitRow& Attacker, const FNinjagoUnitRow& Defender,
		const FNinjagoCombatParams& Params, FRandomStream& Rng);
};
