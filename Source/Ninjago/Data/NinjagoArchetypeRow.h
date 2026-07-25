// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NinjagoArchetypeRow.generated.h"

/**
 * One row of dt_archetypes.csv (the tuning table build.py derives units from). Row key =
 * the Name column. Property names match the CSV headers exactly, including Rng_atk / Rng.
 * Not used by v0.1 gameplay; present so the DataTable round-trips and future tools can read it.
 */
USTRUCT(BlueprintType)
struct FNinjagoArchetypeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	FString Role;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	int32 Size = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	int32 Hp = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	int32 Atk = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	int32 Def = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	int32 Charge = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	int32 Dmg = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	int32 Ap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	int32 Armour = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	int32 Morale = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	float Speed = 0.f;

	// CSV header is "Rng_atk" (ranged accuracy 0..10); property name matches for import.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	int32 Rng_atk = 0;

	// CSV header is "Rng" (range in cm).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	float Rng = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	int32 Ammo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Archetype")
	float Scale = 1.f;
};
