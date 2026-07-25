// Copyright Chris Lawrence. Personal project, not for distribution.

#include "Units/NinjagoFormation.h"

int32 FNinjagoFormation::Columns(int32 Count)
{
	if (Count <= 1)
	{
		return 1;
	}

	// Smallest divisor of Count that is >= ceil(sqrt(Count)) -> a full, wide rectangle.
	int32 Cols = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Count)));
	while (Count % Cols != 0)
	{
		++Cols;
	}
	return Cols;
}

FVector2D FNinjagoFormation::SlotOffset(int32 Index, int32 Count, float Spacing)
{
	if (Count <= 1)
	{
		return FVector2D::ZeroVector;
	}

	Index = FMath::Clamp(Index, 0, Count - 1);

	const int32 Cols = Columns(Count);
	const int32 Rows = Count / Cols; // exact: Cols divides Count

	const int32 Col = Index % Cols;
	const int32 Row = Index / Cols;

	// Centre both axes about the origin so the block is symmetric.
	const float X = (static_cast<float>(Col) - (Cols - 1) * 0.5f) * Spacing;
	const float Y = (static_cast<float>(Row) - (Rows - 1) * 0.5f) * Spacing;
	return FVector2D(X, Y);
}
