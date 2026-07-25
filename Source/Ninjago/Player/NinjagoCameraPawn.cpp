// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Player/NinjagoCameraPawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

ANinjagoCameraPawn::ANinjagoCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Root);
	SpringArm->TargetArmLength = TargetArmLength;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritYaw = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 10.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

void ANinjagoCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	TargetArmLength = FMath::Clamp(TargetArmLength, ZoomMin, ZoomMax);
	SpringArm->TargetArmLength = TargetArmLength;
	// Fixed downward pitch; no rotation input is ever applied.
	SpringArm->SetRelativeRotation(FRotator(CameraPitch, 0.f, 0.f));
}

void ANinjagoCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Pan on the world XY plane (camera never rotates, so axes map straight to world X/Y).
	if (!PendingPan.IsNearlyZero())
	{
		const FVector2D Clamped = PendingPan.GetSafeNormal() * FMath::Min(PendingPan.Size(), 1.f);
		FVector Loc = GetActorLocation();
		Loc.X += Clamped.X * PanSpeed * DeltaSeconds;
		Loc.Y += Clamped.Y * PanSpeed * DeltaSeconds;
		Loc.X = FMath::Clamp(Loc.X, -MapHalfExtent.X, MapHalfExtent.X);
		Loc.Y = FMath::Clamp(Loc.Y, -MapHalfExtent.Y, MapHalfExtent.Y);
		SetActorLocation(Loc);
	}

	if (!FMath::IsNearlyZero(PendingZoom))
	{
		// Positive wheel = zoom in = shorter arm.
		TargetArmLength = FMath::Clamp(TargetArmLength - PendingZoom * ZoomStep, ZoomMin, ZoomMax);
		SpringArm->TargetArmLength = TargetArmLength;
	}

	PendingPan = FVector2D::ZeroVector;
	PendingZoom = 0.f;
}
