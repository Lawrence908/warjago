// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"

/**
 * Morale tuning, passed in by the caller (filled from UNinjagoSettings). Plain struct so the
 * resolver has no engine/settings dependency and stays trivially unit-testable. Defaults mirror
 * UNinjagoSettings.
 */
struct FNinjagoMoraleParams
{
	float CasualtyShock = 5.0f;
	float LowStrengthFraction = 0.5f;
	float LowStrengthPenalty = 5.0f;
	float RegenPerTick = 4.0f;
	float BreakFraction = 0.2f;
	float RallyFraction = 0.5f;
};

/**
 * Pure, static morale math. No engine dependencies beyond core types.
 *
 * A unit's morale is a pool that starts at its maximum (the row Morale). Each combat tick it drops
 * with casualties and under-strength, and recovers when safe. Break/rally uses hysteresis so a unit
 * does not flicker in and out of routing: it breaks at BreakFraction of max and only rallies once it
 * has climbed back to RallyFraction.
 *
 * Morale-immune units (row Morale == 99) are handled by the caller and never enter this path.
 */
class NINJAGO_API FNinjagoMoraleResolver
{
public:
	/** One tick of morale change, clamped to [0, MaxMorale]. */
	static float StepMorale(float Current, float MaxMorale, int32 LostThisTick, float LivingFraction,
		bool bInCombat, const FNinjagoMoraleParams& Params);

	/** Should the unit be routing now, given its morale and whether it is already routing? */
	static bool ResolveRouting(float Morale, float MaxMorale, bool bCurrentlyRouting,
		const FNinjagoMoraleParams& Params);
};
