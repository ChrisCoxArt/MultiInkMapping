/*
Multi Ink Mapping
Copyright (c) 2026 Chris Cox


Is it accurate? Nope.
    Accuracy would need a lot more measurements, and math, and might not look as good.

Does it look reasonable? Yes.
    And that's all I need from it.

NOTE - This started as a simulation of drawing with inks/watercolors.
    I wanted a simulation of how artists map colors with very limited palettes.
    But available software only handles gray, RGB, or CMYK.
    And random colored inks don't mix like idealized CMY.
    I can always lighten inks/paints by dilution, and put down multiple layers for darks.
    And I've rewritten this a few times as I try new ideas.

This assumes primaries are somewhat saturated, not too neutral, and define a convex hull.
Primaries will be sorted by hue to make sure they are in order to make a convex hull.
This further assumes that the primaries are really transparent, so ink order does not matter. (this is not realistic)




Special case 1 ink -- single spline from paper->ink->dark, take only point
Special case 2 ink -- spline 2D surface, closest point on line
Special case 3..N ink -- create closed shape from multiple surfaces,
            find interpolated inside, project point to closest outside
Always smooth the resulting 3D table



TODO
TODO - violet seems a bit too desaturated

TODO - write XML profile data, once I have A2B and B2A working

TODO - write a makefile, or CMakefile



TODO - would be nice to add measured overprint colors
    need some sort of ink1,ink2 -> overprint mapping.
    { "Ink1", "Ink2", measuredOverprint }
    
    What about tints and shades?  need percentages of mixes, plus measurement.
    Um, special case for "paper" and "dark"?
    { "Ink1", 0.25, "Ink2", 0.75, measuredOverprint }

TODO - allow additional combinations of inks (n+2, n+3, tertiary, etc.)
    take max chroma points for hull?
    or build in more combinations and sort midpoints by hue, before interpolating?

*/

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <cmath>
#include <memory>
#include "MiniTIFF.hpp"
#include "MiniICC.hpp"

/******************************************************************************/

const char kVersionString[] = "0.6a";

/******************************************************************************/

struct labColor {
	float L;
	float A;
	float B;
	
public:
	labColor() {};
	labColor( float l, float a, float b) : L(l), A(a), B(b) {}
};

struct labColorNamed {
    std::string name;
    labColor color;
	
public:
	labColorNamed( const std::string &n, float l, float a, float b) : name(n), color(l,a,b) {}
};

struct xyzColor {
	float X;
	float Y;
	float Z;
	
public:
	xyzColor() {};
	xyzColor( float x, float y, float z) : X(x), Y(y), Z(z) {}
};

struct Point {
	float a;
	float b;

public:
	Point() {};
	Point( float A, float B ) : a(A), b(B) {}
    
    bool operator==(const Point& other) const = default;
};

typedef std::vector< Point > PointList;

typedef std::vector< labColor > color_list;

typedef std::vector< labColorNamed > named_color_list;

typedef std::vector< color_list > spline_list;

/******************************************************************************/

color_list ConvertNamedColorList2ColorList( const named_color_list &input )
{
    color_list result( input.size() );
    for (size_t i = 0; i < input.size(); ++i)
        result[i] = input[i].color;
    return result;
}

/******************************************************************************/

struct inkColorSet {
    std::string name;               // what filename to use
    std::string description;        // how to describe this combination
    
    labColor paperColor;            // lightest possible color
    labColor darkColor;             // darkest possible color from combination of inks, calculated if L <= 0
    named_color_list primaries;     // saturated hues
};

std::vector<inkColorSet> colorSets =
{
// 1
    {   "Turquoise",
        "Turquoise Paint",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5 } }
    },

// 2
    {   "Turquoise-Orange",
        "Turquoise and Orange Paint",
        { 97.12126, -0.024685, 0.025155 },
        { 7.6, 2.5, 0.8 },
        { {"Orange", 62.0, 32, 58.0 }, {"Turquoise", 44.4, -35.9, -32.5 } }
    },

// 3
    {   "Turquoise-Orange-Green",
        "Turquoise, Orange, and Green Paint",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Orange", 62.0, 32, 58.0}, {"Turquoise", 44.4, -35.9, -32.5 },
          { "Green", 71.2, -54.2, 62.9} }
    },

    {   "Turquoise-Magenta-Yellow",
        "Turquoise, Magenta, and Yellow Paint",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}  }
    },

// 4
    {   "Turquoise-Magenta-Yellow-Violet",
        "4 Paints",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}, {"Violet", 51.3, 58.0, -67.0} }
    },

// 5
    {   "Turquoise-Magenta-Yellow-Violet-Green",
        "5 Paints",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}, {"Violet", 51.3, 58.0, -67.0},
          {"Green", 71.2, -54.2, 62.9} }
    },

// 6
    {   "Turquoise-Magenta-Yellow-Violet-Green-Blue",
        "6 Paints",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}, {"Violet", 51.3, 58.0, -67.0},
          {"Green", 71.2, -54.2, 62.9}, {"Blue", 38.2, 2.0, -66.4} }
    },

// 7
    {   "Turquoise-Magenta-Yellow-Violet-Green-Blue-Orange",
        "7 Paints",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}, {"Violet", 51.3, 58.0, -67.0},
          {"Green", 71.2, -54.2, 62.9}, {"Blue", 38.2, 2.0, -66.4},
          {"Orange", 71.0, 50.7, 68.6}  }
    },

// 8
    {   "Turquoise-Magenta-Yellow-Violet-Green-Blue-Orange-BlueGreen",
        "8 Paints",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}, {"Violet", 51.3, 58.0, -67.0},
          {"Green", 71.2, -54.2, 62.9}, {"Blue", 38.2, 2.0, -66.4},
          {"Orange", 71.0, 50.7, 68.6}, {"BlueGreen", 70.9, -60.4, 20.5} }
    },

// 9
    {   "Turquoise-Magenta-Yellow-Violet-Green-Blue-Orange-BlueGreen-PinkViolet",
        "9 Paints",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}, {"Violet", 51.3, 58.0, -67.0},
          {"Green", 71.2, -54.2, 62.9}, {"Blue", 38.2, 2.0, -66.4},
          {"Orange", 71.0, 50.7, 68.6}, {"BlueGreen", 70.9, -60.4, 20.5},
          {"PinkViolet", 67.0, 59.0, -25.0 } }
    },

// A
    {   "Turquoise-Magenta-Yellow-Violet-Green-Blue-Orange-BlueGreen-PinkViolet-Red",
        "10! Ten Paints! Hah, ha, ha!",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}, {"Violet", 51.3, 58.0, -67.0},
          {"Green", 71.2, -54.2, 62.9}, {"Blue", 38.2, 2.0, -66.4},
          {"Orange", 71.0, 50.7, 68.6}, {"BlueGreen", 70.9, -60.4, 20.5},
          {"PinkViolet", 67.0, 59.0, -25.0 }, {"Red", 57.7, 78.1, 48.5} }
    },

