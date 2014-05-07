/*
 XBee.h
 
 Copyright: Copyright (C) 2013,2014 Mitsuru Nakada All Rights Reserved.
 License: GNU GPL v2
 */

#ifndef __XBee_h__
#define __XBee_h__

#include "CRC.h"

enum Mode {
  Mode_Unknown = 0,
  Mode_AT,
  Mode_API,
  Mode_Boot,
};

class XBee : private CRC {
public:
  XBee();
  ~XBee();
  
  int Initialize(const char *uart);
  void Finalize();
  
  int SendText(const char *buf);
  int ReceiveText(char *buf, int size);
  int SendATCommand(unsigned int addrL, const char *cmd, unsigned char *data = NULL, int size = 0, unsigned char *retBuf = NULL, int retSize = 0);
  int SendAVRCommand(unsigned int addrL, unsigned char cmd, unsigned char *data = NULL, int size = 0, unsigned char *retBuf = NULL, int retSize = 0);
  int SendFirmware(const char *file);
  int GetMode() { return Mode; };
  int SetBaudRate(int bps);
  int GetBaudRate() { return BaudRate; };
  int EnterBootMode();
  int LeaveBootMode();
  void EnableLog() {LogEnable = 1;};
  void DisableLog() {LogEnable = 0;};
  

private:
  int CheckMode();
  int CheckBootMode();
  int SendPacket(int len, unsigned char *buf);
  int ReceivePacket(unsigned char *buf);
  int GetByte();

  static const unsigned int ADDRH = 0x0013a200;
  int FrameID;
  
  int UartFd;
  int BaudRate;
  int Mode;
  int Timeout;
  int LogEnable;
  
  static const unsigned char SOH = 0x01;
  static const unsigned char STX = 0x02;
  static const unsigned char EOT = 0x04;
  static const unsigned char ACK = 0x06;
  static const unsigned char NAK = 0x15;
  static const unsigned char CAN = 0x18;
  static const unsigned char NAKC = 0x43;
 
};

#endif // __XBee_h__

