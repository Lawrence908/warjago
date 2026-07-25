// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Core/NinjagoGameMode.h"
#include "Core/NinjagoBattleSetup.h"
#include "Core/NinjagoSettings.h"
#include "Player/NinjagoCameraPawn.h"
#include "Player/NinjagoPlayerController.h"
#include "Units/NinjagoUnit.h"
#include "Combat/NinjagoCombatResolver.h"
#include "Effects/NinjagoProjectileFX.h"
#include "NinjagoLog.h"

#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

namespace
{
	const TCHAR* DefaultBattlePath = TEXT("/Game/Data/DA_DefaultBattle.DA_DefaultBattle");
	const TCHAR* UnitTablePath = TEXT("/Game/Data/dt_units.dt_units");

	// Default battle spans the systems: melee, ranged (v0.2), a breakable block for routing (v0.3),
	// and a large monster NIN_SAMURAIX (v0.5) for the Skulkin spears (Watchmen) to brace against.
	// Skulkin are morale-immune by faction and never rout.
	// LRD_ICEEMPEROR is a guest freeze-caster (v0.9): select him and press Space to freeze Skulkin.
	// LRD_SKALES is a guest hypnotist (v0.10): select him and press Space to mind-control a Skulkin unit.
	const TArray<FName> DefaultNinja  = { TEXT("HRO_KAI"), TEXT("HRO_JAY"), TEXT("HRO_COLE"), TEXT("HRO_ZANE"), TEXT("NIN_SHINTARO"), TEXT("NIN_SOLDIERS"), TEXT("NIN_SAMURAIX"), TEXT("LRD_ICEEMPEROR"), TEXT("LRD_SKALES") };
	// HRO_WYPLASH adds a def+3 ally buff (v0.7) to the Skulkin line.
	const TArray<FName> DefaultSkulkin = { TEXT("SKU_MINERS"), TEXT("SKU_WARRIORS"), TEXT("SKU_WATCHMEN"), TEXT("LRD_SAMUKAI"), TEXT("SKU_ENGINEERS"), TEXT("HRO_WYPLASH") };
}

ANinjagoGameMode::ANinjagoGameMode()
{
	DefaultPawnClass = ANinjagoCameraPawn::StaticClass();
	PlayerControllerClass = ANinjagoPlayerController::StaticClass();
	CombatRng.Initialize(20260725);
	PrimaryActorTick.bCanEverTick = true; // draws routing markers
}

void ANinjagoGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Status markers above units: a red flag for routing, a cyan sphere for frozen.
	for (ANinjagoUnit* Unit : AllUnits)
	{
		if (!IsValid(Unit) || Unit->LivingModelCount() == 0)
		{
			continue;
		}
		if (Unit->IsRouting())
		{
			const FVector Base = Unit->GetActorLocation() + FVector(0.f, 0.f, 220.f);
			DrawDebugLine(GetWorld(), Base, Base + FVector(0.f, 0.f, 180.f), FColor::Red, false, -1.f, 0, 12.f);
			DrawDebugLine(GetWorld(), Base + FVector(0.f, 0.f, 180.f), Base + FVector(140.f, 0.f, 130.f), FColor::Red, false, -1.f, 0, 12.f);
			DrawDebugLine(GetWorld(), Base + FVector(140.f, 0.f, 130.f), Base + FVector(0.f, 0.f, 90.f), FColor::Red, false, -1.f, 0, 12.f);
		}
		if (Unit->IsStunned())
		{
			const FVector Base = Unit->GetActorLocation() + FVector(0.f, 0.f, 260.f);
			DrawDebugSphere(GetWorld(), Base, 70.f, 10, FColor::Cyan, false, -1.f, 0, 8.f);
		}
		if (Unit->IsControlled())
		{
			const FVector Base = Unit->GetActorLocation() + FVector(0.f, 0.f, 300.f);
			DrawDebugSphere(GetWorld(), Base, 55.f, 8, FColor::Magenta, false, -1.f, 0, 8.f);
		}
	}
}

