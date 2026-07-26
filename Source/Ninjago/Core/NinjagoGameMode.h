// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Math/RandomStream.h"
#include "Data/NinjagoTypes.h"
#include "NinjagoGameMode.generated.h"

class ANinjagoUnit;
class ANinjagoProjectileFX;
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
	virtual void Tick(float DeltaSeconds) override;

	/** Reload the level to fight the battle again (bound to R, and to the auto-restart timer). */
	UFUNCTION()
	void RestartBattle();

	/** Switch the code-default battle to a preset scenario (0..3) and restart (keys 1-4). */
	void LoadScenario(int32 Index);

	/**
	 * Queue a summoned unit to spawn for Team at Location with the given lifetime. It joins the
	 * combat set at the start of the next combat tick (never mid-iteration), avoiding array churn.
	 */
	void RequestSummon(ETeam Team, const FVector& Location, float Lifetime);

protected:
	/** Optional battle definition. If unset, a default is loaded/synthesised for an empty level. */
	UPROPERTY(EditAnywhere, Category="Ninjago")
	TObjectPtr<UNinjagoBattleSetup> BattleSetup;

	/** Combat resolution interval; sourced from UNinjagoSettings at StartPlay. */
	float CombatInterval = 2.0f;

private:
	UPROPERTY() TArray<TObjectPtr<ANinjagoUnit>> AllUnits;
	UPROPERTY() TArray<TObjectPtr<ANinjagoUnit>> PendingSummons; // join AllUnits at the next combat tick
	UPROPERTY() TObjectPtr<ANinjagoProjectileFX> ProjectileFX;

	FTimerHandle CombatTimer;
	FTimerHandle WinTimer;
	FTimerHandle RestartTimer;
	FRandomStream CombatRng;
	bool bResolved = false;
	bool bCombatHasRun = false;

	/** Units that dealt or took a hit this combat tick (drives the morale "in combat" flag). */
	TSet<ANinjagoUnit*> EngagedThisTick;

	/** Display name of the current preset scenario. */
	FString ActiveScenarioName = TEXT("Grand Battle");

	void GatherUnits();
	void SpawnFromSetup(const UNinjagoBattleSetup* Setup);
	void SpawnDefaultBattle();
	ANinjagoUnit* SpawnUnit(UDataTable* Table, FName RowKey, ETeam Team, const FVector& Location, const FRotator& Rotation);
	void SpawnArmyLine(UDataTable* Table, const TArray<FName>& Army, ETeam Team, float LineX, float Yaw, float UnitSpacing);

	void RunCombatTick();
	void AcquireTargets();
	void MoralePass();
	void AbilityAIPass();
	void FlushPendingSummons();
	void CheckWinCondition();
	int32 LivingCountForTeam(ETeam Team) const;
	ANinjagoUnit* NearestEnemyUnit(const ANinjagoUnit* For) const;
};
