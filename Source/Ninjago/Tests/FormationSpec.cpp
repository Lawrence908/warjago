// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Misc/AutomationTest.h"
#include "Units/NinjagoFormation.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	constexpr float kSpacing = 90.0f;

	// Gather every slot offset for a block of Count models.
	TArray<FVector2D> AllSlots(int32 Count)
	{
		TArray<FVector2D> Slots;
		Slots.Reserve(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			Slots.Add(FNinjagoFormation::SlotOffset(i, Count, kSpacing));
		}
		return Slots;
	}

	bool AllDistinct(const TArray<FVector2D>& Slots)
	{
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			for (int32 j = i + 1; j < Slots.Num(); ++j)
			{
				if (Slots[i].Equals(Slots[j], 1.0f))
				{
					return false;
				}
			}
		}
		return true;
	}

	// Every slot has an opposite slot (point symmetry about the origin).
	bool SymmetricAboutOrigin(const TArray<FVector2D>& Slots)
	{
		for (const FVector2D& S : Slots)
		{
			bool bFoundOpposite = false;
			for (const FVector2D& T : Slots)
			{
				if (T.Equals(-S, 1.0f))
				{
					bFoundOpposite = true;
					break;
				}
			}
			if (!bFoundOpposite)
			{
				return false;
			}
		}
		return true;
	}

	FVector2D Centroid(const TArray<FVector2D>& Slots)
	{
		FVector2D Sum = FVector2D::ZeroVector;
		for (const FVector2D& S : Slots)
		{
			Sum += S;
		}
		return Slots.Num() > 0 ? Sum / Slots.Num() : FVector2D::ZeroVector;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoFormationSingleTest,
	"Ninjago.Formation.SingleEntityAtOrigin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoFormationSingleTest::RunTest(const FString&)
{
	// A single-entity Lord/Hero sits exactly on the origin.
	const FVector2D Only = FNinjagoFormation::SlotOffset(0, 1, kSpacing);
	TestTrue(TEXT("Count 1 -> slot at origin"), Only.IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoFormationBlockTest,
	"Ninjago.Formation.BlockSymmetricNoOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoFormationBlockTest::RunTest(const FString&)
{
	for (const int32 Count : { 24, 32 })
	{
		const TArray<FVector2D> Slots = AllSlots(Count);

		TestEqual(FString::Printf(TEXT("Count %d produces %d slots"), Count, Count), Slots.Num(), Count);
		TestTrue(FString::Printf(TEXT("Count %d: no two slots overlap"), Count), AllDistinct(Slots));
		TestTrue(FString::Printf(TEXT("Count %d: symmetric about origin"), Count), SymmetricAboutOrigin(Slots));
		TestTrue(FString::Printf(TEXT("Count %d: centroid at origin"), Count), Centroid(Slots).IsNearlyZero(0.01f));

		// Full rectangle: columns divide the count evenly.
		const int32 Cols = FNinjagoFormation::Columns(Count);
		TestEqual(FString::Printf(TEXT("Count %d: columns divide count"), Count), Count % Cols, 0);
		TestTrue(FString::Printf(TEXT("Count %d: wider than deep"), Count), Cols >= (Count / Cols));
	}

	// Pin the chosen frontages so the block shape can't silently change.
	TestEqual(TEXT("24 models form a 6-wide block"), FNinjagoFormation::Columns(24), 6);
	TestEqual(TEXT("32 models form an 8-wide block"), FNinjagoFormation::Columns(32), 8);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
