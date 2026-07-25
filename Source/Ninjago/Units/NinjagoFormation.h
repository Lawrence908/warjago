// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"

/**
 * Pure, static formation geometry. No engine dependencies beyond core math types.
 *
 * Models fill a wide rectangular block centred on the origin. The frontage (column count)
 * is chosen as the smallest divisor of Count that is >= ceil(sqrt(Count)), so the block is
 * a full rectangle (no ragged last row), wider than it is deep, and exactly point-symmetric
 * about the origin. A single-entity unit (Count == 1) sits at the origin.
 *
 * Offsets are in the unit's local space; the caller rotates them by the unit yaw and adds
 * the unit location.
 */
class NINJAGO_API FNinjagoFormation
{
public:
	/** Frontage (number of columns) for a block of Count models. Always divides Count. */
	static int32 Columns(int32 Count);

	/**
	 * Local-space (X = frontage, Y = depth) offset for slot Index of Count models,
	 * with Spacing cm between adjacent slots. Index is clamped to [0, Count).
	 */
	static FVector2D SlotOffset(int32 Index, int32 Count, float Spacing);
};
