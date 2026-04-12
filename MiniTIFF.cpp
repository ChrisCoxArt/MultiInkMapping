//
//  MiniTIFF.cpp
//  MultiInkMapping
//  MIT License, Copyright (C) Chris Cox 2026
//
//  Writes a subset of TIFF files, uncompressed - but does what I need it to.
//
//  Created by Chris Cox on March 3, 2026.
//

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <cmath>
#include <memory>
#include <bit>
#include "MiniTIFF.hpp"

/******************************************************************************/

static
void putShort( uint16_t val, FILE *out )
{
    fwrite( &val, 2, 1, out );
}

static
void putShort( int16_t val, FILE *out )
{
    fwrite( &val, 2, 1, out );
}

static
void putLong( uint32_t val, FILE *out )
{
    fwrite( &val, 4, 1, out );
}

static
void putLong( int32_t val, FILE *out )
{
    fwrite( &val, 4, 1, out );
}

#if 0
// not supporting BIGTIFF yet, and don't need any other 8 byte quantities
static
void putLongLong( uint64_t val, FILE *out )
{
    fwrite( &val, 8, 1, out );
}

static
void putLongLong( int64_t val, FILE *out )
{
    fwrite( &val, 8, 1, out );
}
#endif

/******************************************************************************/

// unsigned 0..128..255 -> signed -127..0..127
static
void shiftTIFFLAB( uint8_t *in, size_t count )
{
    for ( size_t i = 0; i < count; ++i ) {
        size_t index = 3*i;
        uint8_t l = in[index+0];
        int a = in[index+1];
        int b = in[index+2];
        in[index+0] = l; // just copy
        in[index+1] = uint8_t(a - 128);
        in[index+2] = uint8_t(b - 128);
    }
}

/******************************************************************************/

// unsigned 0..32768..65535 -> signed -32767..0..32767
static
void shiftTIFFLAB( uint16_t *in, size_t count )
{
    for ( size_t i = 0; i < count; ++i ) {
        size_t index = 3*i;
        uint16_t l = in[index+0];
        int a = in[index+1];
        int b = in[index+2];
        
        in[index+0] = l; // just copy
        in[index+1] = uint16_t(a - 0x8000);
        in[index+2] = uint16_t(b - 0x8000);
    }
}

/******************************************************************************/

static
void putIFDLong( uint16_t tag, uint16_t type, uint32_t count, uint32_t value, FILE *out )
{
    uint16_t tagval = tag;
    uint16_t typeval = type;
    uint32_t countval = count;
    
    fwrite( &tagval, 2, 1, out );
    fwrite( &typeval, 2, 1, out );
    fwrite( &countval, 4, 1, out );
    fwrite( &value, 4, 1, out );
}

/******************************************************************************/

