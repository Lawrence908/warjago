// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "NinjagoCameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * Top-down/isometric battle camera. WASD pans on the flat plane, mouse wheel zooms; no rotation.
 * The player controller feeds input via AddPanInput / AddZoomInput; movement is applied in Tick and
 * clamped to the map bounds. Control-feel constants live here (not gameplay stats, so not in settings).
 */
UCLASS()
class NINJAGO_API ANinjagoCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ANinjagoCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** Pan input this frame in world XY (-1..1 per axis). Accumulated, applied in Tick. */
	void AddPanInput(const FVector2D& Axis) { PendingPan += Axis; }

	/** Zoom input this frame (mouse wheel; positive = zoom in). */
	void AddZoomInput(float Delta) { PendingZoom += Delta; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Ninjago|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category="Ninjago|Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Pan speed in cm/s at full axis deflection. */
	UPROPERTY(EditAnywhere, Category="Ninjago|Camera")
	float PanSpeed = 2500.f;

	/** Camera pitch (looking down), degrees. */
	UPROPERTY(EditAnywhere, Category="Ninjago|Camera")
	float CameraPitch = -60.f;

	UPROPERTY(EditAnywhere, Category="Ninjago|Camera")
	float ZoomMin = 800.f;

	UPROPERTY(EditAnywhere, Category="Ninjago|Camera")
	float ZoomMax = 6000.f;

	/** Arm length change per wheel notch. */
	UPROPERTY(EditAnywhere, Category="Ninjago|Camera")
	float ZoomStep = 400.f;

	/** Half-size of the pannable area on X/Y (cm) about the world origin. */
	UPROPERTY(EditAnywhere, Category="Ninjago|Camera")
	FVector2D MapHalfExtent = FVector2D(8000.f, 8000.f);

private:
	FVector2D PendingPan = FVector2D::ZeroVector;
	float PendingZoom = 0.f;
	float TargetArmLength = 3000.f;
};
