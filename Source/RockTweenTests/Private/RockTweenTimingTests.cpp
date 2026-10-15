// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "RockTweenTiming.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "CQTest.h"
#include <cmath>
#include <limits>
#include "Algo/Count.h"

namespace RockTweenTimingTestsPrivate
{
	using EEvent = ERockTweenTimingEvent;
	using EPhase = ERockTweenPhase;

	struct FRun
	{
		FRockTweenTiming Timing;
		FRockTweenTimingState State;
		TArray<EEvent> Events;
		FRockTweenAdvanceResult LastResult;

		explicit FRun(const FRockTweenTiming& InTiming) : Timing(InTiming) {}

		FRockTweenAdvanceResult Step(double Dt)
		{
			LastResult = RockTweenTiming::Advance(Timing, State, Dt, [this](EEvent Event) { Events.Add(Event); });
			return LastResult;
		}

		double Value() const { return RockTweenTiming::Evaluate(Timing, State); }

		bool IsComplete() const { return State.Phase == EPhase::Complete; }

		int32 Count(EEvent Event) const { return Algo::Count(Events, Event); }
	};

	// Everything dyadic, so Advance and StateAtTime agree exactly.
	FRockTweenTiming DyadicTiming()
	{
		FRockTweenTiming Timing;
		Timing.Duration = 0.25f;
		Timing.Delay = 0.125f;
		Timing.Hold = 0.0625f;
		Timing.bYoyo = true;
		Timing.Repeat = 1;
		Timing.RepeatDelay = 0.125f;
		Timing.Ease = ERockTweenEase::Linear;
		return Timing;
	}

	// The Design section 3 trace spec, not dyadic.
	FRockTweenTiming TraceTiming()
	{
		FRockTweenTiming Timing;
		Timing.Duration = 0.3f;
		Timing.Delay = 0.2f;
		Timing.Hold = 0.1f;
		Timing.bYoyo = true;
		Timing.Repeat = 1;
		Timing.RepeatDelay = 0.15f;
		Timing.Ease = ERockTweenEase::OutCubic;
		return Timing;
	}

	// Steps by Dt until Time is reached; the last step is the remainder.
	void RunTo(FRun& Run, double Time, double Dt)
	{
		double Done = 0.0;
		while (Time - Done > Dt)
		{
			Run.Step(Dt);
			Done += Dt;
		}
		Run.Step(Time - Done);
	}
}

