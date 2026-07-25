// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NinjagoProjectileFX.generated.h"

class UHierarchicalInstancedStaticMeshComponent;

/**
 * Cosmetic tracer visuals for ranged shots. Ranged damage is applied instantly in combat; this
 * actor just draws short-lived bolts arcing from shooter to target so volleys are visible. One
 * HISM holds all in-flight tracers; each is a thin cylinder oriented along its flight path.
 */
UCLASS()
class NINJAGO_API ANinjagoProjectileFX : public AActor
{
	GENERATED_BODY()

public:
	ANinjagoProjectileFX();

	virtual void Tick(float DeltaSeconds) override;

	/** Launch a tracer from Start to End (world space). */
	void FireShot(const FVector& Start, const FVector& End);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Ninjago")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Tracers;

	/** Time a tracer takes to travel, seconds. */
	UPROPERTY(EditAnywhere, Category="Ninjago")
	float FlightTime = 0.35f;

	/** Peak arc height, cm. */
	UPROPERTY(EditAnywhere, Category="Ninjago")
	float ArcHeight = 120.f;

private:
	struct FShot
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float Elapsed = 0.f;
	};
	TArray<FShot> Shots;

	FTransform TracerTransform(const FShot& Shot) const;
};