// B
    {   "Turquoise-Magenta-Yellow-Violet-Green-Blue-Orange-BlueGreen-PinkViolet-Red-Teal",
        "11 Paints",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}, {"Violet", 51.3, 58.0, -67.0},
          {"Green", 71.2, -54.2, 62.9}, {"Blue", 38.2, 2.0, -66.4},
          {"Orange", 71.0, 50.7, 68.6}, {"BlueGreen", 70.9, -60.4, 20.5},
          {"PinkViolet", 67.0, 59.0, -25.0 }, {"Red", 57.7, 78.1, 48.5},
          {"Teal", 66.8, -51.5, -15.4 } }
    },

// C
    {   "Turquoise-Magenta-Yellow-Violet-Green-Blue-Orange-BlueGreen-PinkViolet-Red-Teal-YellowOrange",
        "12 Paints",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}, {"Violet", 51.3, 58.0, -67.0},
          {"Green", 71.2, -54.2, 62.9}, {"Blue", 38.2, 2.0, -66.4},
          {"Orange", 71.0, 50.7, 68.6}, {"BlueGreen", 70.9, -60.4, 20.5},
          {"PinkViolet", 67.0, 59.0, -25.0 }, {"Red", 57.7, 78.1, 48.5},
          {"Teal", 66.8, -51.5, -15.4 }, {"YellowOrange", 81.5, 27.9, 102.3} }
    },

// D
    {   "Turquoise-Magenta-Yellow-Violet-Green-Blue-Orange-BlueGreen-PinkViolet-Red-Teal-YellowOrange-Cerulean",
        "13 Paints",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}, {"Violet", 51.3, 58.0, -67.0},
          {"Green", 71.2, -54.2, 62.9}, {"Blue", 38.2, 2.0, -66.4},
          {"Orange", 71.0, 50.7, 68.6}, {"BlueGreen", 70.9, -60.4, 20.5},
          {"PinkViolet", 67.0, 59.0, -25.0 }, {"Red", 57.7, 78.1, 48.5},
          {"Teal", 66.8, -51.5, -15.4 }, {"YellowOrange", 81.5, 27.9, 102.3},
          {"Cerulean", 63.3, -16.1, -35.3 } }
    },

// E
    {   "Turquoise-Magenta-Yellow-Violet-Green-Blue-Orange-BlueGreen-PinkViolet-Red-Teal-YellowOrange-Cerulean-GreenGold",
        "14 Paints",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}, {"Violet", 51.3, 58.0, -67.0},
          {"Green", 71.2, -54.2, 62.9}, {"Blue", 38.2, 2.0, -66.4},
          {"Orange", 71.0, 50.7, 68.6}, {"BlueGreen", 70.9, -60.4, 20.5},
          {"PinkViolet", 67.0, 59.0, -25.0 }, {"Red", 57.7, 78.1, 48.5},
          {"Teal", 66.8, -51.5, -15.4 }, {"YellowOrange", 81.5, 27.9, 102.3},
          {"Cerulean", 63.3, -16.1, -35.3 }, {"GreenGold", 57.1, -14.3, 54.0} }
    },

// F
    {   "Turquoise-Magenta-Yellow-Violet-Green-Blue-Orange-BlueGreen-PinkViolet-Red-Teal-YellowOrange-Cerulean-GreenGold-Indigo",
        "15 Paints",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {"Turquoise", 44.4, -35.9, -32.5}, {"Magenta", 52.0, 81.1, -1.7},
          {"Yellow", 90.2, 2.7, 97.7}, {"Violet", 51.3, 58.0, -67.0},
          {"Green", 71.2, -54.2, 62.9}, {"Blue", 38.2, 2.0, -66.4},
          {"Orange", 71.0, 50.7, 68.6}, {"BlueGreen", 70.9, -60.4, 20.5},
          {"PinkViolet", 67.0, 59.0, -25.0 }, {"Red", 57.7, 78.1, 48.5},
          {"Teal", 66.8, -51.5, -15.4 }, {"YellowOrange", 81.5, 27.9, 102.3},
          {"Cerulean", 63.3, -16.1, -35.3 }, {"GreenGold", 57.1, -14.3, 54.0},
          {"Indigo", 31, 35, -68} }
    },

};

/********************************************************************************/

// our global variables, just because it was quicker to write it this way
int gDataDepth = 8;
int gDataGridPoints = 17;

/********************************************************************************/

void VerifyDecreasingL( const color_list &list )
{
// TODO - debugging aid only
#if 1
	size_t count = list.size();
	for (size_t i = 1; i < count; ++i)
		{
		float currentL = list[i].L;
		float previousL = list[i-1].L;
		assert( currentL <= previousL);
		}
#endif
}

/********************************************************************************/

const float XD50 = 96.4212;
const float YD50 = 100.0;
const float ZD50 = 82.5188;

float CIECurve( const float input )
{
	const float scale = 7.787037;	        // powf( 29.0/6.0, 2.0) / 3.0;
	const float breakpoint = 0.008856;		// powf( 6.0/29.0, 3.0 );
	
	if (input > breakpoint)
		return cbrtf( input );
	else
		return (input * scale + (4.0/29.0));
}

/********************************************************************************/

float CIEReverseCurve( const float input )
{
	const float scale = 1.0 / 7.787037;		// 0.128418549  // 3.0 * pow( 6.0/29.0, 2.0);
	const float breakpoint = 6.0/29.0;
	
	if (input > breakpoint)
		return input*input*input;   // powf(input,3);
	else
		return scale*(input - (4.0/29.0));
}

/********************************************************************************/

xyzColor LAB2XYZ( const labColor &input )
{
	xyzColor result;
	
	float tempY = (input.L + 16)/116.0;

	float Y = YD50 * CIEReverseCurve( tempY );
	float X = XD50 * CIEReverseCurve( tempY + input.A / 500.0 );
	float Z = ZD50 * CIEReverseCurve( tempY - input.B / 200.0 );

	result.X = X;
	result.Y = Y;
	result.Z = Z;

	return result;
}

/********************************************************************************/

labColor XYZ2LAB( const xyzColor &input )
{
	labColor result;
	
	float tempY = CIECurve( input.Y / YD50 );
	float tempX = CIECurve( input.X / XD50 );
	float tempZ = CIECurve( input.Z / ZD50 );

	float L = 116.0 * tempY - 16.0;
	float a = 500 * ( tempX - tempY );
	float b = 200 * ( tempY - tempZ );

	result.L = L;
	result.A = a;
	result.B = b;

	return result;
}

/********************************************************************************/

float LERP( const float t, const float x1, const float x2 )
{
	return x1 + t*(x2-x1);
}
 
/********************************************************************************/

inline xyzColor operator*( const float &scale, const xyzColor &b)
{
	xyzColor result;
	result.X = scale * b.X;
	result.Y = scale * b.Y;
	result.Z = scale * b.Z;
	return result;
}
 
