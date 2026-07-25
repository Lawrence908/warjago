// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Units/NinjagoUnit.h"
#include "Units/NinjagoModelRenderer.h"
#include "Units/NinjagoFormation.h"
#include "Core/NinjagoSettings.h"
#include "NinjagoLog.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
	const TCHAR* AbilityTablePath = TEXT("/Game/Data/dt_abilities.dt_abilities");

	// Pull the integer after "dmg=" out of a Magnitude string ("dmg=45", "dmg=40/hit").
	int32 ParseDamageMagnitude(const FString& Magnitude)
	{
		const FString Key(TEXT("dmg="));
		int32 Start = Magnitude.Find(Key, ESearchCase::IgnoreCase);
		if (Start == INDEX_NONE)
		{
			return 0;
		}
		Start += Key.Len();
		FString Digits;
		for (int32 i = Start; i < Magnitude.Len(); ++i)
		{
			const TCHAR Ch = Magnitude[i];
			if (Ch >= TEXT('0') && Ch <= TEXT('9'))
			{
				Digits.AppendChar(Ch);
			}
			else
			{
				break;
			}
		}
		return Digits.IsEmpty() ? 0 : FCString::Atoi(*Digits);
	}
}

ANinjagoUnit::ANinjagoUnit()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Renderer = CreateDefaultSubobject<UNinjagoModelRenderer>(TEXT("ModelRenderer"));

	SelectionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SelectionVolume"));
	SelectionVolume->SetupAttachment(SceneRoot);
	SelectionVolume->SetBoxExtent(FVector(100.f, 100.f, 100.f));
	SelectionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SelectionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	SelectionVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SelectionVolume->SetCollisionObjectType(ECC_WorldDynamic);
}

void ANinjagoUnit::BeginPlay()
{
	Super::BeginPlay();

	InitialiseFromRow();

	if (bRowValid && Renderer)
	{
		Renderer->SetupForUnit(SceneRoot, CachedRow);
		const float ModelScale = CachedRow.ModelScale > 0.f ? CachedRow.ModelScale : 1.f;
		Renderer->UpdateInstances(Models, ModelScale);
	}
}

void ANinjagoUnit::InitialiseFromRow()
{
	static const FString Context(TEXT("ANinjagoUnit::InitialiseFromRow"));
	const FNinjagoUnitRow* Row = UnitRowHandle.GetRow<FNinjagoUnitRow>(Context);
	if (!Row)
	{
		UE_LOG(LogNinjago, Error, TEXT("%s: no valid unit row set on %s"), *Context, *GetName());
		bRowValid = false;
		return;
	}

	CachedRow = *Row;
	bRowValid = true;
	State = EUnitState::Idle;
	BuildModels();
	SizeSelectionVolume();
	CacheAbility();

	UE_LOG(LogNinjago, Log, TEXT("Spawned %s (%s) with %d model(s) for team %d"),
		*CachedRow.DisplayName, *UnitRowHandle.RowName.ToString(), Models.Num(), static_cast<int32>(Team));
}

void ANinjagoUnit::BuildModels()
{
	const int32 Count = FMath::Max(1, CachedRow.UnitSize);
	Models.Reset(Count);

	const float Yaw = GetActorRotation().Yaw;
	for (int32 i = 0; i < Count; ++i)
	{
		FNinjagoModel M;
		M.SlotIndex = (Count == 1) ? INDEX_NONE : i;
		M.Hp = FMath::Max(1, CachedRow.HpPerModel);
		M.Yaw = Yaw;
		M.Location = SlotWorldLocation(i, Count);
		M.bAlive = true;
		Models.Add(M);
	}
}

void ANinjagoUnit::SizeSelectionVolume()
{
	if (!SelectionVolume)
	{
		return;
	}

	const int32 Count = FMath::Max(1, Models.Num());
	const float Spacing = GetDefault<UNinjagoSettings>()->ModelSpacingCm;
	const int32 Cols = FNinjagoFormation::Columns(Count);
	const int32 Rows = FMath::DivideAndRoundUp(Count, Cols);

	const float HalfWidth = FMath::Max(Cols, 1) * Spacing * 0.5f + Spacing * 0.5f;
	const float HalfDepth = FMath::Max(Rows, 1) * Spacing * 0.5f + Spacing * 0.5f;
	const float Height = 130.f * (CachedRow.ModelScale > 0.f ? CachedRow.ModelScale : 1.f);

	SelectionVolume->SetBoxExtent(FVector(HalfWidth, HalfDepth, Height * 0.5f), false);
	SelectionVolume->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f));
}

FVector ANinjagoUnit::SlotWorldLocation(int32 SlotIndex, int32 Count) const
{
	const float Spacing = GetDefault<UNinjagoSettings>()->ModelSpacingCm;
	const FVector2D Local = FNinjagoFormation::SlotOffset(SlotIndex, Count, Spacing);

	const FVector Local3D(Local.X, Local.Y, 0.f);
	const FVector Rotated = GetActorRotation().RotateVector(Local3D);
	return GetActorLocation() + Rotated;
}

