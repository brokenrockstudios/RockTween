// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "RockTweenTiming.h"

#include <limits>

namespace RockTweenTimingPrivate
{
	// Negative and NaN count as zero.
	double NonNegative(double Value)
	{
		return Value > 0.0 ? Value : 0.0;
	}

	double LegLength(const FRockTweenTiming& Timing) { return NonNegative(Timing.Duration); }

	// Forward leg, Hold and (with yoyo) the backward leg.
	double PlayLength(const FRockTweenTiming& Timing)
	{
		const double Leg = LegLength(Timing);
		return Leg + NonNegative(Timing.Hold) + (Timing.bYoyo ? Leg : 0.0);
	}

	double PhaseLength(const FRockTweenTiming& Timing, ERockTweenPhase Phase)
	{
		switch (Phase)
		{
		case ERockTweenPhase::Delay: return NonNegative(Timing.Delay);
		case ERockTweenPhase::Forward:
		case ERockTweenPhase::Backward: return LegLength(Timing);
		case ERockTweenPhase::Hold: return NonNegative(Timing.Hold);
		case ERockTweenPhase::RepeatDelay: return NonNegative(Timing.RepeatDelay);
		default: return std::numeric_limits<double>::infinity();
		}
	}

	bool HasMorePlays(const FRockTweenTiming& Timing, const FRockTweenTimingState& State)
	{
		return Timing.Repeat == -1 || State.PlaysDone < Timing.Repeat;
	}

	// The end of a play: another one follows after RepeatDelay, or the tween is complete.
	void FinishPlay(const FRockTweenTiming& Timing, FRockTweenTimingState& State, RockTweenTiming::FEventSink OnEvent, FRockTweenAdvanceResult& Result)
	{
		if (HasMorePlays(Timing, State))
		{
			++State.PlaysDone;
			State.Phase = ERockTweenPhase::RepeatDelay;
		}
		else
		{
			State.Phase = ERockTweenPhase::Complete;
			Result.bCompleted = true;
			OnEvent(ERockTweenTimingEvent::Complete);
		}
	}

	// Leaves the current phase. State is updated before the event is raised.
	void Cross(const FRockTweenTiming& Timing, FRockTweenTimingState& State, RockTweenTiming::FEventSink OnEvent, FRockTweenAdvanceResult& Result)
	{
		switch (State.Phase)
		{
		case ERockTweenPhase::Delay:
			State.Phase = ERockTweenPhase::Forward;
			OnEvent(ERockTweenTimingEvent::Start);
			break;
		case ERockTweenPhase::Forward:
			State.Phase = ERockTweenPhase::Hold;
			break;
		case ERockTweenPhase::Hold:
			if (Timing.bYoyo)
			{
				State.Phase = ERockTweenPhase::Backward;
				OnEvent(ERockTweenTimingEvent::Yoyo);
			}
			else
			{
				FinishPlay(Timing, State, OnEvent, Result);
			}
			break;
		case ERockTweenPhase::Backward:
			FinishPlay(Timing, State, OnEvent, Result);
			break;
		case ERockTweenPhase::RepeatDelay:
			State.Phase = ERockTweenPhase::Forward;
			OnEvent(ERockTweenTimingEvent::Repeat);
			break;
		default:
			break;
		}
	}
}

bool RockTweenTiming::IsZeroLengthLoop(const FRockTweenTiming& Timing)
{
	using namespace RockTweenTimingPrivate;
	return Timing.Repeat == -1 && PlayLength(Timing) + NonNegative(Timing.RepeatDelay) <= 0.0;
}

bool RockTweenTiming::Validate(const FRockTweenTiming& Timing, FString* OutError)
{
	auto Fail = [OutError](const TCHAR* Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	};

	if (!FMath::IsFinite(Timing.Duration) || !FMath::IsFinite(Timing.Delay) || !FMath::IsFinite(Timing.Hold) || !FMath::IsFinite(Timing.RepeatDelay))
	{
		return Fail(TEXT("Duration, Delay, Hold and RepeatDelay must be finite"));
	}
	if (Timing.Repeat < -1)
	{
		return Fail(TEXT("Repeat must be -1 (infinite) or 0 and up"));
	}
	if (IsZeroLengthLoop(Timing))
	{
		return Fail(TEXT("Repeat = -1 with a zero-length play (Duration, Hold and RepeatDelay all 0) would loop forever"));
	}
	return true;
}

double RockTweenTiming::TotalDuration(const FRockTweenTiming& Timing)
{
	using namespace RockTweenTimingPrivate;
	if (Timing.Repeat == -1)
	{
		return std::numeric_limits<double>::infinity();
	}
	const double Play = PlayLength(Timing);
	return NonNegative(Timing.Delay) + Play + static_cast<double>(FMath::Max(Timing.Repeat, 0)) * (NonNegative(Timing.RepeatDelay) + Play);
}

