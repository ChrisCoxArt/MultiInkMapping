/*
Multi Ink Mapping
  MIT License, Copyright (C) Chris Cox 2026

Created by Chris Cox around March 2, 2026.



Simulate inks (1-15) painted or printed on paper - for custom printing, profile, and 3DLUT generation.

Is it accurate? Nope.
    Accuracy would need a lot more measurements, and math, and might not look as good.

Does it look reasonable? Yes.
    And that's all I need from it.

This assumes that the primaries are somewhat saturated, and define a convex hull.
Primaries will be sorted by hue to make sure they are in order to make a convex hull.
This further assumes that the primaries are really transparent, so ink order does not matter. (this is NOT realistic)







TODO - would be nice to add measured overprint colors
        SOLIDS DONE
        Maybe prebuild a faster lookup system by ink fractions that can handle any fractions?
            hash((int)(100*fraction1)) and chain?  Still expensive.
            sum of fractions (scaled to int) for bucket, then sub lookup if match?
                cheaper, might work.
                but still suffers from "find closest" versus "exact" problems

        build predictable lookup for full inks, then fractional extrapolation for partials?
            currently looks up only fulls, then multiply for partials - which fails sometimes (hard edge where they meet)


TODO - allow additional combinations of inks (n+2, n+3, tertiary, etc.) when building splines
    take max chroma points for hull?
    maybe do all binary combinations, with lookup for any measured, then sort.
    or build in more combinations and sort midpoints by hue, before interpolating?
    Mix data would need to be updated along with sort!  merge into single structure?
    tertiary mixtures might be useful in some cases, but not most
    quaternary mixtures... aren't likely to be useful (mostly mud)
    maybe set limits on chroma and luma to keep mixtures?

    for (i=0;i<inks;++i)
      for (j=(i+1);j<inks;++j)
         overprints.push_back( estimate(inkFractions(i,j)) )
    sort( overprints, hue_less );
    make midpoints into larger list?
    then make splines from list?


TODO - What about tints and shades?  need percentages of mixes, plus measurement.
    Um, special case names for "paper" and "dark"?
    How to use these when building splines?
    { "Ink1Name", 0.25, "Ink2Name", 0.75, measuredOverprint }
    { "Ink1Name", 0.25, "none", 0, measuredTint }
    { "InInk1Namek1", 0.25, "dark", 0.25, measuredShade }

    Do I really want to support full IT8 profile data?  No.


*/

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstdlib>
#define _USE_MATH_DEFINES
#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <algorithm>
#include "MultiInkMapping.hpp"
#include "Options.hpp"
#include "MiniTIFF.hpp"
#include "MiniICC.hpp"

/********************************************************************************/

static
void VerifyDecreasingL( const color_list &list )
{
#if !NDEBUG
    size_t count = list.size();
    for (size_t i = 1; i < count; ++i) {
        float currentL = list[i].L;
        float previousL = list[i-1].L;
        assert( currentL <= previousL);
    }
#endif
}

/********************************************************************************/

const float XD50 = 96.4212f;
const float YD50 = 100.0f;
const float ZD50 = 82.5188f;

static
float CIECurve( const float input )
{
    const float scale = 7.787037f;            // powf( 29.0/6.0, 2.0) / 3.0;
    const float breakpoint = 0.008856f;        // powf( 6.0/29.0, 3.0 );
    const float offset = (float)(4.0/29.0);
    
    if (input > breakpoint)
        return cbrtf( input );
    else
        return (input * scale + offset);
}

/********************************************************************************/

static
float CIEReverseCurve( const float input )
{
    const float scale = (float)(1.0 / 7.787037);        // 0.128418549  // 3.0 * powf( 6.0/29.0, 2.0);
    const float breakpoint = (float)(6.0/29.0);
    const float offset = (float)(4.0/29.0);
    
    if (input > breakpoint)
        return input*input*input;   // powf(input,3);
    else
        return scale*(input - offset);
}

/********************************************************************************/

static
xyzColor LAB2XYZ( const labColor &input )
{
    xyzColor result;
    
    float tempY = (input.L + 16.0f)/116.0f;

    float Y = YD50 * CIEReverseCurve( tempY );
    float X = XD50 * CIEReverseCurve( tempY + input.A / 500.0f );
    float Z = ZD50 * CIEReverseCurve( tempY - input.B / 200.0f );

    result.X = X;
    result.Y = Y;
    result.Z = Z;

    return result;
}

/********************************************************************************/

static
labColor XYZ2LAB( const xyzColor &input )
{
    labColor result;
    
    float tempY = CIECurve( input.Y / YD50 );
    float tempX = CIECurve( input.X / XD50 );
    float tempZ = CIECurve( input.Z / ZD50 );

    float L = 116.0f * tempY - 16.0f;
    float a = 500.0f * ( tempX - tempY );
    float b = 200.0f * ( tempY - tempZ );

    result.L = L;
    result.A = a;
    result.B = b;

    return result;
}

/********************************************************************************/

// linear interpolation
inline
xyzColor interp2inks( const float t, const xyzColor &ink1, const xyzColor &ink2 )
{
    xyzColor result;
    result = ink1 + t * (ink2 - ink1);
    return result;
}

/********************************************************************************/

// linear interpolation in LAB - really only useful for nearby colors or neutrals
inline
labColor interp2inks( const float t, const labColor &ink1, const labColor &ink2 )
{
    labColor result;
    result.L = LERP( t, ink1.L, ink2.L );
    result.A = LERP( t, ink1.A, ink2.A );
    result.B = LERP( t, ink1.B, ink2.B );
    return result;
}

/********************************************************************************/

static
color_list mix_pure_ink_spline( int steps, const labColor &paperColor, const labColor &inkColor, const labColor &darkColor)
{
    int i;
    xyzColor mix;
    labColor mixLAB;
    color_list temp;
    
    xyzColor paperColorXYZ = LAB2XYZ( paperColor );
    xyzColor darkColorXYZ = LAB2XYZ( darkColor );
    
    xyzColor inkColorXYZ = LAB2XYZ( inkColor );

    // exact paper
    temp.push_back( paperColor );
    
    // interp paper->ink
    for (i=1; i < (steps/2); ++i) {
        float t = (float) i / (float) (steps/2);
        mix = interp2inks( t, paperColorXYZ, inkColorXYZ );
        mixLAB = XYZ2LAB( mix );
        temp.push_back( mixLAB );
    }
    
    // exact ink
    temp.push_back( inkColor );
 
    // interp ink->dark
    for (i=(steps/2)+1; i < (steps-1); ++i) {
        float t = (float) (i - (steps/2)) / (float) (steps/2);
        mix = interp2inks( t, inkColorXYZ, darkColorXYZ );
        mixLAB = XYZ2LAB( mix );
        temp.push_back( mixLAB );
    }
    
    // exact dark
    temp.push_back( darkColor );
 
    // error checking
    VerifyDecreasingL(temp);

    return temp;
}

/********************************************************************************/

bool labHueLess(const namedColor &a, const namedColor &b)
{
    float angle1 = (float)(M_PI + atan2f(a.color.A,a.color.B));
    float angle2 = (float)(M_PI + atan2f(b.color.A,b.color.B));
    return angle1 < angle2;
}

/********************************************************************************/

// here we want chromatic mixes, not darks
static
xyzColor estimate_ink_mix( inkColorSet &inkSet, const std::vector<xyzColor> &inkList,
                            const std::vector<size_t> &inkIndices,
                            const xyzColor &paperColor, size_t inkCount )
{
    xyzColor overprint = identityXYZ;
#if 1
    uint32_t opBitmap = 0;
    
    // see if we have an overprint that can be looked up
    if (inkSet.overprints.size() > 0) {
        uint32_t lookupBitmap = 0;
        size_t bitCount = inkCount;
        
        for (size_t i = 0; i < inkCount; ++i) {
            size_t index = inkIndices[i];
            lookupBitmap |= (1UL << index);
        }
        
        if (bitCount > 1) {
            // do we have a matching overprint?
            auto iter = inkSet.overprint_bitmask_map.find(lookupBitmap);
            if ( iter != inkSet.overprint_bitmask_map.end() ) {
                size_t opIndex = (size_t) iter->second;
                opBitmap = inkSet.overprints[ opIndex ].inkBitmap;
                overprint = inkSet.overprints[ opIndex ].colorXYZ;
            }

            // Can we at least come close?  Need closest match ignoring some channels, while still having 2 or more channels.
// TODO - Or should I add fake entries to the map for this? preprocess sounds saner.
        }
    }
    
    // interp and multiply remaining inks that weren't in the overprint data
    for (size_t i = 0; i < inkCount; ++i) {
        size_t index = inkIndices[i];
        if ((opBitmap & (1UL << index)) != 0)
            continue;
        
        auto &ink = inkList[i];
        overprint *= ink;
    }

#else
    for ( const auto &ink : inkList ) {
        overprint *= ink;
    }
#endif

    overprint *= paperColor;

    return overprint;
}

/********************************************************************************/

// trying to estimate appearance of overprints among arbitrary inks
static
xyzColor estimate_fractional_ink_mix( const inkColorSet &inkSet, const std::vector<xyzColor> &inkList,
            const std::vector<float> &inkFractionList, const xyzColor &paperColor, size_t inkCount )
{
    assert( inkCount >= 1 && inkCount <= kMaxChannels );
    
    xyzColor overprint = identityXYZ;
#if 1
    uint32_t opBitmap = 0;
    
    // see if we have an overprint that can be looked up
    if (inkSet.overprints.size() > 0) {
        uint32_t lookupBitmap = 0;
        int bitCount = 0;
        for (size_t i = 0; i < inkCount; ++i) {
            float thisFraction = inkFractionList[i];
            if (thisFraction >= 1.0) {
                lookupBitmap |= (1UL << i);
                ++bitCount;
            }
        }
        
        if (bitCount > 1) {
            // do we have a matching overprint?
            auto iter = inkSet.overprint_bitmask_map.find(lookupBitmap);
            if ( iter != inkSet.overprint_bitmask_map.end() ) {
                size_t opIndex = (size_t) iter->second;
                opBitmap = inkSet.overprints[ opIndex ].inkBitmap;
                overprint = inkSet.overprints[ opIndex ].colorXYZ;
            }

            // Can we at least come close?  Need closest match ignoring some channels, while still having 2 or more channels.
// TODO - Or should I add fake entries to the map for this? preprocess sounds saner.
        }
    }
    
    // interp and multiply remaining inks that weren't in the overprint data
    for (size_t i = 0; i < inkCount; ++i) {
        if ((opBitmap & (1UL << i)) != 0)
            continue;
        
        auto &ink = inkList[i];
        float thisFraction = inkFractionList[i];
        if (thisFraction > 0.0) {
            xyzColor fractionalInk = interp2inks( thisFraction, identityXYZ, ink );
            overprint *= fractionalInk;
        }
    }
#else
    for (int i = 0; i < inkCount; ++i) {
        auto &ink = inkList[i];
        float thisFraction = inkFractionList[i];
        if (thisFraction > 0.0) {
            xyzColor fractionalInk = interp2inks( thisFraction, identityXYZ, ink );
            overprint *= fractionalInk;
        }
    }
#endif

    overprint *= paperColor;

    return overprint;
}

