// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NinjagoSettings.generated.h"

/**
 * Project-wide tuning constants. This is the ONLY place gameplay numbers are allowed to live
 * outside the DataTables (per CLAUDE.md). Surfaces under Project Settings -> Ninjago and
 * serialises to Config/DefaultGame.ini.
 *
 * Read at runtime via GetDefault<UNinjagoSettings>().
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Ninjago"))
class NINJAGO_API UNinjagoSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Groups these settings under a "Ninjago" heading in Project Settings. */
	virtual FName GetCategoryName() const override { return FName(TEXT("Ninjago")); }

	/** Seconds between combat resolutions. Combat runs on a timer, never in Tick. */
	UPROPERTY(config, EditAnywhere, Category="Combat", meta=(ClampMin="0.1"))
	float CombatTickSeconds = 2.0f;

	/** A model engages the nearest living enemy model within this range (cm). */
	UPROPERTY(config, EditAnywhere, Category="Combat", meta=(ClampMin="0.0"))
	float EngageRangeCm = 150.0f;

	/** hitChance = Base + PerPoint * (atk.MeleeAttack - def.MeleeDefence), clamped [Min, Max]. */
	UPROPERTY(config, EditAnywhere, Category="Combat")
	float HitChanceBase = 0.35f;

	UPROPERTY(config, EditAnywhere, Category="Combat")
	float HitChancePerPoint = 0.03f;

	UPROPERTY(config, EditAnywhere, Category="Combat", meta=(ClampMin="0.0", ClampMax="1.0"))
	float HitChanceMin = 0.10f;

	UPROPERTY(config, EditAnywhere, Category="Combat", meta=(ClampMin="0.0", ClampMax="1.0"))
	float HitChanceMax = 0.90f;

	/** Damage floor: a landed hit always deals at least this much (keeps armour from being a wall). */
	UPROPERTY(config, EditAnywhere, Category="Combat", meta=(ClampMin="1"))
	int32 MinDamage = 1;

	/** Spacing between formation slots (cm). */
	UPROPERTY(config, EditAnywhere, Category="Formation", meta=(ClampMin="1.0"))
	float ModelSpacingCm = 90.0f;

	/** Damage ticks per second for channeled (duration) abilities, e.g. Four-Armed Fury. */
	UPROPERTY(config, EditAnywhere, Category="Abilities", meta=(ClampMin="1.0"))
	float AbilityChannelTicksPerSecond = 4.0f;
};
