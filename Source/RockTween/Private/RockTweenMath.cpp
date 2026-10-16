// Copyright Broken Rock Studios LLC. All Rights Reserved.

// Easing equations after Robert Penner (BSD) and Phaser (MIT); written fresh for RockTween.

#include "RockTweenMath.h"

#include "Kismet/KismetMathLibrary.h"

namespace
{
	constexpr double Pi = UE_DOUBLE_PI;
	constexpr double HalfPi = UE_DOUBLE_HALF_PI;
	constexpr double TwoPi = 2.0 * UE_DOUBLE_PI;

	double InPoly(double P, double N) { return FMath::Pow(P, N); }
	double OutPoly(double P, double N) { return 1.0 - FMath::Pow(1.0 - P, N); }
	double InOutPoly(double P, double N)
	{
		return P < 0.5 ? FMath::Pow(2.0, N - 1.0) * FMath::Pow(P, N) : 1.0 - FMath::Pow(-2.0 * P + 2.0, N) / 2.0;
	}

	double OutBounce(double P)
	{
		constexpr double N1 = 7.5625;
		constexpr double D1 = 2.75;
		if (P < 1.0 / D1)
		{
			return N1 * P * P;
		}
		if (P < 2.0 / D1)
		{
			P -= 1.5 / D1;
			return N1 * P * P + 0.75;
		}
		if (P < 2.5 / D1)
		{
			P -= 2.25 / D1;
			return N1 * P * P + 0.9375;
		}
		P -= 2.625 / D1;
		return N1 * P * P + 0.984375;
	}

	double InBack(double P, double S) { return (S + 1.0) * P * P * P - S * P * P; }
	double OutBack(double P, double S)
	{
		const double Q = P - 1.0;
		return 1.0 + (S + 1.0) * Q * Q * Q + S * Q * Q;
	}
	double InOutBack(double P, double S)
	{
		const double C2 = S * 1.525;
		if (P < 0.5)
		{
			const double T = 2.0 * P;
			return T * T * ((C2 + 1.0) * T - C2) / 2.0;
		}
		const double T = 2.0 * P - 2.0;
		return (T * T * ((C2 + 1.0) * T + C2) + 2.0) / 2.0;
	}

	// Elastic with amplitude A (>= 1) and period Per (a fraction of the leg). The phase shift puts the wave on 0 at the leg start.
	double ElasticWave(double T, double A, double Per)
	{
		const double Shift = Per / TwoPi * FMath::Asin(1.0 / A);
		return A * FMath::Sin((T - Shift) * TwoPi / Per);
	}
	double InElastic(double P, double A, double Per) { return -FMath::Pow(2.0, 10.0 * (P - 1.0)) * ElasticWave(P - 1.0, A, Per); }
	double OutElastic(double P, double A, double Per) { return FMath::Pow(2.0, -10.0 * P) * ElasticWave(P, A, Per) + 1.0; }
	double InOutElastic(double P, double A, double Per)
	{
		const double T = 2.0 * P;
		if (T < 1.0)
		{
			return -0.5 * FMath::Pow(2.0, 10.0 * (T - 1.0)) * ElasticWave(T - 1.0, A, Per);
		}
		return 0.5 * FMath::Pow(2.0, -10.0 * (T - 1.0)) * ElasticWave(T - 1.0, A, Per) + 1.0;
	}

	double Smoothstep(double P, double X0, double X1)
	{
		if (X0 > X1)
		{
			Swap(X0, X1);
		}
		if (X0 == X1)
		{
			return P < X0 ? 0.0 : 1.0;
		}
		const double X = FMath::Clamp((P - X0) / (X1 - X0), 0.0, 1.0);
		return X * X * (3.0 - 2.0 * X);
	}

	// Integer hash (murmur3 finalizer) to a lattice value in [-1, 1]. Platform independent, no global RNG state.
	double LatticeValue(int32 Seed, int64 Index)
	{
		uint32 H = static_cast<uint32>(Seed) * 0x9E3779B1u ^ static_cast<uint32>(Index) * 0x85EBCA6Bu ^ static_cast<uint32>(Index >> 32);
		H ^= H >> 16; H *= 0x85EBCA6Bu;
		H ^= H >> 13; H *= 0xC2B2AE35u;
		H ^= H >> 16;
		return static_cast<double>(H) / 2147483647.5 - 1.0;
	}

	// Smooth value noise: one random value per lattice point, smoothstep between neighbours.
	double ValueNoise(int32 Seed, double X)
	{
		const double Floor = FMath::FloorToDouble(X);
		const double Frac = X - Floor;
		const double Blend = Frac * Frac * (3.0 - 2.0 * Frac);
		const int64 Index = static_cast<int64>(Floor);
		return FMath::Lerp(LatticeValue(Seed, Index), LatticeValue(Seed, Index + 1), Blend);
	}