/********************************************************************************/

// here, we want the darkest possible result - which mixes all the inks
// DEFERRED - should prefer a near neutral.
static
xyzColor estimate_darkest_ink_overprint( const std::vector<xyzColor> &inkList, const xyzColor &paperColor )
{
    const float Ylimit = 1.3f;   // author's preference
    
    xyzColor overprint = identityXYZ;
    for ( const auto &ink : inkList ) {
        overprint *= ink;
    }
    overprint *= paperColor;
    
    // Bring down luminance if needed to give a reasonable visual result
    // Scaling all channels to reduce chroma of the overprint
    float maxVal = std::max( overprint.X, std::max( overprint.Y, overprint.Z ));
    if (maxVal > Ylimit) {
        float scale = Ylimit / maxVal;
        overprint *= scale;
    }
    
    return overprint;
}

/********************************************************************************/

// convenience converter
static
xyzColor estimate_darkest_ink_overprint( const std::vector<namedColor> &inkList, const xyzColor &paperColor )
{
    std::vector<xyzColor> tempListXYZ( inkList.size() );
    for (size_t i = 0; i < inkList.size(); ++i)
        tempListXYZ[i] = LAB2XYZ( inkList[i].color ) / paperColor;
    
    return estimate_darkest_ink_overprint( tempListXYZ, paperColor );
}

/********************************************************************************/

static
void subdivide_ink_splines( inkColorSet &inkSet, const int divisions, const int steps,
            const size_t ink1Index, const size_t ink2Index, const xyzColor &paperColor )
{
    color_list temp;
    labColor mixLAB;
 
    labColor ink1 = inkSet.primaries[ink1Index].color;
    labColor ink2 = inkSet.primaries[ink2Index].color;

    xyzColor ink1Color = LAB2XYZ( ink1 );
    xyzColor ink2Color = LAB2XYZ( ink2 );

    xyzColor halfwayMix = estimate_ink_mix( inkSet, { ink1Color/paperColor, ink2Color/paperColor },
                                            {ink1Index, ink2Index}, paperColor, 2 );

    // d == 0 is the last pure ink spline
    // d == division is this pure ink spline (handled elsewhere)
    for (int d = 1; d < divisions; ++d) {
        float t = (float)d / (float)divisions;
        float t1 = 1.0f;
        float t2 = 1.0f;

        xyzColor mix;
        if (t <= 0.5f) {
            mix = interp2inks( t*2.0f, ink1Color, halfwayMix );
            t2 = t*2.0f; // going 0 -> 1
        }
        else {
            mix = interp2inks( (t-0.5f)*2.0f, halfwayMix, ink2Color );
            t1 = (1.0f - t) * 2.0f;   // going 1 -> 0
        }

        mixLAB = XYZ2LAB( mix );
        temp = mix_pure_ink_spline( steps, inkSet.paperColor, mixLAB, inkSet.darkColor );
        inkSet.splines.push_back( temp );
        inkSet.mixData.push_back( inkMixPair( ink1Index, ink2Index, t1, t2 ) );
    }
}

/********************************************************************************/

static
void prepare_ink_dark( inkColorSet &inkSet )
{
    // If the combined color has <= 0 L, estimate it from primaries
    // so we can get something sort of realistic for the mix
    if (inkSet.darkColor.L <= 0) {
        xyzColor paperColor = LAB2XYZ( inkSet.paperColor );
        xyzColor mix = estimate_darkest_ink_overprint( inkSet.primaries, paperColor );
        labColor mixLAB = XYZ2LAB( mix );
        if (globalSettings.gDebugMode)
            printf("Estimated overprint for %s is (%f, %f, %f)\n",
                inkSet.name.c_str(),
                mixLAB.L, mixLAB.A, mixLAB.B );
        inkSet.darkColor = mixLAB;
    }
}

/********************************************************************************/

// create splines from mixes of inks and paper colors
static
void mix_ink_splines( inkColorSet &inkSet )
{
    const int steps = 51;    // odd so we have a midpoint
    const int divisions = 4;    // even so we have a midpoint (5 splines per surface)

    size_t inkCount = inkSet.primaries.size();
    assert(inkCount > 0 && inkCount <= kMaxChannels);

    xyzColor paperColor = LAB2XYZ( inkSet.paperColor );

    // first ink spline, always calculated
    color_list temp = mix_pure_ink_spline( steps, inkSet.paperColor, inkSet.primaries[0].color, inkSet.darkColor );
    inkSet.splines.push_back( temp );
    inkSet.mixData.push_back( inkMixPair( 0, 0, 1.0, 0.0 ) );

    // iterate any additional inks, keeping splines in order
    for (size_t k = 1; k < inkCount; ++k) {
        subdivide_ink_splines( inkSet, divisions, steps,
            k-1, k,
            paperColor);

        // pure ink spline paper->ink2->combined
        temp = mix_pure_ink_spline( steps, inkSet.paperColor, inkSet.primaries[k].color, inkSet.darkColor );
        inkSet.splines.push_back( temp );
        inkSet.mixData.push_back( inkMixPair( k, k, 1.0, 0.0 ) );
    }
    
    // if we can make a solid, then wrap around from last ink to the first!
    if (inkCount > 2) {
        subdivide_ink_splines( inkSet, divisions, steps,
            inkCount-1, 0,
            paperColor);
    }
    
    assert( inkSet.splines.size() == inkSet.mixData.size() );
}

/********************************************************************************/

// t ranges 0..1.0
// we are interpolating between B and C
static
float SplineInterp( float t, float A, float B, float C, float D )
{
// catmull rom - cardinal spline with tension = 0.5
// needs scaling by 0.5 at end
    const float    M11 = -1.0, M12 = 3.0, M13 = -3.0, M14 = 1.0;
    const float    M21 = 2.0, M22 = -5.0, M23 = 4.0, M24 = -1.0;
    const float    M31 = -1.0, M32 = 0.0, M33 = 1.0, M34 = 0.0;
    const float    M41 = 0.0, M42 = 2.0, M43 = 0.0, M44 = 0.0;
    
    // cubic interp
    float value;
    value  = A * (M41 + t * (M31 + t * (M21 + t*M11) ) );
    value += B * (M42 + t * (M32 + t * (M22 + t*M12) ) );
    value += C * (M43 + t * (M33 + t * (M23 + t*M13) ) );
    value += D * (M44 + t * (M34 + t * (M24 + t*M14) ) );
    
    return 0.5f * value;
}

/********************************************************************************/


// need function to search spline for correct point and return interpolated values
// given L*, binary search the spline and return the A and B values that go with it
static
void SearchSpline( const color_list &spline, float Ltarget, float &A, float &B )
{
    // find points in list that bracket L
    // list is greatest to least (white to black)
    // ccox - start with a simple linear search
    size_t index = 1;
    for ( ; index < spline.size(); ++index) {
        if (spline[index].L <= Ltarget)
            break;
    }
    
    assert( index < spline.size() );
    
    // find t that gives the correct L to within tolerance (between index-1 and index)

    int sample0 = (int)index - 2;
    int sample1 = (int)index - 1;
    int sample2 = (int)index;
    int sample3 = (int)index + 1;
    
    // clip to end points
    if (sample0 < 0) sample0 = 0;
    if (sample1 < 0) sample1 = 0;
    if (sample3 > (int)(spline.size())-1)    sample3 = (int)(spline.size())-1;
    
    // quick and dirty binary search

    float t = 0.5f;
    const float Ltolerance = 0.1f;   // this seems to be good enough
    
    float Ttop = 0.0f;
    
    float Tbottom = 1.0f;

    float Ltest = SplineInterp( t, spline[(size_t)sample0].L, spline[(size_t)sample1].L, spline[(size_t)sample2].L, spline[(size_t)sample3].L );
    
    // quick and dirty binary search
    while ( fabsf( Ltest - Ltarget ) > Ltolerance) {
        if (Ltest < Ltarget) {
            // between top and current
            Tbottom = t;
        } else {
            // between current and bottom
            Ttop = t;
        }
        
        t = (Ttop + Tbottom) * 0.5f;
        Ltest = SplineInterp( t, spline[(size_t)sample0].L, spline[(size_t)sample1].L, spline[(size_t)sample2].L, spline[(size_t)sample3].L );
    }
    
    // interpolate colors and return result
    A = SplineInterp( t, spline[(size_t)sample0].A, spline[(size_t)sample1].A, spline[(size_t)sample2].A, spline[(size_t)sample3].A );
    B = SplineInterp( t, spline[(size_t)sample0].B, spline[(size_t)sample1].B, spline[(size_t)sample2].B, spline[(size_t)sample3].B );

}

/********************************************************************************/

// are we less than our darkest, or greater than our brightest point?
static
bool ClippedL( float Linput, labColor &output, const inkColorSet &inkSet )
{
    output.L = output.A = output.B = 0.0f;
    
    if (Linput < inkSet.darkColor.L) {
        output = inkSet.darkColor;
        return true;
    }

    if (Linput > inkSet.paperColor.L) {
        output = inkSet.paperColor;
        return true;
    }

    return false;
}

/********************************************************************************/

static
void SplineInterpList( const size_t subdivisions, const PointList &input, PointList &result,
                        bool wrapAround )
{
    const int pointCount = (int)input.size();
    
    result.reserve( subdivisions+1 );
    
    // iterate through list
    for (size_t i = 0; i <= subdivisions; ++i) {
        /// which input points are we between?
        float floatIndex = ((float)pointCount * i) / (float)subdivisions;
        int pointIndex = int( floatIndex );
        
        int p0 = pointIndex - 1;
        int p1 = pointIndex;
        int p2 = pointIndex + 1;
        int p3 = pointIndex + 2;

        if (wrapAround) {
            // wrap around if inks > 2, so we get a solid shape
            if (p0 < 0)
                p0 = p0 + pointCount;
            if (p1 < 0)
                p1 = p1 + pointCount;
            if (p1 >= pointCount)
                p1 = p1 - pointCount;
            if (p2 >= pointCount)
                p2 = p2 - pointCount;
            if (p3 >= pointCount)
                p3 = p3 - pointCount;
        } else {
            // clamp
            if (p0 < 0) p0 = 0;
            if (p1 < 0) p1 = 0;
            if (p1 >= pointCount) p1 = pointCount-1;
            if (p2 >= pointCount) p2 = pointCount-1;
            if (p3 >= pointCount) p3 = pointCount-1;
        }
        
        float t = floatIndex - pointIndex;
        Point newPoint;
        newPoint.a = SplineInterp( t, input[(size_t)p0].a, input[(size_t)p1].a, input[(size_t)p2].a, input[(size_t)p3].a );
        newPoint.b = SplineInterp( t, input[(size_t)p0].b, input[(size_t)p1].b, input[(size_t)p2].b, input[(size_t)p3].b );
        
        result.emplace_back( newPoint );
    }   // end for subdivisions
}

