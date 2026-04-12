//
//  MiniICC.cpp
//  MultiInkMapping
//  MIT License, Copyright (C) Chris Cox 2026
//
//  Writes a subset of ICC Profiles - but does what I need it to.
//
//  Created by Chris Cox on March 4, 2026.
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
#include <map>
#include "MiniICC.hpp"

/********************************************************************************/

struct ICCTag {
    ICCTag() { data = NULL; pointsBackTo = 0; }
    ICCTag( uint32_t sig, uint32_t ds, uint8_t *dd ) :
            signature(sig), dataOffset(0), dataSize(ds), data(dd) { pointsBackTo = 0; }
    ICCTag( uint32_t sig, uint32_t p2 ) :
            signature(sig), dataOffset(0), dataSize(0), data(NULL), pointsBackTo(p2) {}

    uint8_t*    data;
    uint32_t    signature;
    uint32_t    pointsBackTo;
    uint32_t    dataOffset;    // from beginning of file, filled in when writing
    uint32_t    dataSize;    // in bytes
};

typedef std::vector<ICCTag>    tagList;

/********************************************************************************/

// This just adds some internal record keeping, without polluting the public structure
struct profileDataInner : public profileData {

public:
    // This is filled in as we add entries
    tagList tagInfo;

};

/********************************************************************************/

inline
uint32_t constexpr Align4( uint32_t x )
    {
    return ((x + 3) & ~3);
    }

inline
long constexpr Align4( long x )
    {
    return ((x + 3) & ~3);
    }

/********************************************************************************/

static
void padFile4( FILE *output )
    {
    const uint32_t zero = 0;
    
    long current = ftell( output );
    long needed = Align4( current );
    long missing = needed - current;
    
    if (missing)
        fwrite( &zero, missing, 1, output );
    }

/********************************************************************************/

// ICC Profiles are BigEndian
// because Endian Little Despise Programmers
// and Unreadable Is Endian Little that know Programmers

uint16_t constexpr SwabShort( uint16_t x )
{
    if constexpr (std::endian::native == std::endian::big) {
        return x;
    } else {
        return ( (((uint16_t)(x))>>8) | (((uint16_t)(x))<<8) );
    }
}

uint32_t constexpr SwabLong( uint32_t x )
{
    if constexpr (std::endian::native == std::endian::big) {
        return x;
    } else {
        return ( (((uint32_t)(x))>>24)
               | ((((uint32_t)(x))>>8)&0x0000FF00)
               | ((((uint32_t)(x))<<8)&0x00FF0000)
               | (((uint32_t)(x))<<24) );
    }
}

/********************************************************************************/

// MultiLocalizedUniCode
static
void add_mluc_tag( profileDataInner &data, uint32_t signature, const std::string &desc )
{
    const uint16_t iso_english = (uint16_t)((uint16_t)'e' << 8) | (uint16_t)'n';
    const uint16_t iso_unitedstates = (uint16_t)((uint16_t)'u' << 8) | (uint16_t)'s';

    // figure out the size we need
    // strings are ASCII, NULL terminated
    uint32_t stringLength = (uint32_t)desc.length() + 1;

    uint32_t myDataSize = 16 + 12 + 2*stringLength ;   // not using the other localizations
    uint8_t *myData = new uint8_t[ myDataSize ];
    
    memset( myData, 0, myDataSize );

    // fill in the data
    *((uint32_t *)(myData + 0)) = (uint32_t)SwabLong( icSigMultiLocalizedUnicodeType );    // type signature
    *((uint32_t *)(myData + 4)) = 0;                                // reserved
    *((uint32_t *)(myData + 8)) = (uint32_t)SwabLong(1);            // count of strings
    *((uint32_t *)(myData + 12)) = (uint32_t)SwabLong(12);            // record size, fixed
    
    *((uint16_t*)(myData+16)) = (uint16_t)SwabShort(iso_english);        // language
    *((uint16_t*)(myData+18)) = (uint16_t)SwabShort(iso_unitedstates);   // country
    *((uint32_t *)(myData + 20)) = (uint32_t)SwabLong( 2*stringLength ); // bytes in string
    *((uint32_t *)(myData + 24)) = (uint32_t)SwabLong( 28 );             // offset of string
    
    const char *bStringPtr = desc.c_str();
    uint16_t *wStringPtr = (uint16_t*)(myData+28);
    for (uint32_t i = 0; i < stringLength-1; ++i)
        wStringPtr[i] = (uint16_t)SwabShort( bStringPtr[i] );
    // already set to NULL for last bytes
    
    data.tagInfo.emplace_back( ICCTag( signature, myDataSize, myData ) );
}

/********************************************************************************/

#if 0
// Version 2 profiles
static
void add_description_tag( profileDataInner &data, uint32_t signature, const std::string &desc )
{
    // figure out the size we need
    // strings are ASCII, NULL terminated
    uint32_t stringLength = (uint32_t)desc.length() + 1;

    uint32_t myDataSize = 12 + stringLength + 8 + 3 + 67;   // not using the other types
    uint8_t *myData = new uint8_t[ myDataSize ];
    
    memset( myData, 0, myDataSize );

    // fill in the data
    *((uint32_t *)(myData +  0)) = (uint32_t)SwabLong( 'desc' );    // type signature
    *((uint32_t *)(myData +  4)) = 0;                                // reserved
    *((uint32_t *)(myData +  8)) = SwabLong( stringLength );        // reserved
    memcpy( myData + 12, desc.c_str(), stringLength );
    
    data.tagInfo.emplace_back( ICCTag( signature, myDataSize, myData ) );
}
#endif

/********************************************************************************/

static
void add_text_tag( profileDataInner &data, uint32_t signature, const std::string &text )
{
    // figure out the size we need
    // strings are ASCII, NULL terminated
    uint32_t stringLength = (uint32_t)text.length() + 1;

    uint32_t myDataSize = 8 + stringLength;
    uint8_t *myData = new uint8_t[ myDataSize ];
    
    memset( myData, 0, myDataSize );

    // fill in the data
    *((uint32_t *)(myData +  0)) = (uint32_t)SwabLong( icSigTextType );    // type signature
    *((uint32_t *)(myData +  4)) = 0;                                // reserved
    memcpy( myData + 8, text.c_str(), stringLength );

    data.tagInfo.emplace_back( ICCTag( signature, myDataSize, myData ) );
}

/********************************************************************************/

static
void add_xyz_tag( profileDataInner &data, uint32_t signature, int32_t X, int32_t Y, int32_t Z )
{
    // figure out the size we need
    uint32_t myDataSize = 8 + 12;
    uint8_t *myData = new uint8_t[ myDataSize ];
    
    memset( myData, 0, myDataSize );

    // fill in the data
    *((uint32_t *)(myData +  0)) = (uint32_t)SwabLong( kSpaceXYZ );    // type signature
    *((uint32_t *)(myData +  4)) = 0;                                // reserved
    *((uint32_t *)(myData +  8)) = (uint32_t) SwabLong( X );
    *((uint32_t *)(myData +  12)) = (uint32_t) SwabLong( Y );
    *((uint32_t *)(myData +  16)) = (uint32_t) SwabLong( Z );
    
    data.tagInfo.emplace_back( ICCTag( signature, myDataSize, myData ) );
}