void ANinjagoUnit::SteerModels(float DeltaSeconds)
{
	if (!bRowValid || Models.Num() == 0)
	{
		return;
	}

	const float Speed = FMath::Max(0.f, CachedRow.SpeedCmS);
	const int32 Count = Models.Num();

	// Straight-line steer toward each model's formation slot.
	for (FNinjagoModel& M : Models)
	{
		if (!M.bAlive)
		{
			continue;
		}
		const FVector Target = (M.SlotIndex == INDEX_NONE)
			? GetActorLocation()
			: SlotWorldLocation(M.SlotIndex, Count);

		const FVector ToTarget = Target - M.Location;
		const float Dist = ToTarget.Size2D();
		if (Dist > KINDA_SMALL_NUMBER)
		{
			const float Step = FMath::Min(Dist, Speed * DeltaSeconds);
			M.Location += ToTarget.GetSafeNormal2D() * Step;
			M.Yaw = ToTarget.Rotation().Yaw;
		}
	}

	// Simple pairwise separation so models never stack exactly.
	const float SepRadius = GetDefault<UNinjagoSettings>()->ModelSpacingCm * 0.5f;
	for (int32 i = 0; i < Count; ++i)
	{
		if (!Models[i].bAlive)
		{
			continue;
		}
		for (int32 j = i + 1; j < Count; ++j)
		{
			if (!Models[j].bAlive)
			{
				continue;
			}
			FVector Delta = Models[i].Location - Models[j].Location;
			Delta.Z = 0.f;
			const float D = Delta.Size();
			if (D > KINDA_SMALL_NUMBER && D < SepRadius)
			{
				const FVector Push = Delta.GetSafeNormal() * ((SepRadius - D) * 0.5f);
				Models[i].Location += Push;
				Models[j].Location -= Push;
			}
		}
	}
}

void ANinjagoUnit::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bRowValid)
	{
		return;
	}

	if (AbilityCooldownRemaining > 0.f)
	{
		AbilityCooldownRemaining = FMath::Max(0.f, AbilityCooldownRemaining - DeltaSeconds);
	}

	// Advance the unit anchor toward a move/attack order; models steer to their slots relative to
	// it, so the regiment walks in formation rather than teleporting. Combat itself lands in M6.
	if (OrderType == EOrderType::Attack && !AttackTarget.IsValid())
	{
		OrderType = EOrderType::NoOrder;
		State = EUnitState::Idle;
	}

	if (OrderType == EOrderType::Move || OrderType == EOrderType::Attack)
	{
		const bool bAttack = (OrderType == EOrderType::Attack);
		const FVector Anchor = GetActorLocation();
		const FVector RawTarget = bAttack ? AttackTarget->GetActorLocation() : OrderLocation;
		FVector ToTarget = FVector(RawTarget.X, RawTarget.Y, Anchor.Z) - Anchor;
		const float Dist = ToTarget.Size2D();
		const float Speed = FMath::Max(0.f, CachedRow.SpeedCmS);

		// For attack orders, stop short at contact range and hold (M6 resolves the fight there).
		const float StopDist = bAttack ? GetDefault<UNinjagoSettings>()->EngageRangeCm : 0.f;

		if (Dist <= FMath::Max(StopDist, Speed * DeltaSeconds))
		{
			if (!bAttack)
			{
				SetActorLocation(FVector(RawTarget.X, RawTarget.Y, Anchor.Z));
				OrderType = EOrderType::NoOrder;
				State = EUnitState::Idle;
			}
		}
		else
		{
			SetActorLocation(Anchor + ToTarget.GetSafeNormal2D() * Speed * DeltaSeconds);
		}
	}

	SteerModels(DeltaSeconds);

	if (Renderer)
	{
		const float ModelScale = CachedRow.ModelScale > 0.f ? CachedRow.ModelScale : 1.f;
		Renderer->UpdateInstances(Models, ModelScale);
	}
}

int32 ANinjagoUnit::LivingModelCount() const
{
	int32 Alive = 0;
	for (const FNinjagoModel& M : Models)
	{
		if (M.bAlive)
		{
			++Alive;
		}
	}
	return Alive;
}

void ANinjagoUnit::OrderMoveTo(const FVector& WorldTarget)
{
	OrderType = EOrderType::Move;
	OrderLocation = WorldTarget;
	AttackTarget.Reset();
	State = EUnitState::Moving;
	// The anchor advances toward OrderLocation in Tick; models follow their slots.
}

void ANinjagoUnit::ConfigureSpawn(UDataTable* Table, FName RowName, ETeam InTeam)
{
	UnitRowHandle.DataTable = Table;
	UnitRowHandle.RowName = RowName;
	Team = InTeam;
}

void ANinjagoUnit::ApplyModelDamage(int32 ModelIndex, int32 Damage)
{
	if (!Models.IsValidIndex(ModelIndex))
	{
		return;
	}
	FNinjagoModel& M = Models[ModelIndex];
	if (!M.bAlive)
	{
		return;
	}
	M.Hp -= FMath::Max(0, Damage);
	if (M.Hp <= 0)
	{
		M.Hp = 0;
		M.bAlive = false;
	}
	if (LivingModelCount() == 0)
	{
		State = EUnitState::Dead;
	}
}

