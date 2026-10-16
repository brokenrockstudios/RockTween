// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "RockTweenRunner.h"

#include <limits>

#include "Misc/AssertionMacros.h"

DEFINE_LOG_CATEGORY(LogRockTween);

// Design section 2 budgets: hot at most 192 bytes, cold 48, both together 256. The channel payload (24 bytes) of T-268 is not in the hot entry yet.
static_assert(sizeof(FRockTweenHot) <= 192, "FRockTweenHot must fit the 192 byte budget of Design section 2");
static_assert(sizeof(FRockTweenSlot) <= 48, "FRockTweenSlot must fit the 48 byte budget of Design section 2");
static_assert(sizeof(FRockTweenHot) + sizeof(FRockTweenSlot) <= 256, "Hot plus cold must stay under 256 bytes per tween");

const FRockTweenSlot* FRockTweenRunner::FindLive(FRockTweenHandle Handle) const
{
	if (!Handle.IsValid() || !Slots.IsValidIndex(static_cast<int32>(Handle.GetIndex())))
	{
		return nullptr;
	}
	const FRockTweenSlot& Slot = Slots[Handle.GetIndex()];
	const bool bDead = (Slot.Flags & (RockTweenRunnerFlags::Killed | RockTweenRunnerFlags::Finished)) != 0;
	if (!(Slot.Flags & RockTweenRunnerFlags::Allocated) || bDead || Slot.Handle != Handle || Slot.HotIndex == INDEX_NONE)
	{
		return nullptr;
	}
	return &Slot;
}

const FRockTweenSlot* FRockTweenRunner::FindForDispatch(FRockTweenHandle Handle) const
{
	if (!Handle.IsValid() || !Slots.IsValidIndex(static_cast<int32>(Handle.GetIndex())))
	{
		return nullptr;
	}
	const FRockTweenSlot& Slot = Slots[Handle.GetIndex()];
	if (!(Slot.Flags & RockTweenRunnerFlags::Allocated) || (Slot.Flags & RockTweenRunnerFlags::Killed) || Slot.Handle != Handle)
	{
		return nullptr;
	}
	return &Slot;
}

int32 FRockTweenRunner::AllocateSlot()
{
	int32 Index;
	if (FreeSlots.Num() > 0)
	{
		Index = FreeSlots.Pop(EAllowShrinking::No);
	}
	else
	{
		Index = Slots.AddDefaulted();
		if (PoolReserve > 0 && Slots.Num() > PoolReserve && !bWarnedPoolGrowth)
		{
			bWarnedPoolGrowth = true;
			UE_LOG(LogRockTween, Warning, TEXT("RockTween pool grew past its reserve of %d slots"), PoolReserve);
		}
	}
	Stats.HighWaterSlots = FMath::Max(Stats.HighWaterSlots, NumAllocatedSlots());
	return Index;
}

int32 FRockTweenRunner::AddToBucket(FRockTweenHot&& Hot)
{
	FRockTweenSlot& Slot = Slots[Hot.SlotIndex];
	if (!Buckets.IsValidIndex(Slot.Bucket))
	{
		Buckets.SetNum(Slot.Bucket + 1);
	}
	TArray<FRockTweenHot>& Bucket = Buckets[Slot.Bucket];
	Slot.HotIndex = Bucket.Add(MoveTemp(Hot));
	return Slot.HotIndex;
}

void FRockTweenRunner::RemoveFromBucket(FRockTweenSlot& Slot)
{
	TArray<FRockTweenHot>& Bucket = Buckets[Slot.Bucket];
	const int32 Index = Slot.HotIndex;
	Bucket.RemoveAtSwap(Index, EAllowShrinking::No);
	if (Bucket.IsValidIndex(Index))
	{
		// The last entry moved into the hole.
		Slots[Bucket[Index].SlotIndex].HotIndex = Index;
	}
	Slot.HotIndex = INDEX_NONE;
}

void FRockTweenRunner::ReleaseSlot(int32 SlotIndex)
{
	FRockTweenSlot& Slot = Slots[SlotIndex];
	if (Slot.CallbackIndex != INDEX_NONE)
	{
		FCallbackEntry& Entry = CallbackEntries[Slot.CallbackIndex];
		Entry.Callbacks = FRockTweenCallbacks();
		Entry.Owner.Reset();
		FreeCallbackEntries.Add(Slot.CallbackIndex);
	}
	Slot = FRockTweenSlot();
	FreeSlots.Add(SlotIndex);
}

void FRockTweenRunner::Retire(int32 SlotIndex, uint8 Flag)
{
	FRockTweenSlot& Slot = Slots[SlotIndex];
	if (Slot.Flags & (RockTweenRunnerFlags::Killed | RockTweenRunnerFlags::Finished))
	{
		return;
	}
	Slot.Flags |= Flag;
	--NumLive;
	PendingRelease.Add(SlotIndex);
}

