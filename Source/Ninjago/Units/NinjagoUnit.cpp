// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Units/NinjagoUnit.h"
#include "Units/NinjagoModelRenderer.h"
#include "Units/NinjagoFormation.h"
#include "Combat/NinjagoMoraleResolver.h"
#include "Combat/NinjagoAbilityEffect.h"
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

	// Morale init: Morale == 99 is the "immune" flag, not a value.
	InitialSize = Models.Num();
	LastLivingCount = InitialSize;
	MaxMorale = static_cast<float>(CachedRow.Morale);
	bMoraleImmune = (CachedRow.Morale == 99);
	CurrentMorale = MaxMorale;
	bRouting = false;

	// Skulkin "Already Dead": their slain models reassemble once per battle.
	bReviveCapable = (CachedRow.Faction == FName(TEXT("SKULKIN")));

	OriginalTeam = Team; // for reverting mind control

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
		M.Ammo = FMath::Max(0, CachedRow.Ammo);
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

	const float Speed = FMath::Max(0.f, EffSpeedCmS());
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

	Modifiers.Tick(DeltaSeconds);
	ProcessRevives(DeltaSeconds);

	if (PlayerCastGrace > 0.f)
	{
		PlayerCastGrace = FMath::Max(0.f, PlayerCastGrace - DeltaSeconds);
	}

	if (StealthTimer > 0.f)
	{
		StealthTimer = FMath::Max(0.f, StealthTimer - DeltaSeconds);
		if (StealthTimer <= 0.f)
		{
			bStealthed = false;  // reveal when the vanish wears off
			bCritPending = false; // an unused crit is lost
		}
	}

	// Mind control wears off: revert to the original team.
	if (bControlled && !bControlPermanent)
	{
		ControlTimer -= DeltaSeconds;
		if (ControlTimer <= 0.f)
		{
			Team = OriginalTeam;
			bControlled = false;
			ControlTimer = 0.f;
			ClearOrdersForRetarget();
			UE_LOG(LogNinjago, Log, TEXT("%s breaks free of control"), *CachedRow.DisplayName);
		}
	}

	// Fully destroyed (models may still be reviving via ProcessRevives above): render the collapsed
	// instances but take no movement, so a dead unit's anchor does not drift toward a stale order.
	if (LivingModelCount() == 0)
	{
		if (Renderer)
		{
			const float DeadScale = CachedRow.ModelScale > 0.f ? CachedRow.ModelScale : 1.f;
			Renderer->UpdateInstances(Models, DeadScale);
		}
		return;
	}

	// Frozen/stunned: hold in place, take no actions, just keep rendering.
	if (StunTimer > 0.f)
	{
		StunTimer = FMath::Max(0.f, StunTimer - DeltaSeconds);
		if (Renderer)
		{
			const float StunScale = CachedRow.ModelScale > 0.f ? CachedRow.ModelScale : 1.f;
			Renderer->UpdateInstances(Models, StunScale);
		}
		return;
	}

	// Routing overrides orders: flee directly away from the threat at a panic run.
	if (bRouting)
	{
		if (bHasFleeSource)
		{
			const FVector Anchor = GetActorLocation();
			FVector Away = Anchor - FVector(FleeFromLocation.X, FleeFromLocation.Y, Anchor.Z);
			if (Away.SizeSquared2D() > 1.f)
			{
				const float Speed = FMath::Max(0.f, EffSpeedCmS())
					* GetDefault<UNinjagoSettings>()->MoraleRoutSpeedMultiplier;
				SetActorLocation(Anchor + Away.GetSafeNormal2D() * Speed * DeltaSeconds);
			}
		}
		SteerModels(DeltaSeconds);
		if (Renderer)
		{
			const float RoutScale = CachedRow.ModelScale > 0.f ? CachedRow.ModelScale : 1.f;
			Renderer->UpdateInstances(Models, RoutScale);
		}
		return;
	}

	// Advance the unit anchor toward a move/attack order; models steer to their slots relative to
	// it, so the regiment walks in formation rather than teleporting. Combat itself lands in M6.
	// Drop the attack order once the target is gone: destroyed, wiped out (no living models, so it
	// will not be found and the unit would otherwise stand on the corpse), or turned to our side by
	// mind control. Clearing it lets AcquireTargets send the unit to the next enemy.
	if (OrderType == EOrderType::Attack
		&& (!AttackTarget.IsValid() || AttackTarget->LivingModelCount() == 0 || AttackTarget->GetTeam() == GetTeam()))
	{
		OrderType = EOrderType::NoOrder;
		AttackTarget.Reset();
		State = EUnitState::Idle;
	}

	if (OrderType == EOrderType::Move || OrderType == EOrderType::Attack)
	{
		const bool bAttack = (OrderType == EOrderType::Attack);
		const FVector Anchor = GetActorLocation();
		const FVector RawTarget = bAttack ? AttackTarget->GetActorLocation() : OrderLocation;
		FVector ToTarget = FVector(RawTarget.X, RawTarget.Y, Anchor.Z) - Anchor;
		const float Dist = ToTarget.Size2D();
		const float Speed = FMath::Max(0.f, EffSpeedCmS());

		// Attack stop distance: melee closes to contact; a ranged unit with ammo halts inside its
		// firing range (and closes to melee once its quivers are empty).
		float StopDist = 0.f;
		if (bAttack)
		{
			const UNinjagoSettings* S = GetDefault<UNinjagoSettings>();
			StopDist = S->EngageRangeCm;
			if (IsRangedUnit() && HasAmmoRemaining())
			{
				StopDist = FMath::Max(StopDist, CachedRow.RangeCm * S->RangedStandoffFraction);
			}
		}

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
		// Stealthed units render at zero scale (invisible); a shimmer marker shows the player where.
		const float ModelScale = bStealthed ? 0.f : (CachedRow.ModelScale > 0.f ? CachedRow.ModelScale : 1.f);
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
	bChargePending = true; // advancing again: this unit can charge on the next contact
	// The anchor advances toward OrderLocation in Tick; models follow their slots.
}