/********************************************************************************/

static
void add_colorantTable_tag( profileDataInner &data, uint32_t signature, const std::vector< namedICCLABFloat > &colorants )
{
    // figure out the size we need
    uint32_t inkCount = (uint32_t)colorants.size();
    uint32_t myDataSize = 12 + 38*inkCount;
    uint8_t *myData = new uint8_t[ myDataSize ];
    
    memset( myData, 0, myDataSize );

    // fill in the data
    *((uint32_t *)(myData +  0)) = (uint32_t)SwabLong( signature );    // type signature
    *((uint32_t *)(myData +  4)) = 0;                                // reserved
    *((uint32_t *)(myData +  8)) = (uint32_t) SwabLong( inkCount );
    
    char *stringPtr = (char *)(myData + 12);
    uint16_t *labPtr = (uint16_t *)(myData + 12 + 32);
    
    for ( uint32_t i = 0; i < inkCount; ++i ) {
        strncpy( stringPtr, colorants[i].name.c_str(), 31 );
        uint16_t tempL = floatL_to_fileL65535(colorants[i].L);
        uint16_t tempA = floatAB_to_fileAB65535(colorants[i].a);
        uint16_t tempB = floatAB_to_fileAB65535(colorants[i].b);
        labPtr[0] = SwabShort(tempL);
        labPtr[1] = SwabShort(tempA);
        labPtr[2] = SwabShort(tempB);
        stringPtr += 38;
        labPtr += 38/2;
    }
    
    data.tagInfo.emplace_back( ICCTag( signature, myDataSize, myData ) );
}

/********************************************************************************/

// (gridPoints^input_channels) * output_channels
static
uint32_t calcClutSize( int inChannels, int outChannels, int gridPoints )
{
    uint32_t clutSize = gridPoints;
    
    switch (inChannels) {
    case 1:
        clutSize = gridPoints;
        break;
    case 2:
        clutSize = gridPoints*gridPoints;
        break;
    case 3:
        clutSize = gridPoints*gridPoints*gridPoints;
        break;
    case 4:
        clutSize = (gridPoints*gridPoints)*(gridPoints*gridPoints);
        break;
    default:
        clutSize = gridPoints;
        for (int i = 1; i < inChannels; ++i)
            clutSize *= gridPoints;
        break;
    }
    
    return clutSize * outChannels;
}

/********************************************************************************/

// currently written to use the same number of input and output channels
// assumes identity matrix and 1D LUTs
static
void add_lut16_tag( profileDataInner &data, uint32_t signature, int inChannels, int outChannels, int gridPoints, uint16_t* clut )
{
    const int lut_entries = 2;
    
    assert( inChannels >= 1 && inChannels <= 15 );
    assert( outChannels >= 1 && outChannels <= 15 );
    
    uint32_t clutSize = calcClutSize( inChannels, outChannels, gridPoints );
    clutSize *= 2;    // 16 bit
    
    uint32_t myDataSize = 52 + (2*inChannels*lut_entries) + clutSize + (2*outChannels*lut_entries);
    uint8_t *myData = new uint8_t[ myDataSize ];
    
    memset( myData, 0, myDataSize );

    // fill in the data
    *((uint32_t *)(myData +  0)) = (uint32_t)SwabLong( icSigLut16Type );    // type signature
    *((uint32_t *)(myData +  4)) = 0;       // reserved
    *(myData +  8) = inChannels;            // input
    *(myData +  9) = outChannels;           // output
    *(myData +  10) = gridPoints;
    *(myData +  11) = 0;                    // reserved
    
    // identity matrix (must be identity unless data is in XYZ)
    *((uint32_t *)(myData +  12)) = (uint32_t) SwabLong(0x00010000);                // matrix e00
    *((uint32_t *)(myData +  16)) = 0;                // matrix e01
    *((uint32_t *)(myData +  20)) = 0;                // matrix e02
    *((uint32_t *)(myData +  24)) = 0;                // matrix e10
    *((uint32_t *)(myData +  28)) = (uint32_t) SwabLong(0x00010000);                // matrix e11
    *((uint32_t *)(myData +  32)) = 0;                // matrix e12
    *((uint32_t *)(myData +  36)) = 0;                // matrix e20
    *((uint32_t *)(myData +  40)) = 0;                // matrix e21
    *((uint32_t *)(myData +  44)) = (uint32_t) SwabLong(0x00010000);                // matrix e22
    
    *((uint16_t *)(myData +  48)) = (uint16_t) SwabShort( lut_entries );            // input table entries
    *((uint16_t *)(myData +  50)) = (uint16_t) SwabShort( lut_entries );            // output table entries
    
    int current = 52;
    
    // identity input tables
    assert( lut_entries == 2 );
    
    // input_channels
    for (int j = 0; j < inChannels; ++j) {
        // simple 2 point LUT
        *((uint16_t *)(myData + current + j*4 + 0)) = 0;
        *((uint16_t *)(myData + current + j*4 + 2)) = (uint16_t) SwabShort( 65535 );
    }
    
    current += (inChannels*lut_entries*2);
    
    // LUT data assumed to already be in order!
    // RGB, XYZ, LAB, etc.
    memcpy( myData + current, clut, clutSize );
    
    // byte order swap!
    uint16_t *lutPtr = (uint16_t*)(myData + current);
    for (uint32_t i = 0; i < clutSize/2; ++i)
        lutPtr[i] = SwabShort( lutPtr[i] );
    
    current += clutSize;
    
    // identity output tables
    // output_channels
    for (int j = 0; j < outChannels; ++j) {
        // simple 2 point LUT
        *((uint16_t *)(myData + current + j*4 + 0)) = 0;
        *((uint16_t *)(myData + current + j*4 + 2)) = (uint16_t) SwabShort( 65535 );
    }
    
    current += (outChannels*lut_entries*2);
    
    assert( current == myDataSize );
    
    data.tagInfo.emplace_back( ICCTag( signature, myDataSize, myData ) );
}

/********************************************************************************/