void FRockTweenRunner::QueueEvent(const FRockTweenSlot& Slot, ERockTweenTimingEvent Event)
{
	if (Slot.CallbackIndex != INDEX_NONE)
	{
		Events.Add({Slot.Handle, Event});
	}
}

bool FRockTweenRunner::OwnerAndTargetAlive(const FRockTweenHot& Hot) const
{
	return Hot.Owner.IsValid() && (!Hot.bHasTarget || Hot.Target.IsValid());
}

void FRockTweenRunner::Apply(FRockTweenHot& Hot)
{
	const double Alpha = RockTweenTiming::Evaluate(Hot.Timing, Hot.State);
	if (Alpha == Hot.LastAlpha)
	{
		// Nothing moved since the last apply: a delay, a hold or a pause. A NaN LastAlpha (nothing applied yet) never matches.
		++Stats.SkippedApplies;
		return;
	}
	Hot.LastAlpha = Alpha;
	++Stats.Applies;

	// Hot may be gone once user code has run (it can Play, which grows the bucket), so it is not touched after this.
	switch (Hot.Channel)
	{
	case ERockTweenChannel::Value:
		if (OnValueApplied)
		{
			OnValueApplied(Slots[Hot.SlotIndex].Handle);
		}
		break;
	}
}

FRockTweenHandle FRockTweenRunner::Play(FRockTweenPlayDesc&& Desc)
{
	check(IsInGameThread());

	if (!Desc.Owner.IsValid())
	{
		UE_LOG(LogRockTween, Warning, TEXT("RockTween Play needs a valid owner object; nothing was played"));
		return FRockTweenHandle();
	}
	FString Error;
	if (!RockTweenTiming::Validate(Desc.Timing, &Error))
	{
		UE_LOG(LogRockTween, Warning, TEXT("RockTween Play rejected the timing: %s"), *Error);
		return FRockTweenHandle();
	}
	float TimeScale = Desc.TimeScale;
	if (!(TimeScale >= 0.f))
	{
		UE_LOG(LogRockTween, Warning, TEXT("RockTween time scale %f is below 0; using 0"), TimeScale);
		TimeScale = 0.f;
	}

	const int32 SlotIndex = AllocateSlot();
	const FRockTweenHandle Handle = FRockTweenHandle::Make(SlotIndex);
	check(Handle.IsValid());

	FRockTweenSlot& Slot = Slots[SlotIndex];
	Slot = FRockTweenSlot();
	Slot.Handle = Handle;
	Slot.Bucket = Desc.Bucket;
	Slot.Flags = RockTweenRunnerFlags::Allocated;
	if (Desc.Callbacks.IsSet())
	{
		int32 EntryIndex;
		if (FreeCallbackEntries.Num() > 0)
		{
			EntryIndex = FreeCallbackEntries.Pop(EAllowShrinking::No);
		}
		else
		{
			EntryIndex = CallbackEntries.Add(new FCallbackEntry());
		}
		CallbackEntries[EntryIndex].Callbacks = MoveTemp(Desc.Callbacks);
		CallbackEntries[EntryIndex].Owner = Desc.Owner;
		Slot.CallbackIndex = EntryIndex;
		Slot.Flags |= RockTweenRunnerFlags::HasCallbacks;
	}
	++NumLive;

	FRockTweenHot Hot;
	Hot.Timing = Desc.Timing;
	Hot.From = Desc.From;
	Hot.To = Desc.To;
	Hot.LastAlpha = std::numeric_limits<double>::quiet_NaN();
	Hot.Owner = Desc.Owner;
	Hot.Target = Desc.Target;
	Hot.bHasTarget = Desc.bHasTarget;
	Hot.TimeScale = TimeScale;
	Hot.SlotIndex = SlotIndex;
	Hot.Channel = Desc.Channel;
	Hot.bPaused = Desc.bStartPaused;

	// Cross the zero-length phases at the front: OnStart when there is no delay, and the whole play for an instant tween.
	const FRockTweenAdvanceResult Result = RockTweenTiming::Advance(Hot.Timing, Hot.State, 0.0, [this, &Slot](ERockTweenTimingEvent Event) { QueueEvent(Slot, Event); });

	const int32 HotIndex = AddToBucket(MoveTemp(Hot));
	const uint8 Bucket = Slots[SlotIndex].Bucket;
	Apply(Buckets[Bucket][HotIndex]);

	if (Result.bCompleted)
	{
		Retire(SlotIndex, RockTweenRunnerFlags::Finished);
	}
	Flush();
	return Result.bCompleted ? FRockTweenHandle() : Handle;
}

