//
//  MiniTIFF.hpp
//  MultiInkMapping
//
//  Copyright (c) 2026 Chris Cox
//  Created by Chris Cox on 3/3/26.
//

#ifndef MiniTIFF_hpp
#define MiniTIFF_hpp

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>

/******************************************************************************/

// tag types
enum {
    TIFF_BYTE = 1,
    TIFF_ASCII = 2,
    TIFF_SHORT = 3,
    TIFF_LONG = 4,
    TIFF_RATIO = 5,     // 2 longs
    TIFF_SBYTE = 6,
    TIFF_UNDEFINED = 7,
    TIFF_SSHORT = 8,
    TIFF_SLONG = 9,
    TIFF_SRATIO = 10,
    TIFF_FLOAT = 11,
    TIFF_DOUBLE = 12,
};

// tag values
enum {
    TIFF_SUBFILETYPE = 254,         //   usually 0
    TIFF_WIDTH = 256,               // required
    TIFF_HEIGHT = 257,              // required
    TIFF_BITSPERSAMPLE = 258,       // required
    TIFF_COMPRESSION = 259,         // required
    TIFF_INTERPRETATION = 262,      // required
    
    TIFF_STRIPOFFSETS = 273,        // required
    TIFF_SAMPLESPERPIXEL = 277,     // required
    TIFF_ROWSPERSTRIP = 278,        // required
    TIFF_STRIPBYTECOUNTS = 279,     // required
    
    TIFF_XRESOLUTION = 282,         // required
    TIFF_YRESOLUTION = 283,         // required
    TIFF_PLANARCONFIG = 284,        // required for > 1 channel, 1 = interleaved, 2 = not interleaved
    TIFF_RESOLUTIONUNIT = 296,      // required, 1=no unit, 2 = inch, 3 = cm
    
    TIFF_PREDICTOR = 317,           // 1 = none, 2 = horizontal difference
    TIFF_COLORMAP = 320,            // indexed color only
    TIFF_SAMPLE_FORMAT = 339,       // 1 = unsigned, 2 = signed, 3 = float, 4 = undefined
};

enum {
    TIFF_SAMPLE_UINT = 1,
    TIFF_SAMPLE_SINT = 2,
    TIFF_SAMPLE_FLOAT = 3,
    TIFF_SAMPLE_UNDEFINED = 4,
};

enum {
    TIFF_COMPRESS_NONE = 1,
    TIFF_COMPRESS_LZW = 5,
    // 6 was a bad JPEG attempt, and should not be used
    TIFF_COMPRESS_JPEG = 7,
    TIFF_COMPRESS_DEFLATE = 8,
};

enum {
    TIFF_MODE_GRAY_WHITEZERO = 0,
    TIFF_MODE_GRAY_BLACKZERO = 1,
    TIFF_MODE_RGB = 2,
    TIFF_MODE_RGB_PALETTE = 3,
    TIFF_MODE_MASK = 4,
    TIFF_MODE_CMYK = 5,
    TIFF_MODE_YCbCr = 6,
    TIFF_MODE_CIELAB = 8,
};

/******************************************************************************/

void WriteTIFF( const std::string &name, float dpi, int color_model, uint8_t *buffer,
                size_t width, size_t height, int channels, int depth );

/******************************************************************************/

#endif /* MiniTIFF_hpp */