void ANinjagoUnit::ConfigureSpawn(UDataTable* Table, FName RowName, ETeam InTeam)
{
	UnitRowHandle.DataTable = Table;
	UnitRowHandle.RowName = RowName;
	Team = InTeam;
}

bool ANinjagoUnit::ApplyModelDamage(int32 ModelIndex, int32 Damage)
{
	if (!Models.IsValidIndex(ModelIndex))
	{
		return false;
	}
	FNinjagoModel& M = Models[ModelIndex];
	if (!M.bAlive)
	{
		return false;
	}
	M.Hp -= FMath::Max(0, Damage);
	bool bKilled = false;
	if (M.Hp <= 0)
	{
		M.Hp = 0;
		M.bAlive = false;
		bKilled = true;
		// Schedule an "Already Dead" reassembly if this model has not used its revive yet.
		if (bReviveCapable && !M.bHasRevived)
		{
			M.ReviveTimer = GetDefault<UNinjagoSettings>()->ReviveDelaySeconds;
		}
	}
	if (LivingModelCount() == 0)
	{
		State = EUnitState::Dead;
	}
	return bKilled;
}

void ANinjagoUnit::ApplyModelHeal(int32 ModelIndex, int32 Amount)
{
	if (!Models.IsValidIndex(ModelIndex) || !Models[ModelIndex].bAlive || Amount <= 0)
	{
		return;
	}
	const int32 MaxHp = FMath::Max(1, CachedRow.HpPerModel);
	Models[ModelIndex].Hp = FMath::Min(MaxHp, Models[ModelIndex].Hp + Amount);
}

float ANinjagoUnit::GetStrengthFraction() const
{
	if (!bRowValid || InitialSize <= 0)
	{
		return 0.f;
	}
	int32 CurrentHp = 0;
	for (const FNinjagoModel& M : Models)
	{
		if (M.bAlive)
		{
			CurrentHp += M.Hp;
		}
	}
	const float MaxHp = static_cast<float>(InitialSize) * static_cast<float>(FMath::Max(1, CachedRow.HpPerModel));
	return FMath::Clamp(static_cast<float>(CurrentHp) / MaxHp, 0.f, 1.f);
}