// currently written to use the same number of input and output channels
// assumes identity matrix and 1D LUTs
static
void add_lut8_tag( profileDataInner &data, uint32_t signature, int inChannels, int outChannels, int gridPoints, uint8_t* clut )
{
    assert( inChannels >= 1 && inChannels <= 15 );
    assert( outChannels >= 1 && outChannels <= 15 );
    
    uint32_t clutSize = calcClutSize( inChannels, outChannels, gridPoints );
    
    uint32_t myDataSize = 48 + (inChannels*256) + clutSize + (outChannels*256);
    uint8_t *myData = new uint8_t[ myDataSize ];
    
    memset( myData, 0, myDataSize );

    // fill in the data
    *((uint32_t *)(myData +  0)) = (uint32_t)SwabLong( icSigLut8Type );    // type signature
    *((uint32_t *)(myData +  4)) = 0;                                // reserved
    *(myData +  8) = inChannels;            // input
    *(myData +  9) = outChannels;            // output
    *(myData +  10) = gridPoints;
    *(myData +  11) = 0;                            // reserved
    
    // identity matrix (must be identity unless data is in XYZ)
    *((uint32_t *)(myData +  12)) = (uint32_t) SwabLong(0x00010000);                // matrix e00
    *((uint32_t *)(myData +  16)) = 0;                // matrix e01
    *((uint32_t *)(myData +  20)) = 0;                // matrix e02
    *((uint32_t *)(myData +  24)) = 0;                // matrix e10
    *((uint32_t *)(myData +  28)) = (uint32_t) SwabLong(0x00010000);                // matrix e11
    *((uint32_t *)(myData +  32)) = 0;                // matrix e12
    *((uint32_t *)(myData +  36)) = 0;                // matrix e20
    *((uint32_t *)(myData +  40)) = 0;                // matrix e21
    *((uint32_t *)(myData +  44)) = (uint32_t) SwabLong(0x00010000);                // matrix e22
    
    int current = 48;
    
    // identity input tables
    
    // input_channels
    for (int j = 0; j < inChannels; ++j)
        for (int i = 0; i < 256; ++i)
            myData[ current + j*256 + i ] = (uint8_t) i;
    
    current += (inChannels*256);
    
    // LUT data assumed to already be in order!
    // RGB, XYZ, LAB, etc.
    memcpy( myData + current, clut, clutSize );
    
    current += clutSize;
    
    // identity output tables
    // output_channels
    for (int j = 0; j < outChannels; ++j)
        for (int i = 0; i < 256; ++i)
            myData[ current + j*256 + i ] = (uint8_t) i;
    
    current += (outChannels*256);
    
    assert( current == myDataSize );

    data.tagInfo.emplace_back( ICCTag( signature, myDataSize, myData ) );
}

/********************************************************************************/

static
void write_tags_binary( profileDataInner &data, FILE *output )
{
    uint32_t currentOffset = 128;    // just after the header
    
    // account for the size of thet tag table
    currentOffset += 4 + (12 * data.tagInfo.size());
    
    // and align to a 4 byte boundary
    currentOffset = Align4(currentOffset);
    
    // write the tag table
    uint32_t count = (uint32_t)data.tagInfo.size();
    count = SwabLong( count );
    fwrite( &count, 4, 1, output );
    
    for ( auto &iter : data.tagInfo ) {
        assert( (currentOffset & 0x03) == 0 );
        
        uint32_t signature = SwabLong( iter.signature );
        fwrite( &signature, 4, 1, output );
  
        iter.dataOffset = currentOffset;
        if (iter.pointsBackTo != 0) {
            // find previous tag and copy offset and size from it
            iter.dataSize = 0;
            for (auto &prev : data.tagInfo) {
                if (prev.signature == iter.pointsBackTo) {
                    iter.dataOffset = prev.dataOffset;
                    iter.dataSize = prev.dataSize;
                    break;
                }
            }
        assert( iter.dataSize != 0 );
        }
        
        uint32_t offset = SwabLong( iter.dataOffset );
        fwrite( &offset, 4, 1, output );
        
        uint32_t size = SwabLong( iter.dataSize );
        fwrite( &size, 4, 1, output );
        
        currentOffset += iter.dataSize;
        currentOffset = Align4(currentOffset);
    }
    
    assert( (ftell( output ) & 0x03) == 0 );
    padFile4( output );
    

    // write the tag data itself
    for ( auto &iter : data.tagInfo ) {
        if (iter.pointsBackTo != 0)
            continue;
        
        fwrite( iter.data, iter.dataSize, 1, output );
        padFile4( output );
    }
    
    assert( (ftell( output ) & 0x03) == 0 );
}

/********************************************************************************/

static
void create_tags( profileDataInner &data )
{

    // V2 and up always required: description, copyright, mediawhitepoint
    // V4 chromaticAdaptation  9.2.11 ????  Only required if adapted, which we won't be
    add_mluc_tag( data, icSigProfileDescriptionTag, data.description );
    add_mluc_tag( data, icSigCopyrightTag, data.copyright );
 
    if (data.optionalNoteText.length() != 0)
        add_text_tag( data, icSigNote, data.optionalNoteText );
    
    // white point
    add_xyz_tag( data, icSigMediaWhitePointTag, 0x0000F6D6, 0x00010000, 0x0000D32D );    // D50
    
    // change the rest based on type of profile
    if ( data.profileClass == kClassAbstract ) {
        
        assert( data.colorSpace == kSpaceLAB || data.colorSpace == kSpaceXYZ );
        assert( data.pcsSpace == kSpaceLAB || data.pcsSpace == kSpaceXYZ );
        assert( data.colorSpace == data.pcsSpace );
        
        // A2B0 only
    }
    else if ( data.profileClass == kClassDeviceLink ) {
        
        assert( data.colorSpace == kSpaceRGB || data.colorSpace == kSpaceCMYK );
        assert( data.pcsSpace == kSpaceRGB || data.pcsSpace == kSpaceCMYK );
        // colorspace = FIRST profile in sequence
        // pcs = colorspace of LAST profile in sequence
        
        // try leaving the sequence out - because we don't really have a sequence
        //    profileSequenceDesc        'pseq'    v2 6.3.4.1, 6.5.12,    V4? 9.2.32, 10.16
        // NOTE - so far fake/empty profilesequence tags break one thing or another
        
        // A2B0 only
        
        // V4 ColorantTable 9.2.14  if xCLR
        // V4 colorantTableOutTag  9.2.14.1 if xCLR
    }
    else if ( data.profileClass == kClassOutput ) {
        // A2B0,A2B1,A2B2
        // B2A0,B2A1,B2A2
        // gamut
        
        // V4 ColorantTable 9.2.14, 10.4 definition
        
        // V4 single channel shall have a grayTRC tag  9.2.19 ????
    }
    else if ( data.profileClass == kClassSpace ) {
        // A2B0
        // B2A0
    }
    else {
        fprintf(stderr,"Other profile types not implemented yet\n");
        return;
    }
    
    // add colorant tables
    for ( auto &clrTable : data.colorantTables )
        add_colorantTable_tag( data, clrTable.tableSig, clrTable.colorants );

    // add tables
    for ( auto &table : data.LUTtables ) {
        if (table.pointsBackTo != icSigUnknown) {
            // special case pointing back to a previous table
            data.tagInfo.emplace_back( ICCTag( table.tableSig, table.pointsBackTo ) );
            continue;
        }
    
        if (table.tableDepth == 8)
            add_lut8_tag( data, table.tableSig, table.tableDimensions, table.tableChannels,
                        table.tableGridPoints, table.tableData.get() );
        else if (table.tableDepth == 16)
            add_lut16_tag( data, table.tableSig, table.tableDimensions, table.tableChannels,
                        table.tableGridPoints, (uint16_t *)table.tableData.get() );
    }

}

/********************************************************************************/

