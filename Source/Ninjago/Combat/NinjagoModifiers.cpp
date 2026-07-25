// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Combat/NinjagoModifiers.h"

void FNinjagoModifiers::Add(ENinjagoStat Stat, float FlatAdd, float PercentAdd, float DurationSeconds)
{
	if (DurationSeconds <= 0.f || (FlatAdd == 0.f && PercentAdd == 0.f))
	{
		return;
	}
	FNinjagoModifier M;
	M.Stat = Stat;
	M.FlatAdd = FlatAdd;
	M.PercentAdd = PercentAdd;
	M.Remaining = DurationSeconds;
	Active.Add(M);
}

void FNinjagoModifiers::Tick(float DeltaSeconds)
{
	for (int32 i = Active.Num() - 1; i >= 0; --i)
	{
		Active[i].Remaining -= DeltaSeconds;
		if (Active[i].Remaining <= 0.f)
		{
			Active.RemoveAtSwap(i);
		}
	}
}

float FNinjagoModifiers::Apply(ENinjagoStat Stat, float Base) const
{
	float Flat = 0.f;
	float Percent = 0.f;
	for (const FNinjagoModifier& M : Active)
	{
		if (M.Stat == Stat)
		{
			Flat += M.FlatAdd;
			Percent += M.PercentAdd;
		}
	}
	const float Value = (Base + Flat) * (1.f + Percent / 100.f);
	return FMath::Max(0.f, Value);
}