void ANinjagoUnit::RebuildUnit(int32 HpPercent)
{
	if (!bRowValid || Models.Num() == 0)
	{
		return;
	}
	const int32 Hp = FMath::Max(1, (CachedRow.HpPerModel * FMath::Clamp(HpPercent, 1, 100)) / 100);
	const int32 Count = Models.Num();
	for (int32 i = 0; i < Count; ++i)
	{
		FNinjagoModel& M = Models[i];
		M.bAlive = true;
		M.Hp = Hp;
		M.Ammo = FMath::Max(0, CachedRow.Ammo);
		M.ReviveTimer = 0.f;
		// Keep bHasRevived: reform does not refund a Skulkin model's once-per-battle self-revive.
		M.Location = SlotWorldLocation(M.SlotIndex == INDEX_NONE ? 0 : i, Count);
		M.Yaw = GetActorRotation().Yaw;
	}
	LastLivingCount = Count;
	CurrentMorale = MaxMorale;
	bRouting = false;
	StunTimer = 0.f;
	bChargePending = true;
	State = EUnitState::Idle;
	OrderType = EOrderType::NoOrder;
	AttackTarget.Reset();
	UE_LOG(LogNinjago, Log, TEXT("%s reforms at %d%% HP"), *CachedRow.DisplayName, HpPercent);
}

ANinjagoUnit* ANinjagoUnit::FindDestroyedAlly(const FVector& Center, float Radius) const
{
	const float RadiusSq = Radius * Radius;
	ANinjagoUnit* Nearest = nullptr;
	float BestDsq = RadiusSq;
	for (TActorIterator<ANinjagoUnit> It(GetWorld()); It; ++It)
	{
		ANinjagoUnit* Ally = *It;
		// A destroyed friendly unit: our team, no living models and none reassembling.
		if (!IsValid(Ally) || Ally == this || Ally->GetTeam() != Team
			|| Ally->LivingModelCount() > 0 || Ally->PendingReviveCount() > 0)
		{
			continue;
		}
		const float Dsq = FVector::DistSquared2D(Center, Ally->GetActorLocation());
		if (Dsq <= BestDsq)
		{
			BestDsq = Dsq;
			Nearest = Ally;
		}
	}
	return Nearest;
}

int32 ANinjagoUnit::PendingReviveCount() const
{
	int32 Count = 0;
	for (const FNinjagoModel& M : Models)
	{
		if (!M.bAlive && M.ReviveTimer > 0.f)
		{
			++Count;
		}
	}
	return Count;
}

void ANinjagoUnit::ProcessRevives(float DeltaSeconds)
{
	if (!bReviveCapable)
	{
		return;
	}

	const float HpFrac = GetDefault<UNinjagoSettings>()->ReviveHpFraction;
	bool bAnyRevived = false;
	for (FNinjagoModel& M : Models)
	{
		if (!M.bAlive && M.ReviveTimer > 0.f)
		{
			M.ReviveTimer -= DeltaSeconds;
			if (M.ReviveTimer <= 0.f)
			{
				M.bAlive = true;
				M.bHasRevived = true;
				M.ReviveTimer = 0.f;
				M.Hp = FMath::Max(1, FMath::RoundToInt(CachedRow.HpPerModel * HpFrac));
				bAnyRevived = true;
			}
		}
	}

	if (bAnyRevived && State == EUnitState::Dead)
	{
		State = EUnitState::Idle;
		LastLivingCount = LivingModelCount(); // don't count the revive as a morale event
	}
}

bool ANinjagoUnit::HasAmmoRemaining() const
{
	for (const FNinjagoModel& M : Models)
	{
		if (M.bAlive && M.Ammo > 0)
		{
			return true;
		}
	}
	return false;
}

bool ANinjagoUnit::TryConsumeAmmo(int32 ModelIndex)
{
	if (!Models.IsValidIndex(ModelIndex) || !Models[ModelIndex].bAlive || Models[ModelIndex].Ammo <= 0)
	{
		return false;
	}
	--Models[ModelIndex].Ammo;
	return true;
}

