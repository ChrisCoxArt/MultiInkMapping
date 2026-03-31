//
//  Options.cpp
//  MultiInkMapping
//
//  Created by Chris Cox on 3/19/26.
//

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <cstring>
#include <cstdlib>
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

/******************************************************************************/

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

/******************************************************************************/

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
    
    ReadString( j, "filename", p.name );
    ReadString( j, "description", p.description );
    ReadString( j, "copyright", p.copyright );
    
    p.primaries =  j["primariesList"];
    
    auto dataFound1 = j.find("paperColor");
    if (dataFound1 != j.end())
        p.paperColor = dataFound1.value();
    
    auto dataFound2 = j.find("darkColor");
    if (dataFound2 != j.end())
        p.darkColor = dataFound2.value();
    
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
}

/******************************************************************************/

void from_json( const json &j, settings_spec &p )
{
    // copy existing settings as default values
    p = globalSettings;
    p.colorSets.clear();
    
    ReadInt( j, "tableDepth", p.gDataDepth );
    if (p.gDataDepth > 16)
        p.gDataDepth = 16;
    if (p.gDataDepth < 8)
        p.gDataDepth = 8;
    if (p.gDataDepth != 8 && p.gDataDepth != 16)
        p.gDataDepth = 8;
    
    ReadInt( j, "gridPoints", p.gDataGridPoints );
    if (p.gDataGridPoints < 2)
        p.gDataGridPoints = 2;
    if (p.gDataGridPoints > 255)
        p.gDataGridPoints = 255;
    
    ReadSize( j, "tableSizeLimit", p.gTableSizeLimit );
    
    ReadBool( j, "debugEnable", p.gDebugMode );
    ReadBool( j, "createOutputProfiles", p.gCreateOutput );
    ReadBool( j, "createAbstractProfiles", p.gCreateAbstract );
    ReadBool( j, "createTIFFTables", p.gTIFFTables );
    
    ReadString( j, "defaultCopyright", p.gDefaultCopyright );

    p.colorSets = j["colorSets"];
}

/******************************************************************************/
/******************************************************************************/

static void print_usage(char *argv[])
{
    printf("Usage: %s <args> input.json\n", argv[0] );
    
    printf("\t-depth B        bit depth of output data [8 or 16] (default %d)\n", globalSettings.gDataDepth );
    printf("\t-grid G         number of grid points (default %d)\n", globalSettings.gDataGridPoints );
    printf("\t-limit L        upper limit on A2B table size (default %zu)\n", globalSettings.gTableSizeLimit );
    printf("\t-copyright C    copyright string for profiles (default \"%s\")\n", globalSettings.gDefaultCopyright.c_str() );
    printf("\t-debug          enable debugging output\n" );
    printf("\t-tiff           output tables as TIFF files\n" );

    printf("\t-version        Prints this message and exits immediately\n" );
    printf("Version %s, Compiled %s %s\n", kVersionString, __DATE__, __TIME__ );
    printf("Nlohmann JSON Version %d.%d.%d\n", NLOHMANN_JSON_VERSION_MAJOR, NLOHMANN_JSON_VERSION_MINOR, NLOHMANN_JSON_VERSION_PATCH );
}

/******************************************************************************/

void parse_arguments( int argc, char *argv[] )
{
    std::vector<std::string> filenames;

    for ( int c = 1; c < argc; ++c ) {
        
        if ( (strcasecmp( argv[c], "-grid" ) == 0 || strcasecmp( argv[c], "-g" ) == 0 )
            && c < (argc-1) ) {
            int temp = atoi( argv[c+1] );
            if (temp < 2)
                temp = 2;
            if (temp > 255)
                temp = 255;
            globalSettings.gDataGridPoints = temp;
            ++c;
        }
        else if ( (strcasecmp( argv[c], "-depth" ) == 0 || strcasecmp( argv[c], "-d" ) == 0 )
            && c < (argc-1) ) {
            int temp = atoi( argv[c+1] );
            if (temp > 16)
                temp = 16;
            if (temp < 8)
                temp = 8;
            if (temp != 8 && temp != 16)
                temp = 8;
            globalSettings.gDataDepth = temp;
            ++c;
        }
        else if ( (strcasecmp( argv[c], "-limit" ) == 0 || strcasecmp( argv[c], "-l" ) == 0 )
            && c < (argc-1) ) {
            size_t temp = atoll( argv[c+1] );
            if (temp < 1024)
                temp = 1024;
            globalSettings.gTableSizeLimit = temp;
            // upper limit is really the 2 Gig ICC Profile limit
            ++c;
        }
        else if ( (strcasecmp( argv[c], "-copyright" ) == 0 || strcasecmp( argv[c], "-c" ) == 0 )
            && c < (argc-1) ) {
            std::string temp = argv[c+1];
            if (temp != std::string())  // nope, can't be empty
                globalSettings.gDefaultCopyright = temp;
            ++c;
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


    // process json files
    for ( const auto &name : filenames ) {
        
        if (!name.empty()) {
            globalSettings.colorSets.clear();
            
            std::ifstream in( name );
            if (in.is_open()) {
                json settings = json::parse(in);
                globalSettings = settings;
                in.close();
                
                if (globalSettings.gDebugMode) {
                    // rewrite the input, for verification, when debugging
                    json setTemp = globalSettings;
                    std::ofstream out( name + "_verify.json" );
                    out << std::setw(4) << setTemp.dump(4);
                    out.close();
                }

                // process the inksets from this json file
                processInkSetList();
            
            } else {
                std::cerr << "Could not open json file " << name << "\n";
            }
        }
    }

}

/******************************************************************************/
/******************************************************************************/