	// Step response of a unit-gain second order system from 0 to 1: natural frequency 2 pi f, damping ratio Zeta.
	double SpringStep(double T, double Frequency, double Zeta)
	{
		const double W = TwoPi * FMath::Max(1e-4, Frequency);
		Zeta = FMath::Max(0.0, Zeta);
		if (FMath::Abs(Zeta - 1.0) < 1e-6)
		{
			return 1.0 - FMath::Exp(-W * T) * (1.0 + W * T);
		}
		if (Zeta < 1.0)
		{
			const double Wd = W * FMath::Sqrt(1.0 - Zeta * Zeta);
			return 1.0 - FMath::Exp(-Zeta * W * T) * (FMath::Cos(Wd * T) + Zeta * W / Wd * FMath::Sin(Wd * T));
		}
		// Overdamped, written with the two decaying roots so a long leg cannot overflow cosh/sinh.
		const double Root = W * FMath::Sqrt(Zeta * Zeta - 1.0);
		const double R1 = -Zeta * W + Root;
		const double R2 = -Zeta * W - Root;
		return 1.0 - (R2 * FMath::Exp(R1 * T) - R1 * FMath::Exp(R2 * T)) / (R2 - R1);
	}
}

bool RockTweenEase::IsShape(ERockTweenEase Ease)
{
	return Ease == ERockTweenEase::DampedWiggle || Ease == ERockTweenEase::Punch || Ease == ERockTweenEase::Shake || Ease == ERockTweenEase::Spring;
}

double RockTweenEase::EvaluateShape(ERockTweenEase Ease, double Seconds, double Duration, const FRockTweenEaseParams& Params)
{
	if (!IsShape(Ease))
	{
		return Evaluate(Ease, Duration > 0.0 ? Seconds / Duration : 1.0, Params);
	}
	// Before the leg (and NaN) every shape is at rest on 0; from the duration on it is at its end value.
	if (!(Seconds >= 0.0))
	{
		return 0.0;
	}
	if (Seconds >= Duration)
	{
		return Ease == ERockTweenEase::Spring ? 1.0 : 0.0;
	}
	const double Frequency = Params.Frequency;
	switch (Ease)
	{
	case ERockTweenEase::DampedWiggle: return FMath::Cos(TwoPi * Frequency * Seconds) * FMath::Exp(-static_cast<double>(Params.Decay) * Seconds);
	case ERockTweenEase::Punch: return FMath::Sin(TwoPi * Frequency * Seconds) * FMath::Exp(-static_cast<double>(Params.Decay) * Seconds);
	case ERockTweenEase::Shake: return ValueNoise(Params.Seed, Seconds * FMath::Max(0.0, Frequency)) * (1.0 - Seconds / Duration);
	default: return SpringStep(Seconds, Frequency, Params.Decay);
	}
}

double RockTweenEase::Evaluate(ERockTweenEase Ease, double Alpha, const FRockTweenEaseParams& Params)
{
	// A shape is not an easing from 0 to 1: it runs on seconds, here a one second leg.
	if (IsShape(Ease))
	{
		return EvaluateShape(Ease, Alpha, 1.0, Params);
	}
	// !(Alpha > 0) also catches NaN. Exact endpoints for every ease: the closed forms below can land 1 ulp off.
	if (!(Alpha > 0.0))
	{
		return 0.0;
	}
	if (Alpha >= 1.0)
	{
		return 1.0;
	}
	const double P = Alpha;

	switch (Ease)
	{
	case ERockTweenEase::Linear: return P;

	case ERockTweenEase::InSine: return 1.0 - FMath::Cos(P * HalfPi);
	case ERockTweenEase::OutSine: return FMath::Sin(P * HalfPi);
	case ERockTweenEase::InOutSine: return -(FMath::Cos(Pi * P) - 1.0) / 2.0;

	case ERockTweenEase::InQuad: return InPoly(P, 2);
	case ERockTweenEase::OutQuad: return OutPoly(P, 2);
	case ERockTweenEase::InOutQuad: return InOutPoly(P, 2);
	case ERockTweenEase::InCubic: return InPoly(P, 3);
	case ERockTweenEase::OutCubic: return OutPoly(P, 3);
	case ERockTweenEase::InOutCubic: return InOutPoly(P, 3);
	case ERockTweenEase::InQuart: return InPoly(P, 4);
	case ERockTweenEase::OutQuart: return OutPoly(P, 4);
	case ERockTweenEase::InOutQuart: return InOutPoly(P, 4);
	case ERockTweenEase::InQuint: return InPoly(P, 5);
	case ERockTweenEase::OutQuint: return OutPoly(P, 5);
	case ERockTweenEase::InOutQuint: return InOutPoly(P, 5);

	case ERockTweenEase::InExpo: return FMath::Pow(2.0, 10.0 * P - 10.0);
	case ERockTweenEase::OutExpo: return 1.0 - FMath::Pow(2.0, -10.0 * P);
	case ERockTweenEase::InOutExpo:
		return P < 0.5 ? FMath::Pow(2.0, 20.0 * P - 10.0) / 2.0 : (2.0 - FMath::Pow(2.0, -20.0 * P + 10.0)) / 2.0;

	case ERockTweenEase::InCirc: return 1.0 - FMath::Sqrt(1.0 - P * P);
	case ERockTweenEase::OutCirc: return FMath::Sqrt(1.0 - (P - 1.0) * (P - 1.0));
	case ERockTweenEase::InOutCirc:
		return P < 0.5 ? (1.0 - FMath::Sqrt(1.0 - 4.0 * P * P)) / 2.0
			: (FMath::Sqrt(1.0 - (-2.0 * P + 2.0) * (-2.0 * P + 2.0)) + 1.0) / 2.0;

	case ERockTweenEase::InBack: return InBack(P, Params.Overshoot);
	case ERockTweenEase::OutBack: return OutBack(P, Params.Overshoot);
	case ERockTweenEase::InOutBack: return InOutBack(P, Params.Overshoot);

	case ERockTweenEase::InElastic:
	case ERockTweenEase::OutElastic:
	case ERockTweenEase::InOutElastic:
	{
		const double A = FMath::Max(1.0, static_cast<double>(Params.Amplitude));
		const double Per = FMath::Max(1e-4, static_cast<double>(Params.Period));
		return Ease == ERockTweenEase::InElastic ? InElastic(P, A, Per)
			: Ease == ERockTweenEase::OutElastic ? OutElastic(P, A, Per) : InOutElastic(P, A, Per);
	}

	case ERockTweenEase::InBounce: return 1.0 - OutBounce(1.0 - P);
	case ERockTweenEase::OutBounce: return OutBounce(P);
	case ERockTweenEase::InOutBounce:
		return P < 0.5 ? (1.0 - OutBounce(1.0 - 2.0 * P)) / 2.0 : (1.0 + OutBounce(2.0 * P - 1.0)) / 2.0;

	case ERockTweenEase::Smoothstep: return Smoothstep(P, Params.SmoothX0, Params.SmoothX1);
	case ERockTweenEase::Stepped:
	{
		const double Steps = FMath::Max(1, Params.Steps);
		return FMath::Floor(P * Steps) / Steps;
	}

	case ERockTweenEase::DampedWiggle:
	case ERockTweenEase::Punch:
	case ERockTweenEase::Shake:
	case ERockTweenEase::Spring:
		break; // handled above
	}
	return P;
}