/********************************************************************************/

static
void LinearInterpList( const size_t subdivisions, const PointList &input, PointList &result,
                        bool wrapAround )
{
    const int pointCount = (int)input.size();
    
    result.reserve( subdivisions+1 );
    
    for (size_t i = 0; i <= subdivisions; ++i) {
        /// which input points are we between?
        float floatIndex = ((float)pointCount * i) / (float)subdivisions;
        int pointIndex = int( floatIndex );
        
        int p1 = pointIndex;
        int p2 = pointIndex + 1;

        if (wrapAround) {
            // wrap around if inks > 2, so we get a solid shape
            if (p1 < 0)
                p1 = p1 + pointCount;
            if (p1 >= pointCount)
                p1 = p1 - pointCount;
            if (p2 >= pointCount)
                p2 = p2 - pointCount;
        } else {
            // clamp
            if (p1 < 0) p1 = 0;
            if (p1 >= pointCount) p1 = pointCount-1;
            if (p2 >= pointCount) p2 = pointCount-1;
        }
        
        float t = floatIndex - pointIndex;
        Point newPoint;
        newPoint.a = LERP( t, input[(size_t)p1].a, input[(size_t)p2].a );
        newPoint.b = LERP( t, input[(size_t)p1].b, input[(size_t)p2].b );
        
        result.emplace_back( newPoint );
    }   // end for subdivisions
}

/********************************************************************************/

static
void PointListFromSplines( const size_t subdivisions, const PointList &input, PointList &result,
                                bool wrapAround)
{
    // evaluate the spline at a fixed number of points, put those into a list of points
    const int pointCount = (int)input.size();
    
    if (pointCount == 1) {
        // nothing to interpolate
        result = input;
        return;
    }
    
    result.clear();
 
    if (pointCount < 4)
        LinearInterpList( subdivisions, input, result, wrapAround );
    else
        SplineInterpList( subdivisions, input, result, wrapAround );
}

/********************************************************************************/

static
void InterpMixList( const size_t subdivisions, const spline_mix_data &input, spline_mix_data &result,
                        bool wrapAround )
{
    const int pointCount = (int)input.size();
    
    result.reserve( subdivisions+1 );
    
    for (size_t i = 0; i <= subdivisions; ++i) {
        /// which input points are we between?
        float floatIndex = ((float)pointCount * i) / (float)subdivisions;
        int pointIndex = int( floatIndex );
        
        int p1 = pointIndex;
        int p2 = pointIndex + 1;

        if (wrapAround) {
            // wrap around if inks > 2, so we get a solid shape
            if (p1 < 0)
                p1 = p1 + pointCount;
            if (p1 >= pointCount)
                p1 = p1 - pointCount;
            if (p2 >= pointCount)
                p2 = p2 - pointCount;
        } else {
            // clamp
            if (p1 < 0) p1 = 0;
            if (p1 >= pointCount) p1 = pointCount-1;
            if (p2 >= pointCount) p2 = pointCount-1;
        }
        
        float t = floatIndex - pointIndex;
        
        // I need the ink numbers, not spline numbers!
        // could have 2 or 3 different inks specified (on boundaries)
        // need to figure out which pair we're interpolating
        size_t index1 = input[(size_t)p1].inkIndex1;
        size_t index2 = input[(size_t)p1].inkIndex2;
        
        if (input[(size_t)p2].ink1Fraction > input[(size_t)p1].ink1Fraction)
            index1 = input[(size_t)p2].inkIndex1;
        
        if (input[(size_t)p2].ink2Fraction > input[(size_t)p1].ink2Fraction)
            index2 = input[(size_t)p2].inkIndex2;

        float fraction1 = LERP( t, input[(size_t)p1].ink1Fraction, input[(size_t)p2].ink1Fraction );
        float fraction2 = LERP( t, input[(size_t)p1].ink2Fraction, input[(size_t)p2].ink2Fraction );
        
        // fractions will not add up to unity, because we have overprints!
        
        result.emplace_back( inkMixPair(index1,index2,fraction1,fraction2 ) );
        
    }   // end for subdivisions
}

/********************************************************************************/

static
void MixPointsFromSplines( const size_t subdivisions, const spline_mix_data &input, spline_mix_data &result,
                                bool wrapAround)
{
    // evaluate the spline at a fixed number of points, put those into a list of points
    const int pointCount = (int)input.size();
    
    if (pointCount == 1) {
        // nothing to interpolate
        result = input;
        return;
    }
    
    result.clear();
 
    InterpMixList( subdivisions, input, result, wrapAround );
}

/********************************************************************************/

static
size_t FindClosestPointInList( const PointList &list, Point &input )
{
    // ccox - start with brute force linear search
/*
DEFERRED - find a way to accelerate the search
    list is always more or less circular
    needs to work with points inside and outside, and FAR outside
    
        But we only execute this for one slice at a time -- so 21x21 points
            then change data
        
        Could use radial projection for angles, with precomputed angles - we don't have the center here
        Could use grid to narrow search - with aux data structure
        Could limit the points by removing those closer than 0.1 deltaE
        Count is between 1 and 300
 */
    size_t count = list.size();
    assert( count > 0 );
 
    if (count == 1)
        return 0;

    float closest_dist = 1e20f;        // much greater than our maximum possible distance
    size_t closest_index = size_t(-1);  // really largest positive value because it is unsigned
    
    for (size_t i = 0; i < count; ++i) {
        float distA = input.a - list[i].a;
        float distB = input.b - list[i].b;
        float dist = distA*distA + distB*distB;    // leave it squared
        
        if (dist < closest_dist) {
            closest_dist = dist;
            closest_index = i;
        }
    }
    
    assert( closest_index >= 0);
    return closest_index;
}


/********************************************************************************/

// really simple tent, with a twist to reduce scum dots
inline
float Smooth3( float a, float b, float c)
{
    // scum dot reduction
    if (b == 1.0f || b == 0.0f)
        return b;
    
    return (a + 4*b + c) / 6.0f;
}

/********************************************************************************/

// prev, current, next
inline
void constexpr Smooth3( std::vector<float> &a, const std::vector<float> &b, const std::vector<float> &c, size_t channels)
{
    for (size_t i = 0; i < channels; ++i)
        a[i] = Smooth3(a[i],b[i],c[i]);
}

// prev, current, next
inline
void constexpr Smooth3( float *a, float *b, float *c, size_t channels)
{
    for (size_t i = 0; i < channels; ++i)
        a[i] = Smooth3(a[i],b[i],c[i]);
}

/********************************************************************************/

// filter in place, in one dimension, for 3 channels
static
void SmoothOneDirection3( float *data, size_t gridPoints, size_t planeStep, size_t rowStep, size_t colStep )
{
    size_t i, j, k;
    
    for (i = 0; i < gridPoints; ++i) {
        for (j = 0; j < gridPoints; ++j) {
            k = 0;
            
            // special case first value
            
            float last0 = data[ i * planeStep + j * rowStep + k*colStep + 0 ];
            float last1 = data[ i * planeStep + j * rowStep + k*colStep + 1 ];
            float last2 = data[ i * planeStep + j * rowStep + k*colStep + 2 ];
            
            float current0 = last0;
            float current1 = last1;
            float current2 = last2;
            
            float next0 = 0, next1 = 0, next2 = 0;
            float result0, result1, result2;
            
            for (k = 0; k < (gridPoints-1); ++k) {
                
                next0 = data[ i * planeStep + j * rowStep + (k+1)*colStep + 0 ];
                next1 = data[ i * planeStep + j * rowStep + (k+1)*colStep + 1 ];
                next2 = data[ i * planeStep + j * rowStep + (k+1)*colStep + 2 ];
                
                result0 = Smooth3( last0, current0, next0 );
                result1 = Smooth3( last1, current1, next1 );
                result2 = Smooth3( last2, current2, next2 );
                
                // write back smoothed result
                data[ i * planeStep + j * rowStep + k*colStep + 0 ] = result0;
                data[ i * planeStep + j * rowStep + k*colStep + 1 ] = result1;
                data[ i * planeStep + j * rowStep + k*colStep + 2 ] = result2;
                
                // rotate
                last0 = current0;
                last1 = current1;
                last2 = current2;
                
                current0 = next0;
                current1 = next1;
                current2 = next2;
            }
            
            // special case last k value
            // next == current already
            result0 =  Smooth3( last0, current0, next0 );
            result1 =  Smooth3( last1, current1, next1 );
            result2 =  Smooth3( last2, current2, next2 );
            
            // write back smoothed result
            data[ i * planeStep + j * rowStep + k*colStep + 0 ] = result0;
            data[ i * planeStep + j * rowStep + k*colStep + 1 ] = result1;
            data[ i * planeStep + j * rowStep + k*colStep + 2 ] = result2;
        }
        
    }

}

/********************************************************************************/

// filter in place, in one dimension, for arbitrary channel counts
static
void SmoothOneDirection( float *data, size_t gridPoints, size_t planeStep, size_t rowStep, size_t colStep, size_t channels )
{
    assert(channels > 0);
    assert(channels <= kMaxChannels);
 
    if (channels == 3) {
        SmoothOneDirection3( data, gridPoints, planeStep, rowStep, colStep );
        return;
    }
    
    // there isn't an easy way to do this for arbitrary channel counts
    std::vector<float> last(kMaxChannels);
    std::vector<float> current(kMaxChannels);
    std::vector<float> next(kMaxChannels);

    float *lastp = &last[0];
    float *currentp = &current[0];
    float *nextp = &next[0];
    
    for (size_t i = 0; i < gridPoints; ++i) {
 
        for (size_t j = 0; j < gridPoints; ++j) {
            size_t k = 0;
            
            // special case first value
            for (size_t c = 0; c < channels; ++c) {
                auto value = data[ i * planeStep + j * rowStep + k * colStep + c ];
                lastp[c] = value;
                currentp[c] = value;
            }
            
            for (k = 0; k < (gridPoints-1); ++k) {
            
                for (size_t c = 0; c < channels; ++c)
                    nextp[c] = data[ i * planeStep + j * rowStep + (k+1)*colStep + c ];
                
                Smooth3( lastp, currentp, nextp, channels );
                
                // write back smoothed result
                for (size_t c = 0; c < channels; ++c)
                   data[ i * planeStep + j * rowStep + k*colStep + c ] = lastp[c];
                
                // rotate pointers
                float *tempp = lastp;
                lastp = currentp;
                currentp = nextp;
                nextp = tempp;
            }
            
            // special case last k value
            // next == current, duplicating end value
            for (size_t c = 0; c < channels; ++c)
               nextp[c] = currentp[c];
            
            Smooth3( lastp, currentp, nextp, channels );
            
            // write back smoothed result
            for (size_t c = 0; c < channels; ++c)
               data[ i * planeStep + j * rowStep + k*colStep + c ] = lastp[c];
            
        }   // end j loop
        
    }   // end i loop

}

