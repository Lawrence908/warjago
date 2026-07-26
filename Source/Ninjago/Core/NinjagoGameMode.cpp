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
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* DefaultBattlePath = TEXT("/Game/Data/DA_DefaultBattle.DA_DefaultBattle");
	const TCHAR* UnitTablePath = TEXT("/Game/Data/dt_units.dt_units");

	// Default battle spans the systems: melee, ranged (v0.2), a breakable block for routing (v0.3),
	// and a large monster NIN_SAMURAIX (v0.5) for the Skulkin spears (Watchmen) to brace against.
	// Skulkin are morale-immune by faction and never rout.
	// LRD_ICEEMPEROR is a guest freeze-caster (v0.9): select him and press Space to freeze Skulkin.
	// LRD_SKALES is a guest hypnotist (v0.10): select him and press Space to mind-control a Skulkin unit.
	// HRO_RONIN is a Ninja with Vanish (v0.17). LRD_HARUMI is a guest with passive stealth (v0.19):
	// she starts invisible and reveals for good on her first strike.
	const TArray<FName> DefaultNinja  = { TEXT("HRO_KAI"), TEXT("HRO_JAY"), TEXT("HRO_COLE"), TEXT("HRO_ZANE"), TEXT("NIN_SHINTARO"), TEXT("NIN_SOLDIERS"), TEXT("NIN_SAMURAIX"), TEXT("LRD_ICEEMPEROR"), TEXT("LRD_SKALES"), TEXT("HRO_RONIN"), TEXT("LRD_HARUMI") };
	// HRO_WYPLASH: def+3 ally buff (v0.7). HRO_MACHIA: guest resurrector (v0.15). LRD_CLOUSE: guest
	// summoner (v0.21) who calls in a giant serpent to fight for the Skulkin.
	const TArray<FName> DefaultSkulkin = { TEXT("SKU_MINERS"), TEXT("SKU_WARRIORS"), TEXT("SKU_WATCHMEN"), TEXT("LRD_SAMUKAI"), TEXT("SKU_ENGINEERS"), TEXT("HRO_WYPLASH"), TEXT("HRO_MACHIA"), TEXT("LRD_CLOUSE") };

	// Which preset the code-default battle spawns. Persists across level reloads within a session.
	int32 GScenarioIndex = 0;

	// 2D segment intersection. If AB crosses CD, returns true and the parameter T along AB (0..1).
	bool SegmentsCross(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D, float& OutT)
	{
		const FVector2D AB = B - A;
		const FVector2D CD = D - C;
		const float Denom = AB.X * CD.Y - AB.Y * CD.X;
		if (FMath::IsNearlyZero(Denom))
		{
			return false; // parallel
		}
		const FVector2D AC = C - A;
		const float T = (AC.X * CD.Y - AC.Y * CD.X) / Denom;
		const float U = (AC.X * AB.Y - AC.Y * AB.X) / Denom;
		if (T >= 0.f && T <= 1.f && U >= 0.f && U <= 1.f)
		{
			OutT = T;
			return true;
		}
		return false;
	}

	// Fill the two army rosters and the display name for a preset scenario (keys 1-4 in play).
	void GetScenario(int32 Index, TArray<FName>& OutNinja, TArray<FName>& OutSkulkin, FString& OutName)
	{
		switch (Index)
		{
		case 1:
			OutName = TEXT("Heroes vs Horde");
			OutNinja = { TEXT("HRO_KAI"), TEXT("HRO_JAY"), TEXT("HRO_COLE"), TEXT("HRO_ZANE") };
			OutSkulkin = { TEXT("SKU_MINERS"), TEXT("SKU_MINERS"), TEXT("SKU_WARRIORS"), TEXT("SKU_WARRIORS"), TEXT("SKU_MINERS"), TEXT("SKU_WATCHMEN") };
			break;
		case 2:
			OutName = TEXT("Giant Brawl");
			OutNinja = { TEXT("NIN_SAMURAIX"), TEXT("STO_GIANT"), TEXT("HRO_KAI") };
			OutSkulkin = { TEXT("NDR_MECHDRAGON"), TEXT("SER_DEVOURER"), TEXT("LRD_SAMUKAI") };
			break;
		case 3:
			OutName = TEXT("Skirmish (ranged)");
			OutNinja = { TEXT("NIN_SHINTARO"), TEXT("STO_SCOUTS"), TEXT("NIN_SHINTARO") };
			OutSkulkin = { TEXT("SKU_ENGINEERS"), TEXT("VER_ARCHERS"), TEXT("SKU_ENGINEERS") };
			break;
		default:
			OutName = TEXT("Grand Battle");
			OutNinja = DefaultNinja;
			OutSkulkin = DefaultSkulkin;
			break;
		}
	}
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

	// Ice walls: age out expired ones and draw the rest as a cyan barrier.
	for (int32 i = ActiveWalls.Num() - 1; i >= 0; --i)
	{
		ActiveWalls[i].Remaining -= DeltaSeconds;
		if (ActiveWalls[i].Remaining <= 0.f)
		{
			ActiveWalls.RemoveAtSwap(i);
			continue;
		}
		const FNinjagoWall& W = ActiveWalls[i];
		const FVector W1 = W.Center - W.Dir * W.HalfLength;
		const FVector W2 = W.Center + W.Dir * W.HalfLength;
		const FVector Up(0.f, 0.f, 220.f);
		const FColor Ice(140, 210, 255);
		DrawDebugLine(GetWorld(), W1, W2, Ice, false, -1.f, 0, 18.f);
		DrawDebugLine(GetWorld(), W1 + Up, W2 + Up, Ice, false, -1.f, 0, 18.f);
		DrawDebugLine(GetWorld(), W1, W1 + Up, Ice, false, -1.f, 0, 18.f);
		DrawDebugLine(GetWorld(), W2, W2 + Up, Ice, false, -1.f, 0, 18.f);
		DrawDebugLine(GetWorld(), W.Center, W.Center + Up, Ice, false, -1.f, 0, 18.f);
	}

	// Per-unit health bar (green when full, red when nearly dead) and status markers.
	for (ANinjagoUnit* Unit : AllUnits)
	{
		if (!IsValid(Unit) || Unit->LivingModelCount() == 0)
		{
			continue;
		}

		if (!Unit->IsStealthed()) // a vanished unit shows only a shimmer, no health bar
		{
			const float Frac = Unit->GetStrengthFraction();
			const FVector BarBase = Unit->GetActorLocation() + FVector(0.f, 0.f, 175.f);
			const float HalfW = 120.f;
			const FVector BarL = BarBase - FVector(HalfW, 0.f, 0.f);
			const FVector BarR = BarBase + FVector(HalfW, 0.f, 0.f);
			const FVector BarFill = BarL + (BarR - BarL) * Frac;
			const FColor FillColour(static_cast<uint8>((1.f - Frac) * 255.f), static_cast<uint8>(Frac * 255.f), 40);
			DrawDebugLine(GetWorld(), BarL, BarR, FColor(25, 25, 25), false, -1.f, 0, 16.f);
			DrawDebugLine(GetWorld(), BarL, BarFill, FillColour, false, -1.f, 0, 16.f);
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
		if (Unit->IsStealthed())
		{
			// A faint shimmer shows the player where their invisible (vanished) unit is.
			const FVector Base = Unit->GetActorLocation() + FVector(0.f, 0.f, 90.f);
			DrawDebugSphere(GetWorld(), Base, 90.f, 12, FColor(180, 180, 220), false, -1.f, 0, 2.f);
		}
	}

	// On-screen team tally and scenario name so the state of the battle is readable at a glance.
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(19, 0.f, FColor(200, 200, 200),
			FString::Printf(TEXT("%s   (keys 1-4 to switch battles, R to restart)"), *ActiveScenarioName));
		if (bCombatHasRun)
		{
			GEngine->AddOnScreenDebugMessage(20, 0.f, FColor(120, 180, 255),
				FString::Printf(TEXT("Ninja: %d"), LivingCountForTeam(ETeam::Ninja)));
			GEngine->AddOnScreenDebugMessage(21, 0.f, FColor(230, 120, 120),
				FString::Printf(TEXT("Skulkin: %d"), LivingCountForTeam(ETeam::Skulkin)));
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

	const int32 NinjaStart = LivingCountForTeam(ETeam::Ninja);
	const int32 SkulkinStart = LivingCountForTeam(ETeam::Skulkin);
	UE_LOG(LogNinjago, Log, TEXT("Battle start: %d units (Ninja %d models, Skulkin %d models)"),
		AllUnits.Num(), NinjaStart, SkulkinStart);
	if (NinjaStart == 0 || SkulkinStart == 0)
	{
		UE_LOG(LogNinjago, Warning, TEXT("A side started with no models; the battle will resolve immediately. Check the roster."));
	}

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
	TArray<FName> Ninja, Skulkin;
	FString Name;
	GetScenario(GScenarioIndex, Ninja, Skulkin, Name);
	ActiveScenarioName = Name;

	const float HalfSep = 1750.f;
	SpawnArmyLine(Table, Ninja,   ETeam::Ninja,   -HalfSep,   0.f, 900.f);
	SpawnArmyLine(Table, Skulkin, ETeam::Skulkin,  HalfSep, 180.f, 900.f);
}

void ANinjagoGameMode::LoadScenario(int32 Index)
{
	GScenarioIndex = FMath::Max(0, Index);
	RestartBattle(); // reload; the new StartPlay spawns the chosen scenario
}

void ANinjagoGameMode::RaiseWall(const FVector& Center, const FVector& Dir, float HalfLength, float Duration)
{
	FNinjagoWall Wall;
	Wall.Center = Center;
	Wall.Dir = Dir.GetSafeNormal2D();
	Wall.HalfLength = FMath::Max(50.f, HalfLength);
	Wall.Remaining = FMath::Max(1.f, Duration);
	ActiveWalls.Add(Wall);
}

FVector ANinjagoGameMode::BlockMovement(const FVector& From, const FVector& To) const
{
	if (ActiveWalls.Num() == 0)
	{
		return To;
	}
	const FVector2D A(From.X, From.Y);
	const FVector2D B(To.X, To.Y);

	float NearestT = 1.f;
	bool bBlocked = false;
	for (const FNinjagoWall& Wall : ActiveWalls)
	{
		const FVector2D WDir(Wall.Dir.X, Wall.Dir.Y);
		const FVector2D WC(Wall.Center.X, Wall.Center.Y);
		const FVector2D W1 = WC - WDir * Wall.HalfLength;
		const FVector2D W2 = WC + WDir * Wall.HalfLength;
		float T = 1.f;
		if (SegmentsCross(A, B, W1, W2, T) && T < NearestT)
		{
			NearestT = T;
			bBlocked = true;
		}
	}
	if (!bBlocked)
	{
		return To;
	}
	// Stop just short of the wall.
	const FVector2D Stop = A + (B - A) * (NearestT * 0.9f);
	return FVector(Stop.X, Stop.Y, To.Z);
}

bool ANinjagoGameMode::IsPathBlocked(const FVector& From, const FVector& To) const
{
	if (ActiveWalls.Num() == 0)
	{
		return false;
	}
	const FVector2D A(From.X, From.Y);
	const FVector2D B(To.X, To.Y);
	for (const FNinjagoWall& Wall : ActiveWalls)
	{
		const FVector2D WDir(Wall.Dir.X, Wall.Dir.Y);
		const FVector2D WC(Wall.Center.X, Wall.Center.Y);
		const FVector2D W1 = WC - WDir * Wall.HalfLength;
		const FVector2D W2 = WC + WDir * Wall.HalfLength;
		float T = 1.f;
		if (SegmentsCross(A, B, W1, W2, T))
		{
			return true;
		}
	}
	return false;
}

void ANinjagoGameMode::FlushPendingSummons()
{
	// Move queued summons into the live combat set. Called only at safe points (never mid-iteration).
	if (PendingSummons.Num() > 0)
	{
		AllUnits.Append(PendingSummons);
		PendingSummons.Reset();
	}
}

void ANinjagoGameMode::RequestSummon(ETeam Team, const FVector& Location, float Lifetime)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, UnitTablePath);
	const FName RowKey = GetDefault<UNinjagoSettings>()->SummonedUnitRow;
	if (!Table || RowKey.IsNone())
	{
		return;
	}
	if (ANinjagoUnit* Summoned = SpawnUnit(Table, RowKey, Team, Location, FRotator::ZeroRotator))
	{
		Summoned->SetSummoned(Lifetime);
		PendingSummons.Add(Summoned); // do not touch AllUnits here; flushed at the next combat tick
	}
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
	bCombatHasRun = true;
	EngagedThisTick.Reset();
	FlushPendingSummons(); // summons join the combat set here, before any iteration this tick

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
	const bool bWallsActive = ActiveWalls.Num() > 0; // only pay the wall cost when a wall exists

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
				if (!IsValid(Def) || Def->GetTeam() == Atk->GetTeam() || Def->LivingModelCount() == 0
					|| Def->IsStealthed())
				{
					continue; // a vanished unit cannot be targeted
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
						// Cannot fight through an Ice Wall: skip a target the wall stands between.
						if (bWallsActive && IsPathBlocked(AtkModels[ai].Location, DefModels[di].Location))
						{
							continue;
						}
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
				const bool bCrit = Atk->HasCritPending(); // a Vanish crit is a guaranteed hit
				int32 Dmg = FNinjagoCombatResolver::ResolveMelee(
					Atk->EffMeleeAttack(), Atk->EffDamage(), AR.ChargeBonus, AR.ArmourPiercing,
					BestDef->EffMeleeDefence(), DR.Armour,
					bCharging, bAtkSpear, bAtkLarge, BestDef->IsSpear(), BestDef->IsLarge(),
					S->SpearAntiLargeBonus, P, CombatRng, bCrit);
				Dmg *= Atk->ConsumeCritMultiplier(); // Vanish crit (x1 for everyone else)
				if (BestDef->ApplyModelDamage(BestIdx, Dmg))
				{
					Atk->AddKill();
				}
				bEngaged = true;
				bMeleeThisTick = true;
				Struck.Add(BestDef);
				EngagedThisTick.Add(BestDef); // the defender is in combat too (drives its morale)
			}
			else if (bRanged && Atk->TryConsumeAmmo(ai))
			{
				// Beyond melee but within range, and this model still has ammo: fire a shot.
				const FNinjagoUnitRow& AR = Atk->GetRow();
				const bool bCrit = Atk->HasCritPending();
				int32 Dmg = FNinjagoCombatResolver::ResolveRangedAttack(
					AR.RangedAttack, Atk->EffDamage(), AR.ArmourPiercing, BestDef->GetRow().Armour, P, CombatRng, bCrit);
				Dmg *= Atk->ConsumeCritMultiplier(); // Vanish crit (x1 for everyone else)
				if (BestDef->ApplyModelDamage(BestIdx, Dmg))
				{
					Atk->AddKill();
				}
				bEngaged = true;
				EngagedThisTick.Add(BestDef);

				if (ProjectileFX)
				{
					const FVector Chest(0.f, 0.f, 60.f);
					const FVector Target = BestDef->GetModels()[BestIdx].Location + Chest;
					ProjectileFX->FireShot(AtkModels[ai].Location + Chest, Target);
				}
			}
		}

		Atk->MarkFighting(bEngaged);
		if (bEngaged)
		{
			EngagedThisTick.Add(Atk);
			Atk->Reveal(); // a passive-stealth unit reveals once it attacks (vanish self-reveals earlier)
		}

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

	// Abilities before morale, so casualties from AI casts this tick are felt this tick.
	AbilityAIPass();
	MoralePass();
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
		if (!IsValid(Enemy) || Enemy->GetTeam() == For->GetTeam() || Enemy->LivingModelCount() == 0
			|| Enemy->IsStealthed())
		{
			continue; // cannot seek a vanished enemy
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
		// "In combat" = dealt or took a hit this tick (attackers and defenders), and not frozen.
		// This means a unit under fire does not regen morale even if it has no target of its own.
		const bool bInCombat = EngagedThisTick.Contains(Unit) && !Unit->IsStunned();
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
		if (IsValid(Unit) && Unit->GetAllegianceTeam() == Team)
		{
			Total += Unit->LivingModelCount();
		}
	}
	return Total;
}

