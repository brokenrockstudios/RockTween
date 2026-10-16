// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RockTweenMath.generated.h"

namespace EEasingFunc { enum Type : int; }

/**
 * The ease set. Names are the JSON names. Curve (T-282) is added with its task. See Docs/RockTween/Design.md section 7.
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
	// Procedural shapes (T-270): the output does not go from 0 to 1, see RockTweenEase::EvaluateShape.
	DampedWiggle, Punch, Shake, Spring,
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

	/** Shapes: oscillations per second (DampedWiggle, Punch, Spring natural frequency) or noise points per second (Shake). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ease")
	float Frequency = 4.f;

	/** DampedWiggle, Punch: decay rate k in e^(-k t), per second. Spring: the damping ratio (below 1 overshoots, 1 or more does not). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ease")
	float Decay = 3.f;

	/** Shake: seeds the noise. The same seed gives the same shake on every machine; the global RNG is never used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ease")
	int32 Seed = 0;
};

/**
 * Pure ease functions: no UObject state, no world. Formulas are Robert Penner's easing equations (BSD) and Phaser's ease set (MIT),
 * written fresh here; the Elastic generalization with Amplitude and Period follows Phaser.
 */
namespace RockTweenEase
{
	/**
	 * Eased value for Alpha in [0, 1]. Alpha is clamped first (NaN counts as 0). E(0) == 0 and E(1) == 1 exactly for every ease
	 * except the shapes; Back, Elastic and Bounce leave [0, 1] in between. A shape here is EvaluateShape for a one second leg
	 * (Seconds = Alpha); the tween code calls EvaluateShape with the real seconds.
	 */
	ROCKTWEEN_API double Evaluate(ERockTweenEase Ease, double Alpha, const FRockTweenEaseParams& Params = FRockTweenEaseParams());

	/** True for DampedWiggle, Punch, Shake and Spring: their output is not an easing from 0 to 1 and they run on seconds. */
	ROCKTWEEN_API bool IsShape(ERockTweenEase Ease);

	/**
	 * A procedural shape at Seconds into a leg of Duration seconds (frequency and decay are physical, so seconds, not the normalized
	 * alpha). The channel lerps From + (To - From) * s, so To - From is the amplitude and From the rest value.
	 * DampedWiggle cos(2 pi f t) e^(-k t) and Punch sin(2 pi f t) e^(-k t) are cut to 0 at Duration; Shake is seeded value noise
	 * fading linearly to 0 at Duration; Spring is the closed-form damped step response (damping ratio Decay), snapped to 1 at
	 * Duration. Negative or NaN Seconds give 0 for every shape. Not a shape: returns Evaluate(Ease, Seconds / Duration).
	 */
	ROCKTWEEN_API double EvaluateShape(ERockTweenEase Ease, double Seconds, double Duration, const FRockTweenEaseParams& Params = FRockTweenEaseParams());

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
