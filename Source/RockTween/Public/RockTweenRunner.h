// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "RockTweenHandle.h"
#include "RockTweenTiming.h"
#include "RockTweenValue.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRockTween, Log, All);

/** What a tween writes to. Only Value exists until the channel tasks (T-268, T-269, T-278) add the rest. */
enum class ERockTweenChannel : uint8
{
	/** No target: the runner keeps the value, read it with GetValue/GetProgress. */
	Value,
};

/** The four callbacks a tween can have, invoked after the update pass in the order the events were raised (Design section 3). */
struct ROCKTWEEN_API FRockTweenCallbacks
{
	using FFn = TFunction<void(FRockTweenHandle)>;

	FFn OnStart;
	FFn OnYoyo;
	FFn OnRepeat;
	FFn OnComplete;

	bool IsSet() const { return OnStart || OnYoyo || OnRepeat || OnComplete; }
};

/** Everything Play needs. Filled by value; the runner copies what it keeps. */
struct ROCKTWEEN_API FRockTweenPlayDesc
{
	/** Required. Held weakly; when it dies the tween is removed silently. */
	TWeakObjectPtr<UObject> Owner;

	/** Optional typed target, held weakly. When it dies the tween is removed silently. */
	TWeakObjectPtr<UObject> Target;
	bool bHasTarget = false;

	ERockTweenChannel Channel = ERockTweenChannel::Value;
	FRockTweenTiming Timing;
	FRockTweenValue From;
	FRockTweenValue To;

	/** Multiplies the delta this tween receives. Must be at least 0 (negative is clamped to 0 with a warning). */
	float TimeScale = 1.f;

	/** Which update bucket advances this tween (a tick group plus a pause class once the subsystem exists, T-267). */
	uint8 Bucket = 0;

	/** Start in the paused state. */
	bool bStartPaused = false;

	FRockTweenCallbacks Callbacks;

	void SetOwner(const UObject* InOwner) { Owner = const_cast<UObject*>(InOwner); }
	void SetTarget(const UObject* InTarget)
	{
		Target = const_cast<UObject*>(InTarget);
		bHasTarget = InTarget != nullptr;
	}
};

/** Counters for tests and telemetry. */
struct FRockTweenRunnerStats
{
	/** Times a tween's value was recomputed. */
	int64 Applies = 0;
	/** Times the recompute was skipped because the eased progress had not changed (in a delay, hold or pause). */
	int64 SkippedApplies = 0;
	/** Times a tween hit the 256-boundary cap and dropped the rest of its delta. */
	int32 BoundaryCapHits = 0;
	/** Most slots ever allocated at once. */
	int32 HighWaterSlots = 0;
};

namespace RockTweenRunnerFlags
{
	enum : uint8
	{
		Allocated = 1 << 0,
		/** Stopped or owner/target died: stale to every operation, its queued events are dropped. */
		Killed = 1 << 1,
		/** Ended naturally or through Complete: stale to every operation, its queued events are still delivered. */
		Finished = 1 << 2,
		HasCallbacks = 1 << 3,
	};
}

/** Cold side of a tween, indexed by the handle's slot index (about 24 bytes). */
struct FRockTweenSlot
{
	/** The full handle: the serial check. Invalid while the slot is free. */
	FRockTweenHandle Handle;
	/** Position in the bucket's hot array, or INDEX_NONE once detached. */
	int32 HotIndex = INDEX_NONE;
	/** Index into the callback side table, or INDEX_NONE. */
	int32 CallbackIndex = INDEX_NONE;
	uint8 Bucket = 0;
	uint8 Flags = 0;
	/** Reserved for group control (Design section 14). */
	uint16 GroupId = 0;
};

/** Hot side of a tween: what the update loop touches, dense in its bucket and compacted by swap-remove. */
struct FRockTweenHot
{
	// Ordered by alignment to keep padding out of the 192 byte budget (Design section 2).
	FRockTweenTimingState State;
	FRockTweenValue From;
	FRockTweenValue To;
	/** Eased progress at the last apply; NaN until the first one. */
	double LastAlpha = 0.0;
	TWeakObjectPtr<UObject> Owner;
	TWeakObjectPtr<UObject> Target;
	FRockTweenTiming Timing;
	float TimeScale = 1.f;
	int32 SlotIndex = INDEX_NONE;
	ERockTweenChannel Channel = ERockTweenChannel::Value;
	bool bPaused = false;
	bool bHasTarget = false;
};

/**
 * The pool and update loop of RockTween: a plain struct with no world and no tick (Design sections 1 to 4). The subsystem (T-267)
 * owns one and calls Update per bucket; tests call it directly.
 *
 * Rules, in short: handles are validated by serial; Stop and Complete flag a record and the slot is freed after the pass, so the
 * handle is stale at once; Play during an update pass appends to its bucket but is not advanced until the next Update (the pass
 * only walks the entries that existed when it began); events are queued during the pass and
 * dispatched after it, re-looking up each handle so a callback that stops another tween suppresses that tween's queued events;
 * a dead owner or target removes the tween silently and a callback whose owner died does not run. Game thread only.
 */
struct ROCKTWEEN_API FRockTweenRunner
{
	FRockTweenRunner() = default;
	FRockTweenRunner(const FRockTweenRunner&) = delete;
	FRockTweenRunner& operator=(const FRockTweenRunner&) = delete;
	~FRockTweenRunner() = default;