void FRockTweenRunner::Update(int32 Bucket, double Dt)
{
	check(IsInGameThread());
	if (bUpdating || !Buckets.IsValidIndex(Bucket))
	{
		return;
	}

	bUpdating = true;
	// Entries added during the pass (by user code in OnValueApplied) sit past Count and first advance on the next Update.
	const int32 Count = Buckets[Bucket].Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FRockTweenHot& Hot = Buckets[Bucket][Index];
		const int32 SlotIndex = Hot.SlotIndex;
		if (Slots[SlotIndex].Flags & (RockTweenRunnerFlags::Killed | RockTweenRunnerFlags::Finished))
		{
			continue;
		}
		if (!OwnerAndTargetAlive(Hot))
		{
			Retire(SlotIndex, RockTweenRunnerFlags::Killed);
			continue;
		}
		if (Hot.bPaused)
		{
			continue;
		}

		const FRockTweenSlot& Slot = Slots[SlotIndex];
		const FRockTweenAdvanceResult Result = RockTweenTiming::Advance(Hot.Timing, Hot.State, Dt * Hot.TimeScale, [this, &Slot](ERockTweenTimingEvent Event) { QueueEvent(Slot, Event); });
		if (Result.bHitBoundaryCap)
		{
			++Stats.BoundaryCapHits;
			UE_LOG(LogRockTween, Warning, TEXT("RockTween tween crossed more than %d phase boundaries in one update; the rest of the time was dropped"), RockTweenTiming::MaxBoundariesPerAdvance);
		}

		Apply(Hot);

		// User code in Apply may have stopped or completed this tween already.
		if (Result.bCompleted)
		{
			Retire(SlotIndex, RockTweenRunnerFlags::Finished);
		}
	}
	bUpdating = false;
	Flush();
}

void FRockTweenRunner::Flush()
{
	if (bUpdating || bFlushing)
	{
		return;
	}
	bFlushing = true;

	int32 Detached = 0;
	for (;;)
	{
		for (; Detached < PendingRelease.Num(); ++Detached)
		{
			FRockTweenSlot& Slot = Slots[PendingRelease[Detached]];
			if (Slot.HotIndex != INDEX_NONE)
			{
				RemoveFromBucket(Slot);
			}
		}
		if (EventHead >= Events.Num())
		{
			break;
		}
		const FQueuedEvent Queued = Events[EventHead++];
		Dispatch(Queued);
	}

	for (const int32 SlotIndex : PendingRelease)
	{
		ReleaseSlot(SlotIndex);
	}
	PendingRelease.Reset();
	Events.Reset();
	EventHead = 0;
	bFlushing = false;
}

void FRockTweenRunner::Dispatch(const FQueuedEvent& Queued)
{
	// Looked up again for every event: an earlier callback may have stopped this tween.
	const FRockTweenSlot* Slot = FindForDispatch(Queued.Handle);
	if (!Slot || Slot->CallbackIndex == INDEX_NONE)
	{
		return;
	}
	FCallbackEntry& Entry = CallbackEntries[Slot->CallbackIndex];
	if (!Entry.Owner.IsValid())
	{
		return;
	}

	const FRockTweenCallbacks::FFn* Fn = nullptr;
	switch (Queued.Event)
	{
	case ERockTweenTimingEvent::Start: Fn = &Entry.Callbacks.OnStart; break;
	case ERockTweenTimingEvent::Yoyo: Fn = &Entry.Callbacks.OnYoyo; break;
	case ERockTweenTimingEvent::Repeat: Fn = &Entry.Callbacks.OnRepeat; break;
	case ERockTweenTimingEvent::Complete: Fn = &Entry.Callbacks.OnComplete; break;
	}
	if (Fn && *Fn)
	{
		// Entries are heap allocated, so the function stays put even if the callback plays more tweens.
		(*Fn)(Queued.Handle);
	}
}

bool FRockTweenRunner::IsActive(FRockTweenHandle Handle) const
{
	return FindLive(Handle) != nullptr;
}

bool FRockTweenRunner::IsPaused(FRockTweenHandle Handle) const
{
	const FRockTweenSlot* Slot = FindLive(Handle);
	return Slot && HotOf(*Slot).bPaused;
}

bool FRockTweenRunner::Stop(FRockTweenHandle Handle)
{
	check(IsInGameThread());
	const FRockTweenSlot* Slot = FindLive(Handle);
	if (!Slot)
	{
		return false;
	}
	Retire(static_cast<int32>(Handle.GetIndex()), RockTweenRunnerFlags::Killed);
	Flush();
	return true;
}