/********************************************************************************/

inline xyzColor operator*( const xyzColor &a, const xyzColor &b)
{
	xyzColor result;
	result.X = a.X * b.X / 100.0;
	result.Y = a.Y * b.Y / 100.0;
	result.Z = a.Z * b.Z / 100.0;
	return result;
}
 
/********************************************************************************/

inline xyzColor& operator*=( xyzColor &a, const xyzColor &b)
{
	a.X = a.X * b.X / 100.0;
	a.Y = a.Y * b.Y / 100.0;
	a.Z = a.Z * b.Z / 100.0;
	return a;
}
 
/********************************************************************************/

inline xyzColor& operator*=( xyzColor &a, float s)
{
	a.X = a.X * s;
	a.Y = a.Y * s;
	a.Z = a.Z * s;
	return a;
}
 
/********************************************************************************/

inline xyzColor operator/( const xyzColor &a, const xyzColor &b)
{
	xyzColor result;
	result.X = 100.0 * a.X / b.X;
	result.Y = 100.0 * a.Y / b.Y;
	result.Z = 100.0 * a.Z / b.Z;
	return result;
}
 
/********************************************************************************/

inline xyzColor& operator/=( xyzColor &a, const xyzColor &b)
{
	a.X = 100.0 * a.X / b.X;
	a.Y = 100.0 * a.Y / b.Y;
	a.Z = 100.0 * a.Z / b.Z;
	return a;
}
 
/********************************************************************************/

inline xyzColor& operator/=( xyzColor &a, const float s)
{
	a.X /= s;
	a.Y /= s;
	a.Z /= s;
	return a;
}

/********************************************************************************/

inline xyzColor operator+( const xyzColor &a, const xyzColor &b)
{
	xyzColor result;
	result.X = a.X + b.X;
	result.Y = a.Y + b.Y;
	result.Z = a.Z + b.Z;
	return result;
}
 
/********************************************************************************/

inline xyzColor& operator+=( xyzColor &a, const xyzColor &b)
{
	a.X += b.X;
	a.Y += b.Y;
	a.Z += b.Z;
	return a;
}
 
/********************************************************************************/

inline xyzColor operator-( const xyzColor &a, const xyzColor &b)
{
	xyzColor result;
	result.X = a.X - b.X;
	result.Y = a.Y - b.Y;
	result.Z = a.Z - b.Z;
	return result;
}
 
/********************************************************************************/

inline xyzColor& operator-=( xyzColor &a, const xyzColor &b)
{
	a.X -= b.X;
	a.Y -= b.Y;
	a.Z -= b.Z;
	return a;
}

/********************************************************************************/

// linear interpolation
xyzColor interp2inks( const float t, const xyzColor &ink1, const xyzColor &ink2 )
{
	xyzColor result;

	result = ink1 + t * (ink2 - ink1);
	
	return result;
}