/********************************************************************************/

// useful for debugging, but slow
// probably faster to rasterize the poly without antialiasing and sample the bitmap
static
bool pointInPoly( const PointList &poly, const Point a )
{
    bool inside = false;
    size_t count = poly.size();
    for (size_t i = 0, j = count-1; i < count; j = i++) {
        if ( ((poly[i].b > a.b) != (poly[j].b > a.b)) &&
            (a.a < (poly[j].a - poly[i].a) * (a.b - poly[i].b) / (poly[j].b - poly[i].b) + poly[i].a) )
            inside = !inside;
    }
    return inside;
}

/********************************************************************************/
#if 0
// debugging tool
static
void DumpPointList( const std::string &name, const PointList &planePoints )
{
    std::string filename = name + ".csv";
    
    FILE *out = fopen( filename.c_str(), "w");
    if (!out)
        return;
    
    fprintf(out,"name\n");
    fprintf(out,"x, y\n");
    for ( const auto &pt : planePoints )
        fprintf(out,"%f, %f\n", pt.a, pt.b );
    
    fclose(out);
}
#endif
/********************************************************************************/

/*
gamut -- LAB to boolean mapping
always 8 bit
*/
static
void createGamut_table( const inkColorSet &inkSet, int /* depth */, size_t gridPoints, profileData &myProfile )
{
    size_t inkCount = inkSet.primaries.size();
    assert(inkCount > 0 && inkCount <= kMaxChannels);

    // allocate my gamut grid
    size_t gridCount =  (size_t)gridPoints * (size_t)gridPoints * (size_t)gridPoints;
    std::unique_ptr<uint8_t> gamutBuffer(new uint8_t[ gridCount ]);
    uint8_t *gamutData = gamutBuffer.get();
    
    // set everything to out of gamut (inverted from a normal image/table, but ok...)
    memset( gamutData, 255, gridCount );
    
    const size_t gamutPlaneStep = (size_t)gridPoints * (size_t)gridPoints;
    const size_t gamutRowStep = (size_t)gridPoints;
    const size_t gamutColStep = 1;
    
    // 1 or 2 inks... doesn't really have a gamut volume
    if (inkCount > 2) {
        
        for (size_t L = 0; L < gridPoints; ++L) {
            // setup slices variables
            float Lfloat = grid_to_L( L, gridPoints );
            
            //special case less < darkest and > brightest
            labColor clippedColor;
            if (ClippedL( Lfloat, clippedColor, inkSet)) {
                continue;
            }   // end ClippedL
            
            // interpolate splines in L to get points along this AB plane
            PointList planeSpline;
            for ( const auto &oneSpline: inkSet.splines ) {
                float A1, B1;
                SearchSpline( oneSpline, Lfloat, A1, B1 );
                planeSpline.push_back( Point( A1, B1 ) );
            }
            
            // create interpolated point list from the splines
            PointList planePoints;
            size_t subDivisions = std::min( (size_t)300, 50*inkCount );
            PointListFromSplines( subDivisions, planeSpline, planePoints, (inkCount > 2) );

            // now iterate over this plane/slice
            for (size_t A = 0; A < gridPoints; ++A) {
                float Afloat = grid_to_AB( A, gridPoints );
                
                for (size_t B = 0; B < gridPoints; ++B) {
                    float Bfloat = grid_to_AB( B, gridPoints );

                    // find closest point in our line/point list
                    Point thisSpot( Afloat, Bfloat );
                    
                    // for 3 or more inks, test for inside polygon, interpolate inside
                    bool inside = pointInPoly( planePoints, thisSpot );
                    if (!inside) {
                        // See if we're really close to the boundary.
                        // Spreading the gamut slightly to allow for grid sampling.
                        const float threshold = 0.8f;
                        
                        size_t closestIndex = FindClosestPointInList( planePoints, thisSpot );
                        Point result = planePoints[ closestIndex ];
                        float dist = hypot( (result.a - thisSpot.a), (result.b - thisSpot.b) );
                        if (dist < threshold)
                            inside = true;
                    }
                    
                    if (inside)
                        gamutData[ L * gamutPlaneStep + A * gamutRowStep + B * gamutColStep ] = 0;
                    
                }   // end for B
            }   // end for A
        }   // end for L

    }   // if inks > 2


    if ( globalSettings.gTIFFTables ) {
        // order the data for easy viewing as an image
        std::unique_ptr<uint8_t> outBuffer(new uint8_t[ gridCount ]);
        uint8_t *outData = outBuffer.get();
    
        for (size_t B = 0; B < gridPoints; ++B) {
            size_t Bindex = ((size_t)gridPoints-1 - B);    // need to flip B
            for (size_t L = 0; L < gridPoints; ++L) {
                for (size_t A = 0; A < gridPoints; ++A) {
                    outData[0] = gamutData[ L * gamutPlaneStep + A * gamutRowStep + Bindex*gamutColStep ];
                    outData++;
                }
            }
        }
        
        // write TIFF File
        int mode = TIFF_MODE_GRAY_BLACKZERO;
        WriteTIFF( inkSet.name + "_gamut.tiff", 96.0, mode, outBuffer.get(),
                    (size_t)gridPoints*(size_t)gridPoints, (size_t)gridPoints, 1, 8 );
    }

    tableFormat myGamut;
    myGamut.tableSig = icSigGamutTag;
    myGamut.tableDepth = 8;                 // gamut depth really doesn't matter, it's a bool
    myGamut.tableGridPoints = gridPoints;
    myGamut.tableDimensions = 3;    // input
    myGamut.tableChannels = 1;      // output
    myGamut.tableData = std::move(gamutBuffer);
    myProfile.LUTtables.emplace_back(myGamut);
}

/********************************************************************************/

/*
A2B - inks and overprints to LAB, N-dimensional to 3 channels
    use ink mixing model and simple interpolation
    doesn't really need smoothing
*/
static
void createA2B_table( const inkColorSet &inkSet, int depth, profileData &myProfile,
                    const size_t maxGridSize )
{
    const int maxChannels = kMaxChannels;          // ICC spec. limit
    const int maxGridPoints = 31;                  // sanity limit (could be increased)
    
    size_t inkCount = inkSet.primaries.size();
    assert(inkCount > 0);
    assert(inkCount <= maxChannels);
    
    // decide on table size
    size_t gridPoints = 2;         // absolute minimum
    size_t gridSize = (size_t)pow( gridPoints, inkCount );
    
    size_t newPoints = gridPoints;
    size_t newSize = gridSize;
    while (newSize <= maxGridSize && newPoints <= maxGridPoints) {
        gridPoints = newPoints;
        gridSize = newSize;
        ++newPoints;
        newSize = (size_t)pow( newPoints, inkCount );
    }
    
    // setup loops to create the table
    std::vector<uint32_t> loopCounters(maxChannels);
    std::vector<float> inkFractions(maxChannels);

#if 0
    std::vector<uint32_t> loopSteps(maxChannels);   // um, I may not end up using these
    size_t index = inkCount;
    int step = 3;
    while (index) {
        loopSteps[index-1] = step;
        step *= gridPoints;
        --index;
    }
#endif

    xyzColor paperColor = LAB2XYZ( inkSet.paperColor );
    
    std::vector<xyzColor> inkListXYZ(maxChannels);
    for (size_t i = 0; i < inkCount; ++i)
        inkListXYZ[i] = LAB2XYZ(inkSet.primaries[i].color) / paperColor;

    assert( depth == 8 || depth == 16 );
    size_t gridCount = gridSize;
    size_t gridBufferSize = (size_t)gridCount * 3 * ((size_t)depth/8);
    std::unique_ptr<uint8_t> gridBuffer(new uint8_t[ gridBufferSize ]);
    uint8_t *gridData = gridBuffer.get();
    uint16_t *grid16Ptr = (uint16_t*)gridData;
    
    std::fill( loopCounters.begin(), loopCounters.end(), 0 );
    
    // iterate virtual loop to fill table  (faster than doing a dozen divides and modulos)
    // i[k] = (index / (int)pow(gridPoints,(inkCount-1)-k)) % gridPoints;   // loopSteps can be precalcuated, but the divides cannot
// NOTE - I could optimize this for N >= 2 by putting a more predictable loop inside using counters[inkCount-1)]
//      then incrementing above that, which gives the compiler a better chance at vectorization
    for (size_t index = 0; loopCounters[0] < gridPoints; ++index ) {
        
        for (size_t k = 0; k < inkCount; ++k)
            inkFractions[k] = (float)loopCounters[k] / (float)(gridPoints-1);
        
        xyzColor resultXYZ = estimate_fractional_ink_mix( inkSet, inkListXYZ, inkFractions, paperColor, inkCount );
        labColor resultLAB = XYZ2LAB( resultXYZ );
        
        if (depth == 16) {
            // convert to integer output values
            int Lout =   floatL_to_fileL16( resultLAB.L );
            int Aout = floatAB_to_fileAB16( resultLAB.A );
            int Bout = floatAB_to_fileAB16( resultLAB.B  );
            
            // write value out to file (interleaved)
            grid16Ptr[ 3*index + 0 ] = (uint16_t)Lout;
            grid16Ptr[ 3*index + 1 ] = (uint16_t)Aout;
            grid16Ptr[ 3*index + 2 ] = (uint16_t)Bout;
        
        } else {
            int Lout =   floatL_to_fileL8( resultLAB.L );
            int Aout = floatAB_to_fileAB8( resultLAB.A );
            int Bout = floatAB_to_fileAB8( resultLAB.B );
            gridData[ 3*index + 0 ] = (uint8_t)Lout;
            gridData[ 3*index + 1 ] = (uint8_t)Aout;
            gridData[ 3*index + 2 ] = (uint8_t)Bout;
        }
        
        // increment last counters
        //    if incremented is >= gridPoints, reset and roll upward in list
        //    if we don't overflow, save the incremented value and break out of the loop
        for (int j = ((int)inkCount-1); j >= 0; --j) {
            auto temp = loopCounters[(size_t)j] + 1;
            if (temp >= gridPoints && j != 0)   // we want counter 0 to overflow, to end the big loop
                loopCounters[(size_t)j] = 0;
            else {
                loopCounters[(size_t)j] = temp;
                break;
            }
        }   // end loop counter update
    
    }   // overall table loop using a vector of counters


    if ( globalSettings.gTIFFTables ) {
        // calculate an image size that is close to square
        size_t tiles = (size_t)gridCount / ((size_t)gridPoints*(size_t)gridPoints);
        if (tiles < 1) tiles = 1;
        
        size_t tilesWide = (size_t)sqrtf((float)tiles);
        size_t tilesHigh = (tiles + (tilesWide-1)) / tilesWide;
        
        size_t tiffWidth = tilesWide * (size_t)gridPoints;
        size_t tiffHeight = tilesHigh * (size_t)gridPoints;
        if (inkCount == 1)
            tiffWidth = 1;

        // copy the data so LAB adjustments won't affect profile data
        // and because we may need a larger image than the original LUT buffer
        size_t tiffBufferSize = (tiffWidth*tiffHeight) * 3 * ((size_t)depth/8);
        std::unique_ptr<uint8_t> tiffBuffer(new uint8_t[ tiffBufferSize ]);
        memset( tiffBuffer.get(), 0, tiffBufferSize );
        memcpy( tiffBuffer.get(), gridBuffer.get(), gridBufferSize );
        
        WriteTIFF( inkSet.name + "_A2B.tiff", 96.0, TIFF_MODE_CIELAB, tiffBuffer.get(),
                   tiffWidth, tiffHeight, 3, depth );
    }

    tableFormat myTable;
    myTable.tableSig = icSigAToB0Tag;
    myTable.tableDepth = depth;
    myTable.tableGridPoints = gridPoints;
    myTable.tableDimensions = (int)inkCount;    // input
    myTable.tableChannels = 3;                  // output
    myTable.tableData = std::move(gridBuffer);
    myProfile.LUTtables.emplace_back(myTable);
}