static
void write_header_binary( const profileDataInner &data, FILE *output )
{
    uint32_t temp = 0;
    
    // 4 bytes size - write a dummy value now, then fill it in at end
    temp = 0;
    fwrite( &temp, 4, 1, output );
    
    // 4 byte preferred CMM == IccDev
    uint32_t CMM_type = data.preferredCMM; // 'ICCD', iccDev
    temp = SwabLong( CMM_type );
    fwrite( &temp, 4, 1, output );
    
    // 4 byte profile version number
    uint32_t profile_version = 0x04200000;        // 4.2.0 fixed for now
    temp = SwabLong( profile_version );
    fwrite( &temp, 4, 1, output );
    
    // 4 byte profile class
    temp = SwabLong( data.profileClass );
    fwrite( &temp, 4, 1, output );
    
    // 4 byte color space of data
    temp = SwabLong( data.colorSpace );
    fwrite( &temp, 4, 1, output );
    
    // 4 byte PCS space
    temp = SwabLong( data.pcsSpace );
    fwrite( &temp, 4, 1, output );
    
    // 12 bytes data/time created - using gm time since nothing else really makes sense
    /*  binary numbers
        2 bytes year
        2 bytes month
        2 bytes day
        2 bytes hour
        2 bytes minute
        2 bytes second
    */
    time_t now;
    (void)time( &now );
    struct tm *timeData = gmtime( &now );
    uint16_t year = timeData->tm_year + 1900;
    uint16_t month = timeData->tm_mon + 1;
    uint16_t day = timeData->tm_mday;
    uint16_t hour = timeData->tm_hour;
    uint16_t minute = timeData->tm_min;
    uint16_t second = timeData->tm_sec;
    
    year = SwabShort( year );
    fwrite( &year, 2, 1, output );
    month = SwabShort( month );
    fwrite( &month, 2, 1, output );
    day = SwabShort( day );
    fwrite( &day, 2, 1, output );
    hour = SwabShort( hour );
    fwrite( &hour, 2, 1, output );
    minute = SwabShort( minute );
    fwrite( &minute, 2, 1, output );
    second = SwabShort( second );
    fwrite( &second, 2, 1, output );
    
    // 4 byte signature 'acsp' -- MagicNumber for ICC profiles
    uint32_t signature = icSigMagicNumber;
    temp = SwabLong( signature );
    fwrite( &temp, 4, 1, output );
    
    // 4 byte platform signature
    uint32_t platform = data.platform;
    temp = SwabLong( platform );
    fwrite( &temp, 4, 1, output );
    
    // 4 byte cmm flags
    uint32_t flags = 0;
    temp = SwabLong( flags );
    fwrite( &temp, 4, 1, output );
    
    // 4 byte manufacturer
    uint32_t manufacturer = data.manufacturer;
    temp = SwabLong( manufacturer );
    fwrite( &temp, 4, 1, output );
    
    // 4 byte device model
    uint32_t model = 0;
    temp = SwabLong( model );
    fwrite( &temp, 4, 1, output );
    
    // 8 bytes device attributes
    uint32_t deviceAttributes1 = 0;
    uint32_t deviceAttributes2 = 0;
    temp = SwabLong( deviceAttributes1 );
    fwrite( &temp, 4, 1, output );
    temp = SwabLong( deviceAttributes2 );
    fwrite( &temp, 4, 1, output );
    
    // 4 byte default rendering intent
    uint32_t intent = 1;            // relative colorimetric
    temp = SwabLong( intent );
    fwrite( &temp, 4, 1, output );
    
    // 12 bytes D50 white point - fixed value
    uint32_t whitePoint1 = 0x0000F6D6;
    uint32_t whitePoint2 = 0x00010000;
    uint32_t whitePoint3 = 0x0000D32D;
    temp = SwabLong( whitePoint1 );
    fwrite( &temp, 4, 1, output );
    temp = SwabLong( whitePoint2 );
    fwrite( &temp, 4, 1, output );
    temp = SwabLong( whitePoint3 );
    fwrite( &temp, 4, 1, output );
    
    // 4 byte creator signature
    uint32_t creator = data.creator;
    temp = SwabLong( creator );
    fwrite( &temp, 4, 1, output );
    
    // 16 bytes Profile ID
    uint8_t profileID[16];
    memset( profileID, 0, 16 );        // zero = uncalculated
    fwrite( profileID, 16, 1, output );
    
    // 28 bytes set to zero
    uint8_t padding[28];
    memset( padding, 0, 28 );
    fwrite( padding, 28, 1, output );
    
    // verify that we wrote exactly 128 bytes of header
    assert( ftell( output ) == 128 );
}

/******************************************************************************/
/******************************************************************************/

std::string OSTypeToString( uint32_t value )
{
    // break it apart, because byte order is important
    char byte0 = ((value >> 24) & 0xFF);
    char byte1 = ((value >> 16) & 0xFF);
    char byte2 = ((value >>  8) & 0xFF);
    char byte3 = ((value >>  0) & 0xFF);
    
    char byteArray[] = { byte0, byte1, byte2, byte3, 0 };
    return std::string(byteArray);
}

/********************************************************************************/

std::string getTimeString(time_t &timeData)
{
    char timeString[] = "yyyy-mm-ddThh:mm:ssZ";
    std::strftime( timeString, std::size(timeString), "%FT%TZ", std::gmtime(&timeData));
    return timeString;
}

/********************************************************************************/

// because XML uses special names for some tagtypes
std::string tag2XML( uint32_t tagName )
{
    std::map<uint32_t,std::string> tagNameLookup = {
        { icSigAToB0Tag, "AToB0Tag" },
        { icSigAToB1Tag, "AToB1Tag" },
        { icSigAToB2Tag, "AToB2Tag" },
        { icSigAToB3Tag, "AToB3Tag" },
        
        { icSigBToA0Tag, "BToA0Tag" },
        { icSigBToA1Tag, "BToA1Tag" },
        { icSigBToA2Tag, "BToA2Tag" },
        { icSigBToA3Tag, "BToA3Tag" },
        
        { icSigGamutTag, "gamutTag" },
    };
    
    auto iter = tagNameLookup.find( tagName );
    if ( iter == tagNameLookup.end() ) {
        fprintf(stderr,"ERROR - Unknown tag name translation %s\n", OSTypeToString(tagName).c_str() );
        return std::string("unknown");
    }
    
    return iter->second;
}

/********************************************************************************/

static
void add_colorantTable_xml( const profileDataInner &data, FILE *output, uint32_t signature,
                        const std::vector< namedICCLABFloat > &colorants )
{
    fprintf(output, "<colorantTableTag> <colorantTableType>\n\t<ColorantTable>\n");
    
    for ( uint32_t i = 0; i < colorants.size(); ++i ) {
        fprintf(output,"\t  <Colorant Name=\"%s\" Channel1=\"%f\" Channel2=\"%f\" Channel3=\"%f\"/>\n",
            colorants[i].name.c_str(),
            colorants[i].L, colorants[i].a, colorants[i].b );
    }
    
    fprintf(output, "\t</ColorantTable>\n</colorantTableType> </colorantTableTag>\n");
}

/********************************************************************************/