/********************************************************************************/

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
	for (i=1; i < (steps/2); ++i)
		{
		float t = (float) i / (float) (steps/2);
		mix = interp2inks( t, paperColorXYZ, inkColorXYZ );
		mixLAB = XYZ2LAB( mix );
		temp.push_back( mixLAB );
		}
    
	// exact ink
	temp.push_back( inkColor );
 
    // interp ink->dark
	for (i=(steps/2)+1; i < (steps-1); ++i)
		{
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

bool labHueLess(const labColorNamed &a, const labColorNamed &b)
{
    float angle1 = M_PI + atan2(a.color.A,a.color.B);
    float angle2 = M_PI + atan2(b.color.A,b.color.B);
    return angle1 < angle2;
}

/********************************************************************************/

// here we want chromatic mixes, not darks
xyzColor estimate_ink_mix( const std::vector<labColor> &inkList, const xyzColor &paperColor )
{
	xyzColor identity( 100.0, 100.0, 100.0 );
    
    xyzColor overprint = identity;
    xyzColor average(0,0,0);
    for ( const auto &ink : inkList ) {
        xyzColor inkColor = LAB2XYZ( ink );
        average += inkColor;
        xyzColor inkFilter = inkColor / paperColor;
        overprint *= inkFilter;
    }
    overprint *= paperColor;
    average /= (float)inkList.size();

// TODO - find best parameter, 0.5 isn't enough, 1.0 is too much
// 0.0 leads to some crazy intermediate colors, and crazier splines
// 1.0 leads to blah.
    xyzColor mix = interp2inks( 0.6, overprint, average );
    
    return mix;
}

/********************************************************************************/

// trying to estimate appearance of overprints among arbitrary inks
xyzColor estimate_fractional_ink_mix( const std::vector<labColor> &inkList,
            const std::vector<float> inkFractionList, const xyzColor &paperColor )
{
	xyzColor identity( 100.0, 100.0, 100.0 );
    
    xyzColor overprint = identity;
    size_t inkCount = inkList.size();
    for (int i = 0; i < inkCount; ++i) {
        auto &ink = inkList[i];
        float thisFraction = inkFractionList[i];
        if (thisFraction > 0.0) {
            xyzColor inkColor = LAB2XYZ( ink );
            xyzColor inkFilter = inkColor / paperColor;
            xyzColor fractionalInk = interp2inks( thisFraction, identity, inkFilter );
            overprint *= fractionalInk;
        }
    }

    overprint *= paperColor;

    return overprint;
}

/********************************************************************************/

// here, we want the darkest possible result
xyzColor estimate_darkest_ink_overprint( const std::vector<labColor> &inkList, const xyzColor &paperColor )
{
    const float Ylimit = 1.3;
	xyzColor identity( 100.0, 100.0, 100.0 );
    
    xyzColor overprint = identity;
    for ( const auto &ink : inkList ) {
        xyzColor inkColor = LAB2XYZ( ink );
        xyzColor inkFilter = inkColor / paperColor;
        overprint *= inkFilter;
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

// convenience converter
xyzColor estimate_darkest_ink_overprint( const std::vector<labColorNamed> &inkList, const xyzColor &paperColor )
{
    return estimate_darkest_ink_overprint( ConvertNamedColorList2ColorList(inkList), paperColor );
}

/********************************************************************************/

void subdivide_ink_splines( const inkColorSet &inkSet, const int divisions, const int steps, const labColor &ink1, const labColor &ink2, const xyzColor &paperColor, spline_list &splines )
{
	color_list temp;
	labColor mixLAB;
	xyzColor identity( 100.0, 100.0, 100.0 );

    xyzColor ink1Color = LAB2XYZ( ink1 );
    xyzColor ink2Color = LAB2XYZ( ink2 );

    xyzColor halfwayMix = estimate_ink_mix( { ink1, ink2 }, paperColor );

    // d == 0 is the last pure ink spline
    // d == division is this pure ink spline (handled elsewhere)
    for (int d = 1; d < divisions; ++d) {
        float t = (float)d / (float)divisions;

        xyzColor mix;
        if (t <= 0.5)
            mix = interp2inks( t*2.0, ink1Color, halfwayMix );
        else
            mix = interp2inks( (t-0.5)*2.0, halfwayMix, ink2Color );

        mixLAB = XYZ2LAB( mix );
        temp = mix_pure_ink_spline( steps, inkSet.paperColor, mixLAB, inkSet.darkColor );
        splines.push_back( temp );
    }
}

/********************************************************************************/

// create splines from mixes of inks and paper colors
spline_list mix_ink_splines( inkColorSet &inkSet )
{
	const int steps = 51;	// odd so we have a midpoint
    const int divisions = 4;    // even so we have a midpoint (5 splines per surface)
	color_list temp;
	xyzColor mix;
	labColor mixLAB;
    spline_list splines;
	xyzColor identity( 100.0, 100.0, 100.0 );


    size_t inkCount = inkSet.primaries.size();
    assert(inkCount > 0);


    // Need inks in hue angle order so the splines will be in order for hull
    std::sort( inkSet.primaries.begin(), inkSet.primaries.end(), labHueLess );

	xyzColor paperColor = LAB2XYZ( inkSet.paperColor );


    // If the combined color has 0 L, estimate it from primaries
    // so we can get something sort of realistic for the mix
    if (inkSet.darkColor.L <= 0) {
        mix = estimate_darkest_ink_overprint( inkSet.primaries, paperColor );
		mixLAB = XYZ2LAB( mix );
#if 1
        printf("Estimated overprint for %s is (%f, %f, %f)\n",
            inkSet.name.c_str(),
            mixLAB.L, mixLAB.A, mixLAB.B );
#endif
        inkSet.darkColor = mixLAB;
    }
    

    // first ink spline, always calculated
    temp = mix_pure_ink_spline( steps, inkSet.paperColor, inkSet.primaries[0].color, inkSet.darkColor );
	splines.push_back( temp );

    // iterate any additional inks, keeping splines in order
    for (size_t k = 1; k < inkCount; ++k) {
        subdivide_ink_splines( inkSet, divisions, steps,
            inkSet.primaries[k-1].color, inkSet.primaries[k].color,
            paperColor, splines);

        // pure ink spline paper->ink2->combined
        temp = mix_pure_ink_spline( steps, inkSet.paperColor, inkSet.primaries[k].color, inkSet.darkColor );
        splines.push_back( temp );
    }
    
    // if we can make a solid, then wrap around from last ink to the first!
    if (inkCount > 2) {
        subdivide_ink_splines( inkSet, divisions, steps,
            inkSet.primaries[inkCount-1].color, inkSet.primaries[0].color,
            paperColor, splines);
    }

    return splines;
}

/********************************************************************************/

// t ranges 0..1.0
// we are interpolating between B and C
float SplineInterp( float t, float A, float B, float C, float D )
{

// catmull rom - cardinal spline with tension = 0.5
// needs scaling by 0.5 at end
	const float	M11 = -1.0, M12 = 3.0, M13 = -3.0, M14 = 1.0;
	const float	M21 = 2.0, M22 = -5.0, M23 = 4.0, M24 = -1.0;
	const float	M31 = -1.0, M32 = 0.0, M33 = 1.0, M34 = 0.0;
	const float	M41 = 0.0, M42 = 2.0, M43 = 0.0, M44 = 0.0;
	
	// cubic interp
	float value;
	value  = A * (M41 + t * (M31 + t * (M21 + t*M11) ) );
	value += B * (M42 + t * (M32 + t * (M22 + t*M12) ) );
	value += C * (M43 + t * (M33 + t * (M23 + t*M13) ) );
	value += D * (M44 + t * (M34 + t * (M24 + t*M14) ) );
	
	return 0.5 * value;
}

/********************************************************************************/


// need function to search spline for correct point and return interpolated values
// given L*, binary search the spline and return the A and B values that go with it

// splineL, splineA, splineB - search L, use t parameter for A and B

void SearchSpline( const color_list &spline, float L, float &A, float &B )
{
	// find points in list that bracket L
	// list is greatest to least (white to black)
	// ccox - start with a simple linear search
	int index = 1;
	for ( ; index < spline.size(); ++index)
		{
		if (spline[index].L <= L)
			break;
		}
	
	assert( index < spline.size());
	
	// find t that gives the correct L to within tolerance (between index-1 and index)

	int sample0 = index - 2;
	int sample1 = index - 1;
	int sample2 = index;
	int sample3 = index + 1;
	
	// clip to end points
	if (sample0 < 0) sample0 = 0;
	if (sample1 < 0) sample1 = 0;
	if (sample3 > (int)(spline.size())-1)	sample3 = (int)(spline.size())-1;
	
	// quick and dirty binary search

	float t = 0.5;
//	if (sample1 != sample2)
		{
		const float Ltolerance = 0.1;	// ccox - TODO - what tolerance do we need?
		
		float Ltop = spline[sample1].L;
		float Ttop = 0.0;
		
		float Lbottom = spline[sample2].L;
		float Tbottom = 1.0;
	
		float Ltest = SplineInterp( t, spline[sample0].L, spline[sample1].L, spline[sample2].L, spline[sample3].L );
		
		while ( fabs( Ltest - L ) > Ltolerance)
			{
			if (Ltest < L)
				{
				// between top and current
				Lbottom = Ltest;
				Tbottom = t;
				}
			else
				{
				// between current and bottom
				Ltop = Ltest;
				Ttop = t;
				}
			
			t = (Ttop + Tbottom) * 0.5;
			Ltest = SplineInterp( t, spline[sample0].L, spline[sample1].L, spline[sample2].L, spline[sample3].L );
			
			}
		}
	
	// interpolate colors and return result
	A = SplineInterp( t, spline[sample0].A, spline[sample1].A, spline[sample2].A, spline[sample3].A );
	B = SplineInterp( t, spline[sample0].B, spline[sample1].B, spline[sample2].B, spline[sample3].B );

}

/********************************************************************************/

// are we less than our darkest, or greater than our brightest point?
bool ClippedL( float Linput, labColor &output, const inkColorSet &inkSet )
{
	output.L = 0.0;
	output.A = 0.0;
	output.B = 0.0;
	
	if (Linput < inkSet.darkColor.L)
		{
		output = inkSet.darkColor;
		return true;
		}

	if (Linput > inkSet.paperColor.L)
		{
		output = inkSet.paperColor;
		return true;
		}

	return false;
}


/********************************************************************************/

void SplineInterpList( const size_t subdivisions, const PointList &input, PointList &result,
                        bool wrapAround )
{
    const int pointCount = (int)input.size();
    
    result.reserve( subdivisions+1 );
    
	// iterate through list
	for (size_t i = 0; i <= subdivisions; ++i)
		{
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
            if (p0 < 0)	p0 = 0;
            if (p1 < 0) p1 = 0;
            if (p1 >= pointCount) p1 = pointCount-1;
            if (p2 >= pointCount) p2 = pointCount-1;
            if (p3 >= pointCount) p3 = pointCount-1;
        }
        
		float t = floatIndex - pointIndex;
		Point newPoint;
		newPoint.a = SplineInterp( t, input[p0].a, input[p1].a, input[p2].a, input[p3].a );
		newPoint.b = SplineInterp( t, input[p0].b, input[p1].b, input[p2].b, input[p3].b );
		
		result.emplace_back( newPoint );
		}
}

/********************************************************************************/

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
        newPoint.a = LERP( t, input[p1].a, input[p2].a );
        newPoint.b = LERP( t, input[p1].b, input[p2].b );
        
        result.emplace_back( newPoint );
    }   // end for subdivisions
}