void ANinjagoUnit::UpdateMorale(bool bInCombat, bool bHasEnemy, const FVector& NearestEnemyLoc)
{
	const int32 Living = LivingModelCount();
	if (bMoraleImmune || Living == 0)
	{
		LastLivingCount = Living;
		return;
	}

	const int32 Lost = FMath::Max(0, LastLivingCount - Living);
	LastLivingCount = Living;
	const float Frac = static_cast<float>(Living) / static_cast<float>(FMath::Max(1, InitialSize));

	const UNinjagoSettings* S = GetDefault<UNinjagoSettings>();
	FNinjagoMoraleParams P;
	P.CasualtyShock = S->MoraleCasualtyShock;
	P.LowStrengthFraction = S->MoraleLowStrengthFraction;
	P.LowStrengthPenalty = S->MoraleLowStrengthPenalty;
	P.RegenPerTick = S->MoraleRegenPerTick;
	P.BreakFraction = S->MoraleBreakFraction;
	P.RallyFraction = S->MoraleRallyFraction;

	CurrentMorale = FNinjagoMoraleResolver::StepMorale(CurrentMorale, MaxMorale, Lost, Frac, bInCombat, P);
	const bool bNow = FNinjagoMoraleResolver::ResolveRouting(CurrentMorale, MaxMorale, bRouting, P);

	if (bNow && !bRouting)
	{
		UE_LOG(LogNinjago, Log, TEXT("%s breaks and routs!"), *CachedRow.DisplayName);
		OrderType = EOrderType::NoOrder;
		AttackTarget.Reset();
	}
	else if (!bNow && bRouting)
	{
		UE_LOG(LogNinjago, Log, TEXT("%s rallies."), *CachedRow.DisplayName);
		State = EUnitState::Idle;
	}

	bRouting = bNow;
	if (bRouting)
	{
		State = EUnitState::Routing;
		bHasFleeSource = bHasEnemy;
		FleeFromLocation = NearestEnemyLoc;
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
		bChargePending = true; // disengaged; a fresh contact can charge again
	}
}

bool ANinjagoUnit::IsLarge() const
{
	return bRowValid && CachedRow.ModelScale >= GetDefault<UNinjagoSettings>()->LargeModelScaleThreshold;
}

void ANinjagoUnit::ApplyMoraleShock(float Amount)
{
	if (bMoraleImmune || Amount <= 0.f)
	{
		return;
	}
	CurrentMorale = FMath::Max(0.f, CurrentMorale - Amount);
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
	bChargePending = true; // advancing to a new target: charge on contact
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
	return bHasAbility && AbilityCooldownRemaining <= 0.f && LivingModelCount() > 0 && !IsStunned();
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
				if (Enemy->ApplyModelDamage(i, Damage))
				{
					AddKill();
				}
			}
		}
	}
}

void ANinjagoUnit::NotifyPlayerSelected()
{
	PlayerCastGrace = GetDefault<UNinjagoSettings>()->PlayerAbilityGraceSeconds;
}

bool ANinjagoUnit::ShouldAIFireAbility() const
{
	// Defer to the player right after they select this unit, so their Spacebar wins the timing.
	if (!CanFireAbility() || IsRouting() || PlayerCastGrace > 0.f)
	{
		return false;
	}

	const FNinjagoAbilityMagnitude Mag = FNinjagoAbilityEffect::ParseMagnitude(CachedAbility.Magnitude);

	// Resurrection: only worth casting if a destroyed ally is in range to rebuild.
	if (Mag.Verb == TEXT("revive"))
	{
		return FindDestroyedAlly(GetActorLocation(), CachedAbility.RadiusCm) != nullptr;
	}

	const bool bOffensive = (Mag.Verb == TEXT("dmg") || Mag.Verb == TEXT("freeze")
		|| Mag.Verb == TEXT("control") || Mag.Verb == TEXT("slow") || Mag.Verb == TEXT("crit"));
	const bool bSupport = (Mag.Verb == TEXT("heal") || Mag.Verb == TEXT("atk") || Mag.Verb == TEXT("def")
		|| Mag.Verb == TEXT("speed") || Mag.Verb == TEXT("buff") || Mag.Verb == TEXT("atkspeed"));
	if (!bOffensive && !bSupport)
	{
		return false; // unsupported effect (terrain, etc.)
	}

	// Offensive: fire when an enemy is within the ability's reach. Support: fire once the battle is
	// joined (an enemy is reasonably close), so buffs/heals are not wasted at spawn.
	const float EngageR = GetDefault<UNinjagoSettings>()->EngageRangeCm;
	const float SupportProxyCm = 1500.f;
	const float CheckR = bOffensive ? FMath::Max(CachedAbility.RadiusCm, EngageR) : SupportProxyCm;
	const float CheckRsq = CheckR * CheckR;

	for (TActorIterator<ANinjagoUnit> It(GetWorld()); It; ++It)
	{
		const ANinjagoUnit* Other = *It;
		if (!IsValid(Other) || Other == this || Other->GetTeam() == Team || Other->LivingModelCount() == 0)
		{
			continue;
		}
		if (FVector::DistSquared2D(GetActorLocation(), Other->GetActorLocation()) <= CheckRsq)
		{
			return true;
		}
	}
	return false;
}

