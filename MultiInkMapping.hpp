//
//  MultiInkMapping.hpp
//  MultiInkMapping
//  MIT License, Copyright (C) Chris Cox 2026
//
//  Created by Chris Cox on March 18, 2026.
//

#ifndef MultiInkMapping_h
#define MultiInkMapping_h

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
#include "MiniICC.hpp"

/******************************************************************************/

const char kVersionString[] = "0.9b";

/******************************************************************************/

struct labColor {
    float L;    // 0...100
    float A;    // +-128.0
    float B;
    
public:
    labColor() {};
    labColor( float l, float a, float b) : L(l), A(a), B(b) {}
};

/******************************************************************************/

struct namedColor {
    std::string name;
    labColor color;
    
public:
    namedColor( const std::string &n, float l, float a, float b) : name(n), color(l,a,b) {}
    namedColor() {} // type must be default constructable for JSON
};

/******************************************************************************/

struct xyzColor {
    float X;    // 0..100
    float Y;
    float Z;
    
public:
    xyzColor() {};
    xyzColor( float x, float y, float z) : X(x), Y(y), Z(z) {}
};

/******************************************************************************/

struct Point {
    float a;    // from Lab for a particular L plane
    float b;

public:
    Point() {};
    Point( float A, float B ) : a(A), b(B) {}
    
    bool operator==(const Point& other) const = default;
};

/******************************************************************************/

struct inkMixPair {

    // some compilers don't get the default initialization correct
    inkMixPair( size_t dx1, size_t dx2, float f1, float f2 ) : inkIndex1(dx1), inkIndex2(dx2),
        ink1Fraction(f1), ink2Fraction(f2) {}

    size_t inkIndex1;
    size_t inkIndex2;
    float  ink1Fraction;
    float  ink2Fraction;
    
    bool operator==(const inkMixPair& other) const = default;
};

/******************************************************************************/

struct overPrintSwatch {
    labColor color;                         // from JSON
    std::vector< std::string > inkNames;    // from JSON

public:
    uint32_t inkBitmap;                     // filled in by matching names
    xyzColor colorXYZ;                      // filled in after full set read
};

/******************************************************************************/

typedef std::vector< Point > PointList;

typedef std::vector< labColor > color_list;

typedef std::vector< namedColor > named_color_list;

typedef std::vector< color_list > spline_list;

typedef std::vector< inkMixPair > spline_mix_data;

/******************************************************************************/

const xyzColor identityXYZ( 100.0, 100.0, 100.0 );

/******************************************************************************/

inline
float constexpr LERP( const float t, const float x1, const float x2 )
{
    return x1 + t*(x2-x1);
}
 
/********************************************************************************/

inline
xyzColor operator*( const float &scale, const xyzColor &b)
{
    xyzColor result;
    result.X = scale * b.X;
    result.Y = scale * b.Y;
    result.Z = scale * b.Z;
    return result;
}
 
/********************************************************************************/

inline
xyzColor& operator*=( xyzColor &a, float s)
{
    a.X *= s;
    a.Y *= s;
    a.Z *= s;
    return a;
}

/********************************************************************************/

inline
xyzColor operator*( const xyzColor &a, const xyzColor &b)
{
    xyzColor result;
#if 1
    result.X = a.X * b.X * (1.0f / 100.0f);
    result.Y = a.Y * b.Y * (1.0f / 100.0f);
    result.Z = a.Z * b.Z * (1.0f / 100.0f);
#else
    result.X = a.X * b.X / 100.0f;
    result.Y = a.Y * b.Y / 100.0f;
    result.Z = a.Z * b.Z / 100.0f;
#endif
    return result;
}
 
/********************************************************************************/

inline
xyzColor& operator*=( xyzColor &a, const xyzColor &b)
{
#if 1
    a.X = a.X * b.X * (1.0f / 100.0f);
    a.Y = a.Y * b.Y * (1.0f / 100.0f);
    a.Z = a.Z * b.Z * (1.0f / 100.0f);
#else
    a.X = a.X * b.X / 100.0f;
    a.Y = a.Y * b.Y / 100.0f;
    a.Z = a.Z * b.Z / 100.0f;
#endif
    return a;
}
 
/********************************************************************************/

inline
xyzColor operator/( const xyzColor &a, const xyzColor &b)
{
    xyzColor result;
    result.X = 100.0f * a.X / b.X;
    result.Y = 100.0f * a.Y / b.Y;
    result.Z = 100.0f * a.Z / b.Z;
    return result;
}
 
/********************************************************************************/

inline
xyzColor& operator/=( xyzColor &a, const xyzColor &b)
{
    a.X = 100.0f * a.X / b.X;
    a.Y = 100.0f * a.Y / b.Y;
    a.Z = 100.0f * a.Z / b.Z;
    return a;
}
 
/********************************************************************************/

inline
xyzColor& operator/=( xyzColor &a, const float s)
{
    a.X /= s;
    a.Y /= s;
    a.Z /= s;
    return a;
}

