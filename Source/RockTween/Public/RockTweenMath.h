// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RockTweenMath.generated.h"

namespace EEasingFunc { enum Type : int; }

/**
 * The ease set. Names are the JSON names. The procedural shapes (DampedWiggle, Punch, Shake, Spring: T-270) and Curve (T-282)
 * are added with their tasks. See Docs/RockTween/Design.md section 7.
 */
UENUM(BlueprintType)
enum class ERockTweenEase : uint8
{
	Linear,
	InSine, OutSine, InOutSine,
	InQuad, OutQuad, InOutQuad,
	InCubic, OutCubic, InOutCubic,
	InQuart, OutQuart, InOutQuart,
	InQuint, OutQuint, InOutQuint,
	InExpo, OutExpo, InOutExpo,
	InCirc, OutCirc, InOutCirc,
	InBack, OutBack, InOutBack,
	InElastic, OutElastic, InOutElastic,
	InBounce, OutBounce, InOutBounce,
	Smoothstep,
	Stepped,
};

/** Named parameters with explicit defaults (no "zero means default"). Each ease reads only the fields listed beside it. */
USTRUCT(BlueprintType)
struct ROCKTWEEN_API FRockTweenEaseParams
{
	GENERATED_BODY()

	/** Back: overshoot amount (the InOut variant multiplies it by 1.525). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ease")
	float Overshoot = 1.70158f;

	/** Elastic: wave amplitude, clamped to at least 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ease")
	float Amplitude = 1.f;

	/** Elastic: period as a fraction of the leg (never absolute seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ease")
	float Period = 0.3f;

	/** Stepped: the number of jumps. Output is floor(t * Steps) / Steps, and 1 at t = 1. Clamped to at least 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ease")
	int32 Steps = 4;

	/** Smoothstep: the input range mapped to 0..1. Equal values are a hard step at X0; X0 > X1 is swapped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ease")
	float SmoothX0 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ease")
	float SmoothX1 = 1.f;
};

/**
 * Pure ease functions: no UObject state, no world. Formulas are Robert Penner's easing equations (BSD) and Phaser's ease set (MIT),
 * written fresh here; the Elastic generalization with Amplitude and Period follows Phaser.
 */
namespace RockTweenEase
{
	/**
	 * Eased value for Alpha in [0, 1]. Alpha is clamped first (NaN counts as 0). E(0) == 0 and E(1) == 1 exactly for every ease;
	 * Back, Elastic and Bounce leave [0, 1] in between.
	 */
	ROCKTWEEN_API double Evaluate(ERockTweenEase Ease, double Alpha, const FRockTweenEaseParams& Params = FRockTweenEaseParams());

	/** JSON name of an ease (the enum name without the namespace). */
	ROCKTWEEN_API FString ToName(ERockTweenEase Ease);

	/**
	 * Parses a JSON name: every ERockTweenEase name, plus the RockUI aliases EaseIn, EaseOut and EaseInOut (InCubic, OutCubic,
	 * InOutCubic: they match ECurveEaseFunction::Cubic*). Case-sensitive. Returns false and leaves OutEase alone for any other name.
	 */
	ROCKTWEEN_API bool TryFromName(FStringView Name, ERockTweenEase& OutEase);

	/**
	 * The ease for a Blueprint EEasingFunc, for users moving off UKismetMathLibrary::Ease. EaseIn, EaseOut and EaseInOut map to the
	 * Quad family (the Kismet default BlendExp of 2); Step maps to Stepped, with OutParams->Steps set to 2 (Kismet's default) when given.
	 */
	ROCKTWEEN_API ERockTweenEase FromEasingFunc(EEasingFunc::Type Func, FRockTweenEaseParams* OutParams = nullptr);
}