void ANinjagoUnit::ApplyAbilityChainDamage(const FVector& Center, float Radius, int32 Damage, int32 MaxTargets)
{
	if (Damage <= 0 || Radius <= 0.f || MaxTargets <= 0)
	{
		return;
	}

	// Gather every living enemy model, then hit the nearest MaxTargets (the chain).
	TArray<FVector> Locs;
	TArray<ANinjagoUnit*> Owners;
	TArray<int32> Indices;
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
			if (EnemyModels[i].bAlive)
			{
				Locs.Add(EnemyModels[i].Location);
				Owners.Add(Enemy);
				Indices.Add(i);
			}
		}
	}

	const TArray<int32> Chain = FNinjagoAbilityEffect::SelectNearest(Locs, Center, MaxTargets, Radius);
	for (int32 Sel : Chain)
	{
		if (Owners[Sel]->ApplyModelDamage(Indices[Sel], Damage))
		{
			AddKill();
		}
	}
}

void ANinjagoUnit::ApplyAbilityHealInRadius(const FVector& Center, float Radius, int32 Amount, bool bPercent)
{
	if (Amount <= 0 || Radius <= 0.f)
	{
		return;
	}
	const float RadiusSq = Radius * Radius;
	for (TActorIterator<ANinjagoUnit> It(GetWorld()); It; ++It)
	{
		ANinjagoUnit* Ally = *It;
		if (!IsValid(Ally) || Ally->GetTeam() != Team || Ally->LivingModelCount() == 0)
		{
			continue;
		}
		const int32 HealPerModel = bPercent ? (Ally->CachedRow.HpPerModel * Amount) / 100 : Amount;
		const TArray<FNinjagoModel>& AllyModels = Ally->GetModels();
		for (int32 i = 0; i < AllyModels.Num(); ++i)
		{
			if (AllyModels[i].bAlive && FVector::DistSquared2D(Center, AllyModels[i].Location) <= RadiusSq)
			{
				Ally->ApplyModelHeal(i, HealPerModel);
			}
		}
	}
}

void ANinjagoUnit::ApplyModifierInRadius(const FVector& Center, float Radius, bool bEnemies,
	ENinjagoStat Stat, float FlatAdd, float PercentAdd, float Duration)
{
	const float RadiusSq = Radius * Radius;
	for (TActorIterator<ANinjagoUnit> It(GetWorld()); It; ++It)
	{
		ANinjagoUnit* U = *It;
		if (!IsValid(U) || U->LivingModelCount() == 0)
		{
			continue;
		}
		const bool bMatch = bEnemies ? (U->GetTeam() != Team) : (U->GetTeam() == Team);
		if (!bMatch)
		{
			continue;
		}
		if (FVector::DistSquared2D(Center, U->GetActorLocation()) <= RadiusSq)
		{
			U->AddModifier(Stat, FlatAdd, PercentAdd, Duration);
		}
	}
}

void ANinjagoUnit::ApplyVanish(float Seconds, int32 Multiplier)
{
	bStealthed = true;
	StealthTimer = FMath::Max(0.1f, Seconds);
	bCritPending = true;
	CritMultiplier = FMath::Max(2, Multiplier);
}

int32 ANinjagoUnit::ConsumeCritMultiplier()
{
	if (!bCritPending)
	{
		return 1;
	}
	// The crit strike reveals the unit.
	bCritPending = false;
	bStealthed = false;
	StealthTimer = 0.f;
	return CritMultiplier;
}

void ANinjagoUnit::ClearOrdersForRetarget()
{
	OrderType = EOrderType::NoOrder;
	AttackTarget.Reset();
	State = EUnitState::Idle;
	bChargePending = true;
	bRouting = false;
}

