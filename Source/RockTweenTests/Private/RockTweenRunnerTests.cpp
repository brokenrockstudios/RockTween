// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "RockTweenRunner.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "CQTest.h"
#include "Algo/Count.h"
#include "Components/SceneComponent.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace RockTweenRunnerTestsPrivate
{
	using EEvent = ERockTweenTimingEvent;

	// Linear, one second, 0 to 10: dyadic times give exact values.
	FRockTweenPlayDesc MakeDesc(UObject* Owner, float Duration = 1.f)
	{
		FRockTweenPlayDesc Desc;
		Desc.SetOwner(Owner);
		Desc.Timing.Duration = Duration;
		Desc.Timing.Ease = ERockTweenEase::Linear;
		Desc.From = FRockTweenValue(0.0);
		Desc.To = FRockTweenValue(10.0);
		return Desc;
	}

	// Records the order of events with a label per tween.
	struct FLog
	{
		TArray<FString> Lines;

		void AddCallbacks(FRockTweenCallbacks& Callbacks, const TCHAR* Label)
		{
			const FString Name(Label);
			Callbacks.OnStart = [this, Name](FRockTweenHandle) { Lines.Add(Name + TEXT(":Start")); };
			Callbacks.OnYoyo = [this, Name](FRockTweenHandle) { Lines.Add(Name + TEXT(":Yoyo")); };
			Callbacks.OnRepeat = [this, Name](FRockTweenHandle) { Lines.Add(Name + TEXT(":Repeat")); };
			Callbacks.OnComplete = [this, Name](FRockTweenHandle) { Lines.Add(Name + TEXT(":Complete")); };
		}

		FString Joined() const { return FString::Join(Lines, TEXT(" ")); }
		int32 Count(const TCHAR* Line) const { return Algo::Count(Lines, FString(Line)); }
	};

	double ValueOf(const FRockTweenRunner& Runner, FRockTweenHandle Handle)
	{
		FRockTweenValue Value;
		return Runner.GetValue(Handle, Value) ? Value.X : -1.0;
	}
}

