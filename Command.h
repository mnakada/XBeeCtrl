/*
 HA-1 Command.h
 
 Copyright: Copyright (C) 2013 Mitsuru Nakada All Rights Reserved.
 License: GNU GPL v2
 */

#ifndef __Command_h__
#define __Command_h__

typedef unsigned char   byte;
static const byte CmdContinue      = (1 << 7);

enum Command {
  CmdGetVer                    = 0x01,
  CmdBoot                      = 0x02,
  CmdUpdate                    = 0x03,
  CmdRebootFW                  = 0x04,
  CmdEraseFW                   = 0x05,
  CmdReadMemory                = 0x08,
  CmdReadFlash                 = 0x09,
  
  CmdIRSend                    = 0x10 | CmdContinue,
  CmdGetGPI                    = 0x11,
  CmdHACtrl                    = 0x12,
  CmdHAStat                    = 0x13,
  CmdGetADC                    = 0x14,
  CmdLEDTape                   = 0x15,
  CmdValidateHA2               = 0x16,
  CmdIRRecord                  = 0x17,
  CmdGetADCData                = 0x18,
  CmdKeypadCtrl                = 0x19,

  CmdI2CWrite                  = 0x20,
  CmdI2CRead                   = 0x21,
  
  CmdRebootNotification        = 0x80,
  CmdBootModeNotification      = 0x82,
   
  CmdStatusChangeNotification  = 0x92,
  CmdIRReceiveNotification     = 0x94,
  CmdKeypadNotification        = 0x99,
};

static const byte SeqFinal         = (1 << 7);

static const byte FlagSuccess      = (0 << 7);
static const byte FlagError        = (1 << 7);

static const byte RebootPowerOn    = (1 << 0);
static const byte RebootReset      = (1 << 1);
static const byte RebootLowVoltage = (1 << 2);
static const byte RebootWDT        = (1 << 3);

#endif // __Command_h__