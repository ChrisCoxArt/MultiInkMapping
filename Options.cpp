//
//  Options.cpp
//  MultiInkMapping
//  MIT License, Copyright (C) Chris Cox 2026
//
//  Created by Chris Cox on March 19, 2026.
//

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
#include <memory>
#include <algorithm>
#include "MultiInkMapping.hpp"
#include "Options.hpp"
#include "json.hpp"

using json = nlohmann::json;


// work around bogus Windows headers and missing functions
#ifdef _WIN32
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

/******************************************************************************/

static
void ReadString( const json &input, const char *key, std::string &result )
{
    auto dataFound = input.find(key);
    if (dataFound != input.end())
        {
        auto type = dataFound.value().type();
        if (type == json::value_t::null)
            return;
        if (type != json::value_t::string)
            result.clear();
        else
            result = dataFound.value();
        }
}

/*********************************************************************/

static
void ReadInt( const json &input, const char *key, int &result )
{
    auto dataFound = input.find(key);
    if (dataFound != input.end())
        {
        auto type = dataFound.value().type();
        if (type == json::value_t::null )
            return;
        if (type != json::value_t::number_unsigned
         && type != json::value_t::number_integer)
            result = 0;
        else
            result = dataFound.value();
        }
}

/*********************************************************************/

static
void ReadSize( const json &input, const char *key, size_t &result )
{
    auto dataFound = input.find(key);
    if (dataFound != input.end())
        {
        auto type = dataFound.value().type();
        if (type == json::value_t::null )
            return;
        if (type != json::value_t::number_unsigned
         && type != json::value_t::number_integer)
            result = 0;
        else
            result = dataFound.value();
        }
}

/******************************************************************************/

#if 0
static
void ReadFloat( const json &input, const char *key, float &result )
{
    auto dataFound = input.find(key);
    if (dataFound != input.end())
        {
        auto type = dataFound.value().type();
        if (type == json::value_t::null )
            return;
        if (type != json::value_t::number_float
         && type != json::value_t::number_unsigned
         && type != json::value_t::number_integer)
            result = 0.0f;
        else
            result = dataFound.value();
        }
}
#endif

/******************************************************************************/

static
void ReadBool( const json &input, const char *key, bool &result )
{
    auto dataFound = input.find(key);
    if (dataFound != input.end())
        {
        auto type = dataFound.value().type();
        if (type == json::value_t::null )
            return;
        if (type != json::value_t::boolean
         && type != json::value_t::number_unsigned
         && type != json::value_t::number_integer)
            result = false;
        else
            result = (dataFound.value() != false);
        }
}

/******************************************************************************/

void to_json( json &j, const labColor &p )
{
    j = json{ {"L", p.L }, {"a", p.A }, {"b", p.B } };
}

/******************************************************************************/

void from_json( const json &j, labColor &p )
{
    // all required
    p.L = j["L"];
    p.A = j["a"];
    p.B = j["b"];
}

/******************************************************************************/

void to_json( json &j, const namedColor &p )
{
    j = json{ {"Name", p.name }, {"L", p.color.L }, {"a", p.color.A }, {"b", p.color.B } };
}

/******************************************************************************/

void from_json( const json &j, namedColor &p )
{
    // all required
    p.name = j["Name"];
    p.color.L = j["L"];
    p.color.A = j["a"];
    p.color.B = j["b"];
}

/******************************************************************************/

void to_json( json &j, const overPrintSwatch &p )
{
    j = json{ {"L", p.color.L }, {"a", p.color.A }, {"b", p.color.B }, {"inkNames", p.inkNames} };

    // bitmap, xyz are calculated at runtime, and not saved
}

/******************************************************************************/

void from_json( const json &j, overPrintSwatch &p )
{
    // all required
    p.color.L = j["L"];
    p.color.A = j["a"];
    p.color.B = j["b"];

    p.inkNames =  j["inkNames"];
}

/******************************************************************************/

void to_json( json &j, const inkColorSet &p )
{
    j = json{ { "filename", p.name },
              { "description", p.description },
              { "copyright", p.copyright },
              { "paperColor", p.paperColor },
              { "darkColor", p.darkColor },
              { "primariesList", p.primaries },
            };
    
    // overprints are optional
    // so don't record any if there aren't any
    if (p.overprints.size() > 0) {
        j["overprints"] = p.overprints;
    }

    // remaining struct variables are calculated at runtime, and not saved
}

/******************************************************************************/

void from_json( const json &j, inkColorSet &p )
{
    // set some defaults
    p.darkColor =  labColor(-1,0,0);            // flag to automatically calculate
    p.paperColor = labColor(98.0,0.0,0.0);      // unbelievably white
    p.filterColor = labColor(-1,0,0);           // flat to ignore this if unset
    
    // these are required
    ReadString( j, "filename", p.name );
    ReadString( j, "description", p.description );
    ReadString( j, "copyright", p.copyright );
    
    p.primaries =  j["primariesList"];
    
    // these are optional
    auto dataFound1 = j.find("paperColor");
    if (dataFound1 != j.end())
        p.paperColor = dataFound1.value();
    
    auto dataFound2 = j.find("darkColor");
    if (dataFound2 != j.end())
        p.darkColor = dataFound2.value();
    
    auto dataFound4 = j.find("filterColor");
    if (dataFound4 != j.end())
        p.filterColor = dataFound4.value();
    
    // overprints are optional
    auto dataFound3 = j.find("overprints");
    if (dataFound3 != j.end())
        p.overprints = dataFound3.value();
}

