// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NinjagoSettings.generated.h"

/**
 * Project-wide tuning constants. This is the ONLY place gameplay numbers are allowed to live
 * outside the DataTables (per CLAUDE.md). Surfaces under Project Settings -> Ninjago and
 * serialises to Config/DefaultGame.ini.
 *
 * Read at runtime via GetDefault<UNinjagoSettings>().
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Ninjago"))
class NINJAGO_API UNinjagoSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Groups these settings under a "Ninjago" heading in Project Settings. */
	virtual FName GetCategoryName() const override { return FName(TEXT("Ninjago")); }

	/** Seconds between combat resolutions. Combat runs on a timer, never in Tick. */
	UPROPERTY(config, EditAnywhere, Category="Combat", meta=(ClampMin="0.1"))
	float CombatTickSeconds = 2.0f;

	/** A model engages the nearest living enemy model within this range (cm). */
	UPROPERTY(config, EditAnywhere, Category="Combat", meta=(ClampMin="0.0"))
	float EngageRangeCm = 150.0f;

	/** hitChance = Base + PerPoint * (atk.MeleeAttack - def.MeleeDefence), clamped [Min, Max]. */
	UPROPERTY(config, EditAnywhere, Category="Combat")
	float HitChanceBase = 0.35f;

	UPROPERTY(config, EditAnywhere, Category="Combat")
	float HitChancePerPoint = 0.03f;

	UPROPERTY(config, EditAnywhere, Category="Combat", meta=(ClampMin="0.0", ClampMax="1.0"))
	float HitChanceMin = 0.10f;

	UPROPERTY(config, EditAnywhere, Category="Combat", meta=(ClampMin="0.0", ClampMax="1.0"))
	float HitChanceMax = 0.90f;

	/** Damage floor: a landed hit always deals at least this much (keeps armour from being a wall). */
	UPROPERTY(config, EditAnywhere, Category="Combat", meta=(ClampMin="1"))
	int32 MinDamage = 1;

	/** A unit whose ModelScale is at least this is "large" (cavalry, monsters, giants, vehicles). */
	UPROPERTY(config, EditAnywhere, Category="Combat|Melee", meta=(ClampMin="1.0"))
	float LargeModelScaleThreshold = 1.4f;

	/** Bonus damage a spear unit adds when striking a large target (anti-large). */
	UPROPERTY(config, EditAnywhere, Category="Combat|Melee", meta=(ClampMin="0"))
	int32 SpearAntiLargeBonus = 10;

	/** Ranged hit chance = clamp(RangedBase + RangedPerPoint * RangedAttack, HitChanceMin, HitChanceMax). */
	UPROPERTY(config, EditAnywhere, Category="Combat|Ranged")
	float RangedHitChanceBase = 0.15f;

	UPROPERTY(config, EditAnywhere, Category="Combat|Ranged")
	float RangedHitChancePerPoint = 0.06f;

	/** A ranged unit halts at this fraction of its RangeCm to shoot (then closes when out of ammo). */
	UPROPERTY(config, EditAnywhere, Category="Combat|Ranged", meta=(ClampMin="0.1", ClampMax="1.0"))
	float RangedStandoffFraction = 0.8f;

	/** Spacing between formation slots (cm). */
	UPROPERTY(config, EditAnywhere, Category="Formation", meta=(ClampMin="1.0"))
	float ModelSpacingCm = 90.0f;

	/** Damage ticks per second for channeled (duration) abilities, e.g. Four-Armed Fury. */
	UPROPERTY(config, EditAnywhere, Category="Abilities", meta=(ClampMin="1.0"))
	float AbilityChannelTicksPerSecond = 4.0f;

	/** Max targets a chaining ability (e.g. Lightning Bolt) hits. */
	UPROPERTY(config, EditAnywhere, Category="Abilities", meta=(ClampMin="1"))
	int32 AbilityChainMaxTargets = 6;

	/** Duration of a buff/debuff ability that does not specify its own duration (seconds). */
	UPROPERTY(config, EditAnywhere, Category="Abilities", meta=(ClampMin="1.0"))
	float BuffDefaultDurationS = 12.0f;

	/** After the player selects a unit, the AI will not auto-cast its ability for this long. */
	UPROPERTY(config, EditAnywhere, Category="Abilities", meta=(ClampMin="0.0"))
	float PlayerAbilityGraceSeconds = 6.0f;

	// --- Morale (v0.3). A unit whose row Morale == 99 is immune and never routs. ---

	/** Morale lost per model killed, per combat tick. */
	UPROPERTY(config, EditAnywhere, Category="Morale", meta=(ClampMin="0.0"))
	float MoraleCasualtyShock = 5.0f;

	/** Below this fraction of starting strength, the unit takes an extra morale penalty each tick. */
	UPROPERTY(config, EditAnywhere, Category="Morale", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MoraleLowStrengthFraction = 0.5f;

	/** Extra morale lost per tick while under-strength. */
	UPROPERTY(config, EditAnywhere, Category="Morale", meta=(ClampMin="0.0"))
	float MoraleLowStrengthPenalty = 5.0f;

	/** Morale recovered per tick when not taking casualties and not in combat. */
	UPROPERTY(config, EditAnywhere, Category="Morale", meta=(ClampMin="0.0"))
	float MoraleRegenPerTick = 4.0f;

	/** Rout when morale falls to this fraction of the unit's maximum. */
	UPROPERTY(config, EditAnywhere, Category="Morale", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MoraleBreakFraction = 0.2f;

	/** A routing unit rallies once morale recovers to this fraction of its maximum. */
	UPROPERTY(config, EditAnywhere, Category="Morale", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MoraleRallyFraction = 0.5f;

	/** Speed multiplier while routing (panic run). */
	UPROPERTY(config, EditAnywhere, Category="Morale", meta=(ClampMin="1.0"))
	float MoraleRoutSpeedMultiplier = 1.5f;

	/** Morale lost by a unit hit by a charge = charger's ChargeBonus * this scale. */
	UPROPERTY(config, EditAnywhere, Category="Morale", meta=(ClampMin="0.0"))
	float MoraleChargeShockScale = 2.0f;

	// --- Skulkin "Already Dead" revive (v0.8) ---

	/** Seconds a slain Skulkin model waits before reassembling (once per battle). */
	UPROPERTY(config, EditAnywhere, Category="Revive", meta=(ClampMin="0.0"))
	float ReviveDelaySeconds = 20.0f;

	/** Fraction of max HP a reassembled model returns with. */
	UPROPERTY(config, EditAnywhere, Category="Revive", meta=(ClampMin="0.01", ClampMax="1.0"))
	float ReviveHpFraction = 0.5f;

	/** Seconds after a result before the battle auto-restarts. 0 disables auto-restart (press R). */
	UPROPERTY(config, EditAnywhere, Category="Battle", meta=(ClampMin="0.0"))
	float AutoRestartSeconds = 8.0f;

	/** The unit row a "summon" ability calls in (a big monster to fight for the caster). */
	UPROPERTY(config, EditAnywhere, Category="Abilities")
	FName SummonedUnitRow = FName(TEXT("SER_DEVOURER"));

	/** Lifetime of a summoned unit if its ability does not specify a duration (seconds). */
	UPROPERTY(config, EditAnywhere, Category="Abilities", meta=(ClampMin="1.0"))
	float SummonDefaultLifetime = 30.0f;
};