void ANinjagoGameMode::StartPlay()
{
	Super::StartPlay();

	CombatInterval = FMath::Max(0.1f, GetDefault<UNinjagoSettings>()->CombatTickSeconds);

	// Prefer units already placed in the level; otherwise spawn a battle.
	GatherUnits();
	if (AllUnits.Num() == 0)
	{
		const UNinjagoBattleSetup* Setup = BattleSetup;
		if (!Setup)
		{
			Setup = LoadObject<UNinjagoBattleSetup>(nullptr, DefaultBattlePath);
		}
		if (Setup)
		{
			SpawnFromSetup(Setup);
		}
		else
		{
			SpawnDefaultBattle();
		}
		GatherUnits();
	}

	ProjectileFX = GetWorld()->SpawnActor<ANinjagoProjectileFX>();

	UE_LOG(LogNinjago, Log, TEXT("Battle start: %d units (Ninja %d models, Skulkin %d models)"),
		AllUnits.Num(), LivingCountForTeam(ETeam::Ninja), LivingCountForTeam(ETeam::Skulkin));

	GetWorldTimerManager().SetTimer(CombatTimer, this, &ANinjagoGameMode::RunCombatTick, CombatInterval, true, CombatInterval);
	GetWorldTimerManager().SetTimer(WinTimer, this, &ANinjagoGameMode::CheckWinCondition, 1.0f, true, 1.0f);
}

void ANinjagoGameMode::GatherUnits()
{
	AllUnits.Reset();
	for (TActorIterator<ANinjagoUnit> It(GetWorld()); It; ++It)
	{
		AllUnits.Add(*It);
	}
}

void ANinjagoGameMode::SpawnFromSetup(const UNinjagoBattleSetup* Setup)
{
	UDataTable* Table = Setup->UnitTable;
	if (!Table)
	{
		Table = LoadObject<UDataTable>(nullptr, UnitTablePath);
	}
	if (!Table)
	{
		UE_LOG(LogNinjago, Error, TEXT("SpawnFromSetup: no unit table available."));
		return;
	}

	const float HalfSep = Setup->ArmySeparationCm * 0.5f;
	SpawnArmyLine(Table, Setup->NinjaArmy,   ETeam::Ninja,   -HalfSep,   0.f, Setup->UnitSpacingCm);
	SpawnArmyLine(Table, Setup->SkulkinArmy, ETeam::Skulkin,  HalfSep, 180.f, Setup->UnitSpacingCm);
}

void ANinjagoGameMode::SpawnDefaultBattle()
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, UnitTablePath);
	if (!Table)
	{
		UE_LOG(LogNinjago, Error, TEXT("SpawnDefaultBattle: dt_units not found at %s; run import_datatables.py."), UnitTablePath);
		return;
	}
	const float HalfSep = 1750.f;
	SpawnArmyLine(Table, DefaultNinja,   ETeam::Ninja,   -HalfSep,   0.f, 900.f);
	SpawnArmyLine(Table, DefaultSkulkin, ETeam::Skulkin,  HalfSep, 180.f, 900.f);
}

void ANinjagoGameMode::SpawnArmyLine(UDataTable* Table, const TArray<FName>& Army, ETeam Team, float LineX, float Yaw, float UnitSpacing)
{
	static const FString Ctx(TEXT("SpawnArmyLine"));
	const float BackRankOffset = 800.f; // ranged units start a rank behind the melee line
	const int32 N = Army.Num();
	for (int32 i = 0; i < N; ++i)
	{
		const float Y = (i - (N - 1) * 0.5f) * UnitSpacing;
		float X = LineX;
		if (const FNinjagoUnitRow* Row = Table->FindRow<FNinjagoUnitRow>(Army[i], Ctx))
		{
			if (Row->RangeCm > 0.f && Row->Ammo > 0)
			{
				X += FMath::Sign(LineX) * BackRankOffset; // push archers away from the enemy
			}
		}
		SpawnUnit(Table, Army[i], Team, FVector(X, Y, 0.f), FRotator(0.f, Yaw, 0.f));
	}
}

ANinjagoUnit* ANinjagoGameMode::SpawnUnit(UDataTable* Table, FName RowKey, ETeam Team, const FVector& Location, const FRotator& Rotation)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ANinjagoUnit* Unit = GetWorld()->SpawnActorDeferred<ANinjagoUnit>(
		ANinjagoUnit::StaticClass(), FTransform(Rotation, Location), nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Unit)
	{
		return nullptr;
	}
	Unit->ConfigureSpawn(Table, RowKey, Team);
	Unit->FinishSpawning(FTransform(Rotation, Location));
	return Unit;
}