/******************************************************************************/

void to_json( json &j, const settings_spec &p )
{
    j = json{ { "tableDepth", p.gDataDepth },
              { "gridPoints", p.gDataGridPoints },
              { "tableSizeLimit", p.gTableSizeLimit },
              { "debugEnable", p.gDebugMode },
              { "createOutputProfiles", p.gCreateOutput },
              { "createAbstractProfiles", p.gCreateAbstract },
              { "createTIFFTables", p.gTIFFTables },
              { "defaultCopyright", p.gDefaultCopyright },
              { "colorSets", p.colorSets },
            };
    
    if (p.gProfileTypes & kProfileBinary)
        j["ICCBinary"] = true;
    
    if (p.gProfileTypes & kProfileXML)
        j["ICCXML"] = true;
    
    if (p.gProfileTypes & kProfileJSON)
        j["ICCjson"] = true;
}

/******************************************************************************/

void defaultSettings( settings_spec &p )
{
    // Set defaults
    p.gDataDepth = 8;
    p.gDataGridPoints = 21;
    p.gTableSizeLimit = 1024*1024; // 1 Meg points, 3 Meg or 6 Meg bytes depending on depth
    p.gDefaultCopyright = "Copyright (c) Chris Cox 2026";
    p.gDebugMode = false;
    p.gCreateOutput = true;
    p.gCreateAbstract = true;
    p.gTIFFTables = false;
    p.gProfileTypes = kProfileBinary;
    p.colorSets.clear();
}

/******************************************************************************/

void from_json( const json &j, settings_spec &p )
{
    p = globalSettings;

    ReadInt( j, "tableDepth", p.gDataDepth );
    ReadInt( j, "gridPoints", p.gDataGridPoints );
    ReadSize( j, "tableSizeLimit", p.gTableSizeLimit );
    ReadBool( j, "debugEnable", p.gDebugMode );
    ReadBool( j, "createOutputProfiles", p.gCreateOutput );
    ReadBool( j, "createAbstractProfiles", p.gCreateAbstract );
    ReadBool( j, "createTIFFTables", p.gTIFFTables );
    ReadString( j, "defaultCopyright", p.gDefaultCopyright );
    
    // set, or clear, optional filetypes
    if (j.count("ICCBinary") > 0) {
        bool temp = false;
        ReadBool( j, "ICCBinary", temp );
        if (temp)
            p.gProfileTypes |= kProfileBinary;
        else
            p.gProfileTypes &= ~kProfileBinary;
    }
    if (j.count("ICCXML") > 0) {
        bool temp = false;
        ReadBool( j, "ICCXML", temp );
        if (temp)
            p.gProfileTypes |= kProfileXML;
        else
            p.gProfileTypes &= ~kProfileXML;
    }
    if (j.count("ICCjson") > 0) {
        bool temp = false;
        ReadBool( j, "ICCjson", temp );
        if (temp)
            p.gProfileTypes |= kProfileJSON;
        else
            p.gProfileTypes &= ~kProfileJSON;
    }

    p.colorSets = j["colorSets"];
}

/******************************************************************************/
/******************************************************************************/

// make the settings safe & sane
void pinSettings( settings_spec &p )
{
    // assume max size table with A2B with 3 channel PCS output
    const size_t maxTable = ( 1ULL << 31 ) / 3; // upper limit is really the 2 Gig ICC Profile limit

    if (p.gDataDepth > 16)
        p.gDataDepth = 16;
    if (p.gDataDepth < 8)
        p.gDataDepth = 8;
    if (p.gDataDepth != 8 && p.gDataDepth != 16)
        p.gDataDepth = 8;

    if (p.gDataGridPoints < 2)
        p.gDataGridPoints = 2;
    if (p.gDataGridPoints > 255)
        p.gDataGridPoints = 255;

    if (p.gTableSizeLimit < 1024)
        p.gTableSizeLimit = 1024;
    size_t maxSize = (p.gDataDepth > 8) ? (maxTable/2) : maxTable;
    if (p.gTableSizeLimit > maxSize)
        p.gTableSizeLimit = maxSize;

    if (p.gDefaultCopyright == std::string())  // nope, can't be empty
        p.gDefaultCopyright = "Copyright Unknown";

}

/******************************************************************************/