void ANinjagoUnit::ApplyControl(ETeam NewTeam, float Duration)
{
	if (NewTeam == Team)
	{
		return;
	}
	Team = NewTeam;
	bControlled = true;
	bControlPermanent = (Duration <= 0.f);
	ControlTimer = bControlPermanent ? 0.f : Duration;
	ClearOrdersForRetarget(); // fight for the new side from scratch
	UE_LOG(LogNinjago, Log, TEXT("%s is now controlled by team %d%s"),
		*CachedRow.DisplayName, static_cast<int32>(NewTeam), bControlPermanent ? TEXT(" (permanent)") : TEXT(""));
}

void ANinjagoUnit::ApplyControlToEnemies(const FVector& Center, float Radius, bool bSingleTarget, float Duration)
{
	const float RadiusSq = Radius * Radius;
	ANinjagoUnit* Nearest = nullptr;
	float BestDsq = RadiusSq;

	for (TActorIterator<ANinjagoUnit> It(GetWorld()); It; ++It)
	{
		ANinjagoUnit* Enemy = *It;
		if (!IsValid(Enemy) || Enemy->GetTeam() == Team || Enemy->LivingModelCount() == 0)
		{
			continue;
		}
		const float Dsq = FVector::DistSquared2D(Center, Enemy->GetActorLocation());
		if (Dsq > RadiusSq)
		{
			continue;
		}
		if (bSingleTarget)
		{
			if (Dsq < BestDsq)
			{
				BestDsq = Dsq;
				Nearest = Enemy;
			}
		}
		else
		{
			Enemy->ApplyControl(Team, Duration);
		}
	}

	if (bSingleTarget && Nearest)
	{
		Nearest->ApplyControl(Team, Duration);
	}
}

