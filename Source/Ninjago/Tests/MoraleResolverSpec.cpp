// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Misc/AutomationTest.h"
#include "Combat/NinjagoMoraleResolver.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	FNinjagoMoraleParams DefaultParams()
	{
		return FNinjagoMoraleParams{};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoMoraleCasualtyTest,
	"Ninjago.Morale.CasualtiesLowerMorale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoMoraleCasualtyTest::RunTest(const FString&)
{
	const FNinjagoMoraleParams P = DefaultParams();

	// Two losses at full strength: morale drops by CasualtyShock per model.
	const float After = FNinjagoMoraleResolver::StepMorale(60.f, 60.f, /*Lost*/2, /*Frac*/1.0f, /*InCombat*/true, P);
	TestEqual(TEXT("Two casualties cost 2 * CasualtyShock"), After, 60.f - 2.f * P.CasualtyShock, KINDA_SMALL_NUMBER);

	// Morale never goes below zero.
	const float Floored = FNinjagoMoraleResolver::StepMorale(3.f, 60.f, 10, 0.1f, true, P);
	TestTrue(TEXT("Morale clamps at zero"), Floored >= 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoMoraleLowStrengthTest,
	"Ninjago.Morale.UnderStrengthPenalty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoMoraleLowStrengthTest::RunTest(const FString&)
{
	const FNinjagoMoraleParams P = DefaultParams();

	// No losses this tick, but under half strength and in combat: only the low-strength penalty applies.
	const float Under = FNinjagoMoraleResolver::StepMorale(50.f, 60.f, 0, 0.3f, true, P);
	TestEqual(TEXT("Under-strength penalty applied"), Under, 50.f - P.LowStrengthPenalty, KINDA_SMALL_NUMBER);

	// Same but at full strength and in combat: no change (no casualties, no regen).
	const float Full = FNinjagoMoraleResolver::StepMorale(50.f, 60.f, 0, 1.0f, true, P);
	TestEqual(TEXT("Full strength in combat holds steady"), Full, 50.f, KINDA_SMALL_NUMBER);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoMoraleRegenTest,
	"Ninjago.Morale.RecoversWhenSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoMoraleRegenTest::RunTest(const FString&)
{
	const FNinjagoMoraleParams P = DefaultParams();

	// No casualties, not in combat: morale recovers, capped at max.
	const float Regen = FNinjagoMoraleResolver::StepMorale(50.f, 60.f, 0, 1.0f, false, P);
	TestEqual(TEXT("Regen when safe"), Regen, 50.f + P.RegenPerTick, KINDA_SMALL_NUMBER);

	const float Capped = FNinjagoMoraleResolver::StepMorale(59.f, 60.f, 0, 1.0f, false, P);
	TestEqual(TEXT("Regen caps at max morale"), Capped, 60.f, KINDA_SMALL_NUMBER);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoMoraleRoutingTest,
	"Ninjago.Morale.BreakAndRallyHysteresis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoMoraleRoutingTest::RunTest(const FString&)
{
	const FNinjagoMoraleParams P = DefaultParams(); // Break 0.2, Rally 0.5
	const float Max = 100.f;

	// Not yet routing: high morale holds, morale at/under the break line breaks.
	TestFalse(TEXT("High morale does not rout"), FNinjagoMoraleResolver::ResolveRouting(60.f, Max, false, P));
	TestTrue(TEXT("Morale at the break line routs"), FNinjagoMoraleResolver::ResolveRouting(20.f, Max, false, P));

	// Already routing: stays routing until morale climbs past the (higher) rally line.
	TestTrue(TEXT("Below rally stays routing"), FNinjagoMoraleResolver::ResolveRouting(40.f, Max, true, P));
	TestFalse(TEXT("At/above rally line rallies"), FNinjagoMoraleResolver::ResolveRouting(50.f, Max, true, P));

	// The hysteresis gap: 30 morale is above break but below rally -> routing state persists, non-routing stays steady.
	TestFalse(TEXT("30 morale does not newly break"), FNinjagoMoraleResolver::ResolveRouting(30.f, Max, false, P));
	TestTrue(TEXT("30 morale keeps a router routing"), FNinjagoMoraleResolver::ResolveRouting(30.f, Max, true, P));

	return true;
}

#endif // WITH_AUTOMATION_TESTS
