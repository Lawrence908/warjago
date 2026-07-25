// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Combat/NinjagoMoraleResolver.h"

float FNinjagoMoraleResolver::StepMorale(float Current, float MaxMorale, int32 LostThisTick,
	float LivingFraction, bool bInCombat, const FNinjagoMoraleParams& Params)
{
	float Delta = -Params.CasualtyShock * static_cast<float>(FMath::Max(0, LostThisTick));

	if (LivingFraction < Params.LowStrengthFraction)
	{
		Delta -= Params.LowStrengthPenalty;
	}

	// Recover only when safe: no casualties this tick and not standing in combat.
	if (LostThisTick <= 0 && !bInCombat)
	{
		Delta += Params.RegenPerTick;
	}

	return FMath::Clamp(Current + Delta, 0.f, MaxMorale);
}

bool FNinjagoMoraleResolver::ResolveRouting(float Morale, float MaxMorale, bool bCurrentlyRouting,
	const FNinjagoMoraleParams& Params)
{
	const float BreakAt = Params.BreakFraction * MaxMorale;
	const float RallyAt = Params.RallyFraction * MaxMorale;

	if (bCurrentlyRouting)
	{
		// Keep routing until morale climbs back past the (higher) rally line.
		return Morale < RallyAt;
	}
	// Break once morale falls to the break line.
	return Morale <= BreakAt;
}
