// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "RockTweenMath.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "CQTest.h"
#include <limits>
#include "Kismet/KismetMathLibrary.h"

namespace RockTweenEaseTestsPrivate
{
	constexpr ERockTweenEase AllEases[] = {
		ERockTweenEase::Linear,
		ERockTweenEase::InSine, ERockTweenEase::OutSine, ERockTweenEase::InOutSine,
		ERockTweenEase::InQuad, ERockTweenEase::OutQuad, ERockTweenEase::InOutQuad,
		ERockTweenEase::InCubic, ERockTweenEase::OutCubic, ERockTweenEase::InOutCubic,
		ERockTweenEase::InQuart, ERockTweenEase::OutQuart, ERockTweenEase::InOutQuart,
		ERockTweenEase::InQuint, ERockTweenEase::OutQuint, ERockTweenEase::InOutQuint,
		ERockTweenEase::InExpo, ERockTweenEase::OutExpo, ERockTweenEase::InOutExpo,
		ERockTweenEase::InCirc, ERockTweenEase::OutCirc, ERockTweenEase::InOutCirc,
		ERockTweenEase::InBack, ERockTweenEase::OutBack, ERockTweenEase::InOutBack,
		ERockTweenEase::InElastic, ERockTweenEase::OutElastic, ERockTweenEase::InOutElastic,
		ERockTweenEase::InBounce, ERockTweenEase::OutBounce, ERockTweenEase::InOutBounce,
		ERockTweenEase::Smoothstep, ERockTweenEase::Stepped,
	};

	// The In and Out families that must not decrease (Back, Elastic and Bounce are excluded: they overshoot or bounce).
	bool IsMonotonic(ERockTweenEase Ease)
	{
		const FString Name = RockTweenEase::ToName(Ease);
		return !(Name.Contains(TEXT("Back")) || Name.Contains(TEXT("Elastic")) || Name.Contains(TEXT("Bounce")));
	}
}

TEST_CLASS(RockTweenEaseTests, "BRS.RockTween.Ease")
{
	TEST_METHOD(EveryEase_EndsExactlyOnZeroAndOne)
	{
		for (const ERockTweenEase Ease : RockTweenEaseTestsPrivate::AllEases)
		{
			ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(Ease, 0.0) == 0.0));
			ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(Ease, 1.0) == 1.0));
		}
	}

	TEST_METHOD(EveryEase_ClampsAlphaAndSurvivesNaN)
	{
		for (const ERockTweenEase Ease : RockTweenEaseTestsPrivate::AllEases)
		{
			ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(Ease, -3.0) == 0.0));
			ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(Ease, 7.5) == 1.0));
			ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(Ease, std::numeric_limits<double>::quiet_NaN()) == 0.0));
		}
	}

	TEST_METHOD(EveryEase_IsFiniteAcrossTheLeg)
	{
		for (const ERockTweenEase Ease : RockTweenEaseTestsPrivate::AllEases)
		{
			for (int32 I = 0; I <= 1000; ++I)
			{
				ASSERT_THAT(IsTrue(FMath::IsFinite(RockTweenEase::Evaluate(Ease, I / 1000.0))));
			}
		}
	}

	TEST_METHOD(InAndOutFamilies_AreMonotonic)
	{
		for (const ERockTweenEase Ease : RockTweenEaseTestsPrivate::AllEases)
		{
			if (!RockTweenEaseTestsPrivate::IsMonotonic(Ease))
			{
				continue;
			}
			double Previous = 0.0;
			for (int32 I = 1; I <= 1000; ++I)
			{
				const double Value = RockTweenEase::Evaluate(Ease, I / 1000.0);
				ASSERT_THAT(IsTrue(Value >= Previous - 1e-12));
				Previous = Value;
			}
		}
	}

	TEST_METHOD(KnownMidpoints)
	{
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::Linear, 0.3), 0.3, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::InQuad, 0.5), 0.25, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::OutQuad, 0.5), 0.75, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::InCubic, 0.5), 0.125, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::OutCubic, 0.5), 0.875, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::InOutQuad, 0.25), 0.125, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::InOutQuint, 0.5), 0.5, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::InOutSine, 0.5), 0.5, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::InOutExpo, 0.5), 0.5, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::InOutCirc, 0.5), 0.5, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::InOutBounce, 0.5), 0.5, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::Smoothstep, 0.5), 0.5, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::OutBounce, 0.5), 0.765625, 1e-9));
	}

	TEST_METHOD(InOutEases_AreSymmetricAboutTheCentre)
	{
		constexpr ERockTweenEase InOuts[] = {
			ERockTweenEase::InOutSine, ERockTweenEase::InOutQuad, ERockTweenEase::InOutCubic, ERockTweenEase::InOutQuart,
			ERockTweenEase::InOutQuint, ERockTweenEase::InOutExpo, ERockTweenEase::InOutCirc, ERockTweenEase::InOutBack,
			ERockTweenEase::InOutBounce, ERockTweenEase::Smoothstep,
		};
		for (const ERockTweenEase Ease : InOuts)
		{
			for (int32 K = 1; K < 10; ++K)
			{
				const double P = K / 20.0;
				const double Sum = RockTweenEase::Evaluate(Ease, P) + RockTweenEase::Evaluate(Ease, 1.0 - P);
				ASSERT_THAT(IsTrue(FMath::Abs(Sum - 1.0) < 1e-9));
			}
		}
	}

	TEST_METHOD(InOutElastic_IsContinuousAtTheMiddle)
	{
		const FRockTweenEaseParams Defaults;
		FRockTweenEaseParams Strong;
		Strong.Amplitude = 2.5f;
		Strong.Period = 0.45f;
		for (const FRockTweenEaseParams& Params : {Defaults, Strong})
		{
			const double Below = RockTweenEase::Evaluate(ERockTweenEase::InOutElastic, 0.5 - 1e-9, Params);
			const double Above = RockTweenEase::Evaluate(ERockTweenEase::InOutElastic, 0.5 + 1e-9, Params);
			ASSERT_THAT(IsTrue(FMath::Abs(Above - Below) < 1e-5));
			ASSERT_THAT(IsNear(Above, 0.5, 1e-5));
		}
	}

	TEST_METHOD(Back_OvershootsByTheParameterAndZeroOvershootIsPlainCubic)
	{
		// OutBack passes 1 before it lands; InBack dips below 0.
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::OutBack, 0.7) > 1.0));
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::InBack, 0.3) < 0.0));

		// Overshoot 0 is explicit (not "use the default"): OutBack becomes OutCubic, InBack becomes InCubic.
		FRockTweenEaseParams Flat;
		Flat.Overshoot = 0.f;
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::OutBack, 0.4, Flat), RockTweenEase::Evaluate(ERockTweenEase::OutCubic, 0.4), 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::InBack, 0.4, Flat), RockTweenEase::Evaluate(ERockTweenEase::InCubic, 0.4), 1e-12));

		FRockTweenEaseParams Big;
		Big.Overshoot = 4.f;
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::OutBack, 0.7, Big) > RockTweenEase::Evaluate(ERockTweenEase::OutBack, 0.7)));
	}

	TEST_METHOD(Elastic_AmplitudeBelowOneIsClampedAndPeriodIsAFraction)
	{
		FRockTweenEaseParams Small;
		Small.Amplitude = 0.f;
		for (const ERockTweenEase Ease : {ERockTweenEase::InElastic, ERockTweenEase::OutElastic, ERockTweenEase::InOutElastic})
		{
			for (int32 I = 0; I <= 100; ++I)
			{
				const double Value = RockTweenEase::Evaluate(Ease, I / 100.0, Small);
				ASSERT_THAT(IsTrue(FMath::IsFinite(Value)));
				ASSERT_THAT(IsTrue(Value == RockTweenEase::Evaluate(Ease, I / 100.0)));
			}
		}

		// A zero or negative period must not give NaN or Inf.
		FRockTweenEaseParams NoPeriod;
		NoPeriod.Period = 0.f;
		for (int32 I = 0; I <= 100; ++I)
		{
			ASSERT_THAT(IsTrue(FMath::IsFinite(RockTweenEase::Evaluate(ERockTweenEase::OutElastic, I / 100.0, NoPeriod))));
		}

		// OutElastic overshoots 1 early in the leg.
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::OutElastic, 0.1) > 1.0));
	}

	TEST_METHOD(Smoothstep_RangeAndDegenerateRange)
	{
		FRockTweenEaseParams Range;
		Range.SmoothX0 = 0.25f;
		Range.SmoothX1 = 0.75f;
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Smoothstep, 0.2, Range) == 0.0));
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Smoothstep, 0.8, Range) == 1.0));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::Smoothstep, 0.5, Range), 0.5, 1e-12));

		// X0 == X1 is a hard step at X0, never a division by zero.
		FRockTweenEaseParams Hard;
		Hard.SmoothX0 = 0.5f;
		Hard.SmoothX1 = 0.5f;
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Smoothstep, 0.49, Hard) == 0.0));
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Smoothstep, 0.5, Hard) == 1.0));
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Smoothstep, 0.51, Hard) == 1.0));

		// X0 > X1 is swapped.
		FRockTweenEaseParams Swapped;
		Swapped.SmoothX0 = 0.75f;
		Swapped.SmoothX1 = 0.25f;
		for (double P = 0.0; P <= 1.0; P += 0.1)
		{
			ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Smoothstep, P, Swapped) == RockTweenEase::Evaluate(ERockTweenEase::Smoothstep, P, Range)));
		}
	}

	TEST_METHOD(Stepped_CountsJumpsAndEndsOnOne)
	{
		FRockTweenEaseParams Params;
		Params.Steps = 4;
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Stepped, 0.1, Params) == 0.0));
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Stepped, 0.25, Params) == 0.25));
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Stepped, 0.6, Params) == 0.5));
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Stepped, 0.99, Params) == 0.75));
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Stepped, 1.0, Params) == 1.0));

		for (const int32 Steps : {1, 2, 3, 10})
		{
			Params.Steps = Steps;
			TSet<double> Distinct;
			for (int32 I = 0; I < 1000; ++I)
			{
				Distinct.Add(RockTweenEase::Evaluate(ERockTweenEase::Stepped, I / 1000.0, Params));
			}
			ASSERT_THAT(IsTrue(Distinct.Num() == Steps));
		}

		// Zero or negative steps count as one jump rather than dividing by zero.
		Params.Steps = 0;
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Stepped, 0.9, Params) == 0.0));
		Params.Steps = -5;
		ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(ERockTweenEase::Stepped, 0.9, Params) == 0.0));
	}

	TEST_METHOD(Names_RoundTripForEveryEase)
	{
		for (const ERockTweenEase Ease : RockTweenEaseTestsPrivate::AllEases)
		{
			ERockTweenEase Parsed = ERockTweenEase::Stepped;
			ASSERT_THAT(IsTrue(RockTweenEase::TryFromName(RockTweenEase::ToName(Ease), Parsed)));
			ASSERT_THAT(IsTrue(Parsed == Ease));
		}
		ASSERT_THAT(IsTrue(RockTweenEase::ToName(ERockTweenEase::InOutBack) == TEXT("InOutBack")));
	}

	TEST_METHOD(Names_RockUIAliasesMapToCubic)
	{
		ERockTweenEase Parsed = ERockTweenEase::Linear;
		ASSERT_THAT(IsTrue(RockTweenEase::TryFromName(TEXT("EaseIn"), Parsed)));
		ASSERT_THAT(IsTrue(Parsed == ERockTweenEase::InCubic));
		ASSERT_THAT(IsTrue(RockTweenEase::TryFromName(TEXT("EaseOut"), Parsed)));
		ASSERT_THAT(IsTrue(Parsed == ERockTweenEase::OutCubic));
		ASSERT_THAT(IsTrue(RockTweenEase::TryFromName(TEXT("EaseInOut"), Parsed)));
		ASSERT_THAT(IsTrue(Parsed == ERockTweenEase::InOutCubic));
	}

	TEST_METHOD(Names_UnknownLeavesTheOutputAlone)
	{
		ERockTweenEase Parsed = ERockTweenEase::OutBack;
		ASSERT_THAT(IsFalse(RockTweenEase::TryFromName(TEXT("Wobble"), Parsed)));
		ASSERT_THAT(IsFalse(RockTweenEase::TryFromName(TEXT(""), Parsed)));
		ASSERT_THAT(IsFalse(RockTweenEase::TryFromName(TEXT("inquad"), Parsed)));
		ASSERT_THAT(IsFalse(RockTweenEase::TryFromName(TEXT("ERockTweenEase::InQuad"), Parsed)));
		ASSERT_THAT(IsTrue(Parsed == ERockTweenEase::OutBack));
	}

	TEST_METHOD(FromEasingFunc_MapsEveryEngineValue)
	{
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::Linear) == ERockTweenEase::Linear));
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::SinusoidalIn) == ERockTweenEase::InSine));
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::SinusoidalOut) == ERockTweenEase::OutSine));
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::SinusoidalInOut) == ERockTweenEase::InOutSine));
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::EaseIn) == ERockTweenEase::InQuad));
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::EaseOut) == ERockTweenEase::OutQuad));
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::EaseInOut) == ERockTweenEase::InOutQuad));
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::ExpoIn) == ERockTweenEase::InExpo));
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::ExpoOut) == ERockTweenEase::OutExpo));
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::ExpoInOut) == ERockTweenEase::InOutExpo));
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::CircularIn) == ERockTweenEase::InCirc));
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::CircularOut) == ERockTweenEase::OutCirc));
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::CircularInOut) == ERockTweenEase::InOutCirc));
	}

	TEST_METHOD(FromEasingFunc_StepUsesKismetsTwoSteps)
	{
		FRockTweenEaseParams Params;
		ASSERT_THAT(IsTrue(RockTweenEase::FromEasingFunc(EEasingFunc::Step, &Params) == ERockTweenEase::Stepped));
		ASSERT_THAT(IsTrue(Params.Steps == 2));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