	/**
	 * Starts a tween. Returns an invalid handle, with a warning, for a null owner or a timing Validate rejects. A tween that
	 * completes inside Play (all zero lengths) applies its final value, raises OnStart and OnComplete and returns an invalid handle.
	 */
	FRockTweenHandle Play(FRockTweenPlayDesc&& Desc);

	/** Advances every live tween of the bucket by Dt seconds (times its own time scale), then dispatches the queued events. */
	void Update(int32 Bucket, double Dt);

	bool IsActive(FRockTweenHandle Handle) const;
	bool IsPaused(FRockTweenHandle Handle) const;

	/** Left where it is, no events. */
	bool Stop(FRockTweenHandle Handle);

	/** Jumps to the final value of the whole tween (the end of the current play for Repeat = -1) and raises OnStart if it had not fired, then OnComplete. */
	bool Complete(FRockTweenHandle Handle);

	/** Plays on to the end of the current play, then completes. */
	bool FinishAfterCycle(FRockTweenHandle Handle);

	bool Pause(FRockTweenHandle Handle);
	bool Resume(FRockTweenHandle Handle);

	/** Clamps a negative or NaN scale to 0 with a warning. */
	bool SetTimeScale(FRockTweenHandle Handle, float TimeScale);

	/** 0 to 1 over the whole tween, from the time played. Always 0 for Repeat = -1. */
	bool GetProgress(FRockTweenHandle Handle, double& OutProgress) const;

	/** The current value of a tween (From + (To - From) * eased progress). */
	bool GetValue(FRockTweenHandle Handle, FRockTweenValue& OutValue) const;

	/** Stops every live tween of the owner. A scan; meant for rare use. Returns how many were stopped. */
	int32 StopAllForOwner(const UObject* Owner);

	/** Live tweens (not stopped, not finished). */
	int32 NumActive() const { return NumLive; }

	/** Allocated slots, including killed or finished ones that have not been released yet. */
	int32 NumAllocatedSlots() const { return Slots.Num() - FreeSlots.Num(); }

	/** Slots ever created (the pool size). */
	int32 NumSlots() const { return Slots.Num(); }

	/** Warn once when the pool passes this many slots. 0 turns the warning off (the default). */
	void SetPoolReserve(int32 Reserve) { PoolReserve = Reserve; }

	const FRockTweenRunnerStats& GetStats() const { return Stats; }

	/**
	 * Called for every Value tween right after its value is recomputed in an update pass, so user code runs inside the pass. It may call
	 * Play, Stop and Complete (the setter channel of T-269 generalizes this). Per runner, not per tween.
	 */
	TFunction<void(FRockTweenHandle)> OnValueApplied;

	/** Callbacks of live and just-finished tweens. For tests: whether the side table entry was freed with the slot. */
	int32 NumCallbackEntriesInUse() const { return CallbackEntries.Num() - FreeCallbackEntries.Num(); }

private:
	struct FQueuedEvent
	{
		FRockTweenHandle Handle;
		ERockTweenTimingEvent Event;
	};

	struct FCallbackEntry
	{
		FRockTweenCallbacks Callbacks;
		/** Copy of the owner: the hot entry is gone by the time a finished tween's events are delivered. */
		TWeakObjectPtr<UObject> Owner;
	};

	const FRockTweenSlot* FindLive(FRockTweenHandle Handle) const;
	FRockTweenSlot* FindLive(FRockTweenHandle Handle) { return const_cast<FRockTweenSlot*>(static_cast<const FRockTweenRunner*>(this)->FindLive(Handle)); }
	const FRockTweenSlot* FindForDispatch(FRockTweenHandle Handle) const;

	const FRockTweenHot& HotOf(const FRockTweenSlot& Slot) const { return Buckets[Slot.Bucket][Slot.HotIndex]; }
	FRockTweenHot& HotOf(const FRockTweenSlot& Slot) { return Buckets[Slot.Bucket][Slot.HotIndex]; }

	int32 AllocateSlot();
	int32 AddToBucket(FRockTweenHot&& Hot);
	void RemoveFromBucket(FRockTweenSlot& Slot);
	void ReleaseSlot(int32 SlotIndex);
	void Retire(int32 SlotIndex, uint8 Flag);
	void QueueEvent(const FRockTweenSlot& Slot, ERockTweenTimingEvent Event);
	void Apply(FRockTweenHot& Hot);
	void Flush();
	void Dispatch(const FQueuedEvent& Queued);
	bool OwnerAndTargetAlive(const FRockTweenHot& Hot) const;

	TArray<FRockTweenSlot> Slots;
	TArray<int32> FreeSlots;
	TArray<TArray<FRockTweenHot>> Buckets;
	TIndirectArray<FCallbackEntry> CallbackEntries;
	TArray<int32> FreeCallbackEntries;

	/** Killed or finished slots whose hot entry and cold slot are still to be released. */
	TArray<int32> PendingRelease;
	TArray<FQueuedEvent> Events;
	int32 EventHead = 0;

	FRockTweenRunnerStats Stats;
	int32 NumLive = 0;
	int32 PoolReserve = 0;
	bool bUpdating = false;
	bool bFlushing = false;
	bool bWarnedPoolGrowth = false;
};
