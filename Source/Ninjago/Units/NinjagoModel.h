// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "NinjagoModel.generated.h"

/**
 * One rendered body in a unit. A regiment owns UnitSize of these; a Lord/Hero owns one.
 * Plain data: steered on the flat plane, rendered as six rigid HISM instances.
 */
USTRUCT(BlueprintType)
struct FNinjagoModel
{
	GENERATED_BODY()

	/** World-space location on the plane (Z is flat). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninjago|Model")
	FVector Location = FVector::ZeroVector;

	/** Facing, degrees. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninjago|Model")
	float Yaw = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninjago|Model")
	int32 Hp = 0;

	/** Index into the unit's formation, or INDEX_NONE for a single entity. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninjago|Model")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ninjago|Model")
	bool bAlive = true;
};
