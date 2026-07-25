// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Misc/AutomationTest.h"
#include "Combat/NinjagoModifiers.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoModifiersApplyTest,
	"Ninjago.Modifiers.FlatAndPercent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoModifiersApplyTest::RunTest(const FString&)
{
	FNinjagoModifiers Mods;

	// Flat defence buff: +3 to a base of 4 -> 7.
	Mods.Add(ENinjagoStat::MeleeDefence, /*Flat*/3.f, /*Percent*/0.f, /*Dur*/10.f);
	TestEqual(TEXT("Flat def buff"), Mods.Apply(ENinjagoStat::MeleeDefence, 4.f), 7.f, KINDA_SMALL_NUMBER);

	// A different stat is untouched.
	TestEqual(TEXT("Unaffected stat is base"), Mods.Apply(ENinjagoStat::Damage, 10.f), 10.f, KINDA_SMALL_NUMBER);

	// Percent speed buff: +100% doubles.
	Mods.Add(ENinjagoStat::Speed, 0.f, 100.f, 10.f);
	TestEqual(TEXT("Speed +100% doubles"), Mods.Apply(ENinjagoStat::Speed, 400.f), 800.f, KINDA_SMALL_NUMBER);

	// A slow debuff stacks additively on percent: +100% and -50% -> +50% net.
	Mods.Add(ENinjagoStat::Speed, 0.f, -50.f, 10.f);
	TestEqual(TEXT("Percents sum additively"), Mods.Apply(ENinjagoStat::Speed, 400.f), 600.f, KINDA_SMALL_NUMBER);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoModifiersExpiryTest,
	"Ninjago.Modifiers.Expiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoModifiersExpiryTest::RunTest(const FString&)
{
	FNinjagoModifiers Mods;
	Mods.Add(ENinjagoStat::MeleeAttack, 5.f, 0.f, /*Dur*/6.f);
	TestEqual(TEXT("Buff active"), Mods.Apply(ENinjagoStat::MeleeAttack, 5.f), 10.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("One active modifier"), Mods.Num(), 1);

	Mods.Tick(4.f); // still alive
	TestEqual(TEXT("Still buffed after 4s"), Mods.Apply(ENinjagoStat::MeleeAttack, 5.f), 10.f, KINDA_SMALL_NUMBER);

	Mods.Tick(3.f); // total 7s > 6s -> expired
	TestEqual(TEXT("Expired back to base"), Mods.Apply(ENinjagoStat::MeleeAttack, 5.f), 5.f, KINDA_SMALL_NUMBER);
	TestEqual(TEXT("No active modifiers"), Mods.Num(), 0);

	// Effective value never goes negative.
	Mods.Add(ENinjagoStat::Speed, 0.f, -200.f, 5.f);
	TestTrue(TEXT("Clamped at zero"), Mods.Apply(ENinjagoStat::Speed, 400.f) >= 0.f);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
