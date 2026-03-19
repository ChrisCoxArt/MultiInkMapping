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

// TODO - write me!
extern void to_json( json &j, const settings_spec &p );
extern void from_json( const json &j, settings_spec &p );

extern void to_json( json &j, const inkColorSet &p );
extern void from_json( const json &j, inkColorSet &p );

extern void to_json( json &j, const labColor &p );
extern void from_json( const json &j, labColor &p );

extern void to_json( json &j, const labColorNamed &p );
extern void from_json( const json &j, labColorNamed &p );

/******************************************************************************/

static void print_usage(char *argv[])
{
    printf("Usage: %s <args> input.json\n", argv[0] );
    
    printf("\t-depth B        bit depth of data [8 or 16] (default %d)\n", globalSettings.gDataDepth );
    printf("\t-grid G         number of grid points (default %d)\n", globalSettings.gDataGridPoints );
    printf("\t-limit L        upper limit on A2B table size (default %zu)\n", globalSettings.gTableSizeLimit );
    printf("\t-copyright C    default copyright string for profiles (default \"%s\")\n", globalSettings.gDefaultCopyright.c_str() );
    printf("\t-debug          enable debugging output\n" );
    printf("\t-tiff           output tables as TIFF files\n" );

    printf("\t-version        Prints this message and exits immediately\n" );
    printf("Version %s, Compiled %s %s\n", kVersionString, __DATE__, __TIME__ );
}

/******************************************************************************/

void parse_arguments( int argc, char *argv[] )
{
    std::vector<std::string> filenames;


// TODO - first time run, convert hard coded color sets into a JSON file
// then delete that code
// or maybe use the output for debug mode

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
//                globalSettings = settings;
                in.close();

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
