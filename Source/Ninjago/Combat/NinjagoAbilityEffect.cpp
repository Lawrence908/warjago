// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Combat/NinjagoAbilityEffect.h"

FNinjagoAbilityMagnitude FNinjagoAbilityEffect::ParseMagnitude(const FString& Magnitude)
{
	FNinjagoAbilityMagnitude Out;

	int32 Eq = INDEX_NONE;
	if (!Magnitude.FindChar(TEXT('='), Eq))
	{
		return Out; // no verb
	}

	Out.Verb = Magnitude.Left(Eq).TrimStartAndEnd().ToLower();
	const FString Rest = Magnitude.Mid(Eq + 1).TrimStartAndEnd();

	int32 i = 0;
	int32 Sign = 1;
	if (Rest.StartsWith(TEXT("+")))
	{
		i = 1;
	}
	else if (Rest.StartsWith(TEXT("-")))
	{
		Sign = -1;
		i = 1;
	}

	FString Digits;
	while (i < Rest.Len() && FChar::IsDigit(Rest[i]))
	{
		Digits.AppendChar(Rest[i]);
		++i;
	}

	if (Digits.Len() > 0)
	{
		Out.bNumeric = true;
		Out.Value = Sign * FCString::Atoi(*Digits);
		if (i < Rest.Len() && Rest[i] == TEXT('%'))
		{
			Out.bPercent = true;
		}
	}

	return Out;
}

TArray<int32> FNinjagoAbilityEffect::SelectNearest(const TArray<FVector>& Points, const FVector& Center,
	int32 MaxCount, float MaxRadius)
{
	const float MaxSq = MaxRadius * MaxRadius;

	TArray<TPair<float, int32>> InRange;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		const float Dsq = FVector::DistSquared2D(Points[i], Center);
		if (Dsq <= MaxSq)
		{
			InRange.Emplace(Dsq, i);
		}
	}

	InRange.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B)
	{
		return A.Key < B.Key;
	});

	TArray<int32> Result;
	const int32 Count = FMath::Min(MaxCount, InRange.Num());
	Result.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		Result.Add(InRange[i].Value);
	}
	return Result;
}