// currently written to use the same number of input and output channels
// assumes identity matrix and 1D LUTs
static
void add_lut8_xml( const profileDataInner &data, FILE *output, uint32_t signature,
                    int inChannels, int outChannels, int gridPoints, uint8_t* clut )
{
    const int colLimit = 200;

    assert( inChannels >= 1 && inChannels <= 15 );
    assert( outChannels >= 1 && outChannels <= 15 );
    
    auto sigString = tag2XML( signature );
    fprintf(output, "<%s> <lut8Type>\n", sigString.c_str() );
    fprintf(output, "  <Channels InputChannels=\"%d\" OutputChannels=\"%d\"/>\n",
                inChannels, outChannels );
    
    //bool isInput = (signature >> 24) == 'A';

    fprintf(output, "  <BCurves>\n");
    for (int i = 0; i < inChannels; ++i)
        fprintf(output, "    <Curve IdentitySize=\"256\"/>\n");
    fprintf(output, "  </BCurves>\n" );

    fprintf(output, "  <ACurves>\n" );
    for (int i = 0; i < outChannels; ++i)
        fprintf(output, "    <Curve IdentitySize=\"256\"/>\n");
    fprintf(output, "  </ACurves>\n" );

    fprintf(output, "  <CLUT GridGranularity=\"%d\">\n", gridPoints );
    fprintf(output, "    <TableData>\n");
    
    uint32_t clutSize = calcClutSize( inChannels, 1, gridPoints );
    
    fprintf(output,"    ");
    for (int k = 0, col = 0; k < clutSize; ++k) {
        for (int c = 0; c < outChannels; ++c) {
            uint8_t value = clut[ k*outChannels + c ];
            fprintf(output,"%3u ", value );
        }
        
        // line break!
        col += outChannels * 4;
        if (col > colLimit) {
            fprintf(output,"\n    ");
            col = 0;
        }
    }

    fprintf(output, "\n    </TableData>\n");
    fprintf(output, "  </CLUT>\n");
    
    fprintf(output, "</lut8Type> </%s>\n", sigString.c_str() );
}

/********************************************************************************/

// currently written to use the same number of input and output channels
// assumes identity matrix and 1D LUTs
static
void add_lut16_xml( const profileDataInner &data, FILE *output, uint32_t signature,
                    int inChannels, int outChannels, int gridPoints, uint16_t* clut )
{
    const int colLimit = 200;

    assert( inChannels >= 1 && inChannels <= 15 );
    assert( outChannels >= 1 && outChannels <= 15 );
    
    auto sigString = tag2XML( signature );
    fprintf(output, "<%s> <lut16Type>\n", sigString.c_str() );
    fprintf(output, "  <Channels InputChannels=\"%d\" OutputChannels=\"%d\"/>\n",
                inChannels, outChannels );
    
    //bool isInput = (signature >> 24) == 'A';

    fprintf(output, "  <BCurves>\n");
    for (int i = 0; i < inChannels; ++i)
        fprintf(output, "    <Curve IdentitySize=\"256\"/>\n");
    fprintf(output, "  </BCurves>\n" );

    fprintf(output, "  <ACurves>\n" );
    for (int i = 0; i < outChannels; ++i)
        fprintf(output, "    <Curve IdentitySize=\"256\"/>\n");
    fprintf(output, "  </ACurves>\n" );

    fprintf(output, "  <CLUT GridGranularity=\"%d\">\n", gridPoints );
    fprintf(output, "    <TableData>\n");
    
    uint32_t clutSize = calcClutSize( inChannels, 1, gridPoints );
    
    fprintf(output,"    ");
    for (int k = 0, col = 0; k < clutSize; ++k) {
        for (int c = 0; c < outChannels; ++c) {
            uint16_t value = clut[ k*outChannels + c ];
            fprintf(output,"%5u ", value );
        }
        
        // line break!
        col += outChannels * 6;
        if (col > colLimit) {
            fprintf(output,"\n    ");
            col = 0;
        }
    }

    fprintf(output, "\n    </TableData>\n");
    fprintf(output, "  </CLUT>\n");
    
    fprintf(output, "</lut16Type> </%s>\n", sigString.c_str() );

}

/********************************************************************************/

