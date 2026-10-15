// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RockTweenMath.h"

#include "RockTweenTiming.generated.h"

/**
 * The timing of one tween: delay, legs, hold, yoyo and repeats. Pure data; the rules are in Docs/RockTween/Design.md section 3.
 * Clock, tick group and time scale are added with the runner and subsystem (T-266, T-267); Advance takes an already scaled delta.
 */
USTRUCT(BlueprintType)
struct ROCKTWEEN_API FRockTweenTiming
{
	GENERATED_BODY()

	/** Seconds of one leg. Zero or less means instant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	float Duration = 0.25f;

	/** Seconds before the first leg, once per play (not replayed on repeat). Negative counts as 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	float Delay = 0.f;

	/** Seconds spent at the end value after every forward leg: before the yoyo turn, a non-yoyo repeat, and completion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	float Hold = 0.f;

	/** After the forward leg (and Hold), a backward leg of the same duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	bool bYoyo = false;

	/** Extra plays: 0 plays once, N plays N + 1 times, -1 is infinite. A forward plus backward leg is one play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	int32 Repeat = 0;

	/** Seconds before each repeat, holding the start value of the next play. Not applied after the last play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	float RepeatDelay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	ERockTweenEase Ease = ERockTweenEase::OutQuad;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
	FRockTweenEaseParams EaseParams;
};

/** The boundary events a tween can raise, in the order a play raises them (Design section 3, "Events and order"). */
enum class ERockTweenTimingEvent : uint8
{
	/** The first leg begins (after Delay). Once per play. */
	Start,
	/** The backward leg begins (after Hold). */
	Yoyo,
	/** A repeat begins (after RepeatDelay). */
	Repeat,
	/** The tween ended naturally. */
	Complete,
};

enum class ERockTweenPhase : uint8
{
	Delay,
	Forward,
	Hold,
	Backward,
	RepeatDelay,
	Complete,
};

/** Where a tween is in its timeline: the current phase, seconds into it, and how many plays are finished (the index of the current play). */
struct FRockTweenTimingState
{
	ERockTweenPhase Phase = ERockTweenPhase::Delay;
	double Elapsed = 0.0;
	int32 PlaysDone = 0;
};

struct FRockTweenAdvanceResult
{
	bool bCompleted = false;
	/** The boundary cap was hit (or the timing is an endless zero-length loop): the rest of the delta was dropped. */
	bool bHitBoundaryCap = false;
	/** Time-consuming phase boundaries crossed by this call. */
	int32 Boundaries = 0;
};

/** Pure timing functions: no UObject state, no world. */
namespace RockTweenTiming
{
	/** Boundaries one Advance call may cross before it drops the remaining time (Design section 3, "Large dt"). */
	constexpr int32 MaxBoundariesPerAdvance = 256;

	/** Receives each boundary event in time order. It only records; the runner dispatches after the update. */
	using FEventSink = TFunctionRef<void(ERockTweenTimingEvent)>;

	/** True for Repeat == -1 with a play (legs, Hold and RepeatDelay together) of zero length: it would loop forever in one tick. */
	ROCKTWEEN_API bool IsZeroLengthLoop(const FRockTweenTiming& Timing);

	/** False, with a message in OutError, for a timing a Play must reject: a zero-length loop, Repeat below -1, or a non-finite time. */
	ROCKTWEEN_API bool Validate(const FRockTweenTiming& Timing, FString* OutError = nullptr);

	/** Seconds from Play to Complete. Positive infinity when Repeat is -1. */
	ROCKTWEEN_API double TotalDuration(const FRockTweenTiming& Timing);

	/**
	 * Moves State forward by Dt seconds (negative and NaN count as 0). Time carries across every boundary crossed, and each one raises
	 * its event in order, so any step size gives the same events and the same state. Advance(0) crosses the zero-length phases at the
	 * front, which is how an instant tween completes inside Play. Reaching a boundary exactly crosses it.
	 */
	ROCKTWEEN_API FRockTweenAdvanceResult Advance(const FRockTweenTiming& Timing, FRockTweenTimingState& State, double Dt, FEventSink OnEvent);

	/** The eased progress for State: 0 is the start value, 1 the end value. Backward legs play Ease(1 - p); a completed yoyo ends on 0. */
	ROCKTWEEN_API double Evaluate(const FRockTweenTiming& Timing, const FRockTweenTimingState& State);

	/** Closed form of Advance from a fresh state: the state after TimeSincePlay seconds, without events. */
	ROCKTWEEN_API FRockTweenTimingState StateAtTime(const FRockTweenTiming& Timing, double TimeSincePlay);
}
