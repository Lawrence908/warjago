// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "Data/NinjagoTypes.h"
#include "Data/NinjagoUnitRow.h"
#include "Data/NinjagoAbilityRow.h"
#include "Units/NinjagoModel.h"
#include "Combat/NinjagoModifiers.h"
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

	/** Slain-but-reassembling models (Skulkin "Already Dead"). Such a unit is not yet beaten. */
	int32 PendingReviveCount() const;

	/** 0..1 remaining strength (living models' HP over the unit's maximum), for the health bar. */
	float GetStrengthFraction() const;

	bool HasActiveOrder() const { return OrderType != EOrderType::NoOrder; }

	/** Set the row + team before FinishSpawning (used by the game mode when spawning armies). */
	void ConfigureSpawn(UDataTable* Table, FName RowName, ETeam InTeam);

	/** Apply combat damage to one model; kills it at <= 0 HP. Returns true if this call killed it. */
	bool ApplyModelDamage(int32 ModelIndex, int32 Damage);

	/** Enemy models this unit has killed this battle (for the MVP tally). */
	int32 GetKillCount() const { return KillCount; }
	void AddKill(int32 Count = 1) { KillCount += Count; }

	/** Restore HP to one living model, capped at its per-model maximum. */
	void ApplyModelHeal(int32 ModelIndex, int32 Amount);

	/** Rebuild a destroyed unit: every model back alive at HpPercent of max, fresh morale/ammo. */
	void RebuildUnit(int32 HpPercent);

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

	/** Immediate morale loss (e.g. from being charged); ignored by immune units. */
	void ApplyMoraleShock(float Amount);

	// --- Charge (v0.4) ---

	/** True until this unit delivers its first-contact charge; reset when it re-advances. */
	bool IsChargePending() const { return bChargePending; }
	void ClearChargePending() { bChargePending = false; }

	// --- Spears / anti-large (v0.5) ---

	/** Spear archetype: braces against large chargers and hits large targets harder. */
	bool IsSpear() const { return bRowValid && CachedRow.Archetype == FName(TEXT("SPEAR")); }

	/** Large unit (cavalry, monster, giant, vehicle), classified by ModelScale. */
	bool IsLarge() const;

	// --- Crowd control (v0.9) ---

	/** Frozen/stunned: cannot move, fight, or use abilities while this lasts. */
	bool IsStunned() const { return StunTimer > 0.f; }
	void ApplyStun(float Seconds) { StunTimer = FMath::Max(StunTimer, FMath::Max(0.f, Seconds)); }

	// --- Stealth / Vanish (v0.17) ---

	/** Hidden: cannot be targeted by enemies while this lasts. */
	bool IsStealthed() const { return bStealthed; }

	/** Vanish: hide for Seconds and make the next attack a critical of the given multiplier. */
	void ApplyVanish(float Seconds, int32 Multiplier);

	/** Reveal a stealthed unit (e.g. a passive "Quiet One" on its first attack). */
	void Reveal() { bStealthed = false; StealthTimer = 0.f; }

	/** True if the next attack is a pending Vanish critical (peek; does not consume). */
	bool HasCritPending() const { return bCritPending; }

	/** Damage multiplier for this unit's current attack: the crit value once, then 1 (and reveal). */
	int32 ConsumeCritMultiplier();

	// --- Mind control (v0.10) ---

	/** Temporarily (or permanently, Duration <= 0) fight for NewTeam. */
	void ApplyControl(ETeam NewTeam, float Duration);
	bool IsControlled() const { return bControlled; }

	/**
	 * The team a unit truly belongs to for win purposes: its current team, except a *temporarily*
	 * controlled unit still counts for its original side (it is borrowed, not destroyed). Permanent
	 * control does transfer allegiance.
	 */
	ETeam GetAllegianceTeam() const { return (bControlled && !bControlPermanent) ? OriginalTeam : Team; }

	// --- Buffs / effective stats (v0.7) ---

	int32 EffMeleeAttack() const { return FMath::RoundToInt(Modifiers.Apply(ENinjagoStat::MeleeAttack, CachedRow.MeleeAttack)); }
	int32 EffMeleeDefence() const { return FMath::RoundToInt(Modifiers.Apply(ENinjagoStat::MeleeDefence, CachedRow.MeleeDefence)); }
	int32 EffDamage() const { return FMath::RoundToInt(Modifiers.Apply(ENinjagoStat::Damage, CachedRow.Damage)); }
	float EffSpeedCmS() const { return Modifiers.Apply(ENinjagoStat::Speed, CachedRow.SpeedCmS); }

	/** Add a timed stat modifier (used by buff/debuff abilities). */
	void AddModifier(ENinjagoStat Stat, float FlatAdd, float PercentAdd, float DurationSeconds)
	{
		Modifiers.Add(Stat, FlatAdd, PercentAdd, DurationSeconds);
	}

	/** Flag whether the unit has an engaged model this combat tick (drives Fighting state). */
	void MarkFighting(bool bEngaged);

	// --- Lord ability (M7): one button, Spacebar ---

	bool HasAbility() const { return bHasAbility; }
	bool CanFireAbility() const;

	/** Fire the unit's ability if it has one and is off cooldown. Returns true if it fired. */
	bool TryFireAbility();

	/** 0 = ready, 1 = just fired (fraction of cooldown remaining), for the cooldown indicator. */
	float GetAbilityCooldownFraction() const;

	/** AI heuristic: is it worth firing the ability now (off cooldown, valid targets nearby)? */
	bool ShouldAIFireAbility() const;

	/** The player selected this unit: hold off AI auto-cast briefly so their Spacebar wins the timing. */
	void NotifyPlayerSelected();

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

	// Charge state: true when the unit can still deliver a first-contact charge.
	bool bChargePending = true;

	// Enemy models killed this battle.
	int32 KillCount = 0;

	// Active timed stat modifiers (buffs/debuffs).
	FNinjagoModifiers Modifiers;

	// Skulkin "Already Dead": slain models reassemble once.
	bool bReviveCapable = false;

	// Crowd control: seconds remaining frozen/stunned.
	float StunTimer = 0.f;

	// Stealth: hidden (untargetable) with a pending critical strike.
	bool bStealthed = false;
	float StealthTimer = 0.f;
	bool bCritPending = false;
	int32 CritMultiplier = 1;

	// Seconds during which the AI defers ability auto-cast after a player selection.
	float PlayerCastGrace = 0.f;

	// Mind control: fighting for a team other than the one spawned on.
	ETeam OriginalTeam = ETeam::Ninja;
	bool bControlled = false;
	bool bControlPermanent = false;
	float ControlTimer = 0.f;

	void ProcessRevives(float DeltaSeconds);
	void ApplyStunInRadius(const FVector& Center, float Radius, float Seconds);
	void ApplyControlToEnemies(const FVector& Center, float Radius, bool bSingleTarget, float Duration);
	void ClearOrdersForRetarget();

	// Ability state
	FNinjagoAbilityRow CachedAbility;
	bool bHasAbility = false;
	bool bIsPassive = false; // ability is always-on (PASSIVE), not player/AI activated
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
	void ApplyAbilityChainDamage(const FVector& Center, float Radius, int32 Damage, int32 MaxTargets);
	void ApplyAbilityHealInRadius(const FVector& Center, float Radius, int32 Amount, bool bPercent);
	void ApplyModifierInRadius(const FVector& Center, float Radius, bool bEnemies, ENinjagoStat Stat, float FlatAdd, float PercentAdd, float Duration);
	ANinjagoUnit* FindDestroyedAlly(const FVector& Center, float Radius) const;

	/** World location of formation slot SlotIndex, given the unit's transform. */
	FVector SlotWorldLocation(int32 SlotIndex, int32 Count) const;
};