TEST_CLASS(RockTweenTimingTests, "BRS.RockTween.Timing")
{
	using FRun = RockTweenTimingTestsPrivate::FRun;
	using EEvent = ERockTweenTimingEvent;
	using EPhase = ERockTweenPhase;

	// Fields: Duration

	TEST_METHOD(Duration_DefaultIsAQuarterSecond)
	{
		ASSERT_THAT(IsNear(0.25f, FRockTweenTiming().Duration, 1e-6f));
	}

	TEST_METHOD(Duration_LegPlaysEaseOverItsLength)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 2.f;
		Timing.Ease = ERockTweenEase::InQuad;
		FRun Run(Timing);
		Run.Step(0.5);
		ASSERT_THAT(IsTrue(Run.State.Phase == EPhase::Forward));
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::InQuad, 0.25), Run.Value(), 1e-12));
		Run.Step(1.0);
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::InQuad, 0.75), Run.Value(), 1e-12));
		Run.Step(0.5);
		ASSERT_THAT(IsTrue(Run.IsComplete()));
		ASSERT_THAT(IsTrue(Run.Value() == 1.0));
	}

	TEST_METHOD(Duration_ReachingTheBoundaryExactlyCrossesIt)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 0.5f;
		FRun Run(Timing);
		Run.Step(0.5);
		ASSERT_THAT(IsTrue(Run.IsComplete()));
		ASSERT_THAT(AreEqual(1, Run.Count(EEvent::Complete)));
	}

	// Fields: Delay

	TEST_METHOD(Delay_HoldsTheStartValueAndStartFiresWhenItEnds)
	{
		FRockTweenTiming Timing;
		Timing.Delay = 0.5f;
		FRun Run(Timing);
		Run.Step(0.25);
		ASSERT_THAT(IsTrue(Run.State.Phase == EPhase::Delay));
		ASSERT_THAT(IsTrue(Run.Value() == 0.0));
		ASSERT_THAT(AreEqual(0, Run.Events.Num()));
		Run.Step(0.25);
		ASSERT_THAT(IsTrue(Run.State.Phase == EPhase::Forward));
		ASSERT_THAT(AreEqual(1, Run.Count(EEvent::Start)));
	}

	TEST_METHOD(Delay_IsNotReplayedOnRepeat)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 1.f;
		Timing.Delay = 10.f;
		Timing.Repeat = 2;
		FRun Run(Timing);
		Run.Step(10.0);
		ASSERT_THAT(AreEqual(1, Run.Count(EEvent::Start)));
		Run.Step(3.0);
		ASSERT_THAT(IsTrue(Run.IsComplete()));
		ASSERT_THAT(AreEqual(1, Run.Count(EEvent::Start)));
		ASSERT_THAT(AreEqual(2, Run.Count(EEvent::Repeat)));
	}

	TEST_METHOD(Delay_NegativeCountsAsZero)
	{
		FRockTweenTiming Timing;
		Timing.Delay = -3.f;
		FRun Run(Timing);
		Run.Step(0.0);
		ASSERT_THAT(IsTrue(Run.State.Phase == EPhase::Forward));
		ASSERT_THAT(AreEqual(1, Run.Count(EEvent::Start)));
		ASSERT_THAT(IsTrue(RockTweenTiming::StateAtTime(Timing, 0.0).Phase == EPhase::Forward));
		ASSERT_THAT(IsNear(0.25, RockTweenTiming::TotalDuration(Timing), 1e-12));
	}

	// Fields: Hold

	TEST_METHOD(Hold_StaysAtTheEndValueBeforeCompletion)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 1.f;
		Timing.Hold = 0.5f;
		FRun Run(Timing);
		Run.Step(1.25);
		ASSERT_THAT(IsTrue(Run.State.Phase == EPhase::Hold));
		ASSERT_THAT(IsTrue(Run.Value() == 1.0));
		ASSERT_THAT(AreEqual(0, Run.Count(EEvent::Complete)));
		Run.Step(0.25);
		ASSERT_THAT(IsTrue(Run.IsComplete()));
	}

	TEST_METHOD(Hold_PrecedesTheYoyoTurn)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 1.f;
		Timing.Hold = 0.5f;
		Timing.bYoyo = true;
		FRun Run(Timing);
		Run.Step(1.25);
		ASSERT_THAT(AreEqual(0, Run.Count(EEvent::Yoyo)));
		Run.Step(0.25);
		ASSERT_THAT(AreEqual(1, Run.Count(EEvent::Yoyo)));
		ASSERT_THAT(IsTrue(Run.State.Phase == EPhase::Backward));
	}

	TEST_METHOD(Hold_PrecedesANonYoyoRepeat)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 1.f;
		Timing.Hold = 0.5f;
		Timing.Repeat = 1;
		FRun Run(Timing);
		Run.Step(1.25);
		ASSERT_THAT(AreEqual(0, Run.Count(EEvent::Repeat)));
		ASSERT_THAT(IsTrue(Run.Value() == 1.0));
		Run.Step(0.25);
		ASSERT_THAT(AreEqual(1, Run.Count(EEvent::Repeat)));
		ASSERT_THAT(IsTrue(Run.State.Phase == EPhase::Forward));
	}

	// Fields: bYoyo

	TEST_METHOD(Yoyo_BackwardLegPlaysTheForwardCurveBackwardsInTime)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 1.f;
		Timing.bYoyo = true;
		Timing.Ease = ERockTweenEase::OutCubic;
		FRun Run(Timing);
		Run.Step(1.25);
		ASSERT_THAT(IsTrue(Run.State.Phase == EPhase::Backward));
		// Ease(1 - 0.25), not 1 - Ease(0.25): the two differ for a curve that is not symmetric.
		ASSERT_THAT(IsNear(RockTweenEase::Evaluate(ERockTweenEase::OutCubic, 0.75), Run.Value(), 1e-12));
		ASSERT_THAT(IsTrue(FMath::Abs(Run.Value() - (1.0 - RockTweenEase::Evaluate(ERockTweenEase::OutCubic, 0.25))) > 0.01));
	}

	TEST_METHOD(Yoyo_EndsOnTheStartValue)
	{
		FRockTweenTiming Timing;
		Timing.bYoyo = true;
		FRun Run(Timing);
		Run.Step(10.0);
		ASSERT_THAT(IsTrue(Run.IsComplete()));
		ASSERT_THAT(IsTrue(Run.Value() == 0.0));
	}

	// Fields: Repeat

	TEST_METHOD(Repeat_ZeroPlaysOnce)
	{
		FRun Run(FRockTweenTiming{});
		Run.Step(100.0);
		ASSERT_THAT(IsTrue(Run.IsComplete()));
		ASSERT_THAT(AreEqual(0, Run.Count(EEvent::Repeat)));
	}

	TEST_METHOD(Repeat_NPlaysNPlusOneTimes)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 1.f;
		Timing.Repeat = 3;
		FRun Run(Timing);
		Run.Step(3.5);
		ASSERT_THAT(IsFalse(Run.IsComplete()));
		ASSERT_THAT(AreEqual(3, Run.State.PlaysDone));
		Run.Step(0.5);
		ASSERT_THAT(IsTrue(Run.IsComplete()));
		ASSERT_THAT(AreEqual(3, Run.Count(EEvent::Repeat)));
		ASSERT_THAT(IsNear(4.0, RockTweenTiming::TotalDuration(Timing), 1e-12));
	}

	TEST_METHOD(Repeat_ForwardPlusBackwardIsOnePlay_TheDoorJiggle)
	{
		// FCTween's NumLoops = 2 with yoyo: out, back, out, back.
		FRockTweenTiming Timing;
		Timing.Duration = 1.f;
		Timing.bYoyo = true;
		Timing.Repeat = 1;
		FRun Run(Timing);
		Run.Step(100.0);
		ASSERT_THAT(IsTrue(Run.IsComplete()));
		ASSERT_THAT(AreEqual(2, Run.Count(EEvent::Yoyo)));
		ASSERT_THAT(AreEqual(1, Run.Count(EEvent::Repeat)));
		ASSERT_THAT(IsNear(4.0, RockTweenTiming::TotalDuration(Timing), 1e-12));
	}

	TEST_METHOD(Repeat_InfiniteNeverCompletes)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 0.5f;
		Timing.Repeat = -1;
		FRun Run(Timing);
		for (int32 I = 0; I < 100; ++I)
		{
			Run.Step(1.0);
		}
		ASSERT_THAT(IsFalse(Run.IsComplete()));
		ASSERT_THAT(AreEqual(0, Run.Count(EEvent::Complete)));
		ASSERT_THAT(AreEqual(200, Run.Count(EEvent::Repeat)));
		ASSERT_THAT(IsTrue(std::isinf(RockTweenTiming::TotalDuration(Timing))));
	}

	// Fields: RepeatDelay

	TEST_METHOD(RepeatDelay_HoldsTheStartValueAndRepeatFiresWhenItEnds)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 1.f;
		Timing.Repeat = 1;
		Timing.RepeatDelay = 0.5f;
		FRun Run(Timing);
		Run.Step(1.25);
		ASSERT_THAT(IsTrue(Run.State.Phase == EPhase::RepeatDelay));
		ASSERT_THAT(IsTrue(Run.Value() == 0.0));
		ASSERT_THAT(AreEqual(0, Run.Count(EEvent::Repeat)));
		Run.Step(0.25);
		ASSERT_THAT(IsTrue(Run.State.Phase == EPhase::Forward));
		ASSERT_THAT(AreEqual(1, Run.Count(EEvent::Repeat)));
	}

	TEST_METHOD(RepeatDelay_NotAppliedAfterTheLastPlay)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 1.f;
		Timing.Repeat = 1;
		Timing.RepeatDelay = 0.5f;
		ASSERT_THAT(IsNear(2.5, RockTweenTiming::TotalDuration(Timing), 1e-12));
		FRun Run(Timing);
		Run.Step(2.5);
		ASSERT_THAT(IsTrue(Run.IsComplete()));
	}

	// Fields: Ease

	TEST_METHOD(Ease_ParamsReachTheEase)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 1.f;
		Timing.Ease = ERockTweenEase::Stepped;
		Timing.EaseParams.Steps = 2;
		FRun Run(Timing);
		Run.Step(0.75);
		ASSERT_THAT(IsNear(0.5, Run.Value(), 1e-12));
	}

	// Events

	TEST_METHOD(Events_FullPlayRaisesTheDesignTraceInOrder)
	{
		// Start, Yoyo, Repeat, Yoyo, Complete (Design section 3, "Events and order").
		const TArray<EEvent> Expected = { EEvent::Start, EEvent::Yoyo, EEvent::Repeat, EEvent::Yoyo, EEvent::Complete };

		FRun Slow(RockTweenTimingTestsPrivate::TraceTiming());
		RockTweenTimingTestsPrivate::RunTo(Slow, 2.0, 0.016);
		ASSERT_THAT(IsTrue(Slow.Events == Expected));

		FRun Single(RockTweenTimingTestsPrivate::TraceTiming());
		Single.Step(2.0);
		ASSERT_THAT(IsTrue(Single.Events == Expected));
	}

	TEST_METHOD(Events_SeeTheStateAfterTheBoundary)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 1.f;
		Timing.bYoyo = true;
		FRockTweenTimingState State;
		EPhase PhaseAtYoyo = EPhase::Delay;
		RockTweenTiming::Advance(Timing, State, 1.0, [&](EEvent Event)
		{
			if (Event == EEvent::Yoyo)
			{
				PhaseAtYoyo = State.Phase;
			}
		});
		ASSERT_THAT(IsTrue(PhaseAtYoyo == EPhase::Backward));
	}

	// Large dt

	TEST_METHOD(LargeDt_SameEventsAndFinalValueAtEveryStepSize)
	{
		const double StepSizes[] = { 0.001, 0.016, 0.033, 0.4, 2.0 };
		TArray<EEvent> Reference;
		for (const double Dt : StepSizes)
		{
			FRun Run(RockTweenTimingTestsPrivate::TraceTiming());
			RockTweenTimingTestsPrivate::RunTo(Run, 2.0, Dt);
			ASSERT_THAT(IsTrue(Run.IsComplete()));
			ASSERT_THAT(IsTrue(Run.Value() == 0.0));
			if (Reference.IsEmpty())
			{
				Reference = Run.Events;
			}
			ASSERT_THAT(IsTrue(Run.Events == Reference));
		}
		ASSERT_THAT(AreEqual(5, Reference.Num()));
	}

	TEST_METHOD(LargeDt_SameValueAtEveryStepSizeMidPlay)
	{
		// Inside the delay, forward leg, hold, backward leg and repeat delay of TraceTiming.
		const double Times[] = { 0.45, 0.55, 0.75, 0.95, 1.2 };
		const double StepSizes[] = { 0.001, 0.016, 0.033, 0.4, 2.0 };
		for (const double Time : Times)
		{
			const FRockTweenTimingState Closed = RockTweenTiming::StateAtTime(RockTweenTimingTestsPrivate::TraceTiming(), Time);
			const double Expected = RockTweenTiming::Evaluate(RockTweenTimingTestsPrivate::TraceTiming(), Closed);
			for (const double Dt : StepSizes)
			{
				FRun Run(RockTweenTimingTestsPrivate::TraceTiming());
				RockTweenTimingTestsPrivate::RunTo(Run, Time, Dt);
				ASSERT_THAT(IsNear(Expected, Run.Value(), 1e-9));
				ASSERT_THAT(AreEqual(Closed.PlaysDone, Run.State.PlaysDone));
			}
		}
	}

	TEST_METHOD(LargeDt_OverflowCarriesAcrossManyBoundaries)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 0.5f;
		Timing.Repeat = 3;
		FRun Run(Timing);
		Run.Step(1.25);
		ASSERT_THAT(AreEqual(2, Run.State.PlaysDone));
		ASSERT_THAT(IsNear(0.25, Run.State.Elapsed, 1e-12));
		ASSERT_THAT(AreEqual(2, Run.Count(EEvent::Repeat)));
	}

	TEST_METHOD(LargeDt_BoundaryCapDropsTheRestOfTheDelta)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 0.001f;
		Timing.Repeat = -1;
		FRun Run(Timing);
		const FRockTweenAdvanceResult Result = Run.Step(0.4);
		ASSERT_THAT(IsTrue(Result.bHitBoundaryCap));
		ASSERT_THAT(AreEqual(RockTweenTiming::MaxBoundariesPerAdvance, Result.Boundaries));
		ASSERT_THAT(IsFalse(Run.IsComplete()));
	}

	TEST_METHOD(LargeDt_NegativeAndNaNDeltaMoveNothing)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 1.f;
		FRun Run(Timing);
		Run.Step(0.5);
		Run.Step(-2.0);
		Run.Step(std::numeric_limits<double>::quiet_NaN());
		ASSERT_THAT(IsTrue(Run.State.Phase == EPhase::Forward));
		ASSERT_THAT(IsNear(0.5, Run.State.Elapsed, 1e-12));
	}

	// Closed form

	TEST_METHOD(ClosedForm_MatchesAdvanceAtEveryStep)
	{
		const FRockTweenTiming Timing = RockTweenTimingTestsPrivate::DyadicTiming();
		FRun Run(Timing);
		for (int32 I = 1; I <= 128; ++I)
		{
			Run.Step(1.0 / 64.0);
			const FRockTweenTimingState Closed = RockTweenTiming::StateAtTime(Timing, I / 64.0);
			ASSERT_THAT(IsTrue(Closed.Phase == Run.State.Phase));
			ASSERT_THAT(AreEqual(Closed.PlaysDone, Run.State.PlaysDone));
			ASSERT_THAT(IsTrue(Closed.Elapsed == Run.State.Elapsed));
		}
	}

	// Instant tweens

	TEST_METHOD(Instant_CompletesOnAdvanceZero)
	{
		for (const float Duration : { 0.f, -1.f })
		{
			FRockTweenTiming Timing;
			Timing.Duration = Duration;
			FRun Run(Timing);
			Run.Step(0.0);
			ASSERT_THAT(IsTrue(Run.IsComplete()));
			ASSERT_THAT(IsTrue((Run.Events == TArray<EEvent>{ EEvent::Start, EEvent::Complete })));
			ASSERT_THAT(IsTrue(Run.Value() == 1.0));
		}
	}

	TEST_METHOD(Instant_YoyoEndsOnTheStartValue)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 0.f;
		Timing.bYoyo = true;
		FRun Run(Timing);
		Run.Step(0.0);
		ASSERT_THAT(IsTrue(Run.IsComplete()));
		ASSERT_THAT(IsTrue((Run.Events == TArray<EEvent>{ EEvent::Start, EEvent::Yoyo, EEvent::Complete })));
		ASSERT_THAT(IsTrue(Run.Value() == 0.0));
	}

	TEST_METHOD(Instant_FiniteRepeatCompletesWithoutTime)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 0.f;
		Timing.Repeat = 100;
		FRun Run(Timing);
		const FRockTweenAdvanceResult Result = Run.Step(0.0);
		ASSERT_THAT(IsTrue(Run.IsComplete()));
		ASSERT_THAT(IsFalse(Result.bHitBoundaryCap));
		ASSERT_THAT(AreEqual(100, Run.Count(EEvent::Repeat)));
	}

	TEST_METHOD(Instant_DelayStillTakesTime)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 0.f;
		Timing.Delay = 0.5f;
		FRun Run(Timing);
		Run.Step(0.4);
		ASSERT_THAT(IsFalse(Run.IsComplete()));
		Run.Step(0.1);
		ASSERT_THAT(IsTrue(Run.IsComplete()));
	}

	// Rejected timings

	TEST_METHOD(ZeroLengthInfinitePlay_IsRejected)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 0.f;
		Timing.Repeat = -1;
		FString Error;
		ASSERT_THAT(IsTrue(RockTweenTiming::IsZeroLengthLoop(Timing)));
		ASSERT_THAT(IsFalse(RockTweenTiming::Validate(Timing, &Error)));
		ASSERT_THAT(IsFalse(Error.IsEmpty()));

		// Advance must not hang on it either.
		FRun Run(Timing);
		const FRockTweenAdvanceResult Result = Run.Step(1.0);
		ASSERT_THAT(IsTrue(Result.bHitBoundaryCap));
		ASSERT_THAT(AreEqual(0, Run.Events.Num()));
	}

	TEST_METHOD(ZeroLengthInfinitePlay_AnyTimeInThePlayMakesItValid)
	{
		FRockTweenTiming Timing;
		Timing.Duration = 0.f;
		Timing.Repeat = -1;
		Timing.RepeatDelay = 0.1f;
		ASSERT_THAT(IsTrue(RockTweenTiming::Validate(Timing)));
		Timing.RepeatDelay = 0.f;
		Timing.Hold = 0.1f;
		ASSERT_THAT(IsTrue(RockTweenTiming::Validate(Timing)));
	}

	TEST_METHOD(Validate_RejectsBadRepeatAndNonFiniteTimes)
	{
		FRockTweenTiming Timing;
		ASSERT_THAT(IsTrue(RockTweenTiming::Validate(Timing)));
		Timing.Repeat = -2;
		ASSERT_THAT(IsFalse(RockTweenTiming::Validate(Timing)));
		Timing.Repeat = 0;
		Timing.Delay = std::numeric_limits<float>::infinity();
		ASSERT_THAT(IsFalse(RockTweenTiming::Validate(Timing)));
		Timing.Delay = 0.f;
		Timing.Duration = std::numeric_limits<float>::quiet_NaN();
		ASSERT_THAT(IsFalse(RockTweenTiming::Validate(Timing)));
	}

	// Completion

	TEST_METHOD(Complete_AdvanceAfterCompleteDoesNothing)
	{
		FRun Run(FRockTweenTiming{});
		Run.Step(1.0);
		const int32 EventCount = Run.Events.Num();
		const FRockTweenAdvanceResult Result = Run.Step(5.0);
		ASSERT_THAT(IsTrue(Result.bCompleted));
		ASSERT_THAT(AreEqual(EventCount, Run.Events.Num()));
	}

	TEST_METHOD(Complete_TotalDurationIsTheExactEnd)
	{
		const FRockTweenTiming Timing = RockTweenTimingTestsPrivate::DyadicTiming();
		const double Total = RockTweenTiming::TotalDuration(Timing);
		FRun Short(Timing);
		Short.Step(Total - 1.0 / 1024.0);
		ASSERT_THAT(IsFalse(Short.IsComplete()));
		FRun Exact(Timing);
		Exact.Step(Total);
		ASSERT_THAT(IsTrue(Exact.IsComplete()));
	}
};
#endif // WITH_DEV_AUTOMATION_TESTS
