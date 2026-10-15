# Changelog

Version format `YYMM.DDRR` (year, month, day, revision of that day). Newest first. Keep entries to one line where possible.

## 2610.1302

- Pure timing: `FRockTweenTiming`, `FRockTweenTimingState`, `RockTweenTiming::Advance`/`Evaluate`/`StateAtTime`/`Validate`/`TotalDuration`; `BRS.RockTween.Timing.*` tests (T-265).

## 2610.1301

- Pure eases: `ERockTweenEase`, `FRockTweenEaseParams`, `RockTweenEase::Evaluate`, `ToName`/`TryFromName` (with the RockUI `EaseIn`/`EaseOut`/`EaseInOut` aliases as Cubic) and `FromEasingFunc`; `BRS.RockTween.Ease.*` tests (T-264).

## 2610.1101

- Plugin skeleton: `RockTween` runtime module (depends on RockCore only) and `RockTweenTests` (UncookedOnly), and `FRockTweenHandle` with the process-wide serial counter (T-263).
