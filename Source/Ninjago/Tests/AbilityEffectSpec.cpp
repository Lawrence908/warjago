// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Misc/AutomationTest.h"
#include "Combat/NinjagoAbilityEffect.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoAbilityParseTest,
	"Ninjago.Ability.ParseMagnitude",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoAbilityParseTest::RunTest(const FString&)
{
	{
		const FNinjagoAbilityMagnitude M = FNinjagoAbilityEffect::ParseMagnitude(TEXT("dmg=45"));
		TestEqual(TEXT("dmg verb"), M.Verb, FString(TEXT("dmg")));
		TestEqual(TEXT("dmg value"), M.Value, 45);
		TestFalse(TEXT("dmg not percent"), M.bPercent);
		TestTrue(TEXT("dmg numeric"), M.bNumeric);
	}
	{
		const FNinjagoAbilityMagnitude M = FNinjagoAbilityEffect::ParseMagnitude(TEXT("heal=50%"));
		TestEqual(TEXT("heal verb"), M.Verb, FString(TEXT("heal")));
		TestEqual(TEXT("heal value"), M.Value, 50);
		TestTrue(TEXT("heal percent"), M.bPercent);
	}
	{
		const FNinjagoAbilityMagnitude M = FNinjagoAbilityEffect::ParseMagnitude(TEXT("def=+3"));
		TestEqual(TEXT("signed value"), M.Value, 3);
		TestTrue(TEXT("def numeric"), M.bNumeric);
	}
	{
		// dmg=40/hit -> stops at the slash.
		const FNinjagoAbilityMagnitude M = FNinjagoAbilityEffect::ParseMagnitude(TEXT("dmg=40/hit"));
		TestEqual(TEXT("dmg per hit value"), M.Value, 40);
	}
	{
		// Qualitative magnitude: verb present, no number.
		const FNinjagoAbilityMagnitude M = FNinjagoAbilityEffect::ParseMagnitude(TEXT("terrain=wall"));
		TestEqual(TEXT("terrain verb"), M.Verb, FString(TEXT("terrain")));
		TestFalse(TEXT("terrain not numeric"), M.bNumeric);
	}
	{
		// "freeze=6s" -> the seconds value stops at the trailing unit letter.
		const FNinjagoAbilityMagnitude M = FNinjagoAbilityEffect::ParseMagnitude(TEXT("freeze=6s"));
		TestEqual(TEXT("freeze verb"), M.Verb, FString(TEXT("freeze")));
		TestEqual(TEXT("freeze seconds"), M.Value, 6);
	}
	{
		const FNinjagoAbilityMagnitude M = FNinjagoAbilityEffect::ParseMagnitude(TEXT("no verb here"));
		TestTrue(TEXT("no '=' -> empty verb"), M.Verb.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoAbilitySelectNearestTest,
	"Ninjago.Ability.SelectNearest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoAbilitySelectNearestTest::RunTest(const FString&)
{
	TArray<FVector> Pts;
	Pts.Add(FVector(100, 0, 0));   // 0: dist 100
	Pts.Add(FVector(500, 0, 0));   // 1: dist 500
	Pts.Add(FVector(50, 0, 0));    // 2: dist 50  (nearest)
	Pts.Add(FVector(3000, 0, 0));  // 3: dist 3000 (out of a 1000 radius)

	const TArray<int32> Chain = FNinjagoAbilityEffect::SelectNearest(Pts, FVector::ZeroVector, /*MaxCount*/2, /*Radius*/1000.f);

	TestEqual(TEXT("Capped to MaxCount"), Chain.Num(), 2);
	TestEqual(TEXT("Nearest first"), Chain[0], 2);
	TestEqual(TEXT("Then next nearest"), Chain[1], 0);

	// Radius excludes the far point even with room in the count.
	const TArray<int32> All = FNinjagoAbilityEffect::SelectNearest(Pts, FVector::ZeroVector, 10, 1000.f);
	TestEqual(TEXT("Out-of-radius excluded"), All.Num(), 3);
	TestFalse(TEXT("Far point not chosen"), All.Contains(3));

	return true;
}

#endif // WITH_AUTOMATION_TESTS
