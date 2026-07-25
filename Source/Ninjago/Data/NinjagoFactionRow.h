// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NinjagoFactionRow.generated.h"

/**
 * One row of dt_factions.csv. Row key = the Name column. v0.1 uses NINJA and SKULKIN.
 * Colours are "#RRGGBB" strings. StatMods is a mini-DSL string ("hpx0.85; speedx1.05; ...")
 * consumed by build.py, not parsed at runtime in v0.1.
 */
USTRUCT(BlueprintType)
struct FNinjagoFactionRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Faction")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Faction")
	FString Wave;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Faction")
	FString ColourPrimary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Faction")
	FString ColourSecondary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Faction")
	FString Mechanic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Faction")
	FString MechanicDesc;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Faction")
	FString Playstyle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Faction")
	FString StatMods;
};