void ANinjagoGameMode::RunCombatTick()
{
	if (bResolved)
	{
		return;
	}

	const UNinjagoSettings* S = GetDefault<UNinjagoSettings>();
	FNinjagoCombatParams P;
	P.HitChanceBase = S->HitChanceBase;
	P.HitChancePerPoint = S->HitChancePerPoint;
	P.HitChanceMin = S->HitChanceMin;
	P.HitChanceMax = S->HitChanceMax;
	P.MinDamage = S->MinDamage;
	P.RangedHitChanceBase = S->RangedHitChanceBase;
	P.RangedHitChancePerPoint = S->RangedHitChancePerPoint;

	const float EngageR = S->EngageRangeCm;
	const float EngageRSq = EngageR * EngageR;

	for (ANinjagoUnit* Atk : AllUnits)
	{
		if (!IsValid(Atk) || Atk->LivingModelCount() == 0 || Atk->IsRouting() || Atk->IsStunned())
		{
			continue; // routing units flee, stunned units are frozen; neither fights
		}

		// Ranged units search out to their weapon range; melee units only to engage range.
		const bool bRanged = Atk->IsRangedUnit();
		const float SearchR = bRanged ? FMath::Max(EngageR, Atk->GetRangeCm()) : EngageR;
		const float SearchRSq = SearchR * SearchR;

		const TArray<FNinjagoModel>& AtkModels = Atk->GetModels();
		bool bEngaged = false;

		// A unit's first melee contact after advancing is a charge: bonus damage + morale shock.
		const bool bCharging = Atk->IsChargePending();
		const bool bAtkSpear = Atk->IsSpear();
		const bool bAtkLarge = Atk->IsLarge();
		bool bMeleeThisTick = false;
		TSet<ANinjagoUnit*> Struck;

		for (int32 ai = 0; ai < AtkModels.Num(); ++ai)
		{
			if (!AtkModels[ai].bAlive)
			{
				continue;
			}

			ANinjagoUnit* BestDef = nullptr;
			int32 BestIdx = INDEX_NONE;
			float BestDsq = SearchRSq;

			for (ANinjagoUnit* Def : AllUnits)
			{
				if (!IsValid(Def) || Def->GetTeam() == Atk->GetTeam() || Def->LivingModelCount() == 0)
				{
					continue;
				}
				const TArray<FNinjagoModel>& DefModels = Def->GetModels();
				for (int32 di = 0; di < DefModels.Num(); ++di)
				{
					if (!DefModels[di].bAlive)
					{
						continue;
					}
					const float Dsq = FVector::DistSquared2D(AtkModels[ai].Location, DefModels[di].Location);
					if (Dsq <= BestDsq)
					{
						BestDsq = Dsq;
						BestDef = Def;
						BestIdx = di;
					}
				}
			}

			if (BestIdx == INDEX_NONE)
			{
				continue;
			}

			if (BestDsq <= EngageRSq)
			{
				// In melee range: unified strike with charge, anti-large, and spear-brace modifiers.
				const FNinjagoUnitRow& AR = Atk->GetRow();
				const FNinjagoUnitRow& DR = BestDef->GetRow();
				const int32 Dmg = FNinjagoCombatResolver::ResolveMelee(
					Atk->EffMeleeAttack(), Atk->EffDamage(), AR.ChargeBonus, AR.ArmourPiercing,
					BestDef->EffMeleeDefence(), DR.Armour,
					bCharging, bAtkSpear, bAtkLarge, BestDef->IsSpear(), BestDef->IsLarge(),
					S->SpearAntiLargeBonus, P, CombatRng);
				BestDef->ApplyModelDamage(BestIdx, Dmg);
				bEngaged = true;
				bMeleeThisTick = true;
				Struck.Add(BestDef);
			}
			else if (bRanged && Atk->TryConsumeAmmo(ai))
			{
				// Beyond melee but within range, and this model still has ammo: fire a shot.
				const FNinjagoUnitRow& AR = Atk->GetRow();
				const int32 Dmg = FNinjagoCombatResolver::ResolveRangedAttack(
					AR.RangedAttack, Atk->EffDamage(), AR.ArmourPiercing, BestDef->GetRow().Armour, P, CombatRng);
				BestDef->ApplyModelDamage(BestIdx, Dmg);
				bEngaged = true;

				if (ProjectileFX)
				{
					const FVector Chest(0.f, 0.f, 60.f);
					const FVector Target = BestDef->GetModels()[BestIdx].Location + Chest;
					ProjectileFX->FireShot(AtkModels[ai].Location + Chest, Target);
				}
			}
		}

		Atk->MarkFighting(bEngaged);

		// Deliver the charge's morale shock to each unit struck on impact, then spend the charge.
		if (bCharging && bMeleeThisTick)
		{
			const float Shock = static_cast<float>(FMath::Max(0, Atk->GetRow().ChargeBonus)) * S->MoraleChargeShockScale;
			for (ANinjagoUnit* Hit : Struck)
			{
				if (IsValid(Hit))
				{
					Hit->ApplyMoraleShock(Shock);
				}
			}
			Atk->ClearChargePending();
		}
	}

	MoralePass();
	AbilityAIPass();
	AcquireTargets();
}

