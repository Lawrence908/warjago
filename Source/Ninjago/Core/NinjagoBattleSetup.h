// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NinjagoBattleSetup.generated.h"

class UDataTable;

/**
 * Defines a battle: which unit rows make up each army. The game mode spawns the Ninja army and the
 * Skulkin army facing each other and lets them fight. Row keys index UnitTable (dt_units).
 */
UCLASS(BlueprintType)
class NINJAGO_API UNinjagoBattleSetup : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The dt_units DataTable the army row keys index into. */
	UPROPERTY(EditAnywhere, Category="Ninjago")
	TObjectPtr<UDataTable> UnitTable;

	/** Ninja army: unit row keys (e.g. HRO_KAI). */
	UPROPERTY(EditAnywhere, Category="Ninjago")
	TArray<FName> NinjaArmy;

	/** Skulkin army: unit row keys (e.g. SKU_WARRIORS). */
	UPROPERTY(EditAnywhere, Category="Ninjago")
	TArray<FName> SkulkinArmy;

	/** Distance between the two army lines (cm). */
	UPROPERTY(EditAnywhere, Category="Ninjago")
	float ArmySeparationCm = 3500.f;

	/** Spacing between units within an army line (cm). */
	UPROPERTY(EditAnywhere, Category="Ninjago")
	float UnitSpacingCm = 900.f;
};
