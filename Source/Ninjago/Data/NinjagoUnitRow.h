// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "NinjagoTypes.h"
#include "NinjagoUnitRow.generated.h"

class UStaticMesh;

/**
 * One row of dt_units.csv. Fields map 1:1 to CSV columns 2..N; the first CSV
 * column (Name) becomes the DataTable row key and is NOT a field here.
 *
 * Stat semantics (see CLAUDE.md):
 *  - Morale == 99 is an "immune to morale" flag, not a magnitude. v0.1 ignores it.
 *  - HpPerModel is per model; unit total HP = UnitSize * HpPerModel.
 *  - UnitSize == 1 is a single-entity Lord/Hero, not a one-man regiment (no formation).
 *  - SpeedCmS is Unreal units (cm) per second.
 *  - Colours are "#RRGGBB" strings; convert with FColor::FromHex at spawn.
 *  - Mesh columns export empty; resolve with the placeholder fallback, never crash on null.
 */
USTRUCT(BlueprintType)
struct FNinjagoUnitRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Unit")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Unit")
	FName Faction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Unit")
	FName Archetype;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Unit")
	EUnitTier Tier = EUnitTier::TROOP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Unit")
	int32 UnitSize = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Combat")
	int32 HpPerModel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Combat")
	int32 MeleeAttack = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Combat")
	int32 MeleeDefence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Combat")
	int32 ChargeBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Combat")
	int32 Damage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Combat")
	int32 ArmourPiercing = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Combat")
	int32 Armour = 0;

	/** 99 == immune-to-morale flag, not a value. No morale system in v0.1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Combat")
	int32 Morale = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Movement")
	float SpeedCmS = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Ranged")
	int32 RangedAttack = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Ranged")
	float RangeCm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Ranged")
	int32 Ammo = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Render")
	float ModelScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Economy")
	int32 Cost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Economy")
	int32 Upkeep = 0;

	/** FName row key into dt_abilities, or NAME_None. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Unit")
	FName Ability;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Flavour")
	FString Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Flavour")
	FString Headgear;

	/** "#RRGGBB" hex; convert with FColor::FromHex at spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Render")
	FString ColourPrimary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Render")
	FString ColourSecondary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Render")
	TSoftObjectPtr<UStaticMesh> MeshHead;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Render")
	TSoftObjectPtr<UStaticMesh> MeshTorso;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Render")
	TSoftObjectPtr<UStaticMesh> MeshLegs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Render")
	TSoftObjectPtr<UStaticMesh> MeshHat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Render")
	TSoftObjectPtr<UStaticMesh> MeshWeapon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Flavour")
	FString Image;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Flavour")
	FString AssetStatus;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ninjago|Flavour")
	FString Notes;
};