/********************************************************************************/

static
std::vector<float> MixInkWeights( float t, const std::vector<float> &a, const std::vector<float> &b, const size_t channels )
{
    std::vector<float> result(kMaxChannels);
    for (size_t c = 0; c < channels; ++c)
       result[c] = LERP( t, a[c], b[c] );
    return result;
}

/********************************************************************************/
#if 0
static
std::vector<float> AddInkWeights( const std::vector<float> &a, const std::vector<float> &b, const size_t channels )
{
    std::vector<float> result(kMaxChannels);
    for (size_t c = 0; c < channels; ++c)
       result[c] = a[c] + b[c];
    return result;
}
#endif
/********************************************************************************/

static
std::vector<float> ScaleInkWeights( float t, const std::vector<float> &a, const size_t channels )
{
    std::vector<float> result(kMaxChannels);
    for (size_t c = 0; c < channels; ++c)
       result[c] = t * a[c];
    return result;
}
/********************************************************************************/

static
std::vector<float> SaturateInkWeights( const std::vector<float> &a, const size_t channels )
{
    std::vector<float> result(kMaxChannels);
    float maxValue = a[0];
    for (size_t c = 1; c < channels; ++c)
        maxValue = std::max( a[c], maxValue );
    float t = 1.0f / maxValue;
    for (size_t c = 0; c < channels; ++c)
       result[c] = t * a[c];
    return result;
}

/********************************************************************************/

#if 0
// this is for debugging
static
std::vector<float> ClipInkWeights( std::vector<float> &a, const size_t channels )
{
    std::vector<float> result(a);
    float maxVal = a[0];
    float minVal = a[0];
    for (size_t c = 1; c < channels; ++c) {
        maxVal = std::max( a[c], maxVal );
        minVal = std::min( a[c], minVal );
    }
    
    if (maxVal > 1.0) {     // haven't hit this case yet, yay!
        float scale = 1.0 / maxVal;
        result = ScaleInkWeights( scale, a, channels );
    }
    
    assert(minVal >= 0.0);
    
    return result;
}
#endif

/********************************************************************************/

// TODO - get a lot of repeat values here, could cache?
// maybe just cache t, LTarget, LStart ?  Seems to work, but needs verification.
#define CACHE 0

static
void AdjustInkMixForL( const inkColorSet &inkSet, float Ltarget, const std::vector<xyzColor> &inkListXYZ,
            std::vector<float> &inkFractionList, const xyzColor &paperColor, size_t inkCount )
{
    const float tolerance = 0.1f;
    const float epsilon = 1e-4f;
    const std::vector<float> neutralWeights( kMaxChannels, 1.0 );
    
// I used to saturate here, but that was a mistake
    auto satList = inkFractionList;
    
    auto workingList = satList;

    // calc initial L*
    xyzColor tempXYZ = estimate_fractional_ink_mix( inkSet, inkListXYZ, satList, paperColor, inkCount );
    labColor tempLAB = XYZ2LAB( tempXYZ );
    float Lstart = tempLAB.L;
    float Lcurrent = Lstart;
    float t = (Lstart < Ltarget) ? 0.0f : 1.0f;
    float tTop = 1.0f;
    float tBottom = 0.0f;

#if CACHE
    static float cacheLTarget = -50;
    static float cacheLStart = -50;
    static float cacheT = 0.0;
    
    if (Ltarget == cacheLTarget
        && Lstart == cacheLStart) {
        tTop = cacheT + epsilon;    // offset so we iterate once and update the workingList inkWeights
        tBottom = cacheT - epsilon;
        t = cacheT;
    }
    
    cacheLStart = Lstart;
    cacheLTarget = Ltarget;
#endif

    // binary search to find values, and bail with best effort if we can't reach them
    while (fabsf(Lcurrent - Ltarget) > tolerance) {
        
        if (Lcurrent < Ltarget)
            tBottom = t;
        else
            tTop = t;
    
        t = (tBottom + tTop) * 0.5f;
        
        if (Lstart < Ltarget)
            // adjust new blend toward paper
            workingList = ScaleInkWeights( (1.0f-t), satList, inkCount );
        else
            // adjust new blend toward dark
            workingList = MixInkWeights( (1.0f-t), satList, neutralWeights, inkCount );
        
        tempXYZ = estimate_fractional_ink_mix( inkSet, inkListXYZ, workingList, paperColor, inkCount );
        tempLAB = XYZ2LAB( tempXYZ );
        Lcurrent = tempLAB.L;
        
        // sometimes our estimates just don't match expectations
        // but we don't want to loop forever on a goal we can't reach
        if ( (tTop-tBottom) < epsilon || t < 0.0 || t > 1.0)
            break;
    }

#if CACHE
    cacheT = t;
#endif

    inkFractionList = workingList;
}

/********************************************************************************/

struct splineHuePair {
    float angle;
    size_t index;
};

/********************************************************************************/

bool splineHueIndexLess(const splineHuePair &a, const splineHuePair &b)
{
    if (a.angle == b.angle)
        return a.index < b.index;
    return a.angle < b.angle;
}

/********************************************************************************/