/********************************************************************************/

void PointListFromFloatSpline( const size_t subdivisions, const PointList &input, PointList &result,
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

	// remove duplicate points that result from some duplicated spline points
    auto last = std::unique(result.begin(), result.end());
    result.erase(last, result.end());

}

/********************************************************************************/

Point FindClosestPointInList( const PointList &list, Point &input )
{
	// ccox - start with brute force linear search
	// TODO - find a way to accelerate the search
	
	float closest_dist = 256.0*256.0*256.0;		// much greater than our maximum possible distance
	size_t closest_index = -1;
	
	size_t count = list.size();
	assert( count > 0);
	
	for (size_t i = 0; i < count; ++i) {
		float distA = input.a - list[i].a;
		float distB = input.b - list[i].b;
		float dist = distA*distA + distB*distB;	// leave it squared
		
		if (dist < closest_dist) {
			closest_dist = dist;
			closest_index = i;
        }
    }
	
	assert( closest_index >= 0);
	return list[closest_index];
}

/********************************************************************************/

// interpolate between 0 and 100.0
float grid_to_L( int grid_value, int gridPoints )
{
	return (100.0 * (float)grid_value) / (float)(gridPoints - 1);
}

// ccox - FIX ME - cheap version for now -- refine if needed
float grid_to_AB( int grid_value, int gridPoints )
{
	float middle = 0.5 * gridPoints;
	return (127.0 * ((float)grid_value - middle)) / middle;
}

/********************************************************************************/

// convert 0..100 representation to file representation
int floatL_to_fileL8( float L )
{
	if (L <= 0.0) return 0;
	if (L >= 100.0) return 255;
	return (int)( (255.0 / 100.0) * L + 0.5 );
}

int floatAB_to_fileAB8( float A )
{
	if (A > 127.0) return 255;
	if (A < -128.0) return 0;
	return (int)( A + 128.0 );
}

uint8_t float_to_file255( float A )
{
	if (A > 1.0) return 255;
	if (A < 0.0) return 0;
	return (int)( A * 255.0 );
}

/********************************************************************************/

// convert 0..100 representation to file representation
// ICC version 2 profile encoding for LAB 16 bit  --- not usable in TIFF
int floatL_to_fileL16( float L )
{
	if (L <= 0.0) return 0;
	if (L >= 100.0) return 65280;
	return (int)( (65280.0 / 100.0) * L + 0.5 );
}

int floatAB_to_fileAB16( float A )
{
	if (A > 127.0) return 65280;
	if (A < -128.0) return 0;
	return (int)( A + 32768.0 );
}

/********************************************************************************/

// really simple tent
inline float Smooth3( float a, float b, float c)
{
	return (a + 4*b + c) / 6.0;
}

inline void Smooth3( std::vector<float> &a, const std::vector<float> &b, const std::vector<float> &c, int channels)
{
    for (int i = 0; i < channels; ++i)
        a[i] = Smooth3(a[i],b[i],c[i]);
}