void process_json_filelist( const filename_list &filenames )
{
    for ( const auto &name : filenames ) {
        if (name.empty())
            continue;
        
        std::ifstream in( name );
        if (!in.is_open()) {
            std::cerr << "Could not open JSON file " << name << "\n";
            continue;
        }
        
        try {
            json settings = json::parse(in);
            globalSettings = settings;
        }
        catch (const std::exception& e) {
          fprintf(stderr, "ERROR - JSON parsing in file '%s': %s\n", name.c_str(), e.what() );
          continue;
        }
        catch (...) {
          fprintf(stderr, "ERROR - Unknown exception in JSON file '%s'\n", name.c_str() );
          continue;
        }
        
        in.close();

        pinSettings( globalSettings );
        
        if (globalSettings.gDebugMode) {
            std::string outname = name + "_verify.json";
            // rewrite the input, for verification, when debugging
            try {
                json setTemp = globalSettings;
                std::ofstream out( outname );
                out << std::setw(4) << setTemp.dump(4);
                out.close();
            }
            catch (const std::exception& e) {
              fprintf(stderr, "ERROR - writing JSON file '%s': %s\n", outname.c_str(), e.what() );
            }
            catch (...) {
              fprintf(stderr, "ERROR - Unknown exception writing JSON file '%s'\n", outname.c_str() );
            }
        }


        // process the inksets from this json file
        processInkSetList();
    }

}

/******************************************************************************/
/******************************************************************************/

static
void print_usage(char *argv[])
{
    printf("Usage: %s <args> input.json\n", argv[0] );
    
    printf("\t-depth B        bit depth of output data [8 or 16] (default %d)\n", globalSettings.gDataDepth );
    printf("\t-grid G         number of grid points per channel (default %d)\n", globalSettings.gDataGridPoints );
    printf("\t-limit L        upper limit on A2B table size (default %zu)\n", globalSettings.gTableSizeLimit );
    printf("\t-copyright C    copyright string for profiles (default \"%s\")\n", globalSettings.gDefaultCopyright.c_str() );
    printf("\t-tiff           also output tables as TIFF files (default false)\n" );
    printf("\t-json           also write JSON ICC profiles (default false)\n" );
    printf("\t-xml            also write XML ICC profiles (default false)\n" );
    // binary is enabled by default, but can be overridden in json files - does it need a command line option?
    printf("\t-debug          enable additional debugging output\n" );

    printf("\t-version        Prints this message and exits immediately\n" );
    printf("Version %s, Compiled %s %s\n", kVersionString, __DATE__, __TIME__ );
    printf("Nlohmann JSON Version %d.%d.%d\n", NLOHMANN_JSON_VERSION_MAJOR, NLOHMANN_JSON_VERSION_MINOR, NLOHMANN_JSON_VERSION_PATCH );
}

/******************************************************************************/

filename_list parse_arguments( int argc, char *argv[] )
{
    std::vector<std::string> filenames;

    for ( int c = 1; c < argc; ++c ) {
        
        if ( (strcasecmp( argv[c], "-grid" ) == 0 || strcasecmp( argv[c], "-g" ) == 0 )
            && c < (argc-1) ) {
            int temp = atoi( argv[c+1] );
            globalSettings.gDataGridPoints = temp;
            ++c;
        }
        else if ( (strcasecmp( argv[c], "-depth" ) == 0 || strcasecmp( argv[c], "-d" ) == 0 )
            && c < (argc-1) ) {
            int temp = atoi( argv[c+1] );
            globalSettings.gDataDepth = temp;
            ++c;
        }
        else if ( (strcasecmp( argv[c], "-limit" ) == 0 || strcasecmp( argv[c], "-l" ) == 0 )
            && c < (argc-1) ) {
            size_t temp = (size_t)atoll( argv[c+1] );
            globalSettings.gTableSizeLimit = temp;
            ++c;
        }
        else if ( (strcasecmp( argv[c], "-copyright" ) == 0 || strcasecmp( argv[c], "-c" ) == 0 )
            && c < (argc-1) ) {
            std::string temp = argv[c+1];
            globalSettings.gDefaultCopyright = temp;
            ++c;
        }
        else if ( (strcasecmp( argv[c], "-xml" ) == 0 || strcasecmp( argv[c], "-x" ) == 0 ) ) {
            globalSettings.gProfileTypes |= kProfileXML;
        }
        else if ( (strcasecmp( argv[c], "-json" ) == 0 || strcasecmp( argv[c], "-j" ) == 0 ) ) {
            globalSettings.gProfileTypes |= kProfileJSON;
        }
        else if ( strcasecmp( argv[c], "-debug" ) == 0 ) {
            globalSettings.gDebugMode = true;
        }
        else if ( strcasecmp( argv[c], "-tiff" ) == 0 ) {
            globalSettings.gTIFFTables = true;
        }
        else if ( strcasecmp( argv[c], "-V" ) == 0
                || strcasecmp( argv[c], "-help" ) == 0
                || strcasecmp( argv[c], "--help" ) == 0
                || strcasecmp( argv[c], "-version" ) == 0
                ) {
            print_usage( argv );
            exit (0);
        }
        else if (argv[c][0] == '-') {
            // unrecognized switch
            print_usage( argv );
            exit (1);
        }
        else {
            // not a switch, treat it as a json input file
            filenames.push_back( argv[c] );
        }
    }

    // make sure the settings are safe to use
    pinSettings( globalSettings );
    
    return filenames;
}

/******************************************************************************/
/******************************************************************************/
