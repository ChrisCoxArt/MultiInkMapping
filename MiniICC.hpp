//
//  MiniICC.hpp
//  MultiInkMapping
//
//  Copyright (c) 2026 Chris Cox
//  Created by Chris Cox on March 4, 2026.
//

#ifndef MiniICC_hpp
#define MiniICC_hpp

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>
#include <bit>

/********************************************************************************/

typedef enum : uint32_t  {
    kClassInput      = 0x73636E72,  /* 'scnr' */
    kClassMonitor    = 0x6D6E7472,  /* 'mntr' */
    kClassOutput     = 0x70727472,  /* 'prtr' */
    kClassDeviceLink = 0x6C696E6B,  /* 'link' */
    kClassSpace      = 0x73706163,  /* 'spac' */
    kClassAbstract   = 0x61627374,  /* 'abst' */
    kClassNamed      = 0x6e6d636c,  /* 'nmcl' */
} profile_class;

typedef enum : uint32_t {
    kSpaceXYZ   = 0x58595A20,  /* 'XYZ ' */
    kSpaceLAB   = 0x4C616220,  /* 'Lab ' */
    kSpaceLUV   = 0x4C757620,  /* 'Luv ' */
    kSpaceYCbCr = 0x59436272,  /* 'YCbr' */
    kSpaceYxy   = 0x59787920,  /* 'Yxy ' */
    kSpaceRGB   = 0x52474220,  /* 'RGB ' */
    kSpaceGray  = 0x47524159,  /* 'GRAY' */
    kSpaceHSV   = 0x48535620,  /* 'HSV ' */
    kSpaceHLS   = 0x484C5320,  /* 'HLS ' */
    kSpaceCMYK  = 0x434D594B,  /* 'CMYK' */
    kSpaceCMY   = 0x434D5920,  /* 'CMY ' */
    
    kSpace1CLR = 0x31434C52,  /* '1CLR' */
    kSpace2CLR = 0x32434C52,  /* '2CLR' */
    kSpace3CLR = 0x33434C52,  /* '3CLR' */
    kSpace4CLR = 0x34434C52,  /* '4CLR' */
    kSpace5CLR = 0x35434C52,  /* '5CLR' */
    kSpace6CLR = 0x36434C52,  /* '6CLR' */
    kSpace7CLR = 0x37434C52,  /* '7CLR' */
    kSpace8CLR = 0x38434C52,  /* '8CLR' */
    kSpace9CLR = 0x39434C52,  /* '9CLR' */
    kSpaceACLR = 0x41434C52,  /* 'ACLR' */
    kSpaceBCLR = 0x42434C52,  /* 'BCLR' */
    kSpaceCCLR = 0x43434C52,  /* 'CCLR' */
    kSpaceDCLR = 0x44434C52,  /* 'DCLR' */
    kSpaceECLR = 0x45434C52,  /* 'ECLR' */
    kSpaceFCLR = 0x46434C52,  /* 'FCLR' */
    
} color_space;

typedef enum : uint32_t  {
    icSigMagicNumber                 = 0x61637370,  /* 'acsp' */

    icSigCopyrightTag                = 0x63707274,  /* 'cprt' */
    icSigProfileDescriptionTag       = 0x64657363,  /* 'desc' */
    icSigMultiLocalizedUnicodeType   = 0x6D6C7563,  /* 'mluc' */
    icSigTextType                    = 0x74657874,  /* 'text' */

    icSigMediaBlackPointTag          = 0x626B7074,  /* 'bkpt' */
    icSigMediaWhitePointTag          = 0x77747074,  /* 'wtpt' */

    icSigLut16Type                   = 0x6d667432,  /* 'mft2' */
    icSigLut8Type                    = 0x6d667431,  /* 'mft1' */
    
    icSigAToB0Tag                    = 0x41324230,  /* 'A2B0' */
    icSigAToB1Tag                    = 0x41324231,  /* 'A2B1' */
    icSigAToB2Tag                    = 0x41324232,  /* 'A2B2' */
    icSigAToB3Tag                    = 0x41324233,  /* 'A2B3' */
    
    icSigBToA0Tag                    = 0x42324130,  /* 'B2A0' */
    icSigBToA1Tag                    = 0x42324131,  /* 'B2A1' */
    icSigBToA2Tag                    = 0x42324132,  /* 'B2A2' */
    icSigBToA3Tag                    = 0x42324133,  /* 'B2A3' */
    
    icSigGamutTag                    = 0x67616D74,  /* 'gamt' */

    icSigColorantTableTag            = 0x636C7274,  /* 'clrt' */
    icSigColorantTableOutTag         = 0x636C6F74,  /* 'clot' */

    icSigIccDEV                      = 0x49434344,  /* 'ICCD' */
    
    icSigMacintosh                   = 0x4150504C,  /* 'APPL' */
    icSigMicrosoft                   = 0x4D534654,  /* 'MSFT' */

    icSigCCox                        = 0x43436F78,  /* 'CCox' */
    icSigccox                        = 0x43636F78,  /* 'ccox' */
    icSigNone                        = 0x6E6F6E65,  /* 'none' */
    icSigNote                        = 0x6E6F7465,  /* 'note' */

    icSigUnknown = 0,
} profile_sig;

/********************************************************************************/

// all pointers are passed in, not owned
struct tableFormat {

    tableFormat() : pointsBackTo(icSigUnknown),
            tableSig(icSigUnknown) {}

    profile_sig     tableSig;
    profile_sig     pointsBackTo;   // so we have A2B1 and A2B2 refer back to A2B0
    
    int             tableDepth;
    int             tableGridPoints;
    int             tableDimensions;
    int             tableChannels;
    std::shared_ptr<uint8_t> tableData;
};

/********************************************************************************/

struct namedICCLAB16 {
    std::string name;
    uint16_t L;
    uint16_t a;
    uint16_t b;
};

// all pointers are passed in, not owned
struct colorantTableFormat {

    colorantTableFormat() : tableSig(icSigUnknown) {}

    profile_sig tableSig;
    
    std::vector< namedICCLAB16 > colorants;
};

/********************************************************************************/

// all pointers are passed in, not owned
struct profileData {

    profileData() : preferredCMM(icSigIccDEV), platform(icSigMacintosh),
                manufacturer(icSigNone), creator(icSigccox) {}

    std::string     description;        // required
    std::string     copyright;          // required
    uint32_t        preferredCMM;       // required, default set
    uint32_t        platform;           // required, default set
    uint32_t        manufacturer;       // required, default set
    uint32_t        creator;            // required, default set

    profile_class   profileClass;       // required
    color_space     colorSpace;         // required
    color_space     pcsSpace;           // required

    std::string     otherText;          // optional, can be empty

    std::vector<tableFormat> LUTtables;

    std::vector<colorantTableFormat> colorantTables;
};


/********************************************************************************/

int writeICCProfile( const std::string &filename, profileData &profileInfo  );

/********************************************************************************/

#endif /* MiniICC_hpp */