FRockTweenAdvanceResult RockTweenTiming::Advance(const FRockTweenTiming& Timing, FRockTweenTimingState& State, double Dt, FEventSink OnEvent)
{
	using namespace RockTweenTimingPrivate;

	FRockTweenAdvanceResult Result;
	if (State.Phase == ERockTweenPhase::Complete)
	{
		Result.bCompleted = true;
		return Result;
	}
	if (IsZeroLengthLoop(Timing))
	{
		Result.bHitBoundaryCap = true;
		return Result;
	}

	double Remaining = NonNegative(Dt);
	for (;;)
	{
		const double Length = PhaseLength(Timing, State.Phase);
		if (State.Elapsed + Remaining < Length)
		{
			State.Elapsed += Remaining;
			break;
		}

		// Only phases that take time count toward the cap: zero-length ones cost no delta, and a finite Repeat bounds them.
		if (Length > 0.0)
		{
			if (Result.Boundaries >= MaxBoundariesPerAdvance)
			{
				Result.bHitBoundaryCap = true;
				break;
			}
			++Result.Boundaries;
		}

		Remaining -= Length - State.Elapsed;
		if (!(Remaining > 0.0))
		{
			Remaining = 0.0;
		}
		State.Elapsed = 0.0;
		Cross(Timing, State, OnEvent, Result);
		if (State.Phase == ERockTweenPhase::Complete)
		{
			break;
		}
	}
	return Result;
}

double RockTweenTiming::Evaluate(const FRockTweenTiming& Timing, const FRockTweenTimingState& State)
{
	using namespace RockTweenTimingPrivate;

	const double Leg = LegLength(Timing);
	switch (State.Phase)
	{
	case ERockTweenPhase::Forward:
		return Leg > 0.0 ? RockTweenEase::Evaluate(Timing.Ease, State.Elapsed / Leg, Timing.EaseParams) : 1.0;
	case ERockTweenPhase::Hold:
		return 1.0;
	case ERockTweenPhase::Backward:
		return Leg > 0.0 ? RockTweenEase::Evaluate(Timing.Ease, 1.0 - State.Elapsed / Leg, Timing.EaseParams) : 0.0;
	case ERockTweenPhase::Complete:
		return Timing.bYoyo ? 0.0 : 1.0;
	default:
		return 0.0;
	}
}

FRockTweenTimingState RockTweenTiming::StateAtTime(const FRockTweenTiming& Timing, double TimeSincePlay)
{
	using namespace RockTweenTimingPrivate;

	FRockTweenTimingState State;
	double T = NonNegative(TimeSincePlay);

	const double Delay = NonNegative(Timing.Delay);
	if (T < Delay)
	{
		State.Elapsed = T;
		return State;
	}
	T -= Delay;

	const double Play = PlayLength(Timing);
	const double RepeatDelay = NonNegative(Timing.RepeatDelay);
	const double Segment = RepeatDelay + Play;

	auto Complete = [&State, &Timing]()
	{
		State.Phase = ERockTweenPhase::Complete;
		State.Elapsed = 0.0;
		State.PlaysDone = FMath::Max(Timing.Repeat, 0);
		return State;
	};

	// Time inside the play that is current: the first play, or a repeat segment (RepeatDelay, then the play).
	double InPlay = T;
	if (T >= Play)
	{
		if (Timing.Repeat != -1 && (Segment <= 0.0 || T - Play >= static_cast<double>(FMath::Max(Timing.Repeat, 0)) * Segment))
		{
			return Complete();
		}
		if (Segment <= 0.0)
		{
			return Complete();
		}
		const double Past = T - Play;
		const double Index = FMath::FloorToDouble(Past / Segment);
		State.PlaysDone = static_cast<int32>(FMath::Min(Index + 1.0, static_cast<double>(MAX_int32)));
		double InSegment = Past - Index * Segment;
		if (InSegment < 0.0)
		{
			InSegment = 0.0;
		}
		if (InSegment < RepeatDelay)
		{
			State.Phase = ERockTweenPhase::RepeatDelay;
			State.Elapsed = InSegment;
			return State;
		}
		InPlay = InSegment - RepeatDelay;
	}

	const double Leg = LegLength(Timing);
	const double Hold = NonNegative(Timing.Hold);
	if (InPlay < Leg)
	{
		State.Phase = ERockTweenPhase::Forward;
		State.Elapsed = InPlay;
	}
	else if (InPlay < Leg + Hold)
	{
		State.Phase = ERockTweenPhase::Hold;
		State.Elapsed = InPlay - Leg;
	}
	else
	{
		State.Phase = ERockTweenPhase::Backward;
		State.Elapsed = InPlay - Leg - Hold;
	}
	return State;
}