void ANinjagoUnit::ApplyStunInRadius(const FVector& Center, float Radius, float Seconds)
{
	if (Seconds <= 0.f || Radius <= 0.f)
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
		if (FVector::DistSquared2D(Center, Enemy->GetActorLocation()) <= RadiusSq)
		{
			Enemy->ApplyStun(Seconds);
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
	if (IsStunned())
	{
		return; // paused while frozen; the timer keeps ticking and resumes when the stun ends
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

	const UNinjagoSettings* S = GetDefault<UNinjagoSettings>();
	const FNinjagoAbilityMagnitude Mag = FNinjagoAbilityEffect::ParseMagnitude(CachedAbility.Magnitude);
	const FString Target = CachedAbility.Target.ToUpper();
	const FVector Centre = GetActorLocation(); // one button, no aiming: centred on the caster

	if (Mag.Verb == TEXT("dmg"))
	{
		if (Target == TEXT("ENEMY_UNIT"))
		{
			// Chains between the nearest enemies (e.g. Jay's Lightning Bolt).
			ApplyAbilityChainDamage(Centre, CachedAbility.RadiusCm, Mag.Value, S->AbilityChainMaxTargets);
		}
		else if (CachedAbility.DurationS > 0.f)
		{
			// Channeled AoE: N ticks/sec for DurationS seconds (e.g. Four-Armed Fury).
			const float Hz = FMath::Max(1.f, S->AbilityChannelTicksPerSecond);
			ChannelDamagePerTick = Mag.Value;
			RemainingChannelTicks = FMath::Max(1, FMath::RoundToInt(CachedAbility.DurationS * Hz));
			AbilityChannelTick(); // first hit immediately
			GetWorldTimerManager().SetTimer(AbilityChannelTimer, this, &ANinjagoUnit::AbilityChannelTick, 1.f / Hz, true, 1.f / Hz);
		}
		else
		{
			// Instant AoE (e.g. Kai's Fire Blast, Cole's Earthquake).
			ApplyAbilityDamageInRadius(Centre, CachedAbility.RadiusCm, Mag.Value);
		}
	}
	else if (Mag.Verb == TEXT("heal"))
	{
		ApplyAbilityHealInRadius(Centre, CachedAbility.RadiusCm, Mag.Value, Mag.bPercent);
	}
	else if (Mag.Verb == TEXT("freeze"))
	{
		// freeze=6s -> stun enemies in radius for that many seconds.
		ApplyStunInRadius(Centre, CachedAbility.RadiusCm, static_cast<float>(Mag.Value));
	}
	else if (Mag.Verb == TEXT("revive"))
	{
		// Rebuild the nearest destroyed friendly unit at the given HP percent (e.g. Reform 60%).
		if (ANinjagoUnit* Dead = FindDestroyedAlly(Centre, CachedAbility.RadiusCm))
		{
			Dead->RebuildUnit(Mag.Value);
		}
	}
	else if (Mag.Verb == TEXT("crit"))
	{
		// Vanish: hide for the duration and make the next attack a critical (crit=x3).
		const float Dur = CachedAbility.DurationS > 0.f ? CachedAbility.DurationS : 8.f;
		ApplyVanish(Dur, Mag.Value);
	}
	else if (Mag.Verb == TEXT("control"))
	{
		// Convert enemies to this unit's team. DurationS <= 0 (e.g. Helmet Command) is permanent.
		// ENEMY_AOE affects all in radius; ENEMY_UNIT / ENEMY_HERO take the nearest one.
		const bool bSingle = !Target.Contains(TEXT("AOE"));
		ApplyControlToEnemies(Centre, CachedAbility.RadiusCm, bSingle, CachedAbility.DurationS);
	}
	else if (Mag.Verb == TEXT("atk") || Mag.Verb == TEXT("def") || Mag.Verb == TEXT("speed")
		|| Mag.Verb == TEXT("slow") || Mag.Verb == TEXT("buff") || Mag.Verb == TEXT("atkspeed"))
	{
		// Timed stat buff/debuff. def=+3 is flat; speed=+100% / slow=50% are percent.
		const float Dur = CachedAbility.DurationS > 0.f ? CachedAbility.DurationS : S->BuffDefaultDurationS;
		const bool bEnemies = Target.Contains(TEXT("ENEMY"));
		const float Flat = Mag.bPercent ? 0.f : static_cast<float>(Mag.Value);
		const float Pct = Mag.bPercent ? static_cast<float>(Mag.Value) : 0.f;

		auto ApplyStat = [&](ENinjagoStat Stat, float F, float P)
		{
			if (Target == TEXT("SELF"))
			{
				AddModifier(Stat, F, P, Dur);
			}
			else
			{
				ApplyModifierInRadius(Centre, CachedAbility.RadiusCm, bEnemies, Stat, F, P, Dur);
			}
		};

		if (Mag.Verb == TEXT("atk"))
		{
			ApplyStat(ENinjagoStat::MeleeAttack, Flat, Pct);
		}
		else if (Mag.Verb == TEXT("def"))
		{
			ApplyStat(ENinjagoStat::MeleeDefence, Flat, Pct);
		}
		else if (Mag.Verb == TEXT("speed"))
		{
			ApplyStat(ENinjagoStat::Speed, Flat, Pct);
		}
		else if (Mag.Verb == TEXT("slow"))
		{
			ApplyStat(ENinjagoStat::Speed, -Flat, -Pct); // debuff
		}
		else if (Mag.Verb == TEXT("atkspeed"))
		{
			ApplyStat(ENinjagoStat::Damage, Flat, Pct); // more attacks ~= more damage over time
		}
		else // "buff" is compound: attack and defence both.
		{
			ApplyStat(ENinjagoStat::MeleeAttack, Flat, Pct);
			ApplyStat(ENinjagoStat::MeleeDefence, Flat, Pct);
		}
	}
	else
	{
		// Terrain, control, stealth, etc. are not implemented yet (e.g. Zane's Ice Wall).
		UE_LOG(LogNinjago, Warning, TEXT("%s: ability effect '%s' (%s) not yet implemented; on cooldown."),
			*CachedAbility.DisplayName, *CachedAbility.Magnitude, *CachedAbility.Target);
	}

	AbilityCooldownRemaining = CachedAbility.CooldownS;
	UE_LOG(LogNinjago, Log, TEXT("%s fired %s (target %s, radius %.0f, mag %s, cd %.0fs)"),
		*GetName(), *CachedAbility.DisplayName, *CachedAbility.Target, CachedAbility.RadiusCm,
		*CachedAbility.Magnitude, CachedAbility.CooldownS);
	return true;
}