/*
B2A - LAB to ink mixes, needs detail, 3D to N channels
    ignore GCR/UCR just write the raw mixes
    This needs smoothing.
*/
static
void createB2A_table( const inkColorSet &inkSet, int depth, size_t gridPoints, profileData &myProfile )
{
    size_t inkCount = inkSet.primaries.size();
    assert(inkCount > 0);
    assert(inkCount <= kMaxChannels);

    xyzColor paperColor = LAB2XYZ( inkSet.paperColor );
    
    std::vector<xyzColor> inkListXYZ(kMaxChannels);
    for (size_t i = 0; i < inkCount; ++i)
        inkListXYZ[i] = LAB2XYZ(inkSet.primaries[i].color) / paperColor;

    size_t gridSize = (size_t)pow( gridPoints, 3 );

    size_t gridCount = gridSize;
    std::unique_ptr<float> gridBuffer(new float[ (size_t)gridCount * (size_t)inkCount ]);
    float *gridData = gridBuffer.get();

    size_t planeStep = (size_t)gridPoints * (size_t)gridPoints * (size_t)inkCount;
    size_t rowStep = (size_t)gridPoints * (size_t)inkCount;
    size_t colStep = (size_t)inkCount;

#if !defined(NDEBUG)
    // zero the table, just in case
    // can probably remove this after debugging
    memset(gridData,0,(size_t)gridCount*(size_t)inkCount*sizeof(float));
#endif

    std::vector<float> inkWeights( (size_t)inkCount );
    std::vector<float> inkWeights2( (size_t)inkCount );
    std::vector<float> neutralWeights( (size_t)inkCount );
    std::vector<splineHuePair> splineHueAngles( inkSet.splines.size() );
    
    for (size_t L = 0; L < gridPoints; ++L) {
        // setup slices variables
        float Lfloat = grid_to_L( L, gridPoints );
        
        //special case less < darkest and > brightest
        if ( Lfloat <= inkSet.darkColor.L) {    // fill with darkest = all inks
            for (size_t A = 0; A < gridPoints; ++A)
                for (size_t B = 0; B < gridPoints; ++B) {
                    for (size_t c = 0; c < inkCount; ++c)
                        gridData[ L * planeStep + A * rowStep + B*colStep + c ] = 1.0;
                }
            
            continue;
        }
        if ( Lfloat >= inkSet.paperColor.L) {   // fill with paper = no ink
            for (size_t A = 0; A < gridPoints; ++A)
                for (size_t B = 0; B < gridPoints; ++B) {
                    for (size_t c = 0; c < inkCount; ++c)
                        gridData[ L * planeStep + A * rowStep + B*colStep + c ] = 0.0;
                }
            
            continue;
        }
        
        // interpolate dark and paper in L to get neutral mix
        float tNeutral = (Lfloat - inkSet.darkColor.L) / (inkSet.paperColor.L - inkSet.darkColor.L);
        labColor neutral = interp2inks( tNeutral, inkSet.darkColor, inkSet.paperColor );
        // neutral.L should be close to Lfloat/100
// TODO - check this, create fractions, adjustL if needed!


        // interpolate splines in L to get points along this AB plane
        PointList planeSpline;
        planeSpline.reserve( inkSet.splines.size() );
        for ( const auto &oneSpline: inkSet.splines ) {
            float A1, B1;
            SearchSpline( oneSpline, Lfloat, A1, B1 );
            planeSpline.push_back( Point( A1, B1 ) );
        }
        
        // create hue angles from points in this plane
        for (size_t i = 0; i < planeSpline.size(); ++i) {
            float hue = (float)(M_PI + atan2f( planeSpline[i].a - neutral.A, planeSpline[i].b - neutral.B ));
            splineHueAngles[i] = { hue, i };
        }
        
        std::sort( splineHueAngles.begin(), splineHueAngles.end(), splineHueIndexLess );
        
        
        // create interpolated point list from the splines
        PointList planePoints;
        size_t subDivisions = std::min( (size_t)300, 50*(size_t)inkCount );
        PointListFromSplines( subDivisions, planeSpline, planePoints, (inkCount > 2) );

// DEBUG the last set generated to check the gamut shape and area
//DumpPointList( std::string("pointlist_") + std::to_string(L), planePoints );

        spline_mix_data mixPoints;
        MixPointsFromSplines( subDivisions, inkSet.mixData, mixPoints, (inkCount > 2) );
      
        // make sure mixPoints and planePoints have the same size!
        assert( planePoints.size() == mixPoints.size() );


        // now iterate over this plane/slice
        for (size_t A = 0; A < gridPoints; ++A) {
            float Afloat = grid_to_AB( A, gridPoints );
            
            for (size_t B = 0; B < gridPoints; ++B) {
                float Bfloat = grid_to_AB( B, gridPoints );

                // find closest point in our line/point list
                Point thisSpot( Afloat, Bfloat );

                if (inkCount <= 2) {
                    // use closest point outside or for 1 or 2 inks
                    size_t closestIndex = FindClosestPointInList( planePoints, thisSpot );
                    //Point closestPoint = planePoints[ closestIndex ];
                    inkMixPair resultMix = mixPoints[ closestIndex ];

                    std::fill( inkWeights.begin(), inkWeights.end(), 0 );
                    inkWeights[ resultMix.inkIndex1 ] += resultMix.ink1Fraction;
                    inkWeights[ resultMix.inkIndex2 ] += resultMix.ink2Fraction;
                    // now we have full saturation ink mix for this location
                    
                    // in practice, we have some values > 1.0 that need to be scaled back
                    inkWeights = SaturateInkWeights( inkWeights, inkCount );
                }
                
                // for 3 or more inks, test for inside polygon, interpolate inside
                if (inkCount > 2) {
                    //bool inside = pointInPoly( planePoints, thisSpot );

                    if (true) {
                        // this we want relative to our splines, so offset by our neutral axis
                        float thisHue = (float)(M_PI + atan2f( Afloat - neutral.A, Bfloat - neutral.B ));

                        // find bounding hue angles (and handle wrap around!)
                        // FYI - lower and upper bound reverse the arguments to less()
                        auto found = std::lower_bound( splineHueAngles.begin(), splineHueAngles.end(), thisHue,
                                                [](const splineHuePair &a, float b) { return a.angle < b; } );
                        long index = 0;
                        long index1 = 0;
                        if (found != splineHueAngles.end()) {
                            index = (long)( found - splineHueAngles.begin() );
                            index1 = index - 1;
                            
                            if (index1 < 0)
                                index1 = (long)splineHueAngles.size() - 1;
                        }
                        else {
                            index = (long)splineHueAngles.size() - 1;
                            index1 = 0;
                        }
                        

assert( index != index1 );
assert( index >= 0 );
assert( index1 >= 0 );
assert( index < (long)splineHueAngles.size() );
assert( index1 < (long)splineHueAngles.size() );

                        // interpolate to get primary ink mix
                        float angle1 = splineHueAngles[(size_t)index].angle;
                        float angle2 = splineHueAngles[(size_t)index1].angle;
                        
                        // and look up the indices for the mixData based on the hue angle indices
                        size_t mixIndex = splineHueAngles[(size_t)index].index;
                        size_t mixIndex1 = splineHueAngles[(size_t)index1].index;
assert( mixIndex != mixIndex1 );
assert( mixIndex < inkSet.mixData.size() );
assert( mixIndex1 < inkSet.mixData.size() );

                        
                        if (thisHue > angle1) {
                            angle2 += 2.0f*(float)M_PI;
                        }
                        else if (angle2 > angle1)
                            angle2 -= 2.0f*(float)M_PI;
                        
                        if (angle2 > angle1) {
                            std::swap(angle2,angle1);
                            std::swap(mixIndex,mixIndex1);
                        }
                        
                        if (thisHue < angle2)
                            thisHue += 2.0f*(float)M_PI;

assert(angle2 <= angle1);
                        float tempDist = angle1 - angle2;
                        float hueFraction = (thisHue - angle2);
assert(hueFraction >= 0.0);
                        if (fabsf(tempDist) < 1e-6)
                            hueFraction = 0.0f;
                        else
                            hueFraction /= tempDist;
assert(hueFraction <= 1.0);
assert(hueFraction >= 0.0);
                        
                        if (hueFraction < 0.0f)
                            hueFraction = 0.0f;
                        if (hueFraction > 1.0f)
                            hueFraction = 1.0f;

                        std::fill( inkWeights.begin(), inkWeights.end(), 0 );
                        std::fill( inkWeights2.begin(), inkWeights2.end(), 0 );
                        
                        inkWeights[ inkSet.mixData[(size_t)mixIndex].inkIndex1 ] += inkSet.mixData[(size_t)mixIndex].ink1Fraction;
                        inkWeights[ inkSet.mixData[(size_t)mixIndex].inkIndex2 ] += inkSet.mixData[(size_t)mixIndex].ink2Fraction;
                        
                        inkWeights2[ inkSet.mixData[(size_t)mixIndex1].inkIndex1 ] += inkSet.mixData[(size_t)mixIndex1].ink1Fraction;
                        inkWeights2[ inkSet.mixData[(size_t)mixIndex1].inkIndex2 ] += inkSet.mixData[(size_t)mixIndex1].ink2Fraction;
                        
                        // interpolate inks
                        inkWeights = MixInkWeights( hueFraction, inkWeights2, inkWeights, inkCount );

                        // use ratio of distances from neutral and outer point as chroma estimate
                        Point mixedAB;
                        mixedAB.a = LERP( hueFraction, planeSpline[(size_t)mixIndex1].a, planeSpline[(size_t)mixIndex].a );
                        mixedAB.b = LERP( hueFraction, planeSpline[(size_t)mixIndex1].b, planeSpline[(size_t)mixIndex].b );
                        float closestDist = hypotf( mixedAB.a - neutral.A, mixedAB.b - neutral.B );
                        float thisDist = hypotf( Afloat - neutral.A, Bfloat - neutral.B );

                        float tchroma = (closestDist > 1e-10f) ? (thisDist / closestDist) : 0.0f;
                        if (tchroma > 1.0f)  // clamp colors outside of gamut
                            tchroma = 1.0f;
assert( tchroma >= 0.0 );

// this doesn't seem to have an effect here
//                        inkWeights = SaturateInkWeights( inkWeights, inkCount );

                        // scale from full inks to neutral  (aka: interp to no ink)
                        inkWeights = ScaleInkWeights( tchroma, inkWeights, inkCount );

                    }   // inside
                }

                // adjust L* for all ink mixes
                AdjustInkMixForL( inkSet, Lfloat, inkListXYZ, inkWeights, paperColor, inkCount );
                
                // write values to the grid
                for (size_t c = 0; c < inkCount; ++c)
                    gridData[ L * planeStep + A * rowStep + B*colStep + c ] = inkWeights[c];
                
            }   // end for B
        }   // end for A
    }   // end for L




    // smooth the floating point table
    SmoothOneDirection( gridData, (size_t)gridPoints, planeStep, rowStep, colStep, inkCount );
    SmoothOneDirection( gridData, (size_t)gridPoints, rowStep, colStep, planeStep, inkCount );
    SmoothOneDirection( gridData, (size_t)gridPoints, colStep, planeStep, rowStep, inkCount );


    // convert the float table to integer
    assert( depth == 8 || depth == 16 );
    std::unique_ptr<uint8_t> outBuffer(new uint8_t[ (size_t)gridCount * (size_t)inkCount * ((size_t)depth/8) ]);
    uint8_t *outData = outBuffer.get();
    uint16_t *out16Ptr = (uint16_t*)outData;

    if ( globalSettings.gTIFFTables ) {
        // order the data for easy viewing as an image
        uint8_t *tifPtr = outData;
        for (size_t B = 0; B < gridPoints; ++B) {
            size_t Bindex = ((size_t)gridPoints-1 - B);    // need to flip B
            for (size_t L = 0; L < gridPoints; ++L) {
                for (size_t A = 0; A < gridPoints; ++A) {
                    for (size_t c = 0; c < inkCount; ++c) {
                        tifPtr[c] = float_to_file255( gridData[ L * planeStep + A * rowStep + Bindex*colStep + c ] );
                    }
                    tifPtr += inkCount;
                }
            }
        }
        
        // write TIFF File
        int mode = (inkCount < 4) ? TIFF_MODE_GRAY_WHITEZERO : TIFF_MODE_CMYK;
        WriteTIFF( inkSet.name + "_B2A.tiff", 96.0, mode, outBuffer.get(),
                    (size_t)gridPoints*(size_t)gridPoints, (size_t)gridPoints, (int)inkCount, 8 );
    }


    // oganize data for ICC profile
    for (size_t L = 0; L < gridPoints; ++L) {
        for (size_t A = 0; A < gridPoints; ++A) {
            for (size_t B = 0; B < gridPoints; ++B) {
                for (size_t c = 0; c < inkCount; ++c) {
                    if (depth == 16)
                        out16Ptr[c] = float_to_file65535( gridData[ L * planeStep + A * rowStep + B*colStep + c ] );
                    else
                        outData[c] = float_to_file255( gridData[ L * planeStep + A * rowStep + B*colStep + c ] );
                }
                outData += inkCount;
                out16Ptr += inkCount;
            }
        }
    }


    tableFormat myTable;
    myTable.tableSig = icSigBToA0Tag;
    myTable.tableDepth = depth;
    myTable.tableGridPoints = gridPoints;
    myTable.tableDimensions = 3;                // input
    myTable.tableChannels = (int)inkCount;      // output
    myTable.tableData = std::move(outBuffer);
    myProfile.LUTtables.emplace_back(myTable);
}

/********************************************************************************/

