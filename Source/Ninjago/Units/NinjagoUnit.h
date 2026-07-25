// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "Data/NinjagoTypes.h"
#include "Data/NinjagoUnitRow.h"
#include "Data/NinjagoAbilityRow.h"
#include "Units/NinjagoModel.h"
#include "NinjagoUnit.generated.h"

class USceneComponent;
class UBoxComponent;
class UNinjagoModelRenderer;

/**
 * One unit: a regiment of rigid minifig models, or a single-entity Lord/Hero (UnitSize == 1).
 * Owns its model data, a cached stat row, its team, state and current order. Rendering is
 * delegated to a UNinjagoModelRenderer. Steering is straight-line on the flat plane.
 *
 * Editor use (M4 acceptance): place one, pick a row in UnitRowHandle, set Team, press Play.
 */
UCLASS()
class NINJAGO_API ANinjagoUnit : public AActor
{
	GENERATED_BODY()

public:
	ANinjagoUnit();

	virtual void Tick(float DeltaSeconds) override;

	/** Cached stat row for this unit. Valid after BeginPlay / InitialiseFromRow. */
	const FNinjagoUnitRow& GetRow() const { return CachedRow; }
	bool IsRowValid() const { return bRowValid; }
	ETeam GetTeam() const { return Team; }
	EUnitState GetState() const { return State; }

	const TArray<FNinjagoModel>& GetModels() const { return Models; }
	int32 LivingModelCount() const;
	bool HasActiveOrder() const { return OrderType != EOrderType::NoOrder; }

	/** Set the row + team before FinishSpawning (used by the game mode when spawning armies). */
	void ConfigureSpawn(UDataTable* Table, FName RowName, ETeam InTeam);

	/** Apply combat damage to one model; kills it at <= 0 HP. */
	void ApplyModelDamage(int32 ModelIndex, int32 Damage);

	// --- Ranged (v0.2) ---

	/** True if this unit's row can shoot (has range and ammo). */
	bool IsRangedUnit() const { return bRowValid && CachedRow.RangeCm > 0.f && CachedRow.Ammo > 0; }

	/** Row-defined maximum ranged range (cm), 0 for melee-only. */
	float GetRangeCm() const { return bRowValid ? CachedRow.RangeCm : 0.f; }

	/** True if any living model still has ammo. */
	bool HasAmmoRemaining() const;

	/** Spend one shot from a model; returns true if it had ammo. */
	bool TryConsumeAmmo(int32 ModelIndex);

	// --- Morale (v0.3) ---

	bool IsMoraleImmune() const { return bMoraleImmune; }
	bool IsRouting() const { return bRouting; }
	float GetMoraleFraction() const { return MaxMorale > 0.f ? CurrentMorale / MaxMorale : 1.f; }

	/**
	 * Advance morale one combat tick and update routing. bInCombat marks whether the unit was
	 * engaged this tick; NearestEnemyLoc feeds the flee direction when it routs.
	 */
	void UpdateMorale(bool bInCombat, bool bHasEnemy, const FVector& NearestEnemyLoc);

	/** Flag whether the unit has an engaged model this combat tick (drives Fighting state). */
	void MarkFighting(bool bEngaged);

	// --- Lord ability (M7): one button, Spacebar ---

	bool HasAbility() const { return bHasAbility; }
	bool CanFireAbility() const;

	/** Fire the unit's ability if it has one and is off cooldown. Returns true if it fired. */
	bool TryFireAbility();

	/** 0 = ready, 1 = just fired (fraction of cooldown remaining), for the cooldown indicator. */
	float GetAbilityCooldownFraction() const;

	const FNinjagoAbilityRow& GetAbility() const { return CachedAbility; }

	/** Issue a straight-line move to a world location. */
	void OrderMoveTo(const FVector& WorldTarget);

	/** Order this unit to approach and (from M6) fight an enemy unit. */
	void OrderAttack(ANinjagoUnit* Target);

protected:
	virtual void BeginPlay() override;

	/** Which unit row to build from. Pick the dt_units DataTable and a row key in the editor. */
	UPROPERTY(EditAnywhere, Category="Ninjago")
	FDataTableRowHandle UnitRowHandle;

	UPROPERTY(EditAnywhere, Category="Ninjago")
	ETeam Team = ETeam::Ninja;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninjago")
	EUnitState State = EUnitState::Idle;

	UPROPERTY(VisibleAnywhere, Category="Ninjago")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category="Ninjago")
	TObjectPtr<UNinjagoModelRenderer> Renderer;

	/** Cursor-pickable volume for selection/orders (the render HISMs have no collision). */
	UPROPERTY(VisibleAnywhere, Category="Ninjago")
	TObjectPtr<UBoxComponent> SelectionVolume;

private:
	TArray<FNinjagoModel> Models;
	FNinjagoUnitRow CachedRow;
	bool bRowValid = false;

	EOrderType OrderType = EOrderType::NoOrder;
	FVector OrderLocation = FVector::ZeroVector;
	TWeakObjectPtr<ANinjagoUnit> AttackTarget;

	// Morale state
	float MaxMorale = 0.f;
	float CurrentMorale = 0.f;
	bool bMoraleImmune = false;
	bool bRouting = false;
	int32 InitialSize = 1;
	int32 LastLivingCount = 0;
	FVector FleeFromLocation = FVector::ZeroVector;
	bool bHasFleeSource = false;

	// Ability state
	FNinjagoAbilityRow CachedAbility;
	bool bHasAbility = false;
	float AbilityCooldownRemaining = 0.f;
	FTimerHandle AbilityChannelTimer;
	int32 RemainingChannelTicks = 0;
	int32 ChannelDamagePerTick = 0;

	/** Caches the row and builds the models in formation. */
	void InitialiseFromRow();
	void BuildModels();
	void SteerModels(float DeltaSeconds);
	void SizeSelectionVolume();
	void CacheAbility();
	void AbilityChannelTick();
	void ApplyAbilityDamageInRadius(const FVector& Center, float Radius, int32 Damage);

	/** World location of formation slot SlotIndex, given the unit's transform. */
	FVector SlotWorldLocation(int32 SlotIndex, int32 Count) const;
};
