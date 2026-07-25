// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"

/** Stats a timed modifier can affect. */
enum class ENinjagoStat : uint8
{
	MeleeAttack,
	MeleeDefence,
	Damage,
	Speed
};

/** A single timed stat modifier: a flat add and/or a percent add, with a remaining lifetime. */
struct FNinjagoModifier
{
	ENinjagoStat Stat = ENinjagoStat::MeleeAttack;
	float FlatAdd = 0.f;
	float PercentAdd = 0.f; // e.g. +100 doubles, -50 halves
	float Remaining = 0.f;  // seconds
};

/**
 * A pure stack of timed stat modifiers. No engine dependency beyond core types.
 *
 * Effective value for a stat = (Base + sum of flat adds) * (1 + sum of percent adds / 100),
 * clamped at zero. Modifiers expire as time is ticked.
 */
class NINJAGO_API FNinjagoModifiers
{
public:
	void Add(ENinjagoStat Stat, float FlatAdd, float PercentAdd, float DurationSeconds);

	/** Advance time, removing any modifier whose lifetime has elapsed. */
	void Tick(float DeltaSeconds);

	/** Effective value of Base under the current modifiers for Stat. */
	float Apply(ENinjagoStat Stat, float Base) const;

	int32 Num() const { return Active.Num(); }
	void Reset() { Active.Reset(); }

private:
	TArray<FNinjagoModifier> Active;
};
