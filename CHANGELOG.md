# Changelog

Version format `YYMM.DDRR` (year, month, day, revision of that day). Newest first. Keep entries to one line where possible.

## 2610.1502

- `FRockTweenRunner` (pool of cold slots and dense hot buckets, handle ops, deferred events, owner/target checks, `Value` channel), `FRockTweenValue`, `RockTweenTiming::TimeAtState`; `BRS.RockTween.Runner.*` tests (T-266).

## 2610.1501

- Procedural shapes `DampedWiggle`, `Punch`, `Shake` (seeded), `Spring` (closed form) as `ERockTweenEase` entries; `FRockTweenEaseParams::Frequency`/`Decay`/`Seed`; `RockTweenEase::IsShape`/`EvaluateShape`; `BRS.RockTween.Shape.*` tests (T-270).

## 2610.1302

- Pure timing: `FRockTweenTiming`, `FRockTweenTimingState`, `RockTweenTiming::Advance`/`Evaluate`/`StateAtTime`/`Validate`/`TotalDuration`; `BRS.RockTween.Timing.*` tests (T-265).

## 2610.1301

- Pure eases: `ERockTweenEase`, `FRockTweenEaseParams`, `RockTweenEase::Evaluate`, `ToName`/`TryFromName` (with the RockUI `EaseIn`/`EaseOut`/`EaseInOut` aliases as Cubic) and `FromEasingFunc`; `BRS.RockTween.Ease.*` tests (T-264).

## 2610.1101

- Plugin skeleton: `RockTween` runtime module (depends on RockCore only) and `RockTweenTests` (UncookedOnly), and `FRockTweenHandle` with the process-wide serial counter (T-263).
