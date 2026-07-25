// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Units/NinjagoModelRenderer.h"
#include "Units/NinjagoModel.h"
#include "Data/NinjagoUnitRow.h"
#include "NinjagoLog.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Placeholder assets built by Tools/make_placeholder_meshes.py.
	const TCHAR* MasterMaterialPath = TEXT("/Game/Placeholders/M_NinjagoMinifig.M_NinjagoMinifig");

	// Per-part placeholder mesh, with an engine BasicShapes fallback so nothing ever renders null.
	struct FPartAsset { const TCHAR* Placeholder; const TCHAR* EngineFallback; };
	const FPartAsset PartAssets[UNinjagoModelRenderer::Part_Count] =
	{
		{ TEXT("/Game/Placeholders/SM_Minifig_Hips.SM_Minifig_Hips"),   TEXT("/Engine/BasicShapes/Cube.Cube") },
		{ TEXT("/Game/Placeholders/SM_Minifig_Torso.SM_Minifig_Torso"), TEXT("/Engine/BasicShapes/Cube.Cube") },
		{ TEXT("/Game/Placeholders/SM_Minifig_ArmL.SM_Minifig_ArmL"),   TEXT("/Engine/BasicShapes/Cylinder.Cylinder") },
		{ TEXT("/Game/Placeholders/SM_Minifig_ArmR.SM_Minifig_ArmR"),   TEXT("/Engine/BasicShapes/Cylinder.Cylinder") },
		{ TEXT("/Game/Placeholders/SM_Minifig_Head.SM_Minifig_Head"),   TEXT("/Engine/BasicShapes/Cylinder.Cylinder") },
		{ TEXT("/Game/Placeholders/SM_Minifig_Hat.SM_Minifig_Hat"),     TEXT("/Engine/BasicShapes/Cube.Cube") },
	};

	// A warm flesh tone for the head (heads are not faction-coloured).
	const FLinearColor FleshColour(0.92f, 0.78f, 0.55f);

	FLinearColor HexToLinear(const FString& Hex, const FLinearColor& Fallback)
	{
		if (Hex.IsEmpty())
		{
			return Fallback;
		}
		const FColor C = FColor::FromHex(Hex);
		return FLinearColor::FromSRGBColor(C);
	}
}

UNinjagoModelRenderer::UNinjagoModelRenderer()
{
	PrimaryComponentTick.bCanEverTick = false;
	for (int32 i = 0; i < Part_Count; ++i)
	{
		PartLocal[i] = FTransform::Identity;
		PartColour[i] = FLinearColor::White;
	}
}

void UNinjagoModelRenderer::BuildPartLayout()
{
	// Engine BasicShapes are 100 cm; scale.<axis> * 100 = size in cm. Figure faces +X, feet at Z=0.
	// Values are presentation-only (not gameplay stats), so they live here rather than in settings.
	auto Make = [](const FVector& Scale, const FVector& Loc)
	{
		return FTransform(FRotator::ZeroRotator, Loc, Scale);
	};

	PartLocal[Part_Hips]  = Make(FVector(0.35f, 0.60f, 0.45f), FVector(0.f, 0.f,  22.5f));
	PartLocal[Part_Torso] = Make(FVector(0.35f, 0.60f, 0.40f), FVector(0.f, 0.f,  65.0f));
	PartLocal[Part_ArmL]  = Make(FVector(0.14f, 0.14f, 0.40f), FVector(0.f, -37.f, 65.0f));
	PartLocal[Part_ArmR]  = Make(FVector(0.14f, 0.14f, 0.40f), FVector(0.f,  37.f, 65.0f));
	PartLocal[Part_Head]  = Make(FVector(0.34f, 0.34f, 0.22f), FVector(0.f, 0.f,  96.0f));
	PartLocal[Part_Hat]   = Make(FVector(0.40f, 0.66f, 0.12f), FVector(0.f, 0.f, 113.0f));
}

void UNinjagoModelRenderer::CacheColours(const FNinjagoUnitRow& Row)
{
	const FLinearColor Primary   = HexToLinear(Row.ColourPrimary,   FLinearColor(0.12f, 0.44f, 0.75f));
	const FLinearColor Secondary = HexToLinear(Row.ColourSecondary, FLinearColor(0.85f, 0.10f, 0.10f));

	PartColour[Part_Hips]  = Secondary; // trousers
	PartColour[Part_Torso] = Primary;
	PartColour[Part_ArmL]  = Primary;
	PartColour[Part_ArmR]  = Primary;
	PartColour[Part_Head]  = FleshColour;
	PartColour[Part_Hat]   = Secondary;
}

