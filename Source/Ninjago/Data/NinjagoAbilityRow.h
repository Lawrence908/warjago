// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NinjagoAbilityRow.generated.h"

/**
 * One row of dt_abilities.csv. Row key = the Name column (e.g. AB_FIRE_BLAST).
 * Type (ACTIVE/PASSIVE) and Target (SELF_AOE, GROUND_AOE, ...) are kept as strings:
 * the full table has many target kinds and v0.1 only reads radius/cooldown/magnitude.
 * Effect/Magnitude are free-form strings ("dmg=45", "dmg=40/hit").
 */
USTRUCT(BlueprintType)
struct FNinjagoAbilityRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Ability")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Ability")
	FString Type;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Ability")
	FString Target;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Ability")
	float RadiusCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Ability")
	float DurationS = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Ability")
	float CooldownS = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Ability")
	FString Effect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Ability")
	FString Magnitude;
};
