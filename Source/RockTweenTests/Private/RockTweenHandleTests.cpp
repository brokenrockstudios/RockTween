// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "RockTweenHandle.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "CQTest.h"
#include "Containers/Set.h"

TEST_CLASS(RockTweenHandleTests, "BRS.RockTween.Handle")
{
	TEST_METHOD(Default_IsInvalid)
	{
		const FRockTweenHandle Handle;
		ASSERT_THAT(IsFalse(Handle.IsValid()));
		ASSERT_THAT(IsTrue(Handle.GetSerial() == 0));
	}

	TEST_METHOD(Pack_RoundTripsIndexAndSerial)
	{
		const FRockTweenHandle Handle = FRockTweenHandle::Pack(1234, 98765);
		ASSERT_THAT(IsTrue(Handle.IsValid()));
		ASSERT_THAT(IsTrue(Handle.GetIndex() == 1234));
		ASSERT_THAT(IsTrue(Handle.GetSerial() == 98765));
	}

	TEST_METHOD(Pack_Extremes_DoNotBleedIntoTheOtherField)
	{
		const FRockTweenHandle Handle = FRockTweenHandle::Pack(FRockTweenHandle::MaxIndex, FRockTweenHandle::MaxSerial);
		ASSERT_THAT(IsTrue(Handle.IsValid()));
		ASSERT_THAT(IsTrue(Handle.GetIndex() == FRockTweenHandle::MaxIndex));
		ASSERT_THAT(IsTrue(Handle.GetSerial() == FRockTweenHandle::MaxSerial));

		const FRockTweenHandle LowIndex = FRockTweenHandle::Pack(0, FRockTweenHandle::MaxSerial);
		ASSERT_THAT(IsTrue(LowIndex.GetIndex() == 0));
		ASSERT_THAT(IsTrue(LowIndex.GetSerial() == FRockTweenHandle::MaxSerial));

		const FRockTweenHandle LowSerial = FRockTweenHandle::Pack(FRockTweenHandle::MaxIndex, 1);
		ASSERT_THAT(IsTrue(LowSerial.GetIndex() == FRockTweenHandle::MaxIndex));
		ASSERT_THAT(IsTrue(LowSerial.GetSerial() == 1));
	}

	TEST_METHOD(Pack_IndexZero_IsValidWhenTheSerialIsSet)
	{
		const FRockTweenHandle Handle = FRockTweenHandle::Pack(0, 1);
		ASSERT_THAT(IsTrue(Handle.IsValid()));
		ASSERT_THAT(IsTrue(Handle.GetIndex() == 0));
	}

	TEST_METHOD(Pack_SerialZero_IsInvalid)
	{
		ASSERT_THAT(IsFalse(FRockTweenHandle::Pack(5, 0).IsValid()));
	}

	TEST_METHOD(Pack_OutOfRange_IsInvalid)
	{
		ASSERT_THAT(IsFalse(FRockTweenHandle::Pack(FRockTweenHandle::MaxIndex + 1, 1).IsValid()));
		ASSERT_THAT(IsFalse(FRockTweenHandle::Pack(0, FRockTweenHandle::MaxSerial + 1).IsValid()));
	}

	TEST_METHOD(Invalidate_ClearsTheHandle)
	{
		FRockTweenHandle Handle = FRockTweenHandle::Pack(3, 4);
		Handle.Invalidate();
		ASSERT_THAT(IsFalse(Handle.IsValid()));
		ASSERT_THAT(IsTrue(Handle == FRockTweenHandle()));
	}

	TEST_METHOD(Equality_ComparesIndexAndSerial)
	{
		const FRockTweenHandle A = FRockTweenHandle::Pack(7, 9);
		ASSERT_THAT(IsTrue(A == FRockTweenHandle::Pack(7, 9)));
		ASSERT_THAT(IsTrue(A != FRockTweenHandle::Pack(8, 9)));
		ASSERT_THAT(IsTrue(A != FRockTweenHandle::Pack(7, 10)));
	}

	TEST_METHOD(Hash_EqualHandlesMatch_AndSetKeysWork)
	{
		const FRockTweenHandle A = FRockTweenHandle::Pack(7, 9);
		ASSERT_THAT(IsTrue(GetTypeHash(A) == GetTypeHash(FRockTweenHandle::Pack(7, 9))));

		TSet<FRockTweenHandle> Set;
		Set.Add(A);
		Set.Add(FRockTweenHandle::Pack(7, 9));
		Set.Add(FRockTweenHandle::Pack(7, 10));
		ASSERT_THAT(AreEqual(Set.Num(), 2));
	}

	TEST_METHOD(Make_SameIndex_GivesDistinctHandles)
	{
		const FRockTweenHandle A = FRockTweenHandle::Make(5);
		const FRockTweenHandle B = FRockTweenHandle::Make(5);
		ASSERT_THAT(IsTrue(A.IsValid() && B.IsValid()));
		ASSERT_THAT(IsTrue(A.GetIndex() == 5));
		ASSERT_THAT(IsTrue(B.GetIndex() == 5));
		ASSERT_THAT(IsTrue(A != B));
		ASSERT_THAT(IsTrue(B.GetSerial() == A.GetSerial() + 1));
	}

	TEST_METHOD(Make_IndexOutOfRange_IsInvalid)
	{
		ASSERT_THAT(IsFalse(FRockTweenHandle::Make(FRockTweenHandle::MaxIndex + 1).IsValid()));
	}

	TEST_METHOD(NextSerial_CountsUpFromZero)
	{
		bool bWrapped = true;
		ASSERT_THAT(IsTrue(FRockTweenHandle::NextSerial(0, bWrapped) == 1));
		ASSERT_THAT(IsFalse(bWrapped));
		ASSERT_THAT(IsTrue(FRockTweenHandle::NextSerial(41, bWrapped) == 42));
		ASSERT_THAT(IsFalse(bWrapped));
	}

	TEST_METHOD(NextSerial_LastBeforeTheMax_DoesNotWrap)
	{
		bool bWrapped = true;
		ASSERT_THAT(IsTrue(FRockTweenHandle::NextSerial(FRockTweenHandle::MaxSerial - 1, bWrapped) == FRockTweenHandle::MaxSerial));
		ASSERT_THAT(IsFalse(bWrapped));
	}

	TEST_METHOD(NextSerial_AtTheMax_WrapsToOneNeverZero)
	{
		bool bWrapped = false;
		const uint64 Next = FRockTweenHandle::NextSerial(FRockTweenHandle::MaxSerial, bWrapped);
		ASSERT_THAT(IsTrue(bWrapped));
		ASSERT_THAT(IsTrue(Next == 1));
		ASSERT_THAT(IsTrue(FRockTweenHandle::Pack(0, Next).IsValid()));
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