// create LAB to LAB for color mapping/preview
// similar to B2A, but with more exaggerated dark point, and just LAB values
static
void create_abstract_profile( const inkColorSet &inkSet, int depth, size_t gridPoints,
                    const std::string &filename )
{
    size_t A, B;    // my grid iteration indices

    // allocate my grid
    size_t gridCount =  (size_t)gridPoints * (size_t)gridPoints * (size_t)gridPoints;
    std::unique_ptr<float> gridBuffer(new float[ gridCount * 3 ]);
    float *gridData = gridBuffer.get();
    

    size_t planeStep = (size_t)gridPoints * (size_t)gridPoints * 3;
    size_t rowStep = (size_t)gridPoints * 3;
    size_t colStep = 3;
    
    size_t inkCount = inkSet.primaries.size();
    assert(inkCount > 0 && inkCount <= kMaxChannels );
    
    std::vector<splineHuePair> splineHueAngles( inkSet.splines.size() );
    
    for (size_t L = 0; L < gridPoints; ++L) {
        // setup slices variables
        float Lfloat = grid_to_L( L, gridPoints );
        
        //special case less < darkest and > brightest
        labColor clippedColor;
        if (ClippedL( Lfloat, clippedColor, inkSet)) {
            // fill with clipped value
            for (A = 0; A < gridPoints; ++A)
                for (B = 0; B < gridPoints; ++B) {
                    // save the values
                    gridData[ L * planeStep + A * rowStep + B*colStep + 0 ] = clippedColor.L;
                    gridData[ L * planeStep + A * rowStep + B*colStep + 1 ] = clippedColor.A;
                    gridData[ L * planeStep + A * rowStep + B*colStep + 2 ] = clippedColor.B;
                }
            
            continue;
        }   // end ClippedL
        
        // interpolate dark and paper in L to get neutral mix
        float tNeutral = (Lfloat - inkSet.darkColor.L) / (inkSet.paperColor.L - inkSet.darkColor.L);
        labColor neutral = interp2inks( tNeutral, inkSet.darkColor, inkSet.paperColor );
        
        // interpolate splines in L to get points along this AB plane
        PointList planeSpline;
        for ( const auto &oneSpline: inkSet.splines ) {
            float A1, B1;
            SearchSpline( oneSpline, Lfloat, A1, B1 );
            planeSpline.push_back( Point( A1, B1 ) );
        }
        
        // create hue angles from points in this plane
        for (size_t i = 0; i < planeSpline.size(); ++i) {
            float hue = (float)(M_PI + atan2f( planeSpline[i].a - neutral.A, planeSpline[i].b - neutral.B ));
            splineHueAngles[i] = { hue, i };
        }
        
        std::sort( splineHueAngles.begin(), splineHueAngles.end(), splineHueIndexLess );
        
        // create interpolated point list from the splines
        PointList planePoints;
        size_t subDivisions = std::min( (size_t)300, 50*inkCount );
        PointListFromSplines( subDivisions, planeSpline, planePoints, (inkCount > 2) );

// DEBUG the last set generated to check the gamut shape and area
//DumpPointList( std::string("pointSplines_") + std::to_string(L), planeSpline );
//DumpPointList( std::string("pointlist_") + std::to_string(L), planePoints );

        spline_mix_data mixPoints;
        MixPointsFromSplines( subDivisions, inkSet.mixData, mixPoints, (inkCount > 2) );
      
        // make sure mixPoints and planePoints have the same size!
        assert( planePoints.size() == mixPoints.size() );


        // now iterate over this plane/slice
        for (A = 0; A < gridPoints; ++A) {
            float Afloat = grid_to_AB( A, gridPoints );
            
            for (B = 0; B < gridPoints; ++B) {
                float Bfloat = grid_to_AB( B, gridPoints );

                // find closest point in our line/point list
                Point thisSpot( Afloat, Bfloat );
                
                Point result;

                if (inkCount <= 2) {
                // use closest point outside or for 1 or 2 inks
                    size_t closestIndex = FindClosestPointInList( planePoints, thisSpot );
                    result = planePoints[ closestIndex ];
                }
                
                // for 3 or more inks, test for inside polygon, interpolate inside
                if (inkCount > 2) {
#if 0
// old, not very accuate for > 2 inks
                    bool inside = pointInPoly( planePoints, thisSpot );
                    if (inside)
                        result = thisSpot;
#else
                    // this we want relative to our splines, so offset by our neutral axis
                    float thisHue = (float)(M_PI + atan2f( Afloat - neutral.A, Bfloat - neutral.B ));

                    // find bounding hue angles (and handle wrap around!)
                    // FYI - lower and upper bound reverse the arguments to less()
                    auto found = std::lower_bound( splineHueAngles.begin(), splineHueAngles.end(), thisHue,
                                            [](const splineHuePair &a, float b) { return a.angle < b; } );
                    long index = 0;
                    long index1 = 0;
                    if (found != splineHueAngles.end()) {
                        index = (long)( found - splineHueAngles.begin() );
                        index1 = index - 1;
                        
                        if (index1 < 0)
                            index1 = (long)splineHueAngles.size() - 1;
                    }
                    else {
                        index = (long)splineHueAngles.size() - 1;
                        index1 = 0;
                    }
                    
assert( index != index1 );
assert( index >= 0 );
assert( index1 >= 0 );
assert( index < (long)splineHueAngles.size() );
assert( index1 < (long)splineHueAngles.size() );

                    // interpolate to get primary ink mix
                    float angle1 = splineHueAngles[(size_t)index].angle;
                    float angle2 = splineHueAngles[(size_t)index1].angle;
                    
                    // and look up the indices for the mixData based on the hue angle indices
                    size_t mixIndex = splineHueAngles[(size_t)index].index;
                    size_t mixIndex1 = splineHueAngles[(size_t)index1].index;
assert( mixIndex != mixIndex1 );
assert( mixIndex < inkSet.mixData.size() );
assert( mixIndex1 < inkSet.mixData.size() );

                    
                    if (thisHue > angle1) {
                        angle2 += 2.0f*(float)M_PI;
                    }
                    else if (angle2 > angle1)
                        angle2 -= 2.0f*(float)M_PI;
                    
                    if (angle2 > angle1) {
                        std::swap(angle2,angle1);
                        std::swap(mixIndex,mixIndex1);
                    }
                    
                    if (thisHue < angle2)
                        thisHue += 2.0f*(float)M_PI;

assert(angle2 <= angle1);
                    float tempDist = angle1 - angle2;
                    float hueFraction = (thisHue - angle2);
assert(hueFraction >= 0.0);
                    if (fabsf(tempDist) < 1e-6)
                        hueFraction = 0.0f;
                    else
                        hueFraction /= tempDist;
assert(hueFraction <= 1.0);
assert(hueFraction >= 0.0);
                    
                    if (hueFraction < 0.0f)
                        hueFraction = 0.0f;
                    if (hueFraction > 1.0f)
                        hueFraction = 1.0f;
                    
                    // interpolate spline points in LAB
                    Point sample1 = planeSpline[ mixIndex ];
                    Point sample2 = planeSpline[ mixIndex1 ];
                    result.a = LERP( hueFraction, sample2.a, sample1.a );
                    result.b = LERP( hueFraction, sample2.b, sample1.b );
                    
                    // use ratio of distances from neutral and outer point as chroma estimate
                    Point mixedAB;
                    mixedAB.a = LERP( hueFraction, planeSpline[mixIndex1].a, planeSpline[mixIndex].a );
                    mixedAB.b = LERP( hueFraction, planeSpline[mixIndex1].b, planeSpline[mixIndex].b );
                    float closestDist = hypotf( mixedAB.a - neutral.A, mixedAB.b - neutral.B );
                    float thisDist = hypotf( Afloat - neutral.A, Bfloat - neutral.B );

                    float tchroma = (closestDist > 1e-10f) ? (thisDist / closestDist) : 0.0f;
                    if (tchroma > 1.0f)  // clamp colors outside of gamut
                        tchroma = 1.0f;
                    
                    // scale full saturation result to neutral
                    result.a = LERP( tchroma, neutral.A, result.a );
                    result.b = LERP( tchroma, neutral.B, result.b );
#endif
                }
                
                // save the values
                gridData[ L * planeStep + A * rowStep + B*colStep + 0 ] = Lfloat;
                gridData[ L * planeStep + A * rowStep + B*colStep + 1 ] = result.a;
                gridData[ L * planeStep + A * rowStep + B*colStep + 2 ] = result.b;
                
                }   // end for B
            }   // end for A
        }   // end for L

    
    // smooth the 3D table data
    SmoothOneDirection( gridData, (size_t)gridPoints, planeStep, rowStep, colStep, 3 );
    SmoothOneDirection( gridData, (size_t)gridPoints, rowStep, colStep, planeStep, 3 );
    SmoothOneDirection( gridData, (size_t)gridPoints, colStep, planeStep, rowStep, 3 );
    

    assert( depth == 8 || depth == 16 );
    size_t bufferSize = (size_t)gridPoints * (size_t)gridPoints * (size_t)gridPoints * 3 * ((size_t)depth/8);
    std::unique_ptr<uint8_t> outBuffer(new uint8_t[ bufferSize ]);
    uint8_t *outPtr = outBuffer.get();
    uint16_t *out16Ptr = (uint16_t*)outPtr;

    if (globalSettings.gTIFFTables) {
        // order the data for easy viewing as an image
        uint8_t *tifPtr = outPtr;
        for (B = 0; B < gridPoints; ++B) {
            size_t Bindex = ((size_t)gridPoints-1 - B);    // need to flip B
            for (size_t L = 0; L < gridPoints; ++L) {
                for (A = 0; A < gridPoints; ++A) {

                    // convert to integer output values
                    int Lout =   floatL_to_fileL8( gridData[ L * planeStep + A * rowStep + Bindex*colStep + 0 ] );
                    int Aout = floatAB_to_fileAB8( gridData[ L * planeStep + A * rowStep + Bindex*colStep + 1 ] );
                    int Bout = floatAB_to_fileAB8( gridData[ L * planeStep + A * rowStep + Bindex*colStep + 2 ] );
                    
                    // write value out to file (interleaved)
                    tifPtr[0] = (uint8_t)Lout;
                    tifPtr[1] = (uint8_t)Aout;
                    tifPtr[2] = (uint8_t)Bout;
                    tifPtr += 3;
                }
            }
        }
        
        // write TIFF File
        WriteTIFF( filename + "_abstract.tiff", 96.0, TIFF_MODE_CIELAB, outBuffer.get(),
                    (size_t)gridPoints * (size_t)gridPoints, (size_t)gridPoints, 3, 8 );
    }

    // oganize data for ICC profile
    for (size_t L = 0; L < gridPoints; ++L) {
        for (A = 0; A < gridPoints; ++A) {
            for (B = 0; B < gridPoints; ++B) {

                if (depth == 16) {
                    // convert to integer output values
                    int Lout =   floatL_to_fileL16( gridData[ L * planeStep + A * rowStep + B*colStep + 0 ] );
                    int Aout = floatAB_to_fileAB16( gridData[ L * planeStep + A * rowStep + B*colStep + 1 ] );
                    int Bout = floatAB_to_fileAB16( gridData[ L * planeStep + A * rowStep + B*colStep + 2 ] );
                    
                    // write value out to file (interleaved)
                    out16Ptr[0] = (uint16_t)Lout;
                    out16Ptr[1] = (uint16_t)Aout;
                    out16Ptr[2] = (uint16_t)Bout;
                    out16Ptr += 3;
                
                } else {
                    // depth == 8
                    // convert to integer output values
                    int Lout =   floatL_to_fileL8( gridData[ L * planeStep + A * rowStep + B*colStep + 0 ] );
                    int Aout = floatAB_to_fileAB8( gridData[ L * planeStep + A * rowStep + B*colStep + 1 ] );
                    int Bout = floatAB_to_fileAB8( gridData[ L * planeStep + A * rowStep + B*colStep + 2 ] );
                    
                    // write value out to file (interleaved)
                    outPtr[0] = (uint8_t)Lout;
                    outPtr[1] = (uint8_t)Aout;
                    outPtr[2] = (uint8_t)Bout;
                    outPtr += 3;
                }
            }
        }
    }


    // write ICC abstract profiles
    profileData myProfile;
    myProfile.description = inkSet.description;
    myProfile.copyright = inkSet.copyright;
    myProfile.profileClass = kClassAbstract;
    myProfile.colorSpace = kSpaceLAB;
    myProfile.pcsSpace = kSpaceLAB;
    myProfile.preferredCMM = icSigIccDEV;
    myProfile.platform = icSigMacintosh;
    myProfile.manufacturer = icSigNone;
    myProfile.creator = icSigccox;
    myProfile.profileFormats = globalSettings.gProfileTypes;

    tableFormat myTable;
    myTable.tableSig = icSigAToB0Tag;
    myTable.tableDepth = depth;
    myTable.tableGridPoints = gridPoints;
    myTable.tableDimensions = 3;    // input
    myTable.tableChannels = 3;      // output
    myTable.tableData = std::move(outBuffer);
    myProfile.LUTtables.emplace_back(myTable);

    writeICCProfile( filename+"_abstract", myProfile );
    
    // buffers are freed automatically
}