/********************************************************************************/

inline
xyzColor operator+( const xyzColor &a, const xyzColor &b)
{
    xyzColor result;
    result.X = a.X + b.X;
    result.Y = a.Y + b.Y;
    result.Z = a.Z + b.Z;
    return result;
}
 
/********************************************************************************/

inline
xyzColor& operator+=( xyzColor &a, const xyzColor &b)
{
    a.X += b.X;
    a.Y += b.Y;
    a.Z += b.Z;
    return a;
}
 
/********************************************************************************/

inline
xyzColor operator-( const xyzColor &a, const xyzColor &b)
{
    xyzColor result;
    result.X = a.X - b.X;
    result.Y = a.Y - b.Y;
    result.Z = a.Z - b.Z;
    return result;
}
 
/********************************************************************************/

inline
xyzColor& operator-=( xyzColor &a, const xyzColor &b)
{
    a.X -= b.X;
    a.Y -= b.Y;
    a.Z -= b.Z;
    return a;
}

/********************************************************************************/

// interpolate between 0 and 100.0
inline
float constexpr grid_to_L( size_t grid_value, size_t gridPoints )
{
    return (100.0f * (float)grid_value) / (float)(gridPoints - 1);
}

// ccox - FIX ME - cheap version for now -- refine if needed
inline
float constexpr grid_to_AB( size_t grid_value, size_t gridPoints )
{
    float middle = 0.5f * gridPoints;
    return (127.0f * ((float)grid_value - middle)) / middle;
}

/********************************************************************************/

// convert 0..100 representation to file representation
inline
int constexpr floatL_to_fileL8( float L )
{
    if (L <= 0.0f) return 0;
    if (L >= 100.0f) return 255;
    return (int)( (255.0f / 100.0f) * L + 0.5f );
}

inline
int constexpr floatAB_to_fileAB8( float A )
{
    if (A > 127.0f) return 255;
    if (A < -128.0f) return 0;
    return (int)( A + 128.0f );
}

inline
uint8_t constexpr float_to_file255( float A )
{
    if (A > 1.0f) return 255;
    if (A < 0.0f) return 0;
    return (uint8_t)( A * 255.0f );
}

inline
uint16_t constexpr float_to_file65535( float A )
{
    if (A > 1.0f) return 65535;
    if (A < 0.0f) return 0;
    return (uint16_t)( A * 65535.0f );
}

/********************************************************************************/

// convert 0..100 representation to file representation
// ICC version 2/4 profile encoding for LAB 16 bit  --- not usable in TIFF
inline
int constexpr floatL_to_fileL16( float L )
{
    if (L <= 0.0f) return 0;
    if (L >= 100.0f) return 65280;
    return (int)( (65280.0f / 100.0f) * L + 0.5f );
}

inline
int constexpr floatAB_to_fileAB16( float A )
{
    if (A > 127.0f) return 65280;
    if (A < -128.0f) return 0;
    return (int)( A*256.0f + 32768.0f );
}

/********************************************************************************/

const std::vector<color_space> profileSpaceLookup =
{
    kSpace1CLR, // index zero
    kSpace1CLR,
    kSpace2CLR,
    kSpace3CLR,
    kSpace4CLR,
    kSpace5CLR,
    kSpace6CLR,
    kSpace7CLR,
    kSpace8CLR,
    kSpace9CLR,
    kSpaceACLR,
    kSpaceBCLR,
    kSpaceCCLR,
    kSpaceDCLR,
    kSpaceECLR,
    kSpaceFCLR,
};

/******************************************************************************/

struct inkColorSet {
    std::string name;               // what filename to use
    std::string description;        // how to describe this combination
    std::string copyright;          // copyright string for this set
    
    labColor paperColor;            // lightest possible color
    labColor darkColor;             // darkest possible color from combination of inks, calculated if L <= 0
    named_color_list primaries;     // saturated hues
    std::vector< overPrintSwatch > overprints;    // may be empty

public:
    spline_list splines;        // built from basic ink data
    spline_mix_data mixData;    // built from basic ink data
    std::map< std::string, int > name_map;              // built from inks
    std::map< uint32_t, int > overprint_bitmask_map;    // built from inks and overprints
};

/******************************************************************************/

// global variables, just because it was quicker to write it this way
struct settings_spec {

    int gDataDepth;
    int gDataGridPoints;
    size_t gTableSizeLimit;
    bool gDebugMode;
    bool gCreateOutput;
    bool gCreateAbstract;
    bool gTIFFTables;
    std::string gDefaultCopyright;
    uint32_t gProfileTypes;
    
    std::vector<inkColorSet> colorSets;
};

extern settings_spec globalSettings;

/******************************************************************************/

extern void processInkSetList(void);

/******************************************************************************/

// ICC profile v4 limit
const int kMaxChannels = 15;

/******************************************************************************/

#endif /* MultiInkMapping_h */