// filter in place, in one dimension, for 3 channels
void SmoothOneDirection3( float *data, int planeStep, int rowStep, int colStep )
{
	int i, j, k;
    
	for (i = 0; i < gDataGridPoints; ++i) {
		for (j = 0; j < gDataGridPoints; ++j) {
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
			
			for (k = 0; k < (gDataGridPoints-1); ++k) {
				
				next0 = data[ i * planeStep + j * rowStep + (k+1)*colStep + 0 ];
				next1 = data[ i * planeStep + j * rowStep + (k+1)*colStep + 1 ];
				next2 = data[ i * planeStep + j * rowStep + (k+1)*colStep + 2 ];
				
				result0 =  Smooth3( last0, current0, next0 );
				result1 =  Smooth3( last1, current1, next1 );
				result2 =  Smooth3( last2, current2, next2 );
				
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

// filter in place, in one dimension, for arbitrary channel counts
void SmoothOneDirection( float *data, int planeStep, int rowStep, int colStep, int channels )
{
    assert(channels > 0);
    assert(channels <= 15);
 
    if (channels == 3) {
        SmoothOneDirection3( data, planeStep, rowStep, colStep );
        return;
    }
    
    // there has to be a better way to do this for arbitrary channel counts
// TODO - can I just use pointers and rotate those?
    std::vector<float> last(15);
    std::vector<float> current(15);
    std::vector<float> next(15);
	
	for (int i = 0; i < gDataGridPoints; ++i) {
		for (int j = 0; j < gDataGridPoints; ++j) {
            int k = 0;
            
			// special case first value
            for (int c = 0; c < channels; ++c)
                last[c] = data[ i * planeStep + j * rowStep + j * colStep + c ];
			
			current = last;
			
			for (k = 0; k < (gDataGridPoints-1); ++k) {
            
                for (int c = 0; c < channels; ++c)
                    next[c] = data[ i * planeStep + j * rowStep + (k+1)*colStep + c ];
				
				Smooth3( last, current, next, channels );
				
				// write back smoothed result
                for (int c = 0; c < channels; ++c)
                   data[ i * planeStep + j * rowStep + k*colStep + c ] = last[c];
				
				// rotate
				last = current;
				current = next;
            }
			
			// special case last k value
			// next == current already
            Smooth3( last, current, next, channels );
			
			// write back smoothed result
            for (int c = 0; c < channels; ++c)
               data[ i * planeStep + j * rowStep + k*colStep + c ] = last[c];
        }
		
    }

}

/********************************************************************************/

// useful for debugging, but slow
// probably faster to rasterize the poly without antialiasing and sample the bitmap
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

// debugging tool
void DumpPointList( const std::string &name, const PointList &planePoints )
{
    std::string filename = name + ".csv";
    
    FILE *out = fopen( filename.c_str(), "w");
    if (!out)
        return;
    
    fprintf(out,"name\n");
    fprintf(out,"x, y\n");
    for ( const auto &pt : planePoints ) {
        fprintf(out,"%f, %f\n", pt.a, pt.b );
    }
    
    fclose(out);
}

/********************************************************************************/

/*
A2B - inks and overprints to LAB, N-dimensional to 3 channels
    use ink mixing model and simple interpolation
    doesn't really need smoothing
*/
void createA2B_table( const inkColorSet &inkSet, const spline_list &splines, int depth, profileData &myProfile )
{
    const int maxChannels = 15;          // ICC spec. limit
    const int maxGridPoints = 31;        // sanity limit - TODO - increase limit in release build
    const int maxGridSize = 1024*1024;   // limit 1 Meg, 20 Meg?
    
    int inkCount = (int)inkSet.primaries.size();
    assert(inkCount > 0);
    assert(inkCount <= maxChannels);
    
    // decide on table size
    int gridPoints = 2;         // absolute minimum
    int gridSize = pow( gridPoints, inkCount );
    
    int newPoints = gridPoints;
    int newSize = gridSize;
    while (newSize <= maxGridSize && newPoints <= maxGridPoints) {
        gridPoints = newPoints;
        gridSize = newSize;
        ++newPoints;
        newSize = pow( newPoints, inkCount );
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
    
    std::vector<labColor> inkList(maxChannels);
    for (size_t i = 0; i < inkCount; ++i)
        inkList[i] = inkSet.primaries[i].color;

	xyzColor paperColor = LAB2XYZ( inkSet.paperColor );

    size_t gridCount = gridSize;
    std::unique_ptr<uint8_t> gridBuffer(new uint8_t[ gridCount * 3 ]);
    uint8_t *gridData = gridBuffer.get();
    
    std::fill( loopCounters.begin(), loopCounters.end(), 0 );
    
    // iterate virtual loop to fill table  (faster than doing a dozen divides and modulos)
    // i[k] = (index / (int)pow(gridPoints,(inkCount-1)-k)) % gridPoints;   // loopSteps can be precalcuated, but the divides cannot
    for (uint32_t index = 0; loopCounters[0] < gridPoints; ++index ) {
        
        for (size_t k = 0; k < inkCount; ++k)
            inkFractions[k] = (float)loopCounters[k] / (float)(gridPoints-1);
        
        xyzColor resultXYZ = estimate_fractional_ink_mix( inkList, inkFractions, paperColor );
        labColor resultLAB = XYZ2LAB( resultXYZ );
        int Lout =   floatL_to_fileL8( resultLAB.L );
        int Aout = floatAB_to_fileAB8( resultLAB.A );
        int Bout = floatAB_to_fileAB8( resultLAB.B );
        gridData[ 3*index + 0 ] = Lout;
        gridData[ 3*index + 1 ] = Aout;
        gridData[ 3*index + 2 ] = Bout;
        
        // increment last counter
        //    if incremented is >= gridPoints, reset and roll upward in list
        for (int j = (int)(inkCount-1); j >= 0; --j) {
            int temp = loopCounters[j] + 1;
            if (temp >= gridPoints && j != 0)   // we want counter 0 to overflow, to end the big loop
                loopCounters[j] = 0;
            else {
                loopCounters[j] = temp;
                break;
            }
        }
    }   // overall table loop using a vector of counters


    tableFormat myTable;
    myTable.tableSig = icSigAToB0Tag;
    myTable.tableDepth = 8;
    myTable.tableGridPoints = gridPoints;
    myTable.tableDimensions = (int)inkCount;    // input
    myTable.tableChannels = 3;                  // output
    myTable.tableData = std::move(gridBuffer);
    myProfile.tables.emplace_back(myTable);
}

/********************************************************************************/

/*
B2A - LAB to ink mixes, needs detail, 3D to N channels
    ignore GCR/UCR just write the raw mixes
    This needs smoothing.
*/
void createB2A_table( const inkColorSet &inkSet, const spline_list &splines, int depth, int gridPoints, profileData &myProfile )
{
    const int maxChannels = 15;          // ICC spec. limit
    
    int inkCount = (int)inkSet.primaries.size();
    assert(inkCount > 0);
    assert(inkCount <= maxChannels);

    int gridSize = pow( gridPoints, 3 );

    int gridCount = gridSize;
    std::unique_ptr<float> gridBuffer(new float[ gridCount * inkCount ]);
    float *gridData = gridBuffer.get();

	int planeStep = gridPoints*gridPoints * inkCount;
	int rowStep = gridPoints * inkCount;
	int colStep = inkCount;



// TODO - fill in the table!
// out of gamut - find nearest
// in gamut, figure out ink mix using nearby ink hue angles and iterative solver
// the result won't be perfect, but usable
    memset(gridData,0,gridCount*inkCount*sizeof(float));
    

	for (int L = 0; L < gridPoints; ++L) {
		// setup slices variables
		float Lfloat = grid_to_L( L, gridPoints );
		
		//special case less < darkest and > brightest
		if ( Lfloat <= inkSet.darkColor.L) {    // fill with darkest
			for (int A = 0; A < gridPoints; ++A)
				for (int B = 0; B < gridPoints; ++B) {
                    for (int c = 0; c < inkCount; ++c)
                        gridData[ L * planeStep + A * rowStep + B*colStep + c ] = 1.0;
                }
			
			continue;
        }
		if ( Lfloat >= inkSet.paperColor.L) {   // fill with paper
			for (int A = 0; A < gridPoints; ++A)
				for (int B = 0; B < gridPoints; ++B) {
                    for (int c = 0; c < inkCount; ++c)
                        gridData[ L * planeStep + A * rowStep + B*colStep + c ] = 0.0;
                }
			
			continue;
        }
		
		// interpolate splines in L to get points along this AB plane
		PointList planeSpline;
		for ( const auto &oneSpline: splines ) {
			float A1, B1;
			SearchSpline( oneSpline, Lfloat, A1, B1 );
			planeSpline.push_back( Point( A1, B1 ) );
        }
		
		// create interpolated point list from the splines
		PointList planePoints;
        PointListFromFloatSpline( 50*inkCount, planeSpline, planePoints, (inkCount > 2) );

		// now iterate over this plane/slice
		for (int A = 0; A < gridPoints; ++A) {
			float Afloat = grid_to_AB( A, gridPoints );
			
			for (int B = 0; B < gridPoints; ++B) {
				float Bfloat = grid_to_AB( B, gridPoints );

				// find closest point in our line/point list
				Point thisSpot( Afloat, Bfloat );

                // use closest point outside or for 1 or 2 inks
// TODO - need ink value not AB coordinate!
                Point result = FindClosestPointInList( planePoints, thisSpot );
                
                // for 3 or more inks, test for inside polygon, interpolate inside
                if (inkCount > 2) {
                    bool inside = pointInPoly( planePoints, thisSpot );
                    if (inside) {
// TODO - WRITE ME!
                        result = thisSpot;
                    }
                }

#if 0
				// save the values
				gridData[ L * planeStep + A * rowStep + B*colStep + 0 ] = Lfloat;
				gridData[ L * planeStep + A * rowStep + B*colStep + 1 ] = result.a;
				gridData[ L * planeStep + A * rowStep + B*colStep + 2 ] = result.b;
#endif
				
				}   // end for B
			}   // end for A
		}   // end for L




// smooth the floating point table
	// smooth the 3D table data
	SmoothOneDirection( gridData, planeStep, rowStep, colStep, inkCount );
	SmoothOneDirection( gridData, rowStep, colStep, planeStep, inkCount );
	SmoothOneDirection( gridData, colStep, planeStep, rowStep, inkCount );

// convert the float table to integer
    std::unique_ptr<uint8_t> outBuffer(new uint8_t[ gridCount * inkCount ]);
    uint8_t *outData = outBuffer.get();

    // oganize data for ICC profile
    for (int L = 0; L < gridPoints; ++L) {
        for (int A = 0; A < gridPoints; ++A) {
			for (int B = 0; B < gridPoints; ++B) {
                for (int c = 0; c < inkCount; ++c) {
                    outData[c] =float_to_file255( gridData[ L * planeStep + A * rowStep + B*colStep + c ] );
                }
                outData += inkCount;
            }
        }
    }


    tableFormat myTable;
    myTable.tableSig = icSigBToA0Tag;
    myTable.tableDepth = 8;
    myTable.tableGridPoints = gridPoints;
    myTable.tableDimensions = 3;                // input
    myTable.tableChannels = (int)inkCount;      // output
    myTable.tableData = std::move(outBuffer);
    myProfile.tables.emplace_back(myTable);
}


/********************************************************************************/

std::vector<color_space> profileSpaceLookup =
{
    kSpace1CLR, // zero
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

/********************************************************************************/

// create LAB to LAB for color mapping/preview
void create_abstract_profile( const inkColorSet &inkSet, const spline_list &splines, int depth, int gridPoints,
                    const std::string &filename )
{
	int L, A, B;	// my grid iteration indices

	// allocate my grid
    size_t gridCount =  gridPoints*gridPoints*gridPoints;
    std::unique_ptr<float> gridBuffer(new float[ gridCount * 3 ]);
    float *gridData = gridBuffer.get();
	
	int planeStep = gridPoints*gridPoints * 3;
	int rowStep = gridPoints * 3;
	int colStep = 3;
    
    size_t inkCount = inkSet.primaries.size();
    assert(inkCount > 0);
	
	for (L = 0; L < gridPoints; ++L) {
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
		
		// interpolate splines in L to get points along this AB plane
		PointList planeSpline;
		for ( const auto &oneSpline: splines ) {
			float A1, B1;
			SearchSpline( oneSpline, Lfloat, A1, B1 );
			planeSpline.push_back( Point( A1, B1 ) );
        }
		
		// create interpolated point list from the splines
		PointList planePoints;
        PointListFromFloatSpline( 50*inkCount, planeSpline, planePoints, (inkCount > 2) );


// DEBUG the last set generated to check the gamut shape and area
//DumpPointList( std::string("pointSplines_") + std::to_string(L), planeSpline );
//DumpPointList( std::string("pointlist_") + std::to_string(L), planePoints );


		// now iterate over this plane/slice
		for (A = 0; A < gridPoints; ++A) {
			float Afloat = grid_to_AB( A, gridPoints );
			
			for (B = 0; B < gridPoints; ++B) {
				float Bfloat = grid_to_AB( B, gridPoints );

				// find closest point in our line/point list
				Point thisSpot( Afloat, Bfloat );

                // use closest point outside or for 1 or 2 inks
                Point result = FindClosestPointInList( planePoints, thisSpot );
                
                // for 3 or more inks, test for inside polygon, interpolate inside
                if (inkCount > 2) {
                    bool inside = pointInPoly( planePoints, thisSpot );
                    if (inside) {
// TODO - interpolate ink mixture!  the AB value may be in gamut, but how did we mix it?
// this needs a mixing model.  or some quick heuristics...
                        result = thisSpot;
                    }
                }
                
				// save the values
				gridData[ L * planeStep + A * rowStep + B*colStep + 0 ] = Lfloat;
				gridData[ L * planeStep + A * rowStep + B*colStep + 1 ] = result.a;
				gridData[ L * planeStep + A * rowStep + B*colStep + 2 ] = result.b;
				
				}   // end for B
			}   // end for A
		}   // end for L

	
	// smooth the 3D table data
	SmoothOneDirection( gridData, planeStep, rowStep, colStep, 3 );
	SmoothOneDirection( gridData, rowStep, colStep, planeStep, 3 );
	SmoothOneDirection( gridData, colStep, planeStep, rowStep, 3 );
    


    size_t bufferSize = gridPoints*gridPoints*gridPoints * 3;
    std::unique_ptr<uint8_t> outBuffer(new uint8_t[ bufferSize ]);
    uint8_t *outPtr = outBuffer.get();

#if 1
    // order the data for easy viewing as an image
    for (A = 0; A < gridPoints; ++A) {
        for (L = 0; L < gridPoints; ++L) {
			for (B = 0; B < gridPoints; ++B) {

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
    
    // write TIFF File
    WriteTIFF( filename + ".tiff", 96.0, TIFF_MODE_CIELAB, outBuffer.get(),
                gridPoints*gridPoints, gridPoints, 3, 8 );
#endif


    // oganize data for ICC profile
    outPtr = outBuffer.get();
    for (L = 0; L < gridPoints; ++L) {
        for (A = 0; A < gridPoints; ++A) {
			for (B = 0; B < gridPoints; ++B) {

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


    // write ICC abstract profiles
    profileData myProfile;
    myProfile.description = inkSet.description;
    myProfile.copyright = "Copyright (c) Chris Cox 2026";
    myProfile.otherText = inkSet.name;
    myProfile.profileClass = kClassAbstract;
    myProfile.colorSpace = kSpaceLAB;
    myProfile.pcsSpace = kSpaceLAB;
    myProfile.preferredCMM = 'ICCD';
    myProfile.platform = 'APPL';
    myProfile.manufacturer = 'none';
    myProfile.creator = 'ccox';

    tableFormat myTable;
    myTable.tableSig = icSigAToB0Tag;
    myTable.tableDepth = 8;
    myTable.tableGridPoints = gridPoints;
    myTable.tableDimensions = 3;    // input
    myTable.tableChannels = 3;      // output
    myTable.tableData = std::move(outBuffer);
    myProfile.tables.emplace_back(myTable);

    writeICCProfile( filename+"_abstract.icc", myProfile );
    
    // buffers are freed automatically
}

/********************************************************************************/

// full output profile: A2B, B2A, gamut
void create_output_profile( const inkColorSet &inkSet, const spline_list &splines, int depth, int gridPoints,
                    const std::string &filename )
{
	int L, A, B;	// my grid iteration indices

	// allocate my gamut grid
    size_t gridCount =  gridPoints*gridPoints*gridPoints;
    std::unique_ptr<uint8_t> gamutBuffer(new uint8_t[ gridCount ]);
    uint8_t *gamutData = gamutBuffer.get();
    
    // set everything to out of gamut (inverted from a normal image/table, but ok...)
    memset( gamutData, 255, gridCount );
    
    int gamutPlaneStep = gridPoints*gridPoints;
	int gamutRowStep = gridPoints;
	int gamutColStep = 1;

    size_t inkCount = inkSet.primaries.size();
    assert(inkCount > 0);
	
	for (L = 0; L < gridPoints; ++L) {
		// setup slices variables
		float Lfloat = grid_to_L( L, gridPoints );
		
		//special case less < darkest and > brightest
		labColor clippedColor;
		if (ClippedL( Lfloat, clippedColor, inkSet)) {
			continue;
        }   // end ClippedL
		
		// interpolate splines in L to get points along this AB plane
		PointList planeSpline;
		for ( const auto &oneSpline: splines ) {
			float A1, B1;
			SearchSpline( oneSpline, Lfloat, A1, B1 );
			planeSpline.push_back( Point( A1, B1 ) );
        }
		
		// create interpolated point list from the splines
		PointList planePoints;
        PointListFromFloatSpline( 50*inkCount, planeSpline, planePoints, (inkCount > 2) );

// DEBUG the last set generated to check the gamut shape and area
//DumpPointList( std::string("pointlist_") + std::to_string(L), planePoints );

		// now iterate over this plane/slice
		for (A = 0; A < gridPoints; ++A) {
			float Afloat = grid_to_AB( A, gridPoints );
			
			for (B = 0; B < gridPoints; ++B) {
				float Bfloat = grid_to_AB( B, gridPoints );

				// find closest point in our line/point list
				Point thisSpot( Afloat, Bfloat );
                
                // for 3 or more inks, test for inside polygon, interpolate inside
                if (inkCount > 2) {
                    bool inside = pointInPoly( planePoints, thisSpot );
                    if (inside) {
                        gamutData[ L * gamutPlaneStep + A * gamutRowStep + B * gamutColStep ] = 0;
                    }
                }
				
            }   // end for B
        }   // end for A
    }   // end for L



    // write ICC output profiles
    profileData myProfile;
    myProfile.description = inkSet.description;
    myProfile.copyright = "Copyright (c) Chris Cox 2026";
    myProfile.otherText = inkSet.name;
    myProfile.profileClass = kClassOutput;
    myProfile.colorSpace = profileSpaceLookup[ inkCount ];
    myProfile.pcsSpace = kSpaceLAB;
    myProfile.preferredCMM = 'ICCD';
    myProfile.platform = 'APPL';
    myProfile.manufacturer = 'none';
    myProfile.creator = 'ccox';


// TODO - explicit gamut creation
    tableFormat myGamut;
    myGamut.tableSig = icSigGamutTag;
    myGamut.tableDepth = 8;
    myGamut.tableGridPoints = gridPoints;
    myGamut.tableDimensions = 3;    // input
    myGamut.tableChannels = 1;      // output
    myGamut.tableData = std::move(gamutBuffer);
    myProfile.tables.emplace_back(myGamut);


    // make A2B0 (ink to LAB)
    createA2B_table( inkSet, splines, depth, myProfile );

    // make B2A0 (LAB to ink)
    createB2A_table( inkSet, splines, depth, gridPoints, myProfile );
    
    
    // and point the other A2B tables back to A2B0
    tableFormat myFake;
    myFake.tableSig = icSigAToB1Tag;
    myFake.pointsBackTo = icSigAToB0Tag;
    myProfile.tables.emplace_back(myFake);

    myFake.tableSig = icSigAToB2Tag;
    myFake.pointsBackTo = icSigAToB0Tag;
    myProfile.tables.emplace_back(myFake);

    // and point the other B2A tables back to B2A0
    myFake.tableSig = icSigBToA1Tag;
    myFake.pointsBackTo = icSigBToA0Tag;
    myProfile.tables.emplace_back(myFake);
    
    myFake.tableSig = icSigBToA2Tag;
    myFake.pointsBackTo = icSigBToA0Tag;
    myProfile.tables.emplace_back(myFake);



    writeICCProfile( filename+"_output.icc", myProfile );
    
    
    // buffers are freed automatically
}

/******************************************************************************/
/******************************************************************************/

static void print_usage(char *argv[])
{
	printf("Usage: %s <args>\n", argv[0] );
	
	printf("\t-depth B        bit depth of data [8 or 16] (default %d)\n", gDataDepth );
	printf("\t-grid G         number of grid points (default %d)\n", gDataGridPoints );
		
	printf("\t-version        Prints this message and exits immediately\n" );
	printf("Version %s, Compiled %s %s\n", kVersionString, __DATE__, __TIME__ );
}

/******************************************************************************/

static void parse_arguments( int argc, char *argv[] )
{

	for ( int c = 1; c < argc; ++c )
		{
		
		if ( (strcmp( argv[c], "-grid" ) == 0 || strcmp( argv[c], "-g" ) == 0 )
			&& c < (argc-1) )
			{
			gDataGridPoints = atoi( argv[c+1] );
            if (gDataGridPoints < 2)
                gDataGridPoints = 2;
            if (gDataGridPoints > 256)
                gDataGridPoints = 256;
			++c;
			}
		else if ( (strcmp( argv[c], "-depth" ) == 0 || strcmp( argv[c], "-d" ) == 0 )
			&& c < (argc-1) )
			{
			gDataDepth = atoi( argv[c+1] );
            if (gDataDepth > 16)
                gDataDepth = 16;
            if (gDataDepth < 8)
                gDataDepth = 8;
            if (gDataDepth != 8 && gDataDepth != 16)
                gDataDepth = 8;
			++c;
			}
		else if ( strcmp( argv[c], "-V" ) == 0
				|| strcmp( argv[c], "-v" ) == 0
				|| strcmp( argv[c], "-help" ) == 0
				|| strcmp( argv[c], "--help" ) == 0
				|| strcmp( argv[c], "-version" ) == 0
				|| strcmp( argv[c], "-Version" ) == 0
				)
			{
			print_usage( argv );
			exit (0);
			}
		else if (argv[c][0] == '-')
			{
			// unrecognized switch
			print_usage( argv );
			exit (1);
			}
		
		}
}

/********************************************************************************/

int main (int argc, char * argv[])
{
	// handle our command line arguments
	parse_arguments( argc, argv );
	
    // iterate over each named set of inks
    for (auto &inkSet : colorSets) {
        
        // create splines from measured points using mixing model
        auto splines = mix_ink_splines( inkSet );
        
        create_output_profile( inkSet, splines, gDataDepth, 21, inkSet.name );
        create_abstract_profile( inkSet, splines, gDataDepth, 21, inkSet.name );
    
     }  // end for colorSets

    return 0;
}

/********************************************************************************/
/********************************************************************************/
