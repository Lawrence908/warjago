// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Misc/AutomationTest.h"
#include "Combat/NinjagoCombatResolver.h"

#if WITH_AUTOMATION_TESTS

// Named (not anonymous) so file-local helpers don't collide with identically
// named helpers in sibling spec files when UE merges them into a unity build.
namespace NinjagoCombatSpec
{
	// Default tuning, matching UNinjagoSettings defaults.
	FNinjagoCombatParams DefaultParams()
	{
		return FNinjagoCombatParams{};
	}
}
using namespace NinjagoCombatSpec;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoHitChanceClampTest,
	"Ninjago.Combat.HitChanceClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoHitChanceClampTest::RunTest(const FString&)
{
	const FNinjagoCombatParams P = DefaultParams();

	// Even matchup -> base chance.
	TestEqual(TEXT("Attack == Defence gives base chance"),
		FNinjagoCombatResolver::HitChance(5, 5, P), P.HitChanceBase, KINDA_SMALL_NUMBER);

	// Huge attacker advantage clamps to the upper bound, never above.
	TestEqual(TEXT("Overwhelming attack clamps to HitChanceMax"),
		FNinjagoCombatResolver::HitChance(100, 0, P), P.HitChanceMax, KINDA_SMALL_NUMBER);

	// Huge defender advantage clamps to the lower bound, never below.
	TestEqual(TEXT("Overwhelming defence clamps to HitChanceMin"),
		FNinjagoCombatResolver::HitChance(0, 100, P), P.HitChanceMin, KINDA_SMALL_NUMBER);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoDamageFloorTest,
	"Ninjago.Combat.DamageFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoDamageFloorTest::RunTest(const FString&)
{
	const FNinjagoCombatParams P = DefaultParams();

	// The canonical case from CLAUDE.md: Armour 5 vs Damage 14 / AP 0 must deal 1, not 0.
	const int32 Dealt = FNinjagoCombatResolver::DamageOnHit(/*Damage*/14, /*AP*/0, /*Armour*/5, P);
	TestEqual(TEXT("Armour 5 vs Damage 14 / AP 0 deals the floor, not zero"), Dealt, P.MinDamage);
	TestTrue(TEXT("A landed hit is never zero"), Dealt >= 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoArmourPiercingTest,
	"Ninjago.Combat.ArmourPiercing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoArmourPiercingTest::RunTest(const FString&)
{
	const FNinjagoCombatParams P = DefaultParams();

	// Against heavy armour the base damage floors out; AP is added on top, unreduced.
	const int32 WithoutAp = FNinjagoCombatResolver::DamageOnHit(/*Damage*/10, /*AP*/0, /*Armour*/8, P);
	const int32 WithAp    = FNinjagoCombatResolver::DamageOnHit(/*Damage*/10, /*AP*/7, /*Armour*/8, P);

	TestEqual(TEXT("Base portion floors against heavy armour"), WithoutAp, P.MinDamage);
	TestEqual(TEXT("Armour-piercing is added on top, bypassing armour"), WithAp - WithoutAp, 7);
	TestEqual(TEXT("Total = AP + floor"), WithAp, 7 + P.MinDamage);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoResolveDeterminismTest,
	"Ninjago.Combat.ResolveGuaranteedHitAndMiss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoResolveDeterminismTest::RunTest(const FString&)
{
	FRandomStream Rng(1337);

	// Force a guaranteed hit by clamping the chance window to 1.0.
	FNinjagoCombatParams AlwaysHit = DefaultParams();
	AlwaysHit.HitChanceMin = 1.0f;
	AlwaysHit.HitChanceMax = 1.0f;

	// Force a guaranteed miss by clamping the chance window to 0.0.
	FNinjagoCombatParams AlwaysMiss = DefaultParams();
	AlwaysMiss.HitChanceMin = 0.0f;
	AlwaysMiss.HitChanceMax = 0.0f;

	for (int32 i = 0; i < 100; ++i)
	{
		const int32 Hit = FNinjagoCombatResolver::ResolveAttack(5, 20, 3, 5, 2, AlwaysHit, Rng);
		TestTrue(TEXT("Guaranteed hit always deals damage"), Hit > 0);

		const int32 Miss = FNinjagoCombatResolver::ResolveAttack(5, 20, 3, 5, 2, AlwaysMiss, Rng);
		TestEqual(TEXT("Guaranteed miss always deals zero"), Miss, 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoRangedHitChanceTest,
	"Ninjago.Combat.RangedHitChanceClamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoRangedHitChanceTest::RunTest(const FString&)
{
	const FNinjagoCombatParams P = DefaultParams();

	// Accuracy scales the base chance and clamps to the shared [Min, Max] window.
	const float Mid = FNinjagoCombatResolver::RangedHitChance(6, P);
	TestEqual(TEXT("Accuracy 6 -> base + 6*perPoint"),
		Mid, P.RangedHitChanceBase + 6.f * P.RangedHitChancePerPoint, KINDA_SMALL_NUMBER);

	TestEqual(TEXT("Zero accuracy clamps up to HitChanceMin"),
		FNinjagoCombatResolver::RangedHitChance(0, P), FMath::Max(P.HitChanceMin, P.RangedHitChanceBase), KINDA_SMALL_NUMBER);

	TestEqual(TEXT("Huge accuracy clamps to HitChanceMax"),
		FNinjagoCombatResolver::RangedHitChance(100, P), P.HitChanceMax, KINDA_SMALL_NUMBER);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoRangedResolveTest,
	"Ninjago.Combat.RangedResolveHitAndMiss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoRangedResolveTest::RunTest(const FString&)
{
	FRandomStream Rng(99);

	FNinjagoCombatParams AlwaysHit = DefaultParams();
	AlwaysHit.HitChanceMin = 1.0f;
	AlwaysHit.HitChanceMax = 1.0f;

	FNinjagoCombatParams AlwaysMiss = DefaultParams();
	AlwaysMiss.HitChanceMin = 0.0f;
	AlwaysMiss.HitChanceMax = 0.0f;

	// A guaranteed ranged hit deals the same damage model as melee (floor + AP).
	const int32 Hit = FNinjagoCombatResolver::ResolveRangedAttack(6, /*Dmg*/9, /*AP*/0, /*Armour*/5, AlwaysHit, Rng);
	TestEqual(TEXT("Ranged hit uses the damage floor (9 vs armour 5 -> 1)"), Hit, AlwaysHit.MinDamage);

	const int32 Miss = FNinjagoCombatResolver::ResolveRangedAttack(6, 9, 0, 5, AlwaysMiss, Rng);
	TestEqual(TEXT("Guaranteed ranged miss deals zero"), Miss, 0);

	// Armour-piercing arrows still bypass armour.
	const int32 Piercing = FNinjagoCombatResolver::ResolveRangedAttack(5, /*Dmg*/9, /*AP*/4, /*Armour*/5, AlwaysHit, Rng);
	TestEqual(TEXT("Ranged AP adds on top of the floor"), Piercing, 4 + AlwaysHit.MinDamage);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoChargeAttackTest,
	"Ninjago.Combat.ChargeAddsBonusDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoChargeAttackTest::RunTest(const FString&)
{
	FNinjagoCombatParams AlwaysHit = DefaultParams();
	AlwaysHit.HitChanceMin = 1.0f;
	AlwaysHit.HitChanceMax = 1.0f;

	FRandomStream Rng(7);

	// Against no armour, a charge deals exactly ChargeBonus more than a normal strike.
	const int32 Normal = FNinjagoCombatResolver::ResolveAttack(/*Atk*/5, /*Dmg*/14, /*AP*/0, /*Def*/4, /*Armour*/0, AlwaysHit, Rng);
	const int32 Charge = FNinjagoCombatResolver::ResolveChargeAttack(/*Atk*/5, /*Dmg*/14, /*Charge*/4, /*AP*/0, /*Def*/4, /*Armour*/0, AlwaysHit, Rng);
	TestEqual(TEXT("Charge adds ChargeBonus to unarmoured damage"), Charge - Normal, 4);

	// The charge bonus goes through the same floor + armour model (still never zero on a hit).
	const int32 ChargeArmoured = FNinjagoCombatResolver::ResolveChargeAttack(5, 14, 4, 0, 4, /*Armour*/5, AlwaysHit, Rng);
	TestTrue(TEXT("Charged hit is never zero"), ChargeArmoured >= AlwaysHit.MinDamage);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNinjagoSpearAntiLargeTest,
	"Ninjago.Combat.SpearsBraceAndAntiLarge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNinjagoSpearAntiLargeTest::RunTest(const FString&)
{
	FNinjagoCombatParams AlwaysHit = DefaultParams();
	AlwaysHit.HitChanceMin = 1.0f;
	AlwaysHit.HitChanceMax = 1.0f;
	FRandomStream Rng(11);

	const int32 AntiLarge = 10;
	// Spear (attacker) vs large (defender): base + anti-large bonus, no armour.
	const int32 SpearVsLarge = FNinjagoCombatResolver::ResolveMelee(5, /*Dmg*/13, /*Charge*/0, /*AP*/0, 4, /*Armour*/0,
		/*Charging*/false, /*AtkSpear*/true, /*AtkLarge*/false, /*DefSpear*/false, /*DefLarge*/true, AntiLarge, AlwaysHit, Rng);
	TestEqual(TEXT("Spear vs large adds the anti-large bonus"), SpearVsLarge, 13 + AntiLarge);

	// Same spear vs a non-large target: no bonus.
	const int32 SpearVsSmall = FNinjagoCombatResolver::ResolveMelee(5, 13, 0, 0, 4, 0,
		false, true, false, false, /*DefLarge*/false, AntiLarge, AlwaysHit, Rng);
	TestEqual(TEXT("Spear vs non-large gets no bonus"), SpearVsSmall, 13);

	// Large charger into a spear defender: the spear braces, charge bonus negated.
	const int32 Braced = FNinjagoCombatResolver::ResolveMelee(5, 13, /*Charge*/12, 0, 4, 0,
		/*Charging*/true, /*AtkSpear*/false, /*AtkLarge*/true, /*DefSpear*/true, /*DefLarge*/false, AntiLarge, AlwaysHit, Rng);
	TestEqual(TEXT("Spear braces: large charge bonus negated"), Braced, 13);

	// Same charge into a non-spear defender: charge bonus applies.
	const int32 Unbraced = FNinjagoCombatResolver::ResolveMelee(5, 13, 12, 0, 4, 0,
		true, false, true, /*DefSpear*/false, false, AntiLarge, AlwaysHit, Rng);
	TestEqual(TEXT("Non-spear takes the full charge"), Unbraced, 13 + 12);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
