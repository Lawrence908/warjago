// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Core/NinjagoGameMode.h"
#include "Core/NinjagoBattleSetup.h"
#include "Core/NinjagoSettings.h"
#include "Player/NinjagoCameraPawn.h"
#include "Player/NinjagoPlayerController.h"
#include "Units/NinjagoUnit.h"
#include "Combat/NinjagoCombatResolver.h"
#include "NinjagoLog.h"

#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
	const TCHAR* DefaultBattlePath = TEXT("/Game/Data/DA_DefaultBattle.DA_DefaultBattle");
	const TCHAR* UnitTablePath = TEXT("/Game/Data/dt_units.dt_units");

	const TArray<FName> DefaultNinja  = { TEXT("HRO_KAI"), TEXT("HRO_JAY"), TEXT("HRO_COLE"), TEXT("HRO_ZANE") };
	const TArray<FName> DefaultSkulkin = { TEXT("SKU_MINERS"), TEXT("SKU_WARRIORS"), TEXT("SKU_WATCHMEN"), TEXT("LRD_SAMUKAI") };
}

ANinjagoGameMode::ANinjagoGameMode()
{
	DefaultPawnClass = ANinjagoCameraPawn::StaticClass();
	PlayerControllerClass = ANinjagoPlayerController::StaticClass();
	CombatRng.Initialize(20260725);
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
	const int32 N = Army.Num();
	for (int32 i = 0; i < N; ++i)
	{
		const float Y = (i - (N - 1) * 0.5f) * UnitSpacing;
		SpawnUnit(Table, Army[i], Team, FVector(LineX, Y, 0.f), FRotator(0.f, Yaw, 0.f));
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

	const float EngageRSq = S->EngageRangeCm * S->EngageRangeCm;

	for (ANinjagoUnit* Atk : AllUnits)
	{
		if (!IsValid(Atk) || Atk->LivingModelCount() == 0)
		{
			continue;
		}

		const TArray<FNinjagoModel>& AtkModels = Atk->GetModels();
		bool bEngaged = false;

		for (int32 ai = 0; ai < AtkModels.Num(); ++ai)
		{
			if (!AtkModels[ai].bAlive)
			{
				continue;
			}

			ANinjagoUnit* BestDef = nullptr;
			int32 BestIdx = INDEX_NONE;
			float BestDsq = EngageRSq;

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

			if (BestIdx != INDEX_NONE)
			{
				bEngaged = true;
				const int32 Dmg = FNinjagoCombatResolver::ResolveAttack(Atk->GetRow(), BestDef->GetRow(), P, CombatRng);
				BestDef->ApplyModelDamage(BestIdx, Dmg);
			}
		}

		Atk->MarkFighting(bEngaged);
	}

	AcquireTargets();
}

void ANinjagoGameMode::AcquireTargets()
{
	// Idle, un-ordered units advance to the nearest living enemy unit so the battle closes.
	for (ANinjagoUnit* Unit : AllUnits)
	{
		if (!IsValid(Unit) || Unit->LivingModelCount() == 0 || Unit->HasActiveOrder()
			|| Unit->GetState() == EUnitState::Fighting)
		{
			continue;
		}

		ANinjagoUnit* Nearest = nullptr;
		float BestDsq = TNumericLimits<float>::Max();
		for (ANinjagoUnit* Enemy : AllUnits)
		{
			if (!IsValid(Enemy) || Enemy->GetTeam() == Unit->GetTeam() || Enemy->LivingModelCount() == 0)
			{
				continue;
			}
			const float Dsq = FVector::DistSquared2D(Unit->GetActorLocation(), Enemy->GetActorLocation());
			if (Dsq < BestDsq)
			{
				BestDsq = Dsq;
				Nearest = Enemy;
			}
		}

		if (Nearest)
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

	const int32 Ninja = LivingCountForTeam(ETeam::Ninja);
	const int32 Skulkin = LivingCountForTeam(ETeam::Skulkin);

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
