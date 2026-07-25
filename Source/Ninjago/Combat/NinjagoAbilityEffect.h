// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"

/**
 * A parsed ability Magnitude, e.g. "dmg=45", "heal=50%", "def=+3", "terrain=wall".
 * Verb is lower-cased; Value is the signed number when present; bPercent marks a trailing '%';
 * bNumeric is false for qualitative magnitudes like "wall" or "partial".
 */
struct FNinjagoAbilityMagnitude
{
	FString Verb;
	int32 Value = 0;
	bool bPercent = false;
	bool bNumeric = false;
};

/**
 * Pure helpers for ability effects. No engine dependencies beyond core types.
 */
class NINJAGO_API FNinjagoAbilityEffect
{
public:
	/** Parse a Magnitude string into verb/value/percent. Returns an empty verb if there is no '='. */
	static FNinjagoAbilityMagnitude ParseMagnitude(const FString& Magnitude);

	/**
	 * Indices of the up-to-MaxCount points nearest to Center (2D), within MaxRadius, nearest first.
	 * Used to chain an ability between the closest targets.
	 */
	static TArray<int32> SelectNearest(const TArray<FVector>& Points, const FVector& Center,
		int32 MaxCount, float MaxRadius);
};
