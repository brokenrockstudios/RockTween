// Copyright Broken Rock Studios LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RockTweenHandle.generated.h"

/**
 * Names one tween. One packed uint64, like FTimerHandle: bits 0-23 are the slot index, bits 24-63 a serial. Zero is invalid.
 *
 * The serial comes from one process-wide counter, so a handle is unique across worlds and PIE instances, and a stale handle
 * or one used against the wrong world fails the serial check. Whether the tween behind a handle is still alive is a runner
 * question (IsActive), not a handle question. See Docs/RockTween/Design.md section 2.
 */
USTRUCT(BlueprintType)
struct ROCKTWEEN_API FRockTweenHandle
{
	GENERATED_BODY()

	static constexpr uint32 IndexBits = 24;
	static constexpr uint32 SerialBits = 40;
	static constexpr uint32 MaxIndex = (1u << IndexBits) - 1;
	static constexpr uint64 MaxSerial = (uint64(1) << SerialBits) - 1;

	FRockTweenHandle() = default;

	/** Packs an index and a serial. Returns an invalid handle if the index passes MaxIndex or the serial is 0 or passes MaxSerial. */
	static FRockTweenHandle Pack(uint32 Index, uint64 Serial);

	/** Packs the index with the next serial of the process-wide counter. Ensures once if the counter wraps (then restarts at 1). */
	static FRockTweenHandle Make(uint32 Index);

	/** The serial after Last: Last + 1, or 1 with bWrapped set when Last is already MaxSerial. Pure; the counter itself is private to Make. */
	static uint64 NextSerial(uint64 Last, bool& bWrapped);

	bool IsValid() const { return Packed != 0; }
	void Invalidate() { Packed = 0; }

	uint32 GetIndex() const { return static_cast<uint32>(Packed & MaxIndex); }
	uint64 GetSerial() const { return Packed >> IndexBits; }

	friend bool operator==(const FRockTweenHandle& A, const FRockTweenHandle& B) { return A.Packed == B.Packed; }
	friend bool operator!=(const FRockTweenHandle& A, const FRockTweenHandle& B) { return A.Packed != B.Packed; }
	friend uint32 GetTypeHash(const FRockTweenHandle& Handle) { return ::GetTypeHash(Handle.Packed); }

private:
	UPROPERTY(Transient)
	uint64 Packed = 0;
};
