/*
 HA-1 Command.h
 
 Copyright: Copyright (C) 2013 Mitsuru Nakada All Rights Reserved.
 License: GNU GPL v2
 */

#ifndef __Command_h__
#define __Command_h__

static const unsigned char CmdContinue = (1 << 7);

enum Command {
  CmdHalt                  = 0x00,
  CmdGetVer                = 0x01,
  CmdBoot                  = 0x02,
  CmdUpdate                = 0x03,
  CmdRebootFW              = 0x04,
  CmdReadMemory            = 0x08,
  CmdReadFlash             = 0x09,
  CmdOSCCalibration        = 0x0a,

  CmdIRSend                = 0x10 | CmdContinue,
  CmdHACtrl                = 0x12,
  CmdHAStat                = 0x13,
  CmdTest1                 = 0x20,
  CmdTest2                 = 0x21 | CmdContinue,
  
  CmdRebootNotification    = 0x80,
  CmdCalibrateNotification = 0x8a,

  CmdHAChangeNotification  = 0x92,
  CmdIRReceiveNotification = 0x94,
};

static const unsigned char SeqFinal = (1 << 7);

static const unsigned char FlagSuccess = (0 << 7);
static const unsigned char FlagError = (1 << 7);

static const unsigned char RebootPowerOn    = (1 << 0);
static const unsigned char RebootReset      = (1 << 1);
static const unsigned char RebootLowVoltage = (1 << 2);
static const unsigned char RebootWDT        = (1 << 3);

static const unsigned char ReasonLowPower = 1;
static const unsigned char ReasonAutoShutdown = 2;
static const unsigned char ReasonPowerSW = 3;

#endif // __Command_h__