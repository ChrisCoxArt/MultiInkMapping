//
//  MiniICC.hpp
//  MultiInkMapping
//
//  Copyright (c) 2026 Chris Cox
//  Created by Chris Cox on 3/4/26.
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
	kClassInput = 'scnr',
	kClassMonitor = 'mntr',
	kClassOutput = 'prtr',
	kClassDeviceLink = 'link',
	kClassSpace = 'spac',
	kClassAbstract = 'abst',
	kClassNamed = 'nmcl',
} profile_class;

typedef enum : uint32_t {
	kSpaceXYZ = 'XYZ ',
	kSpaceLAB = 'Lab ',
	kSpaceLUV = 'Luv ',
	kSpaceYCbCr = 'YCbr',
	kSpaceYxy = 'Yxy ',
	kSpaceRGB = 'RGB ',
	kSpaceGray = 'GRAY',
	kSpaceHSV = 'HSV ',
	kSpaceHLS = 'HLS ',
	kSpaceCMYK = 'CMYK',
	kSpaceCMY = 'CMY ',
	kSpace1CLR = '1CLR',
	kSpace2CLR = '2CLR',
	kSpace3CLR = '3CLR',
	kSpace4CLR = '4CLR',
	kSpace5CLR = '5CLR',
	kSpace6CLR = '6CLR',
	kSpace7CLR = '7CLR',
	kSpace8CLR = '8CLR',
	kSpace9CLR = '9CLR',
	kSpaceACLR = 'ACLR',
	kSpaceBCLR = 'BCLR',
	kSpaceCCLR = 'CCLR',
	kSpaceDCLR = 'DCLR',
	kSpaceECLR = 'ECLR',
	kSpaceFCLR = 'FCLR',
} color_space;

typedef enum : uint32_t  {

    icSigCopyrightTag                      = 0x63707274,  /* 'cprt' */
    icSigProfileDescriptionTag             = 0x64657363,  /* 'desc' */

    icSigMediaBlackPointTag                = 0x626B7074,  /* 'bkpt' */
    icSigMediaWhitePointTag                = 0x77747074,  /* 'wtpt' */

    icSigAToB0Tag                          = 0x41324230,  /* 'A2B0' */ 
    icSigAToB1Tag                          = 0x41324231,  /* 'A2B1' */
    icSigAToB2Tag                          = 0x41324232,  /* 'A2B2' */ 
    icSigAToB3Tag                          = 0x41324233,  /* 'A2B3' */
    
    icSigBToA0Tag                          = 0x42324130,  /* 'B2A0' */
    icSigBToA1Tag                          = 0x42324131,  /* 'B2A1' */
    icSigBToA2Tag                          = 0x42324132,  /* 'B2A2' */
    icSigBToA3Tag                          = 0x42324133,  /* 'B2A3' */
    
    icSigPreview0Tag                       = 0x70726530,  /* 'pre0' */
    icSigPreview1Tag                       = 0x70726531,  /* 'pre1' */
    icSigPreview2Tag                       = 0x70726532,  /* 'pre2' */
    
    icSigGamutTag                          = 0x67616D74,  /* 'gamt' */

// maybe v4 bits
    icSigColorSpaceNameTag                 = 0x63736e6d,  /* 'csnm' */
    icSigColorantInfoTag                   = 0x636c696e,  /* 'clin' */
    icSigColorantInfoOutTag                = 0x636c696f,  /* 'clio' */
    icSigColorantOrderTag                  = 0x636C726F,  /* 'clro' */
    icSigColorantOrderOutTag               = 0x636c6f6f,  /* 'cloo' */
    icSigColorantTableTag                  = 0x636C7274,  /* 'clrt' */
    icSigColorantTableOutTag               = 0x636C6F74,  /* 'clot' */


    icSigUnknown = 0,
} profile_sig;

/********************************************************************************/

// all pointers are passed in, not owned
struct tableFormat {

    tableFormat() : pointsBackTo(icSigUnknown),
            tableSig(icSigUnknown) {}

    profile_sig     tableSig;
    profile_sig     pointsBackTo;   // so we have A2B1 and A2B2 refer back to A2B0
    
    int				tableDepth;
    int				tableGridPoints;
    int             tableDimensions;
    int             tableChannels;
    std::shared_ptr<uint8_t> tableData;
};

/********************************************************************************/

// all pointers are passed in, not owned
struct profileData {

    profileData() : preferredCMM('ICCD'), platform('APPL'), manufacturer('none'), creator('ccox') {}

    std::string	    description;        // required
    std::string     copyright;          // required
    uint32_t        preferredCMM;       // required, default set
    uint32_t        platform;           // required, default set
    uint32_t        manufacturer;       // required, default set
    uint32_t        creator;            // required, default set

    profile_class	profileClass;       // required
    color_space		colorSpace;         // required
    color_space		pcsSpace;           // required

    std::string     otherText;          // optional
    
    std::vector<tableFormat> tables;
};


/********************************************************************************/

int writeICCProfile( const std::string &filename, profileData &profileInfo  );

/********************************************************************************/

#endif /* MiniICC_hpp */
