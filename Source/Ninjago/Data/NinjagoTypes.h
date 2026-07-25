// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "NinjagoTypes.generated.h"

/** Which side a unit fights for. v0.1 has exactly two. */
UENUM(BlueprintType)
enum class ETeam : uint8
{
	Ninja,
	Skulkin
};

/** Runtime state machine for a regiment/hero. */
UENUM(BlueprintType)
enum class EUnitState : uint8
{
	Idle,
	Moving,
	Fighting,
	Routing,
	Dead
};

/** The current standing order on a unit. One unit = one order. */
UENUM(BlueprintType)
enum class EOrderType : uint8
{
	NoOrder,
	Move,
	Attack
};

/**
 * Unit tier. Enumerator names deliberately match the Tier tokens in dt_units.csv
 * (TROOP / CMD / ELITE / LORD) so DataTable CSV import maps the column by name.
 * Do not rename these without updating build.py's tier tokens.
 */
UENUM(BlueprintType)
enum class EUnitTier : uint8
{
	TROOP,
	CMD,
	ELITE,
	LORD
};