TEST_CLASS(RockTweenRunnerTests, "BRS.RockTween.Runner")
{
	FRockTweenRunner Runner;
	TArray<TStrongObjectPtr<UObject>> KeepAlive;

	UObject* MakeOwner()
	{
		UObject* Object = NewObject<USceneComponent>(GetTransientPackage());
		KeepAlive.Emplace(Object);
		return Object;
	}

	// --- Play, Update, GetValue ---

	TEST_METHOD(Play_ReturnsALiveHandleAtTheFromValue)
	{
		const FRockTweenHandle Handle = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner()));
		ASSERT_THAT(IsTrue(Handle.IsValid()));
		ASSERT_THAT(IsTrue(Runner.IsActive(Handle)));
		ASSERT_THAT(AreEqual(1, Runner.NumActive()));
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Handle) == 0.0));
	}

	TEST_METHOD(Update_AdvancesTheValueAndProgress)
	{
		const FRockTweenHandle Handle = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner()));
		Runner.Update(0, 0.5);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Handle) == 5.0));
		double Progress = -1.0;
		ASSERT_THAT(IsTrue(Runner.GetProgress(Handle, Progress)));
		ASSERT_THAT(IsTrue(Progress == 0.5));
	}

	TEST_METHOD(Update_OnlyAdvancesItsOwnBucket)
	{
		UObject* Owner = MakeOwner();
		FRockTweenPlayDesc First = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		FRockTweenPlayDesc Second = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		Second.Bucket = 1;
		const FRockTweenHandle A = Runner.Play(MoveTemp(First));
		const FRockTweenHandle B = Runner.Play(MoveTemp(Second));
		Runner.Update(1, 0.5);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, A) == 0.0));
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, B) == 5.0));
	}

	TEST_METHOD(Update_UnknownBucket_DoesNothing)
	{
		const FRockTweenHandle Handle = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner()));
		Runner.Update(7, 1.0);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Handle) == 0.0));
	}

	TEST_METHOD(Update_TheEndLandsExactlyOnTo_AndFreesTheSlot)
	{
		const FRockTweenHandle Handle = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner()));
		Runner.Update(0, 5.0);
		ASSERT_THAT(IsFalse(Runner.IsActive(Handle)));
		ASSERT_THAT(AreEqual(0, Runner.NumActive()));
		ASSERT_THAT(AreEqual(0, Runner.NumAllocatedSlots()));
	}

	TEST_METHOD(Update_TimeScaleMultipliesTheDelta)
	{
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Desc.TimeScale = 2.f;
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Desc));
		Runner.Update(0, 0.25);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Handle) == 5.0));
		ASSERT_THAT(IsTrue(Runner.SetTimeScale(Handle, 0.f)));
		Runner.Update(0, 0.25);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Handle) == 5.0));
	}

	TEST_METHOD(NegativeTimeScale_ClampsToZeroWithAWarning)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("time scale"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 2);
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Desc.TimeScale = -1.f;
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Desc));
		Runner.Update(0, 0.5);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Handle) == 0.0));
		ASSERT_THAT(IsTrue(Runner.SetTimeScale(Handle, -3.f)));
		Runner.Update(0, 0.5);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Handle) == 0.0));
	}

	TEST_METHOD(Update_SkipsTheApplyWhenTheProgressDidNotChange)
	{
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Desc.Timing.Delay = 1.f;
		Runner.Play(MoveTemp(Desc));
		ASSERT_THAT(IsTrue(Runner.GetStats().Applies == 1));
		Runner.Update(0, 0.25);
		Runner.Update(0, 0.25);
		ASSERT_THAT(IsTrue(Runner.GetStats().Applies == 1));
		ASSERT_THAT(IsTrue(Runner.GetStats().SkippedApplies == 2));
		Runner.Update(0, 1.0);
		ASSERT_THAT(IsTrue(Runner.GetStats().Applies == 2));
	}

	TEST_METHOD(Update_ALargeStepRaisesEveryEventInOrder)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Desc.Timing.bYoyo = true;
		Desc.Timing.Repeat = 1;
		Log.AddCallbacks(Desc.Callbacks, TEXT("A"));
		Runner.Play(MoveTemp(Desc));
		ASSERT_THAT(AreEqual(FString(TEXT("A:Start")), Log.Joined()));
		Runner.Update(0, 10.0);
		ASSERT_THAT(AreEqual(FString(TEXT("A:Start A:Yoyo A:Repeat A:Yoyo A:Complete")), Log.Joined()));
		ASSERT_THAT(AreEqual(0, Runner.NumActive()));
	}

	TEST_METHOD(Update_ASnappedBoundaryCapDropsTheRestWithOneWarning)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("phase boundaries"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner(), 0.001f);
		Desc.Timing.Repeat = -1;
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Desc));
		Runner.Update(0, 0.4);
		ASSERT_THAT(IsTrue(Runner.IsActive(Handle)));
		ASSERT_THAT(AreEqual(1, Runner.GetStats().BoundaryCapHits));
	}

	// --- Handles, pool ---

	TEST_METHOD(Handle_OfAFinishedTween_IsStaleAfterItsSlotIsReused)
	{
		UObject* Owner = MakeOwner();
		const FRockTweenHandle Old = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		Runner.Update(0, 5.0);
		const FRockTweenHandle New = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		ASSERT_THAT(AreEqual(Old.GetIndex(), New.GetIndex()));
		ASSERT_THAT(IsTrue(Old != New));
		ASSERT_THAT(IsFalse(Runner.IsActive(Old)));
		ASSERT_THAT(IsFalse(Runner.Stop(Old)));
		ASSERT_THAT(IsFalse(Runner.Pause(Old)));
		ASSERT_THAT(IsTrue(Runner.IsActive(New)));
	}

	TEST_METHOD(Handle_InvalidOrUnknown_IsRejectedByEveryOperation)
	{
		const FRockTweenHandle Invalid;
		const FRockTweenHandle Unknown = FRockTweenHandle::Pack(999, 5);
		FRockTweenValue Value;
		double Progress = 0.0;
		for (const FRockTweenHandle Handle : {Invalid, Unknown})
		{
			ASSERT_THAT(IsFalse(Runner.IsActive(Handle)));
			ASSERT_THAT(IsFalse(Runner.IsPaused(Handle)));
			ASSERT_THAT(IsFalse(Runner.Stop(Handle)));
			ASSERT_THAT(IsFalse(Runner.Complete(Handle)));
			ASSERT_THAT(IsFalse(Runner.FinishAfterCycle(Handle)));
			ASSERT_THAT(IsFalse(Runner.Pause(Handle)));
			ASSERT_THAT(IsFalse(Runner.Resume(Handle)));
			ASSERT_THAT(IsFalse(Runner.SetTimeScale(Handle, 1.f)));
			ASSERT_THAT(IsFalse(Runner.GetProgress(Handle, Progress)));
			ASSERT_THAT(IsFalse(Runner.GetValue(Handle, Value)));
		}
	}

	TEST_METHOD(Handle_FromAnotherRunner_FailsTheSerialCheck)
	{
		FRockTweenRunner Other;
		UObject* Owner = MakeOwner();
		const FRockTweenHandle Mine = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		const FRockTweenHandle Theirs = Other.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		ASSERT_THAT(AreEqual(Mine.GetIndex(), Theirs.GetIndex()));
		ASSERT_THAT(IsFalse(Runner.IsActive(Theirs)));
		ASSERT_THAT(IsFalse(Other.IsActive(Mine)));
	}

	TEST_METHOD(Slots_AreReusedLastFreedFirst)
	{
		UObject* Owner = MakeOwner();
		const FRockTweenHandle A = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		const FRockTweenHandle B = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		Runner.Stop(A);
		Runner.Stop(B);
		const FRockTweenHandle C = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		ASSERT_THAT(AreEqual(B.GetIndex(), C.GetIndex()));
		ASSERT_THAT(AreEqual(2, Runner.NumSlots()));
	}

	TEST_METHOD(SwapRemove_KeepsTheOthersUpdatingAndAddressable)
	{
		UObject* Owner = MakeOwner();
		FRockTweenPlayDesc Slow = RockTweenRunnerTestsPrivate::MakeDesc(Owner, 4.f);
		const FRockTweenHandle A = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		const FRockTweenHandle B = Runner.Play(MoveTemp(Slow));
		const FRockTweenHandle C = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner, 2.f));
		ASSERT_THAT(IsTrue(Runner.Stop(A)));
		// C moved into A's place in the dense array; both stay reachable through their handles.
		Runner.Update(0, 1.0);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, B) == 2.5));
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, C) == 5.0));
		ASSERT_THAT(IsTrue(Runner.Pause(C)));
		ASSERT_THAT(IsTrue(Runner.IsPaused(C)));
		ASSERT_THAT(IsFalse(Runner.IsPaused(B)));
	}

	TEST_METHOD(Pool_PastTheReserve_WarnsOnce)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("past its reserve"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		UObject* Owner = MakeOwner();
		Runner.SetPoolReserve(2);
		for (int32 Index = 0; Index < 5; ++Index)
		{
			Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		}
		ASSERT_THAT(AreEqual(5, Runner.GetStats().HighWaterSlots));
	}

	// --- Play rejections ---

	TEST_METHOD(Play_WithoutAnOwner_WarnsAndReturnsInvalid)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("valid owner"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		const FRockTweenHandle Handle = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(nullptr));
		ASSERT_THAT(IsFalse(Handle.IsValid()));
		ASSERT_THAT(AreEqual(0, Runner.NumSlots()));
	}

	TEST_METHOD(Play_AZeroLengthInfiniteLoop_WarnsAndReturnsInvalid)
	{
		TestRunner->AddExpectedMessagePlain(TEXT("rejected the timing"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner(), 0.f);
		Desc.Timing.Repeat = -1;
		ASSERT_THAT(IsFalse(Runner.Play(MoveTemp(Desc)).IsValid()));
		ASSERT_THAT(AreEqual(0, Runner.NumSlots()));
	}

	TEST_METHOD(Play_AnInstantTween_CompletesInsidePlayAndPoolsNothing)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner(), 0.f);
		Log.AddCallbacks(Desc.Callbacks, TEXT("A"));
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Desc));
		ASSERT_THAT(IsFalse(Handle.IsValid()));
		ASSERT_THAT(AreEqual(FString(TEXT("A:Start A:Complete")), Log.Joined()));
		ASSERT_THAT(AreEqual(0, Runner.NumActive()));
		ASSERT_THAT(AreEqual(0, Runner.NumAllocatedSlots()));
		ASSERT_THAT(AreEqual(0, Runner.NumCallbackEntriesInUse()));
	}

	TEST_METHOD(Play_WithoutADelay_RaisesOnStartInsidePlay_WithADelay_OnlyLater)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		UObject* Owner = MakeOwner();
		FRockTweenPlayDesc Now = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		Log.AddCallbacks(Now.Callbacks, TEXT("Now"));
		FRockTweenPlayDesc Later = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		Later.Timing.Delay = 0.5f;
		Log.AddCallbacks(Later.Callbacks, TEXT("Later"));
		Runner.Play(MoveTemp(Now));
		Runner.Play(MoveTemp(Later));
		ASSERT_THAT(AreEqual(FString(TEXT("Now:Start")), Log.Joined()));
		Runner.Update(0, 0.5);
		ASSERT_THAT(AreEqual(FString(TEXT("Now:Start Later:Start")), Log.Joined()));
	}

	// --- Stop, Complete, FinishAfterCycle, Pause ---

	TEST_METHOD(Stop_LeavesTheTweenAndRaisesNoEvents)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Log.AddCallbacks(Desc.Callbacks, TEXT("A"));
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Desc));
		Runner.Update(0, 0.5);
		ASSERT_THAT(IsTrue(Runner.Stop(Handle)));
		ASSERT_THAT(IsFalse(Runner.IsActive(Handle)));
		ASSERT_THAT(IsFalse(Runner.Stop(Handle)));
		Runner.Update(0, 1.0);
		ASSERT_THAT(AreEqual(FString(TEXT("A:Start")), Log.Joined()));
		ASSERT_THAT(AreEqual(0, Runner.NumAllocatedSlots()));
		ASSERT_THAT(AreEqual(0, Runner.NumCallbackEntriesInUse()));
	}

	TEST_METHOD(Complete_RaisesOnStartThenOnCompleteWhenStillInTheDelay)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Desc.Timing.Delay = 1.f;
		Log.AddCallbacks(Desc.Callbacks, TEXT("A"));
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Desc));
		ASSERT_THAT(IsTrue(Runner.Complete(Handle)));
		ASSERT_THAT(AreEqual(FString(TEXT("A:Start A:Complete")), Log.Joined()));
		ASSERT_THAT(IsFalse(Runner.IsActive(Handle)));
		ASSERT_THAT(IsFalse(Runner.Complete(Handle)));
	}

	TEST_METHOD(Complete_AfterTheStart_RaisesOnlyOnComplete)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Log.AddCallbacks(Desc.Callbacks, TEXT("A"));
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Desc));
		Runner.Update(0, 0.5);
		Runner.Complete(Handle);
		ASSERT_THAT(AreEqual(FString(TEXT("A:Start A:Complete")), Log.Joined()));
	}

	TEST_METHOD(Complete_ApplyTheFinalValueOfAYoyoTween_AndEndsAnInfiniteOneAtOnce)
	{
		UObject* Owner = MakeOwner();
		RockTweenRunnerTestsPrivate::FLog Log;
		FRockTweenPlayDesc Forever = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		Forever.Timing.Repeat = -1;
		Log.AddCallbacks(Forever.Callbacks, TEXT("F"));
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Forever));
		Runner.Update(0, 0.5);
		ASSERT_THAT(IsTrue(Runner.Complete(Handle)));
		ASSERT_THAT(AreEqual(FString(TEXT("F:Start F:Complete")), Log.Joined()));
		ASSERT_THAT(AreEqual(0, Runner.NumActive()));

		FRockTweenPlayDesc Yoyo = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		Yoyo.Timing.bYoyo = true;
		const FRockTweenHandle YoyoHandle = Runner.Play(MoveTemp(Yoyo));
		const int64 AppliesBefore = Runner.GetStats().Applies;
		Runner.Update(0, 0.5);
		Runner.Complete(YoyoHandle);
		// 0.5 moved the value; Complete moved it again, to the yoyo's end value From.
		ASSERT_THAT(IsTrue(Runner.GetStats().Applies == AppliesBefore + 2));
	}

	TEST_METHOD(FinishAfterCycle_PlaysOnToTheEndOfTheCurrentPlay)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Desc.Timing.Repeat = -1;
		Log.AddCallbacks(Desc.Callbacks, TEXT("A"));
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Desc));
		Runner.Update(0, 1.5);
		ASSERT_THAT(AreEqual(FString(TEXT("A:Start A:Repeat")), Log.Joined()));
		ASSERT_THAT(IsTrue(Runner.FinishAfterCycle(Handle)));
		Runner.Update(0, 0.25);
		ASSERT_THAT(IsTrue(Runner.IsActive(Handle)));
		Runner.Update(0, 0.25);
		ASSERT_THAT(IsFalse(Runner.IsActive(Handle)));
		ASSERT_THAT(AreEqual(FString(TEXT("A:Start A:Repeat A:Complete")), Log.Joined()));
	}

	TEST_METHOD(PauseAndResume_StopAndRestartTheClock)
	{
		const FRockTweenHandle Handle = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner()));
		Runner.Update(0, 0.25);
		ASSERT_THAT(IsTrue(Runner.Pause(Handle)));
		ASSERT_THAT(IsTrue(Runner.IsPaused(Handle)));
		Runner.Update(0, 0.5);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Handle) == 2.5));
		ASSERT_THAT(IsTrue(Runner.Resume(Handle)));
		Runner.Update(0, 0.25);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Handle) == 5.0));
	}

	TEST_METHOD(Play_StartPaused_WaitsForResume)
	{
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Desc.bStartPaused = true;
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Desc));
		Runner.Update(0, 0.5);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Handle) == 0.0));
		Runner.Resume(Handle);
		Runner.Update(0, 0.5);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Handle) == 5.0));
	}

	TEST_METHOD(GetProgress_CountsTheDelayTheLegsTheHoldAndTheRepeatDelay)
	{
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner(), 0.5f);
		Desc.Timing.Delay = 0.5f;
		Desc.Timing.Hold = 0.25f;
		Desc.Timing.bYoyo = true;
		Desc.Timing.Repeat = 1;
		Desc.Timing.RepeatDelay = 0.25f;
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Desc));
		// Total: 0.5 + (0.5 + 0.25 + 0.5) + 0.25 + 1.25 = 3.25
		double Progress = 0.0;
		Runner.Update(0, 0.25);
		Runner.GetProgress(Handle, Progress);
		ASSERT_THAT(IsTrue(Progress == 0.25 / 3.25));
		Runner.Update(0, 1.5);
		Runner.GetProgress(Handle, Progress);
		ASSERT_THAT(IsTrue(Progress == 1.75 / 3.25));
		Runner.Update(0, 0.25);
		Runner.GetProgress(Handle, Progress);
		ASSERT_THAT(IsTrue(Progress == 2.0 / 3.25));
	}

	TEST_METHOD(StopAllForOwner_StopsOnlyThatOwnersTweens)
	{
		UObject* First = MakeOwner();
		UObject* Second = MakeOwner();
		const FRockTweenHandle A = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(First));
		const FRockTweenHandle B = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Second));
		const FRockTweenHandle C = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(First));
		ASSERT_THAT(AreEqual(2, Runner.StopAllForOwner(First)));
		ASSERT_THAT(IsFalse(Runner.IsActive(A)));
		ASSERT_THAT(IsFalse(Runner.IsActive(C)));
		ASSERT_THAT(IsTrue(Runner.IsActive(B)));
		ASSERT_THAT(AreEqual(0, Runner.StopAllForOwner(nullptr)));
	}

	// --- Owner and target death ---

	TEST_METHOD(OwnerDeath_RemovesTheTweenSilently)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		UObject* Owner = MakeOwner();
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		Log.AddCallbacks(Desc.Callbacks, TEXT("A"));
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Desc));
		Owner->MarkAsGarbage();
		Runner.Update(0, 5.0);
		ASSERT_THAT(IsFalse(Runner.IsActive(Handle)));
		ASSERT_THAT(AreEqual(0, Runner.NumActive()));
		ASSERT_THAT(AreEqual(0, Runner.NumAllocatedSlots()));
		ASSERT_THAT(AreEqual(FString(TEXT("A:Start")), Log.Joined()));
	}

	TEST_METHOD(TargetDeath_RemovesTheTweenSilently)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		UObject* Target = MakeOwner();
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Desc.SetTarget(Target);
		Log.AddCallbacks(Desc.Callbacks, TEXT("A"));
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(Desc));
		Runner.Update(0, 0.25);
		ASSERT_THAT(IsTrue(Runner.IsActive(Handle)));
		Target->MarkAsGarbage();
		Runner.Update(0, 5.0);
		ASSERT_THAT(IsFalse(Runner.IsActive(Handle)));
		ASSERT_THAT(AreEqual(FString(TEXT("A:Start")), Log.Joined()));
	}

	TEST_METHOD(OwnerDeathDuringTheUpdate_SuppressesTheQueuedCallbacks)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		UObject* Owner = MakeOwner();
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		Log.AddCallbacks(Desc.Callbacks, TEXT("A"));
		Runner.Play(MoveTemp(Desc));
		Runner.OnValueApplied = [Owner](FRockTweenHandle) { Owner->MarkAsGarbage(); };
		Runner.Update(0, 1.0);
		// The tween finished in that pass; its OnComplete is queued but the owner is gone before dispatch.
		ASSERT_THAT(AreEqual(FString(TEXT("A:Start")), Log.Joined()));
		ASSERT_THAT(AreEqual(0, Runner.NumAllocatedSlots()));
	}

	// --- Re-entrancy ---

	TEST_METHOD(Callback_StoppingAnotherTween_SuppressesItsQueuedEvents)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		UObject* Owner = MakeOwner();
		FRockTweenHandle Victim;

		FRockTweenPlayDesc First = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		First.Callbacks.OnComplete = [this, &Victim, &Log](FRockTweenHandle)
		{
			Log.Lines.Add(TEXT("First:Complete"));
			Runner.Stop(Victim);
		};
		FRockTweenPlayDesc Second = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		Second.Timing.bYoyo = true;
		Log.AddCallbacks(Second.Callbacks, TEXT("Second"));

		Runner.Play(MoveTemp(First));
		Victim = Runner.Play(MoveTemp(Second));
		// Both cross their forward leg in one pass: First completes, Second turns around (OnYoyo queued behind First's OnComplete).
		Runner.Update(0, 1.0);
		ASSERT_THAT(AreEqual(FString(TEXT("Second:Start First:Complete")), Log.Joined()));
		ASSERT_THAT(IsFalse(Runner.IsActive(Victim)));
	}

	TEST_METHOD(Callback_CompletingItsOwnTween_IsANoOpOnceFinished)
	{
		int32 CompleteCalls = 0;
		bool bCompleteReturned = true;
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Desc.Callbacks.OnComplete = [this, &CompleteCalls, &bCompleteReturned](FRockTweenHandle Self)
		{
			++CompleteCalls;
			bCompleteReturned = Runner.Complete(Self) || Runner.Stop(Self) || Runner.IsActive(Self);
		};
		Runner.Play(MoveTemp(Desc));
		Runner.Update(0, 1.0);
		ASSERT_THAT(AreEqual(1, CompleteCalls));
		ASSERT_THAT(IsFalse(bCompleteReturned));
	}

	TEST_METHOD(Callback_PlayingATween_JoinsAtOnce_AndAdvancesOnTheNextUpdate)
	{
		UObject* Owner = MakeOwner();
		FRockTweenHandle Chained;
		FRockTweenPlayDesc First = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		First.Callbacks.OnComplete = [this, Owner, &Chained](FRockTweenHandle)
		{
			Chained = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		};
		Runner.Play(MoveTemp(First));
		Runner.Update(0, 1.0);
		ASSERT_THAT(IsTrue(Chained.IsValid()));
		ASSERT_THAT(IsTrue(Runner.IsActive(Chained)));
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Chained) == 0.0));
		Runner.Update(0, 0.5);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Chained) == 5.0));
	}

	TEST_METHOD(Callback_PlayingAnInstantTween_DeliversItsEventsAfterTheCurrentCallback)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		UObject* Owner = MakeOwner();
		FRockTweenPlayDesc First = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		First.Callbacks.OnComplete = [this, Owner, &Log](FRockTweenHandle)
		{
			Log.Lines.Add(TEXT("First:Complete begin"));
			FRockTweenPlayDesc Instant = RockTweenRunnerTestsPrivate::MakeDesc(Owner, 0.f);
			Log.AddCallbacks(Instant.Callbacks, TEXT("Instant"));
			Runner.Play(MoveTemp(Instant));
			Log.Lines.Add(TEXT("First:Complete end"));
		};
		Runner.Play(MoveTemp(First));
		Runner.Update(0, 1.0);
		ASSERT_THAT(AreEqual(FString(TEXT("First:Complete begin First:Complete end Instant:Start Instant:Complete")), Log.Joined()));
		ASSERT_THAT(AreEqual(0, Runner.NumAllocatedSlots()));
	}

	TEST_METHOD(PlayDuringTheUpdatePass_IsNotAdvancedUntilTheNextUpdate)
	{
		UObject* Owner = MakeOwner();
		FRockTweenHandle Added;
		bool bPlayed = false;
		Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		Runner.OnValueApplied = [this, Owner, &Added, &bPlayed](FRockTweenHandle)
		{
			if (!bPlayed)
			{
				bPlayed = true;
				Added = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
			}
		};
		Runner.Update(0, 0.5);
		ASSERT_THAT(IsTrue(Added.IsValid()));
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Added) == 0.0));
		Runner.Update(0, 0.5);
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, Added) == 5.0));
	}

	TEST_METHOD(StopDuringTheUpdatePass_OfALaterTween_SkipsItAndCompactsAfterwards)
	{
		UObject* Owner = MakeOwner();
		const FRockTweenHandle A = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		const FRockTweenHandle B = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		const FRockTweenHandle C = Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(Owner));
		bool bStopped = false;
		Runner.OnValueApplied = [this, A, B, &bStopped](FRockTweenHandle Handle)
		{
			if (Handle == A && !bStopped)
			{
				bStopped = true;
				Runner.Stop(B);
			}
		};
		Runner.Update(0, 0.5);
		ASSERT_THAT(IsFalse(Runner.IsActive(B)));
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, A) == 5.0));
		ASSERT_THAT(IsTrue(RockTweenRunnerTestsPrivate::ValueOf(Runner, C) == 5.0));
		ASSERT_THAT(AreEqual(2, Runner.NumActive()));
		ASSERT_THAT(AreEqual(2, Runner.NumAllocatedSlots()));
	}

	TEST_METHOD(CompleteDuringTheUpdatePass_OfTheTweenBeingApplied_DoesNotDoubleComplete)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Log.AddCallbacks(Desc.Callbacks, TEXT("A"));
		Runner.Play(MoveTemp(Desc));
		Runner.OnValueApplied = [this](FRockTweenHandle Handle) { Runner.Complete(Handle); };
		Runner.Update(0, 1.0);
		ASSERT_THAT(AreEqual(1, Log.Count(TEXT("A:Complete"))));
		ASSERT_THAT(AreEqual(0, Runner.NumActive()));
	}

	TEST_METHOD(Callback_SeesTheValueOfThisTick)
	{
		double Seen = -1.0;
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Desc.Timing.bYoyo = true;
		Desc.Callbacks.OnYoyo = [this, &Seen](FRockTweenHandle Handle) { Seen = RockTweenRunnerTestsPrivate::ValueOf(Runner, Handle); };
		Runner.Play(MoveTemp(Desc));
		Runner.Update(0, 1.25);
		// Backward leg, 0.25 in: the eased progress is 0.75.
		ASSERT_THAT(IsTrue(Seen == 7.5));
	}

	// --- Side table ---

	TEST_METHOD(Callbacks_AreFreedWithTheSlot_AndTheEntryIsReused)
	{
		UObject* Owner = MakeOwner();
		FRockTweenPlayDesc First = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		First.Callbacks.OnComplete = [](FRockTweenHandle) {};
		const FRockTweenHandle Handle = Runner.Play(MoveTemp(First));
		ASSERT_THAT(AreEqual(1, Runner.NumCallbackEntriesInUse()));
		Runner.Stop(Handle);
		ASSERT_THAT(AreEqual(0, Runner.NumCallbackEntriesInUse()));

		FRockTweenPlayDesc Second = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		Second.Callbacks.OnStart = [](FRockTweenHandle) {};
		Runner.Play(MoveTemp(Second));
		ASSERT_THAT(AreEqual(1, Runner.NumCallbackEntriesInUse()));
	}

	TEST_METHOD(Callbacks_ACapturedSharedPointerIsReleasedWhenTheTweenEnds)
	{
		TSharedPtr<int32> Token = MakeShared<int32>(1);
		FRockTweenPlayDesc Desc = RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner());
		Desc.Callbacks.OnComplete = [Token](FRockTweenHandle) {};
		Runner.Play(MoveTemp(Desc));
		ASSERT_THAT(AreEqual(2, Token.GetSharedReferenceCount()));
		Runner.Update(0, 1.0);
		ASSERT_THAT(AreEqual(1, Token.GetSharedReferenceCount()));
	}

	TEST_METHOD(Tweens_WithoutCallbacks_UseNoSideTableEntry)
	{
		Runner.Play(RockTweenRunnerTestsPrivate::MakeDesc(MakeOwner()));
		ASSERT_THAT(AreEqual(0, Runner.NumCallbackEntriesInUse()));
	}

	TEST_METHOD(TwoEventsOfTwoTweens_AreDeliveredInTheOrderTheyWereRaised)
	{
		RockTweenRunnerTestsPrivate::FLog Log;
		UObject* Owner = MakeOwner();
		FRockTweenPlayDesc A = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		FRockTweenPlayDesc B = RockTweenRunnerTestsPrivate::MakeDesc(Owner);
		Log.AddCallbacks(A.Callbacks, TEXT("A"));
		Log.AddCallbacks(B.Callbacks, TEXT("B"));
		Runner.Play(MoveTemp(A));
		Runner.Play(MoveTemp(B));
		Runner.Update(0, 1.0);
		ASSERT_THAT(AreEqual(FString(TEXT("A:Start B:Start A:Complete B:Complete")), Log.Joined()));
	}
};
#endif // WITH_DEV_AUTOMATION_TESTS
