// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Player/NinjagoPlayerController.h"
#include "Player/NinjagoCameraPawn.h"
#include "Units/NinjagoUnit.h"
#include "Core/NinjagoGameMode.h"
#include "NinjagoLog.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputActionValue.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

ANinjagoPlayerController::ANinjagoPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ANinjagoPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = false;
	DefaultMouseCursor = EMouseCursor::Default;

	BuildInput();

	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}
}

void ANinjagoPlayerController::BuildInput()
{
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Ninjago"));

	IA_Pan = NewObject<UInputAction>(this, TEXT("IA_Pan"));
	IA_Pan->ValueType = EInputActionValueType::Axis2D;

	IA_Zoom = NewObject<UInputAction>(this, TEXT("IA_Zoom"));
	IA_Zoom->ValueType = EInputActionValueType::Axis1D;

	IA_Select = NewObject<UInputAction>(this, TEXT("IA_Select"));
	IA_Select->ValueType = EInputActionValueType::Boolean;

	IA_Order = NewObject<UInputAction>(this, TEXT("IA_Order"));
	IA_Order->ValueType = EInputActionValueType::Boolean;

	IA_Deselect = NewObject<UInputAction>(this, TEXT("IA_Deselect"));
	IA_Deselect->ValueType = EInputActionValueType::Boolean;

	IA_Ability = NewObject<UInputAction>(this, TEXT("IA_Ability"));
	IA_Ability->ValueType = EInputActionValueType::Boolean;

	IA_Restart = NewObject<UInputAction>(this, TEXT("IA_Restart"));
	IA_Restart->ValueType = EInputActionValueType::Boolean;

	IA_Scenarios.SetNum(4);
	for (int32 i = 0; i < 4; ++i)
	{
		IA_Scenarios[i] = NewObject<UInputAction>(this, *FString::Printf(TEXT("IA_Scenario%d"), i + 1));
		IA_Scenarios[i]->ValueType = EInputActionValueType::Boolean;
	}

	// WASD -> IA_Pan (2D). Digital keys land on X; swizzle to route forward/back onto Y, negate for
	// the down/left directions.
	auto AddSwizzle = [this](FEnhancedActionKeyMapping& Mapping)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MappingContext));
	};
	auto AddNegate = [this](FEnhancedActionKeyMapping& Mapping)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
	};

	{
		FEnhancedActionKeyMapping& W = MappingContext->MapKey(IA_Pan, EKeys::W);
		AddSwizzle(W); // X -> Y (+forward)
	}
	{
		FEnhancedActionKeyMapping& S = MappingContext->MapKey(IA_Pan, EKeys::S);
		AddSwizzle(S);
		AddNegate(S); // -Y
	}
	{
		MappingContext->MapKey(IA_Pan, EKeys::D); // +X
	}
	{
		FEnhancedActionKeyMapping& A = MappingContext->MapKey(IA_Pan, EKeys::A);
		AddNegate(A); // -X
	}

	MappingContext->MapKey(IA_Zoom, EKeys::MouseWheelAxis);
	MappingContext->MapKey(IA_Select, EKeys::LeftMouseButton);
	MappingContext->MapKey(IA_Order, EKeys::RightMouseButton);
	MappingContext->MapKey(IA_Deselect, EKeys::Escape);
	MappingContext->MapKey(IA_Ability, EKeys::SpaceBar);
	MappingContext->MapKey(IA_Restart, EKeys::R);
	MappingContext->MapKey(IA_Scenarios[0], EKeys::One);
	MappingContext->MapKey(IA_Scenarios[1], EKeys::Two);
	MappingContext->MapKey(IA_Scenarios[2], EKeys::Three);
	MappingContext->MapKey(IA_Scenarios[3], EKeys::Four);
}

void ANinjagoPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
	{
		EIC->BindAction(IA_Pan, ETriggerEvent::Triggered, this, &ANinjagoPlayerController::OnPan);
		EIC->BindAction(IA_Zoom, ETriggerEvent::Triggered, this, &ANinjagoPlayerController::OnZoom);
		EIC->BindAction(IA_Select, ETriggerEvent::Started, this, &ANinjagoPlayerController::OnSelect);
		EIC->BindAction(IA_Order, ETriggerEvent::Started, this, &ANinjagoPlayerController::OnOrder);
		EIC->BindAction(IA_Deselect, ETriggerEvent::Started, this, &ANinjagoPlayerController::OnDeselect);
		EIC->BindAction(IA_Ability, ETriggerEvent::Started, this, &ANinjagoPlayerController::OnAbility);
		EIC->BindAction(IA_Restart, ETriggerEvent::Started, this, &ANinjagoPlayerController::OnRestart);
		EIC->BindAction(IA_Scenarios[0], ETriggerEvent::Started, this, &ANinjagoPlayerController::OnScenario1);
		EIC->BindAction(IA_Scenarios[1], ETriggerEvent::Started, this, &ANinjagoPlayerController::OnScenario2);
		EIC->BindAction(IA_Scenarios[2], ETriggerEvent::Started, this, &ANinjagoPlayerController::OnScenario3);
		EIC->BindAction(IA_Scenarios[3], ETriggerEvent::Started, this, &ANinjagoPlayerController::OnScenario4);
	}
	else
	{
		UE_LOG(LogNinjago, Error, TEXT("InputComponent is not an EnhancedInputComponent; check DefaultInput.ini."));
	}
}

void ANinjagoPlayerController::OnPan(const FInputActionValue& Value)
{
	if (ANinjagoCameraPawn* Cam = Cast<ANinjagoCameraPawn>(GetPawn()))
	{
		Cam->AddPanInput(Value.Get<FVector2D>());
	}
}

void ANinjagoPlayerController::OnZoom(const FInputActionValue& Value)
{
	if (ANinjagoCameraPawn* Cam = Cast<ANinjagoCameraPawn>(GetPawn()))
	{
		Cam->AddZoomInput(Value.Get<float>());
	}
}

void ANinjagoPlayerController::OnSelect(const FInputActionValue&)
{
	SetSelected(TraceUnitUnderCursor());
}

void ANinjagoPlayerController::OnOrder(const FInputActionValue&)
{
	ANinjagoUnit* Unit = Selected.Get();
	if (!Unit)
	{
		return;
	}

	if (ANinjagoUnit* Hit = TraceUnitUnderCursor())
	{
		if (Hit != Unit && Hit->GetTeam() != Unit->GetTeam())
		{
			Unit->OrderAttack(Hit);
			return;
		}
		// Friendly or self under cursor: treat as a move to that spot.
		Unit->OrderMoveTo(Hit->GetActorLocation());
		return;
	}

	FVector Ground;
	if (TraceGroundUnderCursor(Ground))
	{
		Unit->OrderMoveTo(Ground);
	}
}

void ANinjagoPlayerController::OnDeselect(const FInputActionValue&)
{
	SetSelected(nullptr);
}

void ANinjagoPlayerController::OnAbility(const FInputActionValue&)
{
	if (ANinjagoUnit* Unit = Selected.Get())
	{
		Unit->TryFireAbility();
	}
}

