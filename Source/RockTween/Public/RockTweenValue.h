// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RockTweenValue.generated.h"

/** One tweened value in channel units: four doubles, enough for a vector, rotator, quaternion or linear color. Each channel says what X..W mean. */
USTRUCT(BlueprintType)
struct ROCKTWEEN_API FRockTweenValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value")
	double X = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value")
	double Y = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value")
	double Z = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Value")
	double W = 0.0;

	FRockTweenValue() = default;
	FRockTweenValue(double InX, double InY = 0.0, double InZ = 0.0, double InW = 0.0) : X(InX), Y(InY), Z(InZ), W(InW) {}

	/** From + (To - From) * Alpha per component, but exactly From at Alpha 0 and exactly To at Alpha 1 (no float error at the leg ends). */
	static FRockTweenValue Lerp(const FRockTweenValue& From, const FRockTweenValue& To, double Alpha)
	{
		if (Alpha == 0.0)
		{
			return From;
		}
		if (Alpha == 1.0)
		{
			return To;
		}
		return FRockTweenValue(From.X + (To.X - From.X) * Alpha, From.Y + (To.Y - From.Y) * Alpha, From.Z + (To.Z - From.Z) * Alpha, From.W + (To.W - From.W) * Alpha);
	}

	friend bool operator==(const FRockTweenValue& A, const FRockTweenValue& B) { return A.X == B.X && A.Y == B.Y && A.Z == B.Z && A.W == B.W; }
	friend bool operator!=(const FRockTweenValue& A, const FRockTweenValue& B) { return !(A == B); }
};