bool FRockTweenRunner::Complete(FRockTweenHandle Handle)
{
	check(IsInGameThread());
	FRockTweenSlot* Slot = FindLive(Handle);
	if (!Slot)
	{
		return false;
	}
	const int32 SlotIndex = static_cast<int32>(Handle.GetIndex());
	FRockTweenHot& Hot = HotOf(*Slot);
	if (!OwnerAndTargetAlive(Hot))
	{
		Retire(SlotIndex, RockTweenRunnerFlags::Killed);
		Flush();
		return false;
	}

	// A tween that ended naturally in the running pass has raised its OnComplete already; a hook calling Complete on it must not raise a second.
	const bool bAlreadyComplete = Hot.State.Phase == ERockTweenPhase::Complete;
	if (Hot.State.Phase == ERockTweenPhase::Delay)
	{
		QueueEvent(*Slot, ERockTweenTimingEvent::Start);
	}
	Hot.State.Phase = ERockTweenPhase::Complete;
	Hot.State.Elapsed = 0.0;
	if (Hot.Timing.Repeat >= 0)
	{
		Hot.State.PlaysDone = Hot.Timing.Repeat;
	}
	if (!bAlreadyComplete)
	{
		QueueEvent(*Slot, ERockTweenTimingEvent::Complete);
	}
	Retire(SlotIndex, RockTweenRunnerFlags::Finished);
	Apply(Hot);
	Flush();
	return true;
}

bool FRockTweenRunner::FinishAfterCycle(FRockTweenHandle Handle)
{
	check(IsInGameThread());
	const FRockTweenSlot* Slot = FindLive(Handle);
	if (!Slot)
	{
		return false;
	}
	FRockTweenHot& Hot = HotOf(*Slot);
	if (Hot.Timing.Repeat == -1 || Hot.Timing.Repeat > Hot.State.PlaysDone)
	{
		Hot.Timing.Repeat = Hot.State.PlaysDone;
	}
	return true;
}

bool FRockTweenRunner::Pause(FRockTweenHandle Handle)
{
	check(IsInGameThread());
	const FRockTweenSlot* Slot = FindLive(Handle);
	if (!Slot)
	{
		return false;
	}
	HotOf(*Slot).bPaused = true;
	return true;
}

bool FRockTweenRunner::Resume(FRockTweenHandle Handle)
{
	check(IsInGameThread());
	const FRockTweenSlot* Slot = FindLive(Handle);
	if (!Slot)
	{
		return false;
	}
	HotOf(*Slot).bPaused = false;
	return true;
}

bool FRockTweenRunner::SetTimeScale(FRockTweenHandle Handle, float TimeScale)
{
	check(IsInGameThread());
	const FRockTweenSlot* Slot = FindLive(Handle);
	if (!Slot)
	{
		return false;
	}
	if (!(TimeScale >= 0.f))
	{
		UE_LOG(LogRockTween, Warning, TEXT("RockTween time scale %f is below 0; using 0"), TimeScale);
		TimeScale = 0.f;
	}
	HotOf(*Slot).TimeScale = TimeScale;
	return true;
}

bool FRockTweenRunner::GetProgress(FRockTweenHandle Handle, double& OutProgress) const
{
	const FRockTweenSlot* Slot = FindLive(Handle);
	if (!Slot)
	{
		return false;
	}
	const FRockTweenHot& Hot = HotOf(*Slot);
	const double Total = RockTweenTiming::TotalDuration(Hot.Timing);
	if (!(Total > 0.0) || !FMath::IsFinite(Total))
	{
		OutProgress = 0.0;
		return true;
	}
	OutProgress = FMath::Clamp(RockTweenTiming::TimeAtState(Hot.Timing, Hot.State) / Total, 0.0, 1.0);
	return true;
}

bool FRockTweenRunner::GetValue(FRockTweenHandle Handle, FRockTweenValue& OutValue) const
{
	const FRockTweenSlot* Slot = FindLive(Handle);
	if (!Slot)
	{
		return false;
	}
	const FRockTweenHot& Hot = HotOf(*Slot);
	OutValue = FRockTweenValue::Lerp(Hot.From, Hot.To, FMath::IsNaN(Hot.LastAlpha) ? 0.0 : Hot.LastAlpha);
	return true;
}

int32 FRockTweenRunner::StopAllForOwner(const UObject* Owner)
{
	check(IsInGameThread());
	if (!Owner)
	{
		return 0;
	}
	TArray<int32, TInlineAllocator<16>> Matches;
	for (const TArray<FRockTweenHot>& Bucket : Buckets)
	{
		for (const FRockTweenHot& Hot : Bucket)
		{
			const FRockTweenSlot& Slot = Slots[Hot.SlotIndex];
			if (Hot.Owner.Get() == Owner && !(Slot.Flags & (RockTweenRunnerFlags::Killed | RockTweenRunnerFlags::Finished)))
			{
				Matches.Add(Hot.SlotIndex);
			}
		}
	}
	for (const int32 SlotIndex : Matches)
	{
		Retire(SlotIndex, RockTweenRunnerFlags::Killed);
	}
	Flush();
	return Matches.Num();
}