/// Write the image buffer to a TIFF (.tif) file
void WriteTIFF( const std::string &name, float dpi, int color_model, uint8_t *buffer,
                size_t width, size_t height, int channels, int depth )
{
    FILE *outfile = NULL;
    
    // see if we can create or update this filename
    if((outfile=fopen(name.c_str(),"wb"))==NULL) {
        fprintf(stderr,"Could not create output file %s\n", name.c_str());
        exit(-1);
    }
    
    // TIFF header, and byte order indicator
    if constexpr (std::endian::native == std::endian::big) {
        putc('M',outfile);
        putc('M',outfile);
    } else {
        putc('I',outfile);
        putc('I',outfile);
    }

    putShort( (int16_t)42, outfile );
    putLong( (int32_t)8, outfile );    // offset to first IFD, from start of file

    // IFD
    // number of entries
// TODO: make a data structure to store IFD entries, sort, then write values and offsets
    uint16_t tagCount = 15;
    putShort( tagCount, outfile );
    
    uint32_t width32 = uint32_t(width);
    putIFDLong( TIFF_WIDTH, TIFF_LONG, 1, width32, outfile );
    uint32_t height32 = uint32_t(height);
    putIFDLong( TIFF_HEIGHT, TIFF_LONG, 1, height32, outfile );
    
    uint16_t bits = (uint16_t) depth;
    
    uint32_t ifd_end = 8 + 2 + 4 + tagCount*12;
    uint32_t align_bytes = (4 - (ifd_end & 0x03)) & 0x03;
    uint32_t start_data = ifd_end + align_bytes;     // align to 4 byte boundary
    
    uint32_t bits_offset = start_data;
    uint32_t xres_offset = bits_offset + channels*2;
    uint32_t yres_offset = xres_offset + 8;
    
    // some readers break if the bitsPerSample is not a short value
    if (channels == 1)
        putIFDLong( TIFF_BITSPERSAMPLE, TIFF_LONG, 1, bits, outfile );
    else if (channels == 2)
        putIFDLong( TIFF_BITSPERSAMPLE, TIFF_SHORT, channels, ((uint32_t)bits << 16) | bits, outfile );
    else
        putIFDLong( TIFF_BITSPERSAMPLE, TIFF_SHORT, channels, bits_offset, outfile );

    putIFDLong( TIFF_COMPRESSION, TIFF_LONG, 1, TIFF_COMPRESS_NONE, outfile );

    size_t nrowBytes = (channels * width * depth + 7) / 8;
    assert( nrowBytes > 0 );
    size_t rowsPerBuffer = height;
    size_t stripCount = 1;
    
    size_t rowsPerStrip = rowsPerBuffer;
    //size_t stripBytes = rowsPerStrip * nrowBytes;
    
    uint32_t stripOffset_offset = yres_offset + 8;
    
    size_t stripCountSize = stripCount * 4;
    assert( (stripOffset_offset + stripCountSize) <= UINT_MAX );
    uint32_t stripByteCount_offset = uint32_t( stripOffset_offset + stripCountSize );
    
    assert( (stripByteCount_offset + stripCountSize) <= UINT_MAX );
    uint32_t pixelData_offset = uint32_t ( stripByteCount_offset + stripCountSize );

    putIFDLong( TIFF_INTERPRETATION, TIFF_LONG, 1, color_model, outfile );

    // using an offset for offsets and bytecounts trips up tiffinfo
    if (stripCount > 1)
        putIFDLong( TIFF_STRIPOFFSETS, TIFF_LONG, uint32_t(stripCount), stripOffset_offset, outfile );
    else
        putIFDLong( TIFF_STRIPOFFSETS, TIFF_LONG, 1, pixelData_offset, outfile );
    
    putIFDLong( TIFF_SAMPLESPERPIXEL, TIFF_LONG, 1, channels, outfile );
    putIFDLong( TIFF_ROWSPERSTRIP, TIFF_LONG, 1, uint32_t(rowsPerStrip), outfile );

    long byteCountOffset = ftell( outfile );
    if (stripCount > 1)
        putIFDLong( TIFF_STRIPBYTECOUNTS, TIFF_LONG, uint32_t(stripCount), stripByteCount_offset, outfile );
    else
        putIFDLong( TIFF_STRIPBYTECOUNTS, TIFF_LONG, 1, 0, outfile );

    uint32_t resDenom32 = 1000;
    uint32_t resRatio32 = dpi * resDenom32;
    putIFDLong( TIFF_XRESOLUTION, TIFF_RATIO, 1, xres_offset, outfile );
    putIFDLong( TIFF_YRESOLUTION, TIFF_RATIO, 1, yres_offset, outfile );
    putIFDLong( TIFF_PLANARCONFIG, TIFF_LONG, 1, 1, outfile );
    putIFDLong( TIFF_RESOLUTIONUNIT, TIFF_LONG, 1, 2, outfile );    // inches
    
    putIFDLong( TIFF_PREDICTOR, TIFF_LONG, 1, 1, outfile );    // no predictor
    
    if (bits == 32 || bits == 64)
        putIFDLong( TIFF_SAMPLE_FORMAT, TIFF_LONG, 1, TIFF_SAMPLE_FLOAT, outfile );
    else
        putIFDLong( TIFF_SAMPLE_FORMAT, TIFF_LONG, 1, TIFF_SAMPLE_UINT, outfile );

    putLong( (uint32_t)0, outfile );    // offset to next IFD
    
    // align to 4 byte boundary
    for (uint32_t i = 0; i < align_bytes; ++i)
        putc( 0, outfile );

// bits_offset:
    // bits per sample, because some readers break if it's just a byte instead of short
    for (int i = 0; i < channels; ++i)
        putShort( bits, outfile );
    
    // resolution ratios
// xres_offset:
    putLong( resRatio32, outfile );    // X dpi
    putLong( resDenom32, outfile );    // denominator

// yres_offset:
    putLong( resRatio32, outfile );    // Y dpi
    putLong( resDenom32, outfile );    // denominator

// stripOffset_offset
    // file with zeros, then backfill once we compress the strips
    for (size_t i = 0; i < stripCount; ++i)
        putLong( 0, outfile );

// stripByteCount_offset
    // file with zeros, then backfill once we compress the strips
    for (size_t i = 0; i < stripCount; ++i)
        putLong( 0, outfile );
    
// pixelData_offset:
    // Pixel Data

    std::vector<uint32_t> stripOffsetList( stripCount );
    std::vector<uint32_t> stripSizeList( stripCount );
    
    for (size_t strip = 0; strip < stripCount; ++strip) {
    
        size_t startRow = strip * rowsPerStrip;
        size_t rowCount = rowsPerStrip;
        if (startRow + rowsPerStrip > height)
            rowCount = height - startRow;

        //size_t stripBytes2 = rowCount * nrowBytes;
        size_t offset = startRow * nrowBytes;

        if (color_model == TIFF_MODE_CIELAB) {
            if (depth == 8)
                shiftTIFFLAB( buffer + offset, width * rowCount );
            else if (depth == 16)
                shiftTIFFLAB( (uint16_t *)(buffer + offset), width * rowCount );
        }

        long stripStart = ftell( outfile );
        
        fwrite( buffer, width*channels*(depth/8), rowCount, outfile );
        
        long stripEnd = ftell( outfile );
        assert( stripStart < UINT_MAX );
        stripOffsetList[strip] = uint32_t(stripStart);
        size_t len = stripEnd - stripStart;
        assert ( len < UINT_MAX );
        stripSizeList[strip] = uint32_t(len);

    }   // for strip

    // and update our strip offsets and sizes
    if (stripCount == 1) {
        fseek(outfile, byteCountOffset, SEEK_SET);
        uint32_t compressed = stripSizeList[0];
        putIFDLong( TIFF_STRIPBYTECOUNTS, TIFF_LONG, 1, compressed, outfile );
    } else {
        fseek(outfile, stripOffset_offset, SEEK_SET);
        for (size_t i = 0; i < stripCount; ++i)
            putLong( stripOffsetList[i], outfile );
    
        fseek(outfile, stripByteCount_offset, SEEK_SET);
        for (size_t i = 0; i < stripCount; ++i)
            putLong( stripSizeList[i], outfile );
    }

    // and close the file
    fclose(outfile);
}

/******************************************************************************/
/******************************************************************************/