FString RockTweenEase::ToName(ERockTweenEase Ease)
{
	return StaticEnum<ERockTweenEase>()->GetNameStringByValue(static_cast<int64>(Ease));
}

bool RockTweenEase::TryFromName(FStringView Name, ERockTweenEase& OutEase)
{
	if (Name.Equals(TEXTVIEW("EaseIn"), ESearchCase::CaseSensitive)) { OutEase = ERockTweenEase::InCubic; return true; }
	if (Name.Equals(TEXTVIEW("EaseOut"), ESearchCase::CaseSensitive)) { OutEase = ERockTweenEase::OutCubic; return true; }
	if (Name.Equals(TEXTVIEW("EaseInOut"), ESearchCase::CaseSensitive)) { OutEase = ERockTweenEase::InOutCubic; return true; }

	// Exact, case-sensitive match on the short name (GetValueByNameString also takes the qualified name; FStringView == ignores case).
	const UEnum* Enum = StaticEnum<ERockTweenEase>();
	for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index) // the last entry is the generated _MAX
	{
		if (Name.Equals(Enum->GetNameStringByIndex(Index), ESearchCase::CaseSensitive))
		{
			OutEase = static_cast<ERockTweenEase>(Enum->GetValueByIndex(Index));
			return true;
		}
	}
	return false;
}

ERockTweenEase RockTweenEase::FromEasingFunc(EEasingFunc::Type Func, FRockTweenEaseParams* OutParams)
{
	switch (Func)
	{
	case EEasingFunc::Linear: return ERockTweenEase::Linear;
	case EEasingFunc::Step:
		if (OutParams)
		{
			OutParams->Steps = 2;
		}
		return ERockTweenEase::Stepped;
	case EEasingFunc::SinusoidalIn: return ERockTweenEase::InSine;
	case EEasingFunc::SinusoidalOut: return ERockTweenEase::OutSine;
	case EEasingFunc::SinusoidalInOut: return ERockTweenEase::InOutSine;
	case EEasingFunc::EaseIn: return ERockTweenEase::InQuad;
	case EEasingFunc::EaseOut: return ERockTweenEase::OutQuad;
	case EEasingFunc::EaseInOut: return ERockTweenEase::InOutQuad;
	case EEasingFunc::ExpoIn: return ERockTweenEase::InExpo;
	case EEasingFunc::ExpoOut: return ERockTweenEase::OutExpo;
	case EEasingFunc::ExpoInOut: return ERockTweenEase::InOutExpo;
	case EEasingFunc::CircularIn: return ERockTweenEase::InCirc;
	case EEasingFunc::CircularOut: return ERockTweenEase::OutCirc;
	case EEasingFunc::CircularInOut: return ERockTweenEase::InOutCirc;
	default: return ERockTweenEase::Linear;
	}
}
