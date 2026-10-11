#include "Core/Maths/Maths.h"

#include <cmath>

using namespace Opaax;

float Maths::Atan2(float Y, float X)
{
    // Minimax approximation of atan2f (max relative error 7.15e-7): atan2f can return NaN
    // on valid input, and this is about 2x faster.

    const float lAbsX = Abs(X);
    const float lAbsY = Abs(Y);
    const bool  lYAbsBigger = (lAbsY > lAbsX);
    
    float lT0 = lYAbsBigger ? lAbsY : lAbsX; // Max(absY, absX)
    float lT1 = lYAbsBigger ? lAbsX : lAbsY; // Min(absX, absY)
	
    if (lT0 == FZERO)
    {
        return FZERO;
    }

    float lT3 = lT1 / lT0;
    float lT4 = lT3 * lT3;

    static const float lConsts[7] = {
        +7.2128853633444123e-03f,
        -3.5059680836411644e-02f,
        +8.1675882859940430e-02f,
        -1.3374657325451267e-01f,
        +1.9856563505717162e-01f,
        -3.3324998579202170e-01f,
        +1.0f
    };

    lT0 = lConsts[0];
    lT0 = lT0 * lT4 + lConsts[1];
    lT0 = lT0 * lT4 + lConsts[2];
    lT0 = lT0 * lT4 + lConsts[3];
    lT0 = lT0 * lT4 + lConsts[4];
    lT0 = lT0 * lT4 + lConsts[5];
    lT0 = lT0 * lT4 + lConsts[6];
    lT3 = lT0 * lT3;

    lT3 = lYAbsBigger ? (FHALF_PI)-lT3 : lT3;
    lT3 = (X < FZERO) ? FPI       -lT3 : lT3;
    lT3 = (Y < FZERO) ?           -lT3 : lT3;

    return lT3;
}

double Maths::Atan2(double Y, double X)
{
    if (X == DZERO && Y == DZERO)
    {
        return DZERO;
    }

    return std::atan2(Y,X);
}

float Maths::QuakeInvSqrt(float InValue)
{
    long i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = InValue * FONE_HALF;
    y  = InValue;

    i  = * ( long * ) &y;                       // evil floating point bit level hacking
    i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
    y  = * ( float * ) &i;                      // convert new bits into float
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration of Newton's method
    
    return y;
}

float Maths::FMod(float X, float Y)
{
    const float lAbsY = Abs(Y);
    if (lAbsY <= FSMALL_NUMBER)
    {
        return 0.0;
    }

    return std::fmod(X, Y);
}

double Maths::FMod(double X, double Y)
{
    const double lAbsY = Abs(Y);
    if (lAbsY <= DSMALL_NUMBER)
    {
        return 0.0;
    }

    return std::fmod(X, Y);
}
