/*
Copyright (c) 2026 Chris Cox


Is it accurate? Nope.  Accuracy would need a lot more measurements, and might not look as good.
Does it look reasonable? Yes.  And that's all I need from it.


Assume primaries are saturated, not too neutral, and define a convex hull.
Primaries will be sorted by hue to make sure they are in order to make a convex hull.

TODO:
5+ inks the splines are distorted - too concave, too many peaks


Special case 1 ink -- single spline from paper->ink->dark, take only point
Special case 2 ink -- spline surface, closest point on line
Special case 3..N ink -- create closed shape, find interpolated inside, project point to closest outside

Smooth the resulting 3D table

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
	Point( float A, float B) : a(A), b(B) {}
    
    bool operator==(const Point& other) const = default;
};

typedef std::vector< Point > PointList;

typedef std::vector< labColor > color_list;

typedef std::vector< color_list > spline_list;

/******************************************************************************/

struct inkColorSet {
    std::string name;           // what filename to use
    std::string description;    // how to describe this combination
    
    labColor paperColor;        // lightest possible color
    labColor combinationColor;  // darkest possible color
    color_list primaries;       // saturated hues
};

std::vector<inkColorSet> colorSets =
{
// 1
    {   "Turquoise",
        "Turquoise Paint",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {44.4, -35.9, -32.5} }
    },

// 2
    {   "Orange-Turquoise",
        "Orange and Turquoise Paint",
        { 97.12126, -0.024685, 0.025155 },
        { 7.6, 2.5, 0.8 },
        { {62.0, 32, 58.0}, {47.8, -34.2, -43.0 } }
    },

// 3
    {   "Orange-Turquoise-Green",
        "Orange, Turquoise, and Green Paint",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {62.0, 32, 58.0}, {47.8, -34.2, -43.0}, {71.2, -54.2, 62.9} }
    },

    {   "Turquoise-Magenta-Yellow",
        "Turquoise, Magenta, and Yellow Paint",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {47.8, -34.2, -43.0}, {52.0, 81.1, -1.7}, {90.2, 2.7, 97.7}  }
    },
    
// 4
    {   "Turquoise-Magenta-Yellow-Violet",
        "Turquoise, Magenta, Yellow, and Violet Paint",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {47.8, -34.2, -43.0}, {52.0, 81.1, -1.7},
          {90.2, 2.7, 97.7}, {51.3, 26.0, -37.4} }
    },

// 5
    {   "Turquoise-Magenta-Yellow-Violet-Green",
        "Turquoise, Magenta, Yellow, Violet, and Green Paint",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {47.8, -34.2, -43.0}, {52.0, 81.1, -1.7},
          {90.2, 2.7, 97.7}, {51.3, 26.0, -37.4},
          {71.2, -54.2, 62.9} }
    },

// 6
    {   "Turquoise-Magenta-Yellow-Violet-Green-Blue",
        "Turquoise, Magenta, Yellow, Violet, Green, and Blue Paint",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {47.8, -34.2, -43.0}, {52.0, 81.1, -1.7},
          {90.2, 2.7, 97.7}, {51.3, 26.0, -37.4},
          {71.2, -54.2, 62.9}, {38.2, 13.3, -66.4} }
    },

// 7
    {   "Turquoise-Magenta-Yellow-Violet-Green-Blue-Orange",
        "Turquoise, Magenta, Yellow, Violet, Green, Blue, and Orange Paint",
        { 97.12126, -0.024685, 0.025155 },
        { -1,0,0 },
        { {47.8, -34.2, -43.0}, {52.0, 81.1, -1.7},
          {90.2, 2.7, 97.7}, {51.3, 26.0, -37.4},
          {71.2, -54.2, 62.9}, {38.2, 13.3, -66.4},
          {71.0, 50.7, 68.6}  }
    },


// 8
// 9
// A
// B
// C
// D
// E
// F

};

/********************************************************************************/

// our global variables, just because it was quicker to write it this way
int gDataDepth = 8;
int gDataGridPoints = 17;

/********************************************************************************/

void ASSERT( bool condition, const char *message )
{
	if (!condition)
		fprintf(stderr,"%s\n", message);
	
//	assert( condition );
}

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
		ASSERT( currentL <= previousL, "bad interpolation of colors");
		}
#endif
}

/********************************************************************************/

const float XD50 = 96.4212;
const float YD50 = 100.0;
const float ZD50 = 82.5188;

float CIECurve( const float input )
{
	const float scale = 7.787037;	// pow( 29.0/6.0, 2.0) / 3.0;
	const float breakpoint = 0.008856;		// powf( 6.0/29.0, 3.0 );
	
	if (input > breakpoint)
		return powf( input, 1.0/3.0);
	else
		return (input * scale + (4.0/29.0));
}

/********************************************************************************/

