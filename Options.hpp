//
//  Options.hpp
//  MultiInkMapping
//  MIT License, Copyright (C) Chris Cox 2026
//
//  Created by Chris Cox on March 19, 2026.
//

#ifndef Options_hpp
#define Options_hpp

#include <cstdio>
#include <vector>
#include <string>
#include "MultiInkMapping.hpp"

/******************************************************************************/

typedef std::vector<std::string> filename_list;

/******************************************************************************/

extern filename_list parse_arguments( int argc, char *argv[] );
extern void pinSettings( settings_spec &p );
extern void defaultSettings( settings_spec &p );
extern void process_json_filelist( const filename_list &filenames );

/******************************************************************************/

#endif /* Options_hpp */