/********************************************************************************/

// full output profile: A2B, B2A, gamut
static
void create_output_profile( const inkColorSet &inkSet, int depth, size_t gridPoints,
                    const std::string &filename, size_t tableSizeLimit )
{
    size_t inkCount = inkSet.primaries.size();
    assert(inkCount > 0 && inkCount <= kMaxChannels);


    // write ICC output profiles
    profileData myProfile;
    myProfile.description = inkSet.description;
    myProfile.copyright = inkSet.copyright;
    myProfile.profileClass = kClassOutput;
    myProfile.colorSpace = profileSpaceLookup[ inkCount ];
    myProfile.pcsSpace = kSpaceLAB;
    myProfile.preferredCMM = icSigIccDEV;
    myProfile.platform = icSigMacintosh;
    myProfile.manufacturer = icSigNone;
    myProfile.creator = icSigccox;
    myProfile.profileFormats = globalSettings.gProfileTypes;

    // make A2B0 (ink to LAB) - determines grid size internally
    createA2B_table( inkSet, depth, myProfile, tableSizeLimit );

    // make B2A0 (LAB to ink)
    createB2A_table( inkSet, depth, gridPoints, myProfile );

    // make gamut (LAB to bool)
    createGamut_table( inkSet, depth, gridPoints, myProfile );
    
    
    // and point the other A2B tables back to A2B0
    tableFormat myFake;
    myFake.tableSig = icSigAToB1Tag;
    myFake.pointsBackTo = icSigAToB0Tag;
    myProfile.LUTtables.emplace_back(myFake);

    myFake.tableSig = icSigAToB2Tag;
    myFake.pointsBackTo = icSigAToB0Tag;
    myProfile.LUTtables.emplace_back(myFake);

    // and point the other B2A tables back to B2A0
    myFake.tableSig = icSigBToA1Tag;
    myFake.pointsBackTo = icSigBToA0Tag;
    myProfile.LUTtables.emplace_back(myFake);
    
    myFake.tableSig = icSigBToA2Tag;
    myFake.pointsBackTo = icSigBToA0Tag;
    myProfile.LUTtables.emplace_back(myFake);

    // colorant table, showing order, names, and LAB values
    colorantTableFormat clrTable;
    clrTable.tableSig = icSigColorantTableTag;
    clrTable.colorants.resize(inkCount);
    for (size_t i = 0; i < inkCount; ++i) {
        namedICCLABFloat temp;
        temp.name = inkSet.primaries[i].name;
        temp.L = inkSet.primaries[i].color.L;
        temp.a = inkSet.primaries[i].color.A;
        temp.b = inkSet.primaries[i].color.B;
        clrTable.colorants[i] = temp;
    }
    myProfile.colorantTables.emplace_back(clrTable);


    writeICCProfile( filename+"_output", myProfile );
    
    
    // buffers are freed automatically
}

/********************************************************************************/

// create utility mappings for an inkSet
// and do a bit more error checking if overprints are present
static
int create_utility_maps( inkColorSet &inkSet )
{
    // Need inks in hue angle order so the splines will be in order for hull
    // and we need the indices to remain fixed after this
    std::sort( inkSet.primaries.begin(), inkSet.primaries.end(), labHueLess );
    
    // create map of primary names -> indices
    for (size_t index = 0; index< inkSet.primaries.size(); ++index) {
        const auto &ink = inkSet.primaries[index];
        inkSet.name_map.emplace( ink.name, index );
    }
    
    float lightL = inkSet.paperColor.L;
    float darkL = inkSet.darkColor.L;
    xyzColor paperColor = LAB2XYZ( inkSet.paperColor );
    
    // check overprints and make sure the ink names are in our map
    // construct ink bitmaps, add to our bitmap->index list
    for (size_t index = 0; index< inkSet.overprints.size(); ++index) {
        auto &op = inkSet.overprints[index];
        op.inkBitmap = 0;
        
        float L = op.color.L;
        float A = op.color.A;
        float B = op.color.B;
    
        if (L > lightL) {
            fprintf(stderr,"ERROR - Overprint %zu is lighter than the paper in set %s\n",
                        index, inkSet.name.c_str() );
            return 1;
        }
        if (L < darkL) {
            fprintf(stderr,"ERROR - Overprint %zu is darker than the dark in set %s\n",
                        index, inkSet.name.c_str() );
            return 1;
        }
        
        // check for NaN and Inf, just in case
        if (std::isnan(L) || std::isnan(A) || std::isnan(B)) {
            fprintf(stderr,"ERROR - What color is NaN in overprint %zu in set %s?\n",
                        index, inkSet.name.c_str() );
            return 1;
        }
        if (std::isinf(L) || std::isinf(A) || std::isinf(B)) {
            fprintf(stderr,"ERROR - What color is Infinity in overprint %zu in set %s?\n",
                        index, inkSet.name.c_str() );
            return 1;
        }
        
        
        for (const auto &name: op.inkNames ) {
            if (name.empty()) {
                fprintf(stderr,"ERROR - Empty Overprint name in set %s?\n",
                        inkSet.name.c_str() );
                return 1;
            }
            
            auto iter = inkSet.name_map.find(name);
            if ( iter == inkSet.name_map.end() ) {
                fprintf(stderr,"ERROR - Unknown overprint ink name %s, in set %s\n",
                    name.c_str(),
                    inkSet.description.c_str() );
                return 1;
            }
            int colorIndex = iter->second;
            op.inkBitmap |= ( 1UL << colorIndex );
        }
        
        op.colorXYZ = LAB2XYZ(op.color) / paperColor;
        inkSet.overprint_bitmask_map.emplace( op.inkBitmap, index );
    }

    return 0;
}

/******************************************************************************/
/******************************************************************************/

// isolate this so we can change globals per json file
void processInkSetList(void)
{
    // iterate over each named set of inks
    for (auto &inkSet : globalSettings.colorSets) {
        
        if (inkSet.copyright.empty())
            inkSet.copyright = globalSettings.gDefaultCopyright;
        
        // first, check for errors
        if (inkSet.name.empty() ) {
            fprintf(stderr,"ERROR - There is an inkset without a filename, description is %s\n", inkSet.description.c_str() );
            continue;
        }
        if (inkSet.description.empty() ) {
            fprintf(stderr,"ERROR - There is no description for set %s\n", inkSet.name.c_str() );
            continue;
        }
        
        // ink count must be >=1 && <= kMaxChannels
        size_t inkCount = inkSet.primaries.size();
        if (inkCount < 1 ) {
            fprintf(stderr,"ERROR - There are no inks defined in set %s\n", inkSet.name.c_str() );
            continue;
        }
        if (inkCount > kMaxChannels) {
            fprintf(stderr,"ERROR - There are more than %d inks in set %s\n", kMaxChannels, inkSet.name.c_str() );
            continue;
        }
        
        // calc dark if needed
        prepare_ink_dark( inkSet );
        
        // no ink can be lighter than the paper color
        // no ink can be darker than the dark color
        float lightL = inkSet.paperColor.L;
        float darkL = inkSet.darkColor.L;
        
        if (lightL > 100.0) {
            fprintf(stderr,"ERROR - Paper color is brighter than white in set %s\n", inkSet.name.c_str() );
            continue;
        }
        if (lightL < 0.0) {
            fprintf(stderr,"ERROR - Paper color is darker than black in set %s\n", inkSet.name.c_str() );
            continue;
        }
        if (darkL < 0.0) {
            fprintf(stderr,"ERROR - Dark color is darker than black in set %s\n", inkSet.name.c_str() );
            continue;
        }
        if (lightL < darkL) {
            fprintf(stderr,"ERROR - Paper is darker than the darkest ink combination in set %s\n", inkSet.name.c_str() );
            continue;
        }
        
        bool fail = false;
        int inkNameCounter = 1;
        for (auto &ink : inkSet.primaries ) {
            float L = ink.color.L;
            float A = ink.color.A;
            float B = ink.color.B;
            
            if (ink.name.empty()) {
                fprintf(stderr,"WARNING - A blank ink name really isn't a good idea in set %s\n", inkSet.name.c_str() );
                ink.name = std::string("unnamed") + std::to_string(inkNameCounter);
                inkNameCounter++;
            }
            
            if (L > lightL) {
                fprintf(stderr,"ERROR - Ink %s is lighter than the paper in set %s\n",
                            ink.name.c_str(), inkSet.name.c_str() );
                fail = true;
                break;
            }
            if (L < darkL) {
                fprintf(stderr,"ERROR - Ink %s is darker than the dark in set %s\n",
                            ink.name.c_str(), inkSet.name.c_str() );
                fail = true;
                break;
            }
            
            // check for NaN and Inf, just in case
            if (std::isnan(L) || std::isnan(A) || std::isnan(B)) {
                fprintf(stderr,"ERROR - What color is NaN %s in set %s?\n",
                            ink.name.c_str(), inkSet.name.c_str() );
                fail = true;
                break;
            }
            if (std::isinf(L) || std::isinf(A) || std::isinf(B)) {
                fprintf(stderr,"ERROR - What color is Infinity %s in set %s?\n",
                            ink.name.c_str(), inkSet.name.c_str() );
                fail = true;
                break;
            }
        }   // end ink check loop

        // if errors are bad, then continue to the next set
        if (fail)
            continue;
        
        // create utility mappings and do more error checking
        if ( create_utility_maps( inkSet ) != 0 )
            continue;
        

        printf("Processing set %s\n", inkSet.name.c_str() );
        
        try {
            // create splines from measured points using approximate mixing model
            mix_ink_splines( inkSet );
            
            // create output files from the splines and measured data
            if (globalSettings.gCreateOutput)
                create_output_profile( inkSet, globalSettings.gDataDepth,
                                    (size_t)globalSettings.gDataGridPoints, inkSet.name,
                                    globalSettings.gTableSizeLimit );
            
            if (globalSettings.gCreateAbstract)
                create_abstract_profile( inkSet, globalSettings.gDataDepth,
                                (size_t)globalSettings.gDataGridPoints, inkSet.name );
        }
        catch (const std::exception& e) {
          fprintf(stderr, "error in set '%s': %s\n", inkSet.name.c_str(), e.what() );
        }
        catch (...) {
          fprintf(stderr, "Unknown exception in set '%s'\n", inkSet.name.c_str() );
        }
    
     }  // end for colorSets

}

/******************************************************************************/

settings_spec globalSettings;

/******************************************************************************/

int main (int argc, char * argv[])
{
    // settings to default
    defaultSettings( globalSettings );

    // handle our command line arguments
    // build list of json filenames from arguments
    auto filenames = parse_arguments( argc, argv );
    
    // load inksets from json files
    // and process the json files
    process_json_filelist( filenames );

    // and we're all done
    return 0;
}

/********************************************************************************/
/********************************************************************************/
