/*
 HA-1 Error.h
 
 Copyright: Copyright (C) 2013,2014 Mitsuru Nakada All Rights Reserved.
 License: GNU GPL v2
 */

#ifndef __Error_h__
#define __Error_h__

enum Error {
    NoExec          = 3,
    NoRes           = 2,
    Finish          = 1,
    Success         = 0,
    Error           = -1,
    Disconnect      = -2
};

#endif // __Error_h__