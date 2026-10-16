// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "RockTweenMath.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "CQTest.h"
#include <limits>

namespace RockTweenShapeTestsPrivate
{
	constexpr ERockTweenEase AllShapes[] = {
		ERockTweenEase::DampedWiggle, ERockTweenEase::Punch, ERockTweenEase::Shake, ERockTweenEase::Spring,
	};

	FRockTweenEaseParams Make(float Frequency, float Decay, int32 Seed = 0)
	{
		FRockTweenEaseParams Params;
		Params.Frequency = Frequency;
		Params.Decay = Decay;
		Params.Seed = Seed;
		return Params;
	}

	// The projectile Stick's formula (URockProjectileStickComponent::WiggleAngleAtTime), restated so the plugin test needs no game module.
	double StickWiggle(double Amplitude, double Frequency, double Decay, double Duration, double Elapsed)
	{
		if (Elapsed < 0.0 || Elapsed >= Duration)
		{
			return 0.0;
		}
		return Amplitude * FMath::Cos(UE_DOUBLE_TWO_PI * Frequency * Elapsed) * FMath::Exp(-Decay * Elapsed);
	}
}

TEST_CLASS(RockTweenShapeTests, "BRS.RockTween.Shape")
{
	TEST_METHOD(IsShape_OnlyTheFourShapes)
	{
		for (const ERockTweenEase Ease : RockTweenShapeTestsPrivate::AllShapes)
		{
			ASSERT_THAT(IsTrue(RockTweenEase::IsShape(Ease)));
		}
		ASSERT_THAT(IsFalse(RockTweenEase::IsShape(ERockTweenEase::Linear)));
		ASSERT_THAT(IsFalse(RockTweenEase::IsShape(ERockTweenEase::Stepped)));
		ASSERT_THAT(IsFalse(RockTweenEase::IsShape(ERockTweenEase::OutElastic)));
	}

	TEST_METHOD(DampedWiggle_EqualsStickOnTheStickTestNumbers)
	{
		struct FCase { double Amplitude, Frequency, Decay, Duration, Elapsed; };
		const FCase Cases[] = {
			{15.0, 6.0, 6.0, 1.0, 0.0},
			{10.0, 4.0, 0.0, 1.0, 1.0 / 16.0},
			{10.0, 4.0, 0.0, 1.0, 0.25},
			{10.0, 4.0, 3.0, 2.0, 0.25},
			{10.0, 4.0, 3.0, 2.0, 0.5},
			{15.0, 6.0, 0.0, 0.5, 0.5},
			{15.0, 6.0, 0.0, 0.5, 9.0},
			{15.0, 6.0, 1.0, 1.0, -1.0},
		};
		for (const FCase& Case : Cases)
		{
			const FRockTweenEaseParams Params = RockTweenShapeTestsPrivate::Make(static_cast<float>(Case.Frequency), static_cast<float>(Case.Decay));
			const double Shape = Case.Amplitude * RockTweenEase::EvaluateShape(ERockTweenEase::DampedWiggle, Case.Elapsed, Case.Duration, Params);
			ASSERT_THAT(IsNear(Shape, RockTweenShapeTestsPrivate::StickWiggle(Case.Amplitude, Case.Frequency, Case.Decay, Case.Duration, Case.Elapsed), 1e-9));
		}
	}

	TEST_METHOD(DampedWiggle_KnownValues)
	{
		const FRockTweenEaseParams Params = RockTweenShapeTestsPrivate::Make(4.f, 3.f);
		ASSERT_THAT(IsNear(RockTweenEase::EvaluateShape(ERockTweenEase::DampedWiggle, 0.0, 2.0, Params), 1.0, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::EvaluateShape(ERockTweenEase::DampedWiggle, 0.25, 2.0, Params), FMath::Exp(-0.75), 1e-9));
		ASSERT_THAT(IsNear(RockTweenEase::EvaluateShape(ERockTweenEase::DampedWiggle, 1.0 / 16.0, 2.0, RockTweenShapeTestsPrivate::Make(4.f, 0.f)), 0.0, 1e-9));
	}

	TEST_METHOD(Punch_StartsAtRestAndPeaksAtAQuarterPeriod)
	{
		const FRockTweenEaseParams Params = RockTweenShapeTestsPrivate::Make(4.f, 0.f);
		ASSERT_THAT(IsNear(RockTweenEase::EvaluateShape(ERockTweenEase::Punch, 0.0, 1.0, Params), 0.0, 1e-12));
		ASSERT_THAT(IsNear(RockTweenEase::EvaluateShape(ERockTweenEase::Punch, 1.0 / 16.0, 1.0, Params), 1.0, 1e-9));
		const FRockTweenEaseParams Decaying = RockTweenShapeTestsPrivate::Make(4.f, 3.f);
		ASSERT_THAT(IsNear(RockTweenEase::EvaluateShape(ERockTweenEase::Punch, 1.0 / 16.0, 1.0, Decaying), FMath::Exp(-3.0 / 16.0), 1e-9));
	}

	TEST_METHOD(EveryShape_IsAtRestAtTheDurationAndAfter)
	{
		for (const ERockTweenEase Ease : RockTweenShapeTestsPrivate::AllShapes)
		{
			const double Rest = Ease == ERockTweenEase::Spring ? 1.0 : 0.0;
			ASSERT_THAT(IsTrue(RockTweenEase::EvaluateShape(Ease, 0.5, 0.5) == Rest));
			ASSERT_THAT(IsTrue(RockTweenEase::EvaluateShape(Ease, 9.0, 0.5) == Rest));
			ASSERT_THAT(IsTrue(RockTweenEase::EvaluateShape(Ease, 0.0, 0.0) == Rest));
			ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(Ease, 1.0) == Rest));
			ASSERT_THAT(IsTrue(RockTweenEase::Evaluate(Ease, 4.0) == Rest));
		}
	}

	TEST_METHOD(EveryShape_IsZeroBeforeTheLegAndForNaN)
	{
		for (const ERockTweenEase Ease : RockTweenShapeTestsPrivate::AllShapes)
		{
			ASSERT_THAT(IsTrue(RockTweenEase::EvaluateShape(Ease, -0.1, 1.0) == 0.0));
			ASSERT_THAT(IsTrue(RockTweenEase::EvaluateShape(Ease, std::numeric_limits<double>::quiet_NaN(), 1.0) == 0.0));
		}
	}

	TEST_METHOD(EveryShape_IsFiniteWithExtremeParameters)
	{
		const FRockTweenEaseParams Odd[] = {
			RockTweenShapeTestsPrivate::Make(0.f, 0.f), RockTweenShapeTestsPrivate::Make(-3.f, -2.f),
			RockTweenShapeTestsPrivate::Make(1000.f, 1000.f), RockTweenShapeTestsPrivate::Make(0.f, 1.f),
			RockTweenShapeTestsPrivate::Make(5.f, 50.f), RockTweenShapeTestsPrivate::Make(5.f, 0.0001f),
		};
		for (const ERockTweenEase Ease : RockTweenShapeTestsPrivate::AllShapes)
		{
			for (const FRockTweenEaseParams& Params : Odd)
			{
				for (int32 I = 0; I <= 400; ++I)
				{
					ASSERT_THAT(IsTrue(FMath::IsFinite(RockTweenEase::EvaluateShape(Ease, I * 0.05, 20.0, Params))));
				}
			}
		}
	}

	TEST_METHOD(Shake_SameSeedSameShakeDifferentSeedDiffers)
	{
		const FRockTweenEaseParams A = RockTweenShapeTestsPrivate::Make(12.f, 0.f, 7);
		const FRockTweenEaseParams B = RockTweenShapeTestsPrivate::Make(12.f, 0.f, 8);
		bool bDiffers = false;
		for (int32 I = 0; I < 100; ++I)
		{
			const double T = I * 0.01;
			ASSERT_THAT(IsTrue(RockTweenEase::EvaluateShape(ERockTweenEase::Shake, T, 1.0, A) == RockTweenEase::EvaluateShape(ERockTweenEase::Shake, T, 1.0, A)));
			bDiffers |= RockTweenEase::EvaluateShape(ERockTweenEase::Shake, T, 1.0, A) != RockTweenEase::EvaluateShape(ERockTweenEase::Shake, T, 1.0, B);
		}
		ASSERT_THAT(IsTrue(bDiffers));
	}

	TEST_METHOD(Shake_StaysInsideTheFadingEnvelopeAndIsContinuous)
	{
		const FRockTweenEaseParams Params = RockTweenShapeTestsPrivate::Make(20.f, 0.f, 3);
		double Previous = RockTweenEase::EvaluateShape(ERockTweenEase::Shake, 0.0, 1.0, Params);
		double MaxAbs = FMath::Abs(Previous);
		for (int32 I = 1; I < 1000; ++I)
		{
			const double T = I * 0.001;
			const double Value = RockTweenEase::EvaluateShape(ERockTweenEase::Shake, T, 1.0, Params);
			ASSERT_THAT(IsTrue(FMath::Abs(Value) <= 1.0 - T + 1e-12));
			ASSERT_THAT(IsTrue(FMath::Abs(Value - Previous) < 0.1)); // 20 points per second, 1 ms steps: no jumps
			MaxAbs = FMath::Max(MaxAbs, FMath::Abs(Value));
			Previous = Value;
		}
		ASSERT_THAT(IsTrue(MaxAbs > 0.2)); // it actually moves
	}

	TEST_METHOD(Spring_StartsAtZeroSettlesOnOneAndOvershootsOnlyWhenUnderdamped)
	{
		const FRockTweenEaseParams Bouncy = RockTweenShapeTestsPrivate::Make(3.f, 0.2f);
		ASSERT_THAT(IsNear(RockTweenEase::EvaluateShape(ERockTweenEase::Spring, 0.0, 5.0, Bouncy), 0.0, 1e-12));
		double Peak = 0.0;
		for (int32 I = 0; I < 500; ++I)
		{
			Peak = FMath::Max(Peak, RockTweenEase::EvaluateShape(ERockTweenEase::Spring, I * 0.01, 5.0, Bouncy));
		}
		ASSERT_THAT(IsTrue(Peak > 1.1));
		ASSERT_THAT(IsNear(RockTweenEase::EvaluateShape(ERockTweenEase::Spring, 4.99, 5.0, Bouncy), 1.0, 1e-3));

		// Critically damped and overdamped never pass 1.
		for (const float Zeta : {1.f, 1.5f, 6.f})
		{
			const FRockTweenEaseParams Calm = RockTweenShapeTestsPrivate::Make(3.f, Zeta);
			double Previous = 0.0;
			for (int32 I = 1; I < 500; ++I)
			{
				const double Value = RockTweenEase::EvaluateShape(ERockTweenEase::Spring, I * 0.01, 5.0, Calm);
				ASSERT_THAT(IsTrue(Value <= 1.0 + 1e-9));
				ASSERT_THAT(IsTrue(Value >= Previous - 1e-9));
				Previous = Value;
			}
		}
	}

	TEST_METHOD(Spring_IsContinuousAcrossTheCriticalDamping)
	{
		const double Below = RockTweenEase::EvaluateShape(ERockTweenEase::Spring, 0.3, 5.0, RockTweenShapeTestsPrivate::Make(3.f, 0.999f));
		const double At = RockTweenEase::EvaluateShape(ERockTweenEase::Spring, 0.3, 5.0, RockTweenShapeTestsPrivate::Make(3.f, 1.f));
		const double Above = RockTweenEase::EvaluateShape(ERockTweenEase::Spring, 0.3, 5.0, RockTweenShapeTestsPrivate::Make(3.f, 1.001f));
		ASSERT_THAT(IsNear(Below, At, 1e-2));
		ASSERT_THAT(IsNear(Above, At, 1e-2));
	}

	TEST_METHOD(Spring_IsExactAtAnyFrameRate)
	{
		// A pure function of time: sampling the same instant after different step histories gives the same number.
		const FRockTweenEaseParams Params = RockTweenShapeTestsPrivate::Make(2.f, 0.4f);
		ASSERT_THAT(IsTrue(RockTweenEase::EvaluateShape(ERockTweenEase::Spring, 0.75, 2.0, Params) == RockTweenEase::EvaluateShape(ERockTweenEase::Spring, 3.0 * 0.25, 2.0, Params)));
	}

	TEST_METHOD(NonShape_EvaluateShapeFallsBackToTheNormalizedEase)
	{
		ASSERT_THAT(IsNear(RockTweenEase::EvaluateShape(ERockTweenEase::InQuad, 0.5, 2.0), 0.0625, 1e-12));
	}

	TEST_METHOD(Names_RoundTripForTheShapes)
	{
		for (const ERockTweenEase Ease : RockTweenShapeTestsPrivate::AllShapes)
		{
			ERockTweenEase Parsed = ERockTweenEase::Linear;
			ASSERT_THAT(IsTrue(RockTweenEase::TryFromName(RockTweenEase::ToName(Ease), Parsed)));
			ASSERT_THAT(IsTrue(Parsed == Ease));
		}
		ASSERT_THAT(IsTrue(RockTweenEase::ToName(ERockTweenEase::DampedWiggle) == TEXT("DampedWiggle")));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
