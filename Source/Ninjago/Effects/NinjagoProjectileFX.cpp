// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Effects/NinjagoProjectileFX.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

ANinjagoProjectileFX::ANinjagoProjectileFX()
{
	PrimaryActorTick.bCanEverTick = true;

	Tracers = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Tracers"));
	SetRootComponent(Tracers);
	Tracers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tracers->SetCastShadow(false);
}

void ANinjagoProjectileFX::BeginPlay()
{
	Super::BeginPlay();

	// A thin cylinder tracer. Engine BasicShapes are always available.
	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		Tracers->SetStaticMesh(Mesh);
	}
}

void ANinjagoProjectileFX::FireShot(const FVector& Start, const FVector& End)
{
	FShot Shot;
	Shot.Start = Start;
	Shot.End = End;
	Shot.Elapsed = 0.f;
	Shots.Add(Shot);
}

FTransform ANinjagoProjectileFX::TracerTransform(const FShot& Shot) const
{
	const float T = FMath::Clamp(Shot.Elapsed / FMath::Max(0.01f, FlightTime), 0.f, 1.f);

	FVector Pos = FMath::Lerp(Shot.Start, Shot.End, T);
	Pos.Z += ArcHeight * FMath::Sin(PI * T); // parabolic lift

	FVector Dir = (Shot.End - Shot.Start);
	Dir.Z = 0.f;
	const FVector Axis = Dir.IsNearlyZero() ? FVector::UpVector : Dir.GetSafeNormal();

	// Cylinder long axis is local Z; align it to the flight direction.
	const FRotator Rot = FRotationMatrix::MakeFromZ(Axis).Rotator();
	const FVector Scale(0.08f, 0.08f, 0.5f); // thin bolt
	return FTransform(Rot, Pos, Scale);
}

void ANinjagoProjectileFX::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Shots.Num() == 0)
	{
		if (Tracers->GetInstanceCount() > 0)
		{
			Tracers->ClearInstances();
		}
		return;
	}

	// Advance and drop finished tracers.
	for (int32 i = Shots.Num() - 1; i >= 0; --i)
	{
		Shots[i].Elapsed += DeltaSeconds;
		if (Shots[i].Elapsed >= FlightTime)
		{
			Shots.RemoveAtSwap(i);
		}
	}

	// Rebuild instances for the surviving tracers (counts are tiny).
	Tracers->ClearInstances();
	for (const FShot& Shot : Shots)
	{
		Tracers->AddInstance(TracerTransform(Shot), /*bWorldSpace*/ true);
	}
}
