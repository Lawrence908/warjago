// Copyright Chris Lawrence. Personal project, not for distribution.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NinjagoModelRenderer.generated.h"

struct FNinjagoModel;
struct FNinjagoUnitRow;
class USceneComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * Renders a unit's models as rigid LEGO minifig parts. Owns one
 * UHierarchicalInstancedStaticMeshComponent per body part (hips, torso, both arms, head, hat) and
 * one instance per model per part. Animation is done by writing rigid transforms; faction colour
 * rides in PerInstanceCustomData (RGB) read by a single master material.
 *
 * Transforms are pushed with BatchUpdateInstancesTransforms once per part per frame, never per
 * instance. Dead models collapse their instances to zero scale.
 */
UCLASS(ClassGroup=(Ninjago), meta=(BlueprintSpawnableComponent))
class NINJAGO_API UNinjagoModelRenderer : public UActorComponent
{
	GENERATED_BODY()

public:
	UNinjagoModelRenderer();

	/** Body parts, in the fixed order the six HISM components are created. */
	enum EPart : uint8
	{
		Part_Hips = 0,
		Part_Torso,
		Part_ArmL,
		Part_ArmR,
		Part_Head,
		Part_Hat,
		Part_Count
	};

	/** Create and register the six HISM components, resolve part meshes/material, cache colours. */
	void SetupForUnit(USceneComponent* AttachRoot, const FNinjagoUnitRow& Row);

	/** Push current model transforms into every part HISM (one batched update per part). */
	void UpdateInstances(const TArray<FNinjagoModel>& Models, float ModelScale);

private:
	UPROPERTY()
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Parts;

	/** Local (figure-space) transform of each part, assembling a minifig with feet at Z=0. */
	FTransform PartLocal[Part_Count];

	/** Colour used for each part's PerInstanceCustomData (RGB), derived from the row's hex colours. */
	FLinearColor PartColour[Part_Count];

	bool bReady = false;
	int32 InstanceCount = 0;

	void BuildPartLayout();
	static UStaticMesh* ResolvePartMesh(const FNinjagoUnitRow& Row, int32 Part);
	static UMaterialInterface* LoadMasterMaterial();
	void CacheColours(const FNinjagoUnitRow& Row);
};
