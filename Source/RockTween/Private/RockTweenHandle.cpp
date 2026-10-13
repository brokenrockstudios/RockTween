// Copyright Broken Rock Studios LLC. All Rights Reserved.

#include "RockTweenHandle.h"

#include <atomic>

namespace
{
	std::atomic<uint64> LastSerial{0};
}

FRockTweenHandle FRockTweenHandle::Pack(uint32 Index, uint64 Serial)
{
	FRockTweenHandle Handle;
	if (Index <= MaxIndex && Serial != 0 && Serial <= MaxSerial)
	{
		Handle.Packed = (Serial << IndexBits) | Index;
	}
	return Handle;
}

uint64 FRockTweenHandle::NextSerial(uint64 Last, bool& bWrapped)
{
	bWrapped = Last >= MaxSerial;
	return bWrapped ? 1 : Last + 1;
}

FRockTweenHandle FRockTweenHandle::Make(uint32 Index)
{
	uint64 Last = LastSerial.load(std::memory_order_relaxed);
	uint64 Next;
	bool bWrapped;
	do
	{
		Next = NextSerial(Last, bWrapped);
	} while (!LastSerial.compare_exchange_weak(Last, Next, std::memory_order_relaxed));

	ensureMsgf(!bWrapped, TEXT("RockTween handle serial counter wrapped; old handles can now match new tweens."));
	return Pack(Index, Next);
}