void ANinjagoGameMode::CheckWinCondition()
{
	if (bResolved || !bCombatHasRun)
	{
		return; // do not declare a winner before the first combat tick has actually run
	}

	// Count any just-summoned units toward their side, so a pending summon is not lost to an
	// early "win" if its summoner died on the same tick it was cast.
	FlushPendingSummons();

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
	GetWorldTimerManager().ClearTimer(WinTimer);

	// Freeze the tableau under the result banner: stop the units ticking.
	for (ANinjagoUnit* Unit : AllUnits)
	{
		if (IsValid(Unit))
		{
			Unit->SetActorTickEnabled(false);
		}
	}

	FString Result;
	if (Ninja == 0 && Skulkin == 0)
	{
		Result = TEXT("Draw");
	}
	else
	{
		Result = (Skulkin == 0) ? TEXT("Ninja win") : TEXT("Skulkin win");
	}

	// Battle MVP: the unit that killed the most enemy models.
	ANinjagoUnit* Mvp = nullptr;
	int32 BestKills = 0;
	for (ANinjagoUnit* Unit : AllUnits)
	{
		if (IsValid(Unit) && Unit->GetKillCount() > BestKills)
		{
			BestKills = Unit->GetKillCount();
			Mvp = Unit;
		}
	}
	if (Mvp)
	{
		Result += FString::Printf(TEXT("   -   MVP: %s (%d kills)"), *Mvp->GetRow().DisplayName, BestKills);
	}

	UE_LOG(LogNinjago, Log, TEXT("Battle resolved: %s"), *Result);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(/*Key*/ 1, /*Time*/ 1.0e9f, FColor::Yellow, Result + TEXT("  -  press R to fight again"));
	}

	// Auto-restart so battles loop for a watching child; R restarts sooner.
	const float AutoRestart = GetDefault<UNinjagoSettings>()->AutoRestartSeconds;
	if (AutoRestart > 0.f)
	{
		GetWorldTimerManager().SetTimer(RestartTimer, this, &ANinjagoGameMode::RestartBattle, AutoRestart, false);
	}
}

void ANinjagoGameMode::RestartBattle()
{
	// Reloading the level re-runs StartPlay and spawns a fresh battle.
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}