void ANinjagoPlayerController::OnRestart(const FInputActionValue&)
{
	// Reload the level for a fresh battle (works during or after a fight).
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

void ANinjagoPlayerController::LoadScenario(int32 Index)
{
	if (ANinjagoGameMode* GM = GetWorld()->GetAuthGameMode<ANinjagoGameMode>())
	{
		GM->LoadScenario(Index);
	}
}

void ANinjagoPlayerController::OnScenario1(const FInputActionValue&) { LoadScenario(0); }
void ANinjagoPlayerController::OnScenario2(const FInputActionValue&) { LoadScenario(1); }
void ANinjagoPlayerController::OnScenario3(const FInputActionValue&) { LoadScenario(2); }
void ANinjagoPlayerController::OnScenario4(const FInputActionValue&) { LoadScenario(3); }

void ANinjagoPlayerController::SetSelected(ANinjagoUnit* Unit)
{
	Selected = Unit;
	if (Unit)
	{
		Unit->NotifyPlayerSelected(); // brief AI auto-cast hold so the player can time the ability
		UE_LOG(LogNinjago, Verbose, TEXT("Selected %s"), *Unit->GetName());
	}
}

ANinjagoUnit* ANinjagoPlayerController::TraceUnitUnderCursor() const
{
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
	{
		return Cast<ANinjagoUnit>(Hit.GetActor());
	}
	return nullptr;
}

bool ANinjagoPlayerController::TraceGroundUnderCursor(FVector& OutLocation) const
{
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Visibility, false, Hit) && Hit.bBlockingHit)
	{
		OutLocation = Hit.ImpactPoint;
		return true;
	}
	return false;
}

void ANinjagoPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Selection highlight: a ground ring under the selected unit (debug-draw stand-in for a decal).
	if (ANinjagoUnit* Unit = Selected.Get())
	{
		const FVector Centre = Unit->GetActorLocation() + FVector(0.f, 0.f, 5.f);
		DrawDebugCircle(GetWorld(), Centre, 220.f, 48, FColor::Yellow, false, -1.f, 0, 6.f,
			FVector(0.f, 1.f, 0.f), FVector(1.f, 0.f, 0.f), false);

		// Ability cooldown bar above the unit (green = ready fraction), only for ability-bearers.
		if (Unit->HasAbility())
		{
			const float Ready = 1.f - Unit->GetAbilityCooldownFraction();
			const float HalfLen = 200.f;
			const FVector Base = Unit->GetActorLocation() + FVector(0.f, 0.f, 260.f);
			const FVector Left = Base - FVector(HalfLen, 0.f, 0.f);
			const FVector Right = Base + FVector(HalfLen, 0.f, 0.f);
			const FVector Fill = Left + (Right - Left) * FMath::Clamp(Ready, 0.f, 1.f);
			DrawDebugLine(GetWorld(), Left, Right, FColor(60, 60, 60), false, -1.f, 0, 10.f);
			DrawDebugLine(GetWorld(), Left, Fill, Ready >= 1.f ? FColor::Green : FColor::Orange, false, -1.f, 0, 10.f);
		}

		// Selected-unit info panel (on-screen). Refreshed each frame with time 0, so it clears on
		// deselect. Reuses the row data, including the Notes flavour text.
		if (GEngine && Unit->IsRowValid())
		{
			const FNinjagoUnitRow& Row = Unit->GetRow();
			const int32 StrPct = FMath::RoundToInt(Unit->GetStrengthFraction() * 100.f);
			GEngine->AddOnScreenDebugMessage(30, 0.f, FColor::Yellow, Row.DisplayName);
			if (!Row.Notes.IsEmpty())
			{
				GEngine->AddOnScreenDebugMessage(31, 0.f, FColor(210, 210, 210), Row.Notes);
			}
			GEngine->AddOnScreenDebugMessage(32, 0.f, FColor(180, 220, 180),
				FString::Printf(TEXT("Models %d/%d   Str %d%%   Atk %d  Dmg %d  Def %d   Kills %d"),
					Unit->LivingModelCount(), FMath::Max(1, Row.UnitSize), StrPct,
					Row.MeleeAttack, Row.Damage, Row.MeleeDefence, Unit->GetKillCount()));
			if (Unit->HasAbility())
			{
				GEngine->AddOnScreenDebugMessage(33, 0.f, FColor(160, 200, 255),
					FString::Printf(TEXT("Ability: %s  [Space]"), *Unit->GetAbility().DisplayName));
			}
		}
	}
}