UStaticMesh* UNinjagoModelRenderer::ResolvePartMesh(const FNinjagoUnitRow& Row, int32 Part)
{
	// Prefer the row's authored soft mesh for parts that have a column; fall back to placeholder,
	// then to the engine basic shape. Arms have no CSV column and always use the placeholder.
	const TSoftObjectPtr<UStaticMesh>* Soft = nullptr;
	switch (Part)
	{
		case Part_Hips:  Soft = &Row.MeshLegs;  break;
		case Part_Torso: Soft = &Row.MeshTorso; break;
		case Part_Head:  Soft = &Row.MeshHead;  break;
		case Part_Hat:   Soft = &Row.MeshHat;   break;
		default: break; // arms
	}

	if (Soft && !Soft->IsNull())
	{
		if (UStaticMesh* Loaded = Soft->LoadSynchronous())
		{
			return Loaded;
		}
	}

	if (UStaticMesh* Placeholder = LoadObject<UStaticMesh>(nullptr, PartAssets[Part].Placeholder))
	{
		return Placeholder;
	}
	return LoadObject<UStaticMesh>(nullptr, PartAssets[Part].EngineFallback);
}

UMaterialInterface* UNinjagoModelRenderer::LoadMasterMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, MasterMaterialPath);
}

void UNinjagoModelRenderer::SetupForUnit(USceneComponent* AttachRoot, const FNinjagoUnitRow& Row)
{
	if (bReady || !AttachRoot)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	BuildPartLayout();
	CacheColours(Row);

	UMaterialInterface* Master = LoadMasterMaterial();
	if (!Master)
	{
		UE_LOG(LogNinjago, Warning,
			TEXT("Master material not found at %s; run Tools/make_placeholder_meshes.py. Parts will use the mesh default material."),
			MasterMaterialPath);
	}

	Parts.SetNum(Part_Count);
	for (int32 i = 0; i < Part_Count; ++i)
	{
		UHierarchicalInstancedStaticMeshComponent* Hism =
			NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner);
		Hism->SetMobility(EComponentMobility::Movable);
		Hism->SetStaticMesh(ResolvePartMesh(Row, i));
		Hism->NumCustomDataFloats = 3; // RGB
		if (Master)
		{
			Hism->SetMaterial(0, Master);
		}
		Hism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Hism->SetCastShadow(true);
		Hism->RegisterComponent();
		Hism->AttachToComponent(AttachRoot, FAttachmentTransformRules::KeepRelativeTransform);
		Parts[i] = Hism;
	}

	bReady = true;
}

void UNinjagoModelRenderer::UpdateInstances(const TArray<FNinjagoModel>& Models, float ModelScale)
{
	if (!bReady)
	{
		return;
	}

	const int32 Num = Models.Num();

	for (int32 p = 0; p < Part_Count; ++p)
	{
		UHierarchicalInstancedStaticMeshComponent* Hism = Parts[p];
		if (!Hism)
		{
			continue;
		}

		// First update (or model-count change): (re)create instances and set constant colour data.
		if (Hism->GetInstanceCount() != Num)
		{
			Hism->ClearInstances();
			for (int32 m = 0; m < Num; ++m)
			{
				const int32 Idx = Hism->AddInstance(FTransform::Identity, /*bWorldSpace*/ false);
				Hism->SetCustomDataValue(Idx, 0, PartColour[p].R, false);
				Hism->SetCustomDataValue(Idx, 1, PartColour[p].G, false);
				Hism->SetCustomDataValue(Idx, 2, PartColour[p].B, false);
			}
		}

		// Compose each instance's world transform: part-local, then the model's world transform.
		TArray<FTransform> Transforms;
		Transforms.Reserve(Num);
		for (int32 m = 0; m < Num; ++m)
		{
			const FNinjagoModel& Model = Models[m];
			const FTransform World(FRotator(0.f, Model.Yaw, 0.f), Model.Location, FVector(ModelScale));
			FTransform Final = PartLocal[p] * World;
			if (!Model.bAlive)
			{
				Final.SetScale3D(FVector::ZeroVector); // collapse dead instances
			}
			Transforms.Add(Final);
		}

		Hism->BatchUpdateInstancesTransforms(0, Transforms, /*bWorldSpace*/ true,
			/*bMarkRenderStateDirty*/ true, /*bTeleport*/ true);
	}

	InstanceCount = Num;
}