void ANinjagoGameMode::AbilityAIPass()
{
	// Units cast their own abilities when it is worthwhile, so both armies fight with their full
	// kit. The player's Spacebar still works as optional, better-timed control.
	for (ANinjagoUnit* Unit : AllUnits)
	{
		if (IsValid(Unit) && Unit->ShouldAIFireAbility())
		{
			Unit->TryFireAbility();
		}
	}
}

ANinjagoUnit* ANinjagoGameMode::NearestEnemyUnit(const ANinjagoUnit* For) const
{
	if (!IsValid(For))
	{
		return nullptr;
	}
	ANinjagoUnit* Nearest = nullptr;
	float BestDsq = TNumericLimits<float>::Max();
	for (ANinjagoUnit* Enemy : AllUnits)
	{
		if (!IsValid(Enemy) || Enemy->GetTeam() == For->GetTeam() || Enemy->LivingModelCount() == 0)
		{
			continue;
		}
		const float Dsq = FVector::DistSquared2D(For->GetActorLocation(), Enemy->GetActorLocation());
		if (Dsq < BestDsq)
		{
			BestDsq = Dsq;
			Nearest = Enemy;
		}
	}
	return Nearest;
}

void ANinjagoGameMode::MoralePass()
{
	for (ANinjagoUnit* Unit : AllUnits)
	{
		if (!IsValid(Unit) || Unit->IsMoraleImmune() || Unit->LivingModelCount() == 0)
		{
			continue;
		}
		// A frozen unit is not "in combat" for morale purposes (its Fighting state may be stale).
		const bool bInCombat = (Unit->GetState() == EUnitState::Fighting && !Unit->IsStunned());
		ANinjagoUnit* Enemy = NearestEnemyUnit(Unit);
		const bool bHasEnemy = (Enemy != nullptr);
		const FVector EnemyLoc = bHasEnemy ? Enemy->GetActorLocation() : FVector::ZeroVector;
		Unit->UpdateMorale(bInCombat, bHasEnemy, EnemyLoc);
	}
}

void ANinjagoGameMode::AcquireTargets()
{
	// Idle, un-ordered units advance to the nearest living enemy unit so the battle closes.
	// Routing units are skipped: they flee, they do not seek fights.
	for (ANinjagoUnit* Unit : AllUnits)
	{
		if (!IsValid(Unit) || Unit->LivingModelCount() == 0 || Unit->HasActiveOrder()
			|| Unit->IsRouting() || Unit->IsStunned() || Unit->GetState() == EUnitState::Fighting)
		{
			continue;
		}

		if (ANinjagoUnit* Nearest = NearestEnemyUnit(Unit))
		{
			Unit->OrderAttack(Nearest);
		}
	}
}

int32 ANinjagoGameMode::LivingCountForTeam(ETeam Team) const
{
	int32 Total = 0;
	for (const ANinjagoUnit* Unit : AllUnits)
	{
		if (IsValid(Unit) && Unit->GetTeam() == Team)
		{
			Total += Unit->LivingModelCount();
		}
	}
	return Total;
}

void ANinjagoGameMode::CheckWinCondition()
{
	if (bResolved)
	{
		return;
	}

	// A team is only beaten when it has no living models AND none reassembling ("Already Dead").
	int32 Ninja = 0;
	int32 Skulkin = 0;
	for (const ANinjagoUnit* Unit : AllUnits)
	{
		if (!IsValid(Unit))
		{
			continue;
		}
		const int32 Remaining = Unit->LivingModelCount() + Unit->PendingReviveCount();
		// Use allegiance, not current team, so temporarily mind-controlled units are not counted as
		// having destroyed their own side.
		((Unit->GetAllegianceTeam() == ETeam::Ninja) ? Ninja : Skulkin) += Remaining;
	}

	if (Ninja > 0 && Skulkin > 0)
	{
		return;
	}

	bResolved = true;
	GetWorldTimerManager().ClearTimer(CombatTimer);

	FString Result;
	if (Ninja == 0 && Skulkin == 0)
	{
		Result = TEXT("Draw");
	}
	else
	{
		Result = (Skulkin == 0) ? TEXT("Ninja win") : TEXT("Skulkin win");
	}

	UE_LOG(LogNinjago, Log, TEXT("Battle resolved: %s"), *Result);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(/*Key*/ 1, /*Time*/ 1.0e9f, FColor::Yellow, Result);
	}
}
