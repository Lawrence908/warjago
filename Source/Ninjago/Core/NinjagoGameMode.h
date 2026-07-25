// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Math/RandomStream.h"
#include "Data/NinjagoTypes.h"
#include "NinjagoGameMode.generated.h"

class ANinjagoUnit;
class UNinjagoBattleSetup;
class UDataTable;

/**
 * v0.1 game mode. Installs the battle camera + player controller, spawns both armies (from a
 * UNinjagoBattleSetup, or a code default when none is provided and the level is empty), runs the
 * 2.0s combat resolution, drives simple advance-to-contact, and polls the win condition.
 */
UCLASS()
class NINJAGO_API ANinjagoGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANinjagoGameMode();

	virtual void StartPlay() override;

protected:
	/** Optional battle definition. If unset, a default is loaded/synthesised for an empty level. */
	UPROPERTY(EditAnywhere, Category="Ninjago")
	TObjectPtr<UNinjagoBattleSetup> BattleSetup;

	/** Combat resolution interval; sourced from UNinjagoSettings at StartPlay. */
	float CombatInterval = 2.0f;

private:
	UPROPERTY() TArray<TObjectPtr<ANinjagoUnit>> AllUnits;

	FTimerHandle CombatTimer;
	FTimerHandle WinTimer;
	FRandomStream CombatRng;
	bool bResolved = false;

	void GatherUnits();
	void SpawnFromSetup(const UNinjagoBattleSetup* Setup);
	void SpawnDefaultBattle();
	ANinjagoUnit* SpawnUnit(UDataTable* Table, FName RowKey, ETeam Team, const FVector& Location, const FRotator& Rotation);
	void SpawnArmyLine(UDataTable* Table, const TArray<FName>& Army, ETeam Team, float LineX, float Yaw, float UnitSpacing);

	void RunCombatTick();
	void AcquireTargets();
	void CheckWinCondition();
	int32 LivingCountForTeam(ETeam Team) const;
};