void ANinjagoUnit::MarkFighting(bool bEngaged)
{
	if (LivingModelCount() == 0)
	{
		State = EUnitState::Dead;
		return;
	}
	if (bEngaged)
	{
		State = EUnitState::Fighting;
	}
	else if (State == EUnitState::Fighting)
	{
		State = EUnitState::Idle;
	}
}

void ANinjagoUnit::OrderAttack(ANinjagoUnit* Target)
{
	if (!Target || Target == this)
	{
		return;
	}
	OrderType = EOrderType::Attack;
	AttackTarget = Target;
	State = EUnitState::Moving;
	UE_LOG(LogNinjago, Verbose, TEXT("%s ordered to attack %s"), *GetName(), *Target->GetName());
}

void ANinjagoUnit::CacheAbility()
{
	bHasAbility = false;
	if (CachedRow.Ability.IsNone())
	{
		return;
	}

	UDataTable* Table = LoadObject<UDataTable>(nullptr, AbilityTablePath);
	if (!Table)
	{
		UE_LOG(LogNinjago, Warning, TEXT("Ability table not found at %s; %s cannot use %s."),
			AbilityTablePath, *GetName(), *CachedRow.Ability.ToString());
		return;
	}

	static const FString Context(TEXT("ANinjagoUnit::CacheAbility"));
	if (const FNinjagoAbilityRow* Row = Table->FindRow<FNinjagoAbilityRow>(CachedRow.Ability, Context))
	{
		CachedAbility = *Row;
		bHasAbility = true;
	}
}

bool ANinjagoUnit::CanFireAbility() const
{
	return bHasAbility && AbilityCooldownRemaining <= 0.f && LivingModelCount() > 0;
}

float ANinjagoUnit::GetAbilityCooldownFraction() const
{
	if (!bHasAbility || CachedAbility.CooldownS <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(AbilityCooldownRemaining / CachedAbility.CooldownS, 0.f, 1.f);
}

void ANinjagoUnit::ApplyAbilityDamageInRadius(const FVector& Center, float Radius, int32 Damage)
{
	if (Damage <= 0 || Radius <= 0.f)
	{
		return;
	}
	const float RadiusSq = Radius * Radius;
	for (TActorIterator<ANinjagoUnit> It(GetWorld()); It; ++It)
	{
		ANinjagoUnit* Enemy = *It;
		if (!IsValid(Enemy) || Enemy->GetTeam() == Team || Enemy->LivingModelCount() == 0)
		{
			continue;
		}
		const TArray<FNinjagoModel>& EnemyModels = Enemy->GetModels();
		for (int32 i = 0; i < EnemyModels.Num(); ++i)
		{
			if (EnemyModels[i].bAlive && FVector::DistSquared2D(Center, EnemyModels[i].Location) <= RadiusSq)
			{
				Enemy->ApplyModelDamage(i, Damage);
			}
		}
	}
}

void ANinjagoUnit::AbilityChannelTick()
{
	if (RemainingChannelTicks <= 0 || LivingModelCount() == 0)
	{
		GetWorldTimerManager().ClearTimer(AbilityChannelTimer);
		return;
	}
	ApplyAbilityDamageInRadius(GetActorLocation(), CachedAbility.RadiusCm, ChannelDamagePerTick);
	--RemainingChannelTicks;
}

bool ANinjagoUnit::TryFireAbility()
{
	if (!CanFireAbility())
	{
		return false;
	}

	const int32 Magnitude = ParseDamageMagnitude(CachedAbility.Magnitude);

	// No aiming UI (one button): both v0.1 abilities are centred on the Lord.
	if (CachedAbility.DurationS > 0.f)
	{
		// Channeled: N ticks/sec for DurationS seconds, Magnitude damage per tick in radius.
		const float Hz = FMath::Max(1.f, GetDefault<UNinjagoSettings>()->AbilityChannelTicksPerSecond);
		ChannelDamagePerTick = Magnitude;
		RemainingChannelTicks = FMath::Max(1, FMath::RoundToInt(CachedAbility.DurationS * Hz));
		AbilityChannelTick(); // first hit immediately
		GetWorldTimerManager().SetTimer(AbilityChannelTimer, this, &ANinjagoUnit::AbilityChannelTick, 1.f / Hz, true, 1.f / Hz);
	}
	else
	{
		ApplyAbilityDamageInRadius(GetActorLocation(), CachedAbility.RadiusCm, Magnitude);
	}

	AbilityCooldownRemaining = CachedAbility.CooldownS;
	UE_LOG(LogNinjago, Log, TEXT("%s fired %s (radius %.0f, dmg %d, cd %.0fs)"),
		*GetName(), *CachedAbility.DisplayName, CachedAbility.RadiusCm, Magnitude, CachedAbility.CooldownS);
	return true;
}