static
void write_header_xml( const profileDataInner &data, FILE *output )
{
    fprintf(output, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(output, "<IccProfile>\n");
    fprintf(output, "<Header>\n");
    fprintf(output, "<ProfileVersion>4.20</ProfileVersion>\n");     // 4.2.0 fixed for now
    
    auto classString = OSTypeToString( data.profileClass );
    fprintf(output, "<ProfileDeviceClass>%4s</ProfileDeviceClass>\n", classString.c_str() );

    auto colorspaceString = OSTypeToString( data.colorSpace );
    fprintf(output, "<DataColourSpace>%4s</DataColourSpace>\n", colorspaceString.c_str() );
    
    auto pcsString = OSTypeToString( data.pcsSpace );
    fprintf(output, "<PCS>%4s</PCS>\n", pcsString.c_str() );
    
    auto platformString = OSTypeToString( data.platform );
    fprintf(output, "<PrimaryPlatform>%4s</PrimaryPlatform>\n", platformString.c_str() );
    
    auto cmmString = OSTypeToString( data.preferredCMM );
    fprintf(output, "<PreferredCMMType>%4s</PreferredCMMType>\n", cmmString.c_str() );
    
    auto manufacturerString = OSTypeToString( data.manufacturer );
    fprintf(output, "<DeviceManufacturer>%4s</DeviceManufacturer>\n", manufacturerString.c_str() );
    
    auto creatorString = OSTypeToString( data.creator );
    fprintf(output, "<ProfileCreator>%4s</ProfileCreator>\n", creatorString.c_str() );

    fprintf(output, "<ProfileFlags EmbeddedInFile=\"false\" UseWithEmbeddedDataOnly=\"false\"/>\n");
    fprintf(output, "<DeviceAttributes ReflectiveOrTransparency=\"reflective\" GlossyOrMatte=\"glossy\" MediaPolarity=\"positive\" MediaColour=\"colour\"/>\n");     // can this be left out?

    fprintf(output, "<RenderingIntent>Relative Colorimetric</RenderingIntent>\n");
    fprintf(output, "<PCSIlluminant> <XYZNumber X=\"0.964202880859\" Y=\"1.00\" Z=\"0.824905395508\"/> </PCSIlluminant>\n");    // D50 fixed value, based on V4 binary values

    time_t now;
    (void)time( &now );
    auto timeFormat = getTimeString(now);
    fprintf(output, "<CreationDateTime>%s</CreationDateTime>\n", timeFormat.c_str() );
    
    fprintf(output, "</Header>\n");
}

/********************************************************************************/

static
void write_footer_xml( const profileDataInner &data, FILE *output )
{
    fprintf(output, "</IccProfile>\n");
}

/********************************************************************************/

static
void write_tags_xml( const profileDataInner &data, FILE *output )
{
    fprintf(output, "<Tags>\n");
    
    fprintf(output, "<profileDescriptionTag> <multiLocalizedUnicodeType>\n");
    fprintf(output, "<LocalizedText LanguageCountry=\"enus\"><![CDATA[%s]]></LocalizedText>\n", data.description.c_str() );
    fprintf(output, "</multiLocalizedUnicodeType> </profileDescriptionTag>\n");

    fprintf(output, "<copyrightTag> <multiLocalizedUnicodeType>\n");
    fprintf(output, "<LocalizedText LanguageCountry=\"enus\"><![CDATA[%s]]></LocalizedText>\n", data.copyright.c_str() );
    fprintf(output, "</multiLocalizedUnicodeType> </copyrightTag>\n");

    if (data.optionalNoteText.length() != 0)
        fprintf(output, "<note><![CDATA[%s]]></note>\n", data.optionalNoteText.c_str() );

    fprintf(output, "<mediaWhitePointTag> <XYZArrayType>\n");
    fprintf(output, "<XYZNumber X=\"0.964202880859\" Y=\"1.00\" Z=\"0.824905395508\"/>\n"); // D50 fixed value, based on V4 binary values
    fprintf(output, "</XYZArrayType> </mediaWhitePointTag>\n");

    if ( data.profileClass == kClassAbstract ) {
        
        assert( data.colorSpace == kSpaceLAB || data.colorSpace == kSpaceXYZ );
        assert( data.pcsSpace == kSpaceLAB || data.pcsSpace == kSpaceXYZ );
        assert( data.colorSpace == data.pcsSpace );
        
        // A2B0 only
    }
    else if ( data.profileClass == kClassDeviceLink ) {
        
        assert( data.colorSpace == kSpaceRGB || data.colorSpace == kSpaceCMYK );
        assert( data.pcsSpace == kSpaceRGB || data.pcsSpace == kSpaceCMYK );
        // colorspace = FIRST profile in sequence
        // pcs = colorspace of LAST profile in sequence
        
        // try leaving the sequence out - because we don't really have a sequence
        //    profileSequenceDesc        'pseq'    v2 6.3.4.1, 6.5.12,    V4? 9.2.32, 10.16
        // NOTE - so far fake/empty profilesequence tags break one thing or another
        
        // A2B0 only
        
        // V4 ColorantTable 9.2.14  if xCLR
        // V4 colorantTableOutTag  9.2.14.1 if xCLR
    }
    else if ( data.profileClass == kClassOutput ) {
        // A2B0,A2B1,A2B2
        // B2A0,B2A1,B2A2
        // gamut
        
        // V4 ColorantTable 9.2.14, 10.4 definition
        
        // V4 single channel shall have a grayTRC tag  9.2.19 ????
    }
    else if ( data.profileClass == kClassSpace ) {
        // A2B0
        // B2A0
    }
    else {
        fprintf(stderr,"Other profile types not implemented yet\n");
        return;
    }


    // add colorant tables
    for ( auto &clrTable : data.colorantTables )
        add_colorantTable_xml( data, output, clrTable.tableSig, clrTable.colorants );

    // add tables
    for ( auto &table : data.LUTtables ) {
        if (table.pointsBackTo != icSigUnknown) {
            // special case pointing back to a previous table
            auto sig = tag2XML( table.tableSig );
            auto backTo = tag2XML( table.pointsBackTo );
            fprintf(output, " <%s SameAs=\"%s\"/>\n", sig.c_str(), backTo.c_str());
            continue;
        }

        if (table.tableDepth == 8)
            add_lut8_xml( data, output, table.tableSig, table.tableDimensions, table.tableChannels,
                        table.tableGridPoints, table.tableData.get() );
        else if (table.tableDepth == 16)
            add_lut16_xml( data, output, table.tableSig, table.tableDimensions, table.tableChannels,
                        table.tableGridPoints, (uint16_t *)table.tableData.get() );
    }

    
    fprintf(output, "</Tags>\n");
}

/********************************************************************************/

static
void add_colorantTable_json( const profileDataInner &data, FILE *output, uint32_t signature,
                        const std::vector< namedICCLABFloat > &colorants )
{
    fprintf(output, "{ \"colorantTableTag\": {\n");
    fprintf(output, "\"data\": {\n");
    fprintf(output, "\"type\": \"colorantTableType\",\n");
    fprintf(output, "\"colorantTable\": [\n");
    
    for ( uint32_t i = 0; i < colorants.size(); ++i ) {

// ALERT, BUG, TODO -- currently using 16 bit values not float
#if 1
        uint16_t tempL = floatL_to_fileL65535(colorants[i].L);
        uint16_t tempA = floatAB_to_fileAB65535(colorants[i].a);
        uint16_t tempB = floatAB_to_fileAB65535(colorants[i].b);
        fprintf(output, "\t{ \"name\": \"%s\", \"pcs\": [ %u, %u, %u ] }",
            colorants[i].name.c_str(),
            tempL, tempA, tempB );
#else
        fprintf(output, "\t{ \"name\": \"%s\", \"pcs\": [ %f, %f, %f ] }",
            colorants[i].name.c_str(),
            colorants[i].L, colorants[i].a, colorants[i].b );
#endif

        if (i < (colorants.size()-1))
            fprintf(output, ",\n");
        else
            fprintf(output, "\n");
    }
    
    fprintf(output, "] }\n"); // end array, end data
    fprintf(output, "} },\n");   // end colorant table
}

/********************************************************************************/

// currently written to use the same number of input and output channels
// assumes identity matrix and 1D LUTs
static
void add_lut8_json( const profileDataInner &data, FILE *output, uint32_t signature,
                    int inChannels, int outChannels, int gridPoints, uint8_t* clut )
{
    const int colLimit = 200;

    assert( inChannels >= 1 && inChannels <= 15 );
    assert( outChannels >= 1 && outChannels <= 15 );
    
    auto sigString = tag2XML( signature );
    fprintf(output, "{ \"%s\": {\n", sigString.c_str() );
    fprintf(output, "\"data\": {\n");

    fprintf(output, "\"inputChannels\": %d,\n", inChannels );
    fprintf(output, "\"outputChannels\": %d,\n", outChannels );
    fprintf(output, "\"type\": \"lut8Type\",\n" );
    

    fprintf(output, "\"aCurves\": [\n" );
    for (int i = 0; i < outChannels; ++i) {
        fprintf(output, "{ \"curveType\": \"table\", \"precision\": 1, \"type\": \"Curve\",\n");
        
// ALERT, BUG, TODO -- have to write out all values
        fprintf(output,"\"table\": [ ");
        for (int j = 0; j < 256; ++j) {
            fprintf(output,"%d", j );
            if (j < (256-1))
                fprintf(output,", ");
        }
        fprintf(output," ] }");
        
        if (i < (outChannels-1))
            fprintf(output,",\n");
        else
            fprintf(output,"\n");
    }
    fprintf(output, "],\n" );
    
    
    
    fprintf(output, "\"bCurves\": [\n" );
    for (int i = 0; i < inChannels; ++i) {
        fprintf(output, "{ \"curveType\": \"table\", \"precision\": 1, \"type\": \"Curve\",\n");
        
// ALERT, BUG, TODO -- have to write out all values
        fprintf(output,"\"table\": [ ");
        for (int j = 0; j < 256; ++j) {
            fprintf(output,"%d", j );
            if (j < (256-1))
                fprintf(output,", ");
        }
        fprintf(output," ] }");
        
        if (i < (inChannels-1))
            fprintf(output,",\n");
        else
            fprintf(output,"\n");
    }
    fprintf(output, "],\n" );


    fprintf(output, "\"clut\": {\n");
    fprintf(output, "\"precision\": 1,\n" );
    
    fprintf(output, "\"gridPoints\": [ " );
    for (int i = 0; i < inChannels; ++i) {
        fprintf(output, "%d", gridPoints );
        if (i < (inChannels-1))
            fprintf(output,", ");
    }
    fprintf(output, " ],\n" );
    
    fprintf(output, "\"data\": [\n");
    
    uint32_t clutSize = calcClutSize( inChannels, 1, gridPoints );
    
    fprintf(output,"  ");
    for (int k = 0, col = 0; k < clutSize; ++k) {
        for (int c = 0; c < outChannels; ++c) {
            uint8_t value = clut[ k*outChannels + c ];
            fprintf(output,"%3u, ", value );
        }
        
        // line break!
        col += outChannels * 5;
        if (k < (clutSize-1) && col > colLimit) {
            fprintf(output,"\n  ");
            col = 0;
        }
    }
    // back up to get rid of the last ", "
    fseek( output, -2, SEEK_CUR );

    fprintf(output, "\n]\n}\n}\n"); // end data array, end clut, and data block
    fprintf(output, "} },\n");   // end table, end tag
}

/********************************************************************************/

// currently written to use the same number of input and output channels
// assumes identity matrix and 1D LUTs
static
void add_lut16_json( const profileDataInner &data, FILE *output, uint32_t signature,
                    int inChannels, int outChannels, int gridPoints, uint16_t* clut )
{
    const int colLimit = 200;

    assert( inChannels >= 1 && inChannels <= 15 );
    assert( outChannels >= 1 && outChannels <= 15 );
    
    auto sigString = tag2XML( signature );
    fprintf(output, "{ \"%s\": {\n", sigString.c_str() );
    fprintf(output, "\"data\": {\n");

    fprintf(output, "\"inputChannels\": %d,\n", inChannels );
    fprintf(output, "\"outputChannels\": %d,\n", outChannels );
    fprintf(output, "\"type\": \"lut16Type\",\n" );
    

    fprintf(output, "\"aCurves\": [\n" );
    for (int i = 0; i < outChannels; ++i) {
        fprintf(output, "{ \"curveType\": \"table\", \"precision\": 1, \"type\": \"Curve\",\n");
        
// ALERT, BUG, TODO -- have to write out all values
        fprintf(output,"\"table\": [ ");
        for (int j = 0; j < 256; ++j) {
            fprintf(output,"%d", j );
            if (j < (256-1))
                fprintf(output,", ");
        }
        fprintf(output," ] }");
        
        if (i < (outChannels-1))
            fprintf(output,",\n");
        else
            fprintf(output,"\n");
    }
    fprintf(output, "],\n" );
    
    
    
    fprintf(output, "\"bCurves\": [\n" );
    for (int i = 0; i < inChannels; ++i) {
        fprintf(output, "{ \"curveType\": \"table\", \"precision\": 1, \"type\": \"Curve\",\n");
        
// ALERT, BUG, TODO -- have to write out all values
        fprintf(output,"\"table\": [ ");
        for (int j = 0; j < 256; ++j) {
            fprintf(output,"%d", j );
            if (j < (256-1))
                fprintf(output,", ");
        }
        fprintf(output," ] }");
        
        if (i < (inChannels-1))
            fprintf(output,",\n");
        else
            fprintf(output,"\n");
    }
    fprintf(output, "],\n" );


    fprintf(output, "\"clut\": {\n");
    fprintf(output, "\"precision\": 1,\n" );
    
    fprintf(output, "\"gridPoints\": [ " );
    for (int i = 0; i < inChannels; ++i) {
        fprintf(output, "%d", gridPoints );
        if (i < (inChannels-1))
            fprintf(output,", ");
    }
    fprintf(output, " ],\n" );
    
    fprintf(output, "\"data\": [\n");
    
    uint32_t clutSize = calcClutSize( inChannels, 1, gridPoints );
    
    fprintf(output,"  ");
    for (int k = 0, col = 0; k < clutSize; ++k) {
        for (int c = 0; c < outChannels; ++c) {
            uint16_t value = clut[ k*outChannels + c ];
            fprintf(output,"%5u, ", value );
        }
        
        // line break!
        col += outChannels * 7;
        if (k < (clutSize-1) && col > colLimit) {
            fprintf(output,"\n  ");
            col = 0;
        }
    }
    // back up to get rid of the last ", "
    fseek( output, -2, SEEK_CUR );

    fprintf(output, "\n]\n}\n}\n"); // end data array, end clut, and data block
    fprintf(output, "} },\n");   // end table, end tag

}

/********************************************************************************/

static
void write_header_json( const profileDataInner &data, FILE *output )
{
    fprintf(output, "{\n");
    fprintf(output, "\"IccProfile\": {\n");
    fprintf(output, "\"Header\": {\n");
    fprintf(output, "\"ProfileVersion\": \"4.20\",\n");     // 4.2.0 fixed for now
    
    auto classString = OSTypeToString( data.profileClass );
    fprintf(output, "\"ProfileDeviceClass\": \"%4s\",\n", classString.c_str() );

    auto colorspaceString = OSTypeToString( data.colorSpace );
    fprintf(output, "\"DataColourSpace\": \"%4s\",\n", colorspaceString.c_str() );
    
    auto pcsString = OSTypeToString( data.pcsSpace );
    fprintf(output, "\"PCS\": \"%4s\",\n", pcsString.c_str() );
    
    auto platformString = OSTypeToString( data.platform );
    fprintf(output, "\"PrimaryPlatform\": \"%4s\",\n", platformString.c_str() );
    
    auto cmmString = OSTypeToString( data.preferredCMM );
    fprintf(output, "\"PreferredCMMType\": \"%4s\",\n", cmmString.c_str() );
    
    auto manufacturerString = OSTypeToString( data.manufacturer );
    fprintf(output, "\"DeviceManufacturer\": \"%4s\",\n", manufacturerString.c_str() );
    
    auto creatorString = OSTypeToString( data.creator );
    fprintf(output, "\"ProfileCreator\": \"%4s\",\n", creatorString.c_str() );

    fprintf(output, "\"CMMFlags\": { \"EmbeddedInFile\": \"false\", \"UseWithEmbeddedDataOnly\": \"false\" },\n");
    fprintf(output, "\"DeviceAttributes\": { \"ReflectiveOrTransparency\": \"reflective\", \"GlossyOrMatte\": \"glossy\", \"MediaPolarity\": \"positive\", \"MediaColour\": \"colour\" },\n");     // can this be left out?

    fprintf(output, "\"RenderingIntent\": \"Relative Colorimetric\",\n");
    fprintf(output, "\"PCSIlluminant\": [ 0.964202880859, 1.00, 0.824905395508 ],\n");    // D50 fixed value, based on V4 binary values

    time_t now;
    (void)time( &now );
    auto timeFormat = getTimeString(now);
    fprintf(output, "\"CreationDateTime\": \"%s\"\n", timeFormat.c_str() );
    
    fprintf(output, "},\n"); // end header

}

/********************************************************************************/

static
void write_footer_json( const profileDataInner &data, FILE *output )
{
    fprintf(output, "}\n"); // end profile
    fprintf(output, "}\n"); // end file
}

/********************************************************************************/

static void write_mluc_json( FILE *output, const std::string &desc )
{
    fprintf(output, "\"data\": {\n");
    fprintf(output, "\"type\": \"multiLocalizedUnicodeType\",\n");
    fprintf(output, "\"localizedStrings\": [\n");
        // could have a loop here
        fprintf(output,"{ \"country\": \"us\", \"language\": \"en\", \"text\": \"%s\" }\n", desc.c_str());
    fprintf(output, "] }\n"); // end list of strings, end data
}

/********************************************************************************/

static
void write_tags_json( const profileDataInner &data, FILE *output )
{
    fprintf(output, "\"Tags\": [\n");
    
    fprintf(output, "{ \"profileDescriptionTag\": {\n");
    write_mluc_json( output, data.description );
    fprintf(output, "} },\n");    // end description

    fprintf(output, "{ \"copyrightTag\": {\n");
    write_mluc_json( output, data.copyright );
    fprintf(output, "} },\n");    // end copyright

    if (data.optionalNoteText.length() != 0)
        fprintf(output, "{ \"note\": { \"data\": \"%s\" } },\n", data.optionalNoteText.c_str() );


    if ( data.profileClass == kClassAbstract ) {
        
        assert( data.colorSpace == kSpaceLAB || data.colorSpace == kSpaceXYZ );
        assert( data.pcsSpace == kSpaceLAB || data.pcsSpace == kSpaceXYZ );
        assert( data.colorSpace == data.pcsSpace );
        
        // A2B0 only
    }
    else if ( data.profileClass == kClassDeviceLink ) {
        
        assert( data.colorSpace == kSpaceRGB || data.colorSpace == kSpaceCMYK );
        assert( data.pcsSpace == kSpaceRGB || data.pcsSpace == kSpaceCMYK );
        // colorspace = FIRST profile in sequence
        // pcs = colorspace of LAST profile in sequence
        
        // try leaving the sequence out - because we don't really have a sequence
        //    profileSequenceDesc        'pseq'    v2 6.3.4.1, 6.5.12,    V4? 9.2.32, 10.16
        // NOTE - so far fake/empty profilesequence tags break one thing or another
        
        // A2B0 only
        
        // V4 ColorantTable 9.2.14  if xCLR
        // V4 colorantTableOutTag  9.2.14.1 if xCLR
    }
    else if ( data.profileClass == kClassOutput ) {
        // A2B0,A2B1,A2B2
        // B2A0,B2A1,B2A2
        // gamut
        
        // V4 ColorantTable 9.2.14, 10.4 definition
        
        // V4 single channel shall have a grayTRC tag  9.2.19 ????
    }
    else if ( data.profileClass == kClassSpace ) {
        // A2B0
        // B2A0
    }
    else {
        fprintf(stderr,"Other profile types not implemented yet\n");
        return;
    }


    // add colorant tables
    for ( auto &clrTable : data.colorantTables )
        add_colorantTable_json( data, output, clrTable.tableSig, clrTable.colorants );

    // add tables
    for ( auto &table : data.LUTtables ) {
        if (table.pointsBackTo != icSigUnknown) {
            // special case pointing back to a previous table
            auto sig = tag2XML( table.tableSig );
            auto backTo = tag2XML( table.pointsBackTo );
            fprintf(output, "{ \"%s\": { \"sameAs\": \"%s\" } },\n", sig.c_str(), backTo.c_str());
            continue;
        }

        if (table.tableDepth == 8)
            add_lut8_json( data, output, table.tableSig, table.tableDimensions, table.tableChannels,
                        table.tableGridPoints, table.tableData.get() );
        else if (table.tableDepth == 16)
            add_lut16_json( data, output, table.tableSig, table.tableDimensions, table.tableChannels,
                        table.tableGridPoints, (uint16_t *)table.tableData.get() );
    }


    fprintf(output, "{ \"mediaWhitePointTag\": {\n");
    fprintf(output, "\"data\": {\n");
    fprintf(output, "\"type\": \"XYZArrayType\",\n");
    fprintf(output, "\"XYZ\": [ [ 0.964202880859, 1.00, 0.824905395508 ] ]\n");
    fprintf(output, "}\n"); //end data
    fprintf(output, "} }\n");    // end whitepoint, no more tags, so no comma
    
    fprintf(output, "]\n"); // nothing after this, so no comma
}

/******************************************************************************/
/******************************************************************************/

int writeICCProfileXML( const std::string &filename, profileDataInner &thisProfileData  )
{
    thisProfileData.tagInfo.reserve(8);
    
    // create output file
    std::string outputFileName = filename + ".xml";
    FILE *output = fopen( outputFileName.c_str(), "w" );
    if (output == NULL) {
        fprintf(stderr,"Could not create profile %s\n", outputFileName.c_str());
        return -1;
    }

    write_header_xml( thisProfileData, output );
    write_tags_xml( thisProfileData, output );
    write_footer_xml( thisProfileData, output );
    
    // done with the output file
    fclose( output );
 
    return 0;
}

/******************************************************************************/

int writeICCProfileJSON( const std::string &filename, profileDataInner &thisProfileData  )
{
    thisProfileData.tagInfo.reserve(8);
    
    // create output file
    std::string outputFileName = filename + ".json";
    FILE *output = fopen( outputFileName.c_str(), "w" );
    if (output == NULL) {
        fprintf(stderr,"Could not create profile %s\n", outputFileName.c_str());
        return -1;
    }

    write_header_json( thisProfileData, output );
    write_tags_json( thisProfileData, output );
    write_footer_json( thisProfileData, output );

    // done with the output file
    fclose( output );
 
    return 0;
}

/******************************************************************************/

int writeICCProfileBinary( const std::string &filename, profileDataInner &thisProfileData  )
{
    thisProfileData.tagInfo.reserve(8);
    
    // create output file
    std::string outputFileName = filename + ".icc";
    FILE *output = fopen( outputFileName.c_str(), "wb" );
    if (output == NULL) {
        fprintf(stderr,"Could not create profile %s\n", outputFileName.c_str());
        return -1;
    }

    write_header_binary( thisProfileData, output );
    create_tags( thisProfileData );
    write_tags_binary( thisProfileData, output );
    
    // get the final file size and update size field at beginning of the file
    uint32_t totalSize = (int32_t)ftell( output );
    assert( (totalSize & 0x03) == 0);
    fseek( output, 0, SEEK_SET );
    totalSize = SwabLong( totalSize );
    fwrite( &totalSize, 4, 1, output );

    // done with the output file
    fclose( output );
 
    return 0;
}

/******************************************************************************/

// currently limited in several ways, but a starting point for doing what I need
// without pulling in the entire iccDEV library
int writeICCProfile( const std::string &filename, profileData &profileInfo  )
{
    // copy data into our internal record structure
    profileDataInner thisProfileData( profileInfo );
    
    int result = 0;

    if ( (thisProfileData.profileFormats & kProfileBinary) != 0)
        result += writeICCProfileBinary( filename, thisProfileData );
    
    if ( (thisProfileData.profileFormats & kProfileXML) != 0)
        result += writeICCProfileXML( filename, thisProfileData );
    
    if ( (thisProfileData.profileFormats & kProfileJSON) != 0)
        result += writeICCProfileJSON( filename, thisProfileData );
 
    // release our tag data
    for ( auto &i : thisProfileData.tagInfo )
        delete[] i.data;
    
    thisProfileData.tagInfo.clear();
 
    return result;
}

/******************************************************************************/
/******************************************************************************/
