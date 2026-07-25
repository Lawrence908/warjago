// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Combat/NinjagoCombatResolver.h"
#include "Data/NinjagoUnitRow.h"

float FNinjagoCombatResolver::HitChance(int32 AttackerMeleeAttack, int32 DefenderMeleeDefence, const FNinjagoCombatParams& Params)
{
	const float Raw = Params.HitChanceBase + Params.HitChancePerPoint * static_cast<float>(AttackerMeleeAttack - DefenderMeleeDefence);
	return FMath::Clamp(Raw, Params.HitChanceMin, Params.HitChanceMax);
}

int32 FNinjagoCombatResolver::DamageOnHit(int32 AttackerDamage, int32 AttackerArmourPiercing, int32 DefenderArmour, const FNinjagoCombatParams& Params)
{
	// Floor applies to the armour-reduced portion; armour-piercing is added on top and never floored away.
	const int32 Reduced = FMath::Max(Params.MinDamage, AttackerDamage - DefenderArmour * 4);
	return AttackerArmourPiercing + Reduced;
}

int32 FNinjagoCombatResolver::ResolveAttack(
	int32 AttackerMeleeAttack, int32 AttackerDamage, int32 AttackerArmourPiercing,
	int32 DefenderMeleeDefence, int32 DefenderArmour,
	const FNinjagoCombatParams& Params, FRandomStream& Rng)
{
	const float Chance = HitChance(AttackerMeleeAttack, DefenderMeleeDefence, Params);
	// FRandomStream::FRand() returns [0, 1). A hit chance of 1.0 always lands; 0.0 never lands.
	if (Rng.FRand() < Chance)
	{
		return DamageOnHit(AttackerDamage, AttackerArmourPiercing, DefenderArmour, Params);
	}
	return 0;
}

int32 FNinjagoCombatResolver::ResolveAttack(const FNinjagoUnitRow& Attacker, const FNinjagoUnitRow& Defender,
	const FNinjagoCombatParams& Params, FRandomStream& Rng)
{
	return ResolveAttack(
		Attacker.MeleeAttack, Attacker.Damage, Attacker.ArmourPiercing,
		Defender.MeleeDefence, Defender.Armour,
		Params, Rng);
}

int32 FNinjagoCombatResolver::ResolveMelee(
	int32 AttackerMeleeAttack, int32 AttackerDamage, int32 AttackerChargeBonus, int32 AttackerArmourPiercing,
	int32 DefenderMeleeDefence, int32 DefenderArmour,
	bool bCharging, bool bAttackerSpear, bool bAttackerLarge, bool bDefenderSpear, bool bDefenderLarge,
	int32 AntiLargeBonus, const FNinjagoCombatParams& Params, FRandomStream& Rng)
{
	int32 EffDamage = AttackerDamage;

	// Anti-large: spears hit large targets harder.
	if (bAttackerSpear && bDefenderLarge)
	{
		EffDamage += FMath::Max(0, AntiLargeBonus);
	}

	// Charge bonus applies on first contact, unless a spear defender braces a large charger.
	const bool bBraced = bDefenderSpear && bAttackerLarge;
	if (bCharging && !bBraced)
	{
		EffDamage += FMath::Max(0, AttackerChargeBonus);
	}

	const float Chance = HitChance(AttackerMeleeAttack, DefenderMeleeDefence, Params);
	if (Rng.FRand() < Chance)
	{
		return DamageOnHit(EffDamage, AttackerArmourPiercing, DefenderArmour, Params);
	}
	return 0;
}

int32 FNinjagoCombatResolver::ResolveChargeAttack(
	int32 AttackerMeleeAttack, int32 AttackerDamage, int32 AttackerChargeBonus, int32 AttackerArmourPiercing,
	int32 DefenderMeleeDefence, int32 DefenderArmour,
	const FNinjagoCombatParams& Params, FRandomStream& Rng)
{
	// Hit chance is unchanged; the charge adds its bonus to base damage on the impact strike.
	const float Chance = HitChance(AttackerMeleeAttack, DefenderMeleeDefence, Params);
	if (Rng.FRand() < Chance)
	{
		return DamageOnHit(AttackerDamage + FMath::Max(0, AttackerChargeBonus), AttackerArmourPiercing, DefenderArmour, Params);
	}
	return 0;
}

int32 FNinjagoCombatResolver::ResolveChargeAttack(const FNinjagoUnitRow& Attacker, const FNinjagoUnitRow& Defender,
	const FNinjagoCombatParams& Params, FRandomStream& Rng)
{
	return ResolveChargeAttack(
		Attacker.MeleeAttack, Attacker.Damage, Attacker.ChargeBonus, Attacker.ArmourPiercing,
		Defender.MeleeDefence, Defender.Armour,
		Params, Rng);
}

float FNinjagoCombatResolver::RangedHitChance(int32 RangedAttack, const FNinjagoCombatParams& Params)
{
	const float Raw = Params.RangedHitChanceBase + Params.RangedHitChancePerPoint * static_cast<float>(RangedAttack);
	return FMath::Clamp(Raw, Params.HitChanceMin, Params.HitChanceMax);
}

int32 FNinjagoCombatResolver::ResolveRangedAttack(
	int32 AttackerRangedAttack, int32 AttackerDamage, int32 AttackerArmourPiercing,
	int32 DefenderArmour, const FNinjagoCombatParams& Params, FRandomStream& Rng)
{
	const float Chance = RangedHitChance(AttackerRangedAttack, Params);
	if (Rng.FRand() < Chance)
	{
		// Ranged damage reuses the melee damage model (floor + armour-piercing).
		return DamageOnHit(AttackerDamage, AttackerArmourPiercing, DefenderArmour, Params);
	}
	return 0;
}

int32 FNinjagoCombatResolver::ResolveRangedAttack(const FNinjagoUnitRow& Attacker, const FNinjagoUnitRow& Defender,
	const FNinjagoCombatParams& Params, FRandomStream& Rng)
{
	return ResolveRangedAttack(
		Attacker.RangedAttack, Attacker.Damage, Attacker.ArmourPiercing,
		Defender.Armour, Params, Rng);
}