float CIEReverseCurve( const float input )
{
	const float scale = 1.0 / 7.787037;		// 0.128418549  // 3.0 * pow( 6.0/29.0, 2.0);
	const float breakpoint = 6.0/29.0;
	
	if (input > breakpoint)
		return powf( input, 3.0);
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

inline xyzColor operator+( const xyzColor &a, const xyzColor &b)
{
	xyzColor result;
	result.X = a.X + b.X;
	result.Y = a.Y + b.Y;
	result.Z = a.Z + b.Z;
	return result;
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

// exponental interpolation - helps blues, hurts reds :-(
xyzColor expInterp2inks( const float t, const xyzColor &ink1, const xyzColor &ink2 )
{
	xyzColor result;

// should be equivelent
#if 1
	float factorX = ink2.X /ink1.X;
	float factorY = ink2.Y /ink1.Y;
	float factorZ = ink2.Z /ink1.Z;
	
	result.X = ink1.X * pow( factorX, t );
	result.Y = ink1.Y * pow( factorY, t );
	result.Z = ink1.Z * pow( factorZ, t );
#else
	float factorX = log(ink2.X/ink1.X);
	float factorY = log(ink2.Y/ink1.Y);
	float factorZ = log(ink2.Z/ink1.Z);
	
	result.X = ink1.X * exp( factorX * t );
	result.Y = ink1.Y * exp( factorY * t );
	result.Z = ink1.Z * exp( factorZ * t );
#endif
	
	return result;
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

color_list mix_one_spline( int steps, const labColor &paperColor, const labColor &inkColor, const labColor &combinedColor)
{
    int i;
	xyzColor mix;
	labColor mixLAB;
	color_list temp;
    
	xyzColor paperColorXYZ = LAB2XYZ( paperColor );
	xyzColor overprintColorXYZ = LAB2XYZ( combinedColor );
    
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
 
    // interp ink->combined
	for (i=(steps/2)+1; i < (steps-1); ++i)
		{
		float t = (float) (i - (steps/2)) / (float) (steps/2);
		mix = interp2inks( t, inkColorXYZ, overprintColorXYZ );
		mixLAB = XYZ2LAB( mix );
		temp.push_back( mixLAB );
		}
    
	// exact overprint
	temp.push_back( combinedColor);
 
    // error checking
    VerifyDecreasingL(temp);

    return temp;
}

/********************************************************************************/

bool labHueLess(const labColor &a, const labColor &b)
{
    float angle1 = M_PI + atan2(a.A,a.B);
    float angle2 = M_PI + atan2(b.A,b.B);
    return angle1 < angle2;
}

/********************************************************************************/

void subdivide_ink_splines( const inkColorSet &inkSet, const int divisions, const int steps, const labColor &ink1, const labColor &ink2, const xyzColor &paperColor, spline_list &splines )
{
	color_list temp;
	labColor mixLAB;
	xyzColor identity( 100.0, 100.0, 100.0 );

    xyzColor ink1Color = LAB2XYZ( ink1 );
    xyzColor ink1Filter = ink1Color / paperColor;
    xyzColor ink2Color = LAB2XYZ( ink2 );
    xyzColor ink2Filter = ink2Color / paperColor;
    xyzColor halfwayMix = ink1Filter * ink2Filter * paperColor;

#if 1
// this seems to work better everywhere
// interp isn't right, but full overprint isn't right, either
    xyzColor halfInterp = interp2inks( 0.5, ink1Color, ink2Color );
// TODO - find best parameter, 0.5 isn't enough, 1.0 is too much
    halfwayMix = interp2inks( 0.7, halfwayMix, halfInterp );
#endif
    
    // d == 0 is the last pure ink spline
    // d == division is this pure ink spline (handled elsewhere)
    for (int d = 1; d < divisions; ++d) {
        float t = (float)d / (float)divisions;

#if 1
// this works much better, so far
        xyzColor mix;
        if (t <= 0.5)
            mix = interp2inks( t*2.0, ink1Color, halfwayMix );
        else
            mix = interp2inks( (t-0.5)*2.0, halfwayMix, ink2Color );
#else
// TODO - this mix model is wrong
        xyzColor filter1 = interp2inks( (1-t), identity, ink1Filter );
        xyzColor filter2 = interp2inks( t, identity, ink2Filter );
        xyzColor mix = filter1 * filter2 * paperColor;
#endif

        mixLAB = XYZ2LAB( mix );
        temp = mix_one_spline( steps, inkSet.paperColor, mixLAB, inkSet.combinationColor );
        splines.push_back( temp );
    }
}

/********************************************************************************/

// create splines from mixes of inks and paper colors
// Need white -> ink -> black/overprint/combined

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
    std::sort(inkSet.primaries.begin(), inkSet.primaries.end(), labHueLess );

	xyzColor paperColor = LAB2XYZ( inkSet.paperColor );


    // If the combined color has 0 L, estimate it from primaries
    // so we can get something sort of realistic for the mix
    if (inkSet.combinationColor.L <= 0) {
        const float Ylimit = 1.3;
        
        mix = identity;
        for (size_t k = 0; k < inkCount; ++k) {
            xyzColor inkColor = LAB2XYZ( inkSet.primaries[k] );
            xyzColor inkFilter = inkColor / paperColor;
            mix *= inkFilter;
        }
        mix *= paperColor;
        
        // Bring down luminance if needed to give a reasonable result
        // Scaling all channels to reduce chroma of the overprint
        float maxVal = std::max( mix.X, std::max( mix.Y, mix.Z ));
        if (maxVal > Ylimit) {
            float scale = Ylimit / maxVal;
            mix *= scale;
        }
        
		mixLAB = XYZ2LAB( mix );
#if 1
        printf("Estimated overprint for %s is (%f, %f, %f)\n",
            inkSet.name.c_str(),
            mixLAB.L, mixLAB.A, mixLAB.B );
#endif
        
        inkSet.combinationColor = mixLAB;
    }
    

    // first ink spline, always calculated
    temp = mix_one_spline( steps, inkSet.paperColor, inkSet.primaries[0], inkSet.combinationColor );
	splines.push_back( temp );

    // iterate any additional inks, keeping splines in order
    for (size_t k = 1; k < inkCount; ++k) {
        subdivide_ink_splines( inkSet, divisions, steps,
            inkSet.primaries[k-1], inkSet.primaries[k],
            paperColor, splines);

        // pure ink spline paper->ink2->combined
        temp = mix_one_spline( steps, inkSet.paperColor, inkSet.primaries[k], inkSet.combinationColor );
        splines.push_back( temp );
    }
    
    // if we can make a solid, then wrap around from last ink to the first!
    if (inkCount > 2) {
        subdivide_ink_splines( inkSet, divisions, steps,
            inkSet.primaries[inkCount-1], inkSet.primaries[0],
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
	
	ASSERT( index < spline.size(), "BAD spline search");
	
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
	
	if (Linput < inkSet.combinationColor.L)
		{
		output = inkSet.combinationColor;
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

void SplineInterpList( const int subdivisions, const PointList &input, PointList &result,
                        bool wrapAround )
{
    const int pointCount = (int)input.size();
    
    result.reserve( subdivisions+1 );
    
	// iterate through list
	for (int i = 0; i <= subdivisions; ++i)
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

void LinearInterpList( const int subdivisions, const PointList &input, PointList &result,
                        bool wrapAround )
{
    const int pointCount = (int)input.size();
    
    result.reserve( subdivisions+1 );
    
    for (int i = 0; i <= subdivisions; ++i) {
        /// which input points are we between?
        float floatIndex = ((float)pointCount * i) / (float)subdivisions;
        int pointIndex = int( floatIndex );
        
        int p1 = pointIndex;
        int p2 = pointIndex + 1;

        if (wrapAround) {
            // treat list as a continuous shape
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

void PointListFromFloatSpline( const int subdivisions, const PointList &input, PointList &result,
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
	ASSERT( count > 0, "empty point list");
	
	for (size_t i = 0; i < count; ++i) {
		float distA = input.a - list[i].a;
		float distB = input.b - list[i].b;
		float dist = distA*distA + distB*distB;	// leave it squared
		
		if (dist < closest_dist) {
			closest_dist = dist;
			closest_index = i;
        }
    }
	
	ASSERT( closest_index >= 0, "failed to find point in list");
	return list[closest_index];
}

/********************************************************************************/

// interpolate between 0 and 100.0
float grid_to_L( int grid_value )
{
	return (100.0 * (float)grid_value) / (float)(gDataGridPoints - 1);
}

// ccox - FIX ME - cheap version for now -- refine if needed
float grid_to_AB( int grid_value )
{
	float middle = 0.5 * gDataGridPoints;
	return (127.0 * ((float)grid_value - middle)) / middle;
}

/********************************************************************************/

// convert 0..100 representation to file representation
int floatL_to_fileL( float L )
{
	if (L <= 0.0) return 0;
	if (L >= 100.0) return 255;
	return (int)( (255.0 / 100.0) * L + 0.5 );
}

// ccox - FIX ME - cheap version for now -- refine if needed
int floatAB_to_fileAB( float A )
{
	if (A > 127.0) return 255;
	if (A < -128.0) return 0;
	return (int)( A + 128.0 );
}

/********************************************************************************/

// really simple tent
inline float Smooth3( float a, float b, float c)
{
	return (a + 4*b + c) / 6.0;
}

// filter in place, in one dimension
// TODO - make this work with arbitrary channel counts!
void SmoothOneDirection( float *data, int planeStep, int rowStep, int colStep )
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

/********************************************************************************/

// useful for debugging, but slow
// probably faster to rasterize the poly without antialiasing
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

// currently creating LAB to LAB for color mapping
void create_table( FILE *output, const inkColorSet &inkSet, const spline_list &splines, int depth )
{
	int L, A, B;	// my grid iteration indices

	// allocate my grid
	float *gGridData = new float[ gDataGridPoints*gDataGridPoints*gDataGridPoints * 3 ];
	
	int planeStep = gDataGridPoints*gDataGridPoints * 3;
	int rowStep = gDataGridPoints * 3;
	int colStep = 3;
    
    size_t inkCount = inkSet.primaries.size();
    assert(inkCount > 0);
	
	for (L = 0; L < gDataGridPoints; ++L) {
		// setup slices variables
		float Lfloat = grid_to_L( L );
		
		//special case less < darkest and > brightest
		labColor clippedColor;
		if (ClippedL( Lfloat, clippedColor, inkSet)) {
			// fill with clipped value
			for (A = 0; A < gDataGridPoints; ++A)
				for (B = 0; B < gDataGridPoints; ++B) {
					// save the values
					gGridData[ L * planeStep + A * rowStep + B*colStep + 0 ] = clippedColor.L;
					gGridData[ L * planeStep + A * rowStep + B*colStep + 1 ] = clippedColor.A;
					gGridData[ L * planeStep + A * rowStep + B*colStep + 2 ] = clippedColor.B;
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
// TODO - needs more points for > number of inks
// maybe 100*inks?   50*inks?
        PointListFromFloatSpline( 50*inkCount, planeSpline, planePoints, (inkCount > 2) );


// DEBUG the last set generated to check the gamut shape and area
DumpPointList( std::string("pointlist_") + std::to_string(L), planePoints );


		// now iterate over this plane/slice
		for (A = 0; A < gDataGridPoints; ++A) {
			float Afloat = grid_to_AB( A );
			
			for (B = 0; B < gDataGridPoints; ++B) {
				float Bfloat = grid_to_AB( B );

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
				gGridData[ L * planeStep + A * rowStep + B*colStep + 0 ] = Lfloat;
				gGridData[ L * planeStep + A * rowStep + B*colStep + 1 ] = result.a;
				gGridData[ L * planeStep + A * rowStep + B*colStep + 2 ] = result.b;
				
				}   // end for B
			}   // end for A
		}   // end for L
	
	
#if 1
	// smooth the 3D table data
// TODO - make this work with arbitrary dimensions!
// TODO - make this work with arbitraray channel counts!
	SmoothOneDirection( gGridData, planeStep, rowStep, colStep );
	SmoothOneDirection( gGridData, rowStep, colStep, planeStep );
	SmoothOneDirection( gGridData, colStep, planeStep, rowStep );
#endif


// TODO - allow 16 bit output

    
	// write out the grid to our file
    fprintf(output,"P6\n%d %d\n%d\n", gDataGridPoints*gDataGridPoints, gDataGridPoints, ((depth == 16) ? 65535 : 255 ) );

#if 1
// easier to see while debugging
    for (A = 0; A < gDataGridPoints; ++A) {
        for (L = 0; L < gDataGridPoints; ++L) {
			for (B = 0; B < gDataGridPoints; ++B) {
#else
	for (L = 0; L < gDataGridPoints; ++L) {
		for (A = 0; A < gDataGridPoints; ++A) {
			for (B = 0; B < gDataGridPoints; ++B) {
#endif
				
				// convert to integer output values
				int Lout =   floatL_to_fileL( gGridData[ L * planeStep + A * rowStep + B*colStep + 0 ] );
				int Aout = floatAB_to_fileAB( gGridData[ L * planeStep + A * rowStep + B*colStep + 1 ] );
				int Bout = floatAB_to_fileAB( gGridData[ L * planeStep + A * rowStep + B*colStep + 2 ] );
				
				// write value out to file (interleaved)
				// FIX ME - ccox - currently 8 bit only
				putc( Lout, output );
				putc( Aout, output );
				putc( Bout, output );
            }
        }
    }
	
	
	// free our memory
	delete[] gGridData;

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
	

    for (auto &inkSet : colorSets) {
        
        std::string outFileName = inkSet.name + ".ppm";
        
        // create output file
        FILE *output = fopen( outFileName.c_str(), "wb" );
        if (output == NULL) {
            fprintf(stderr,"Could not create output\n");
            exit(-1);
        }
 
        // create splines from measured points using mixing model
        auto splines = mix_ink_splines( inkSet );
        
        // create and write color data
        create_table( output, inkSet, splines, gDataDepth );
        
        // done with the output file
        fclose( output );
    
     }  // end for colorSets

    return 0;
}

/********************************************************************************/
/********************************************************************************/
