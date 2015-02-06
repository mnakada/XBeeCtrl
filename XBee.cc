/*
 XBee.cc

 Copyright: Copyright (C) 2013,2014 Mitsuru Nakada All Rights Reserved.
 License: GNU GPL v2
*/

#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/time.h>

#include "XBee.h"
#include "Error.h"

XBee::XBee() {

  FrameID = 1;
  UartFd = -1;
  BaudRate = 0;
  Mode = Mode_Unknown;
  Timeout = 100;
  LogEnable = 0;
  EscapedFlag = 0;
}

XBee::~XBee() {

  Finalize();
}

int XBee::Initialize(const char *device) {
  
  if(UartFd < 0) {
    UartFd = open(device, O_RDWR | O_NONBLOCK | O_NOCTTY);
    if(UartFd < 0) {
      fprintf(stderr, "cannot open %s\n", device);
      return Error;
    }
  }
  
  Timeout = 200;
  int error = Error;
  BaudRate = B9600;
  if(SetBaudRate(BaudRate)) return Error;
  error = CheckMode();
  if(error) {
    BaudRate = B115200;
    if(SetBaudRate(BaudRate)) return Error;
    error = CheckMode();
  }
  if(error) error = CheckBootMode();
  if(error) {
    BaudRate = B38400;
    if(SetBaudRate(BaudRate)) return Error;
    error = CheckMode();
  }
  if(error) {
    BaudRate = B19200;
    if(SetBaudRate(BaudRate)) return Error;
    error = CheckMode();
  }
  if(error) {
    BaudRate = B57600;
    if(SetBaudRate(BaudRate)) return Error;
    error = CheckMode();
  }
  if(error) {    
    fprintf(stderr, "Device Error\n");
    close(UartFd);
    UartFd = -1;
    return Error;
  }
  Timeout = 100;
  return Success;
}

void XBee::Finalize() {

  if(UartFd) close(UartFd);
}

int XBee::SetBaudRate(int bps) {
  
  if(LogEnable) fprintf(stderr, "SetBaudRate %d\n", bps);
  struct termios current_termios;
  tcgetattr(UartFd, &current_termios);
  cfmakeraw(&current_termios);
  cfsetspeed(&current_termios, bps);
  current_termios.c_cflag &= ~(PARENB | PARODD);
  current_termios.c_cflag = (current_termios.c_cflag & ~CSIZE) | CS8;
  current_termios.c_cflag &= ~(CRTSCTS);
  current_termios.c_iflag &= ~(IXON | IXOFF | IXANY);
  current_termios.c_cflag |= CLOCAL;
  current_termios.c_cflag &= ~(HUPCL);
  if(int error = tcsetattr(UartFd, TCSAFLUSH, &current_termios)) {
    fprintf(stderr, "Illegal parameter\n");
    close(UartFd);
    UartFd = -1;
    return Error;
  }
  return Success;
}

int XBee::CheckMode() {
  
  Mode = Mode_Unknown;
  char buf[256];
  SendText("+++");
  int size = ReceiveText(buf, 256);
  if((size == 2) && !strcmp(buf, "OK")) { // AT mode
    Mode= Mode_AT;
    SendText("ATVR\r");
    size = ReceiveText(buf, 256);
    if(size < 0) return size;
    return Success;
  }
  
  unsigned char retBuf[16];
  size = SendATCommand(0, "AP", NULL, 0, retBuf, 16);
  if(size != 2) return Error;
  Mode = Mode_API;
  if(retBuf[1] == 2) Mode = Mode_API2;
  return Success;
}

int XBee::CheckBootMode() {
  
  int retryCount = 0;
  int blFlag = 0;
  do {
    SendText("\r");
    do {
      char buf[256];
      int size = ReceiveText(buf, 255);
      if(size < 0) break;
      if(LogEnable) fprintf(stderr, "[%s]\n", buf);
      if(!strncmp(buf, "BL >", 4)) blFlag = 1;
    } while(!blFlag);
  } while(!blFlag && (retryCount++ < 4));
  if(!blFlag) return Error;
  Mode = Mode_Boot;
  return Success;
}

int XBee::SendText(const char *buf) {
  
  int size = strlen(buf);
  if(LogEnable) {
    for(int i = 0; i < size; i++) fprintf(stderr, "<%02x>", buf[i]);
    fprintf(stderr, "\n");
  }
  int num = write(UartFd, buf, size);
  if(num != size) return Error;
  return Success;
}

int XBee::ReceiveText(char *buf, int size) {
  
  int p = 0;
  int d = 0;
  do {
    d = GetByte();
    if(LogEnable) fprintf(stderr, "[%02x]", d & 0xff);
    if(d < 0) break;
    if((d == 0x0d) || (d == 0x0a)) break;
    buf[p++] = d;
  } while(p < size - 1);
  buf[p] = 0;
  if(p > 0) return p;
  return d;
}

int XBee::SendATCommand(unsigned int addrL, const char *cmd, unsigned char *data, int size, unsigned char *retBuf, int retSize) {

  unsigned char sendBuf[256];
  int cmdOffset;
  int cmpLen;
  int resOffset;
  int lastTimeout = Timeout;
  if(addrL) {
    sendBuf[0] = 0x17;
    sendBuf[2] = (ADDRH >> 24) & 0xff;
    sendBuf[3] = (ADDRH >> 16) & 0xff;
    sendBuf[4] = (ADDRH >> 8) & 0xff;
    sendBuf[5] = (ADDRH) & 0xff;
    sendBuf[6] = (addrL >> 24) & 0xff;
    sendBuf[7] = (addrL >> 16) & 0xff;
    sendBuf[8] = (addrL >> 8) & 0xff;
    sendBuf[9] = (addrL) & 0xff;
    sendBuf[10] = 0xff;
    sendBuf[11] = 0xfe;
    sendBuf[12] = 0x02; // apply changes
    cmpLen = 9;
    cmdOffset = 13;
    resOffset = 14;
    Timeout = 3000;
  } else {
    sendBuf[0] = 0x08;
    cmpLen = 1;
    cmdOffset = 2;
    resOffset = 4;
  }
  sendBuf[cmdOffset] = cmd[0];
  sendBuf[cmdOffset + 1] = cmd[1];
  for(int i = 0; i < size; i++) sendBuf[cmdOffset + 2 + i] = data[i];

  int retry = 0;
  int ret = 0;
  while(retry < 3) {
    retry++;
    sendBuf[1] = FrameID;
    int error = SendPacket(cmdOffset + 2 + size, sendBuf);
    if(error < 0) continue;
    
    unsigned char receiveBuf[256];
    struct timeval timeout;
    gettimeofday(&timeout, NULL);
    timeout.tv_sec++;
    while(1) {
      ret = error = ReceivePacket(receiveBuf);
      if(error < 0) break;

      if((receiveBuf[0] == (sendBuf[0] | 0x80)) && !memcmp(receiveBuf + 1, sendBuf + 1, cmpLen)) 
        break;

      struct timeval now;
      gettimeofday(&now, NULL);
      if(timercmp(&now, &timeout, >)) {
        fprintf(stderr, "timeout\n");
        error = Error;
        break;
      }
    }
    FrameID++;
    if(FrameID > 255) FrameID = 1;
    if(error < 0) continue;
    
    if(retSize > ret - resOffset) retSize = ret - resOffset;
    if(retSize < 0) retSize = 0;
    if(retBuf) memcpy(retBuf, receiveBuf + resOffset, retSize);
    Timeout = lastTimeout;
    return ret  - resOffset;
  }
  Timeout = lastTimeout;
  return Error;
}

int XBee::SendAVRCommand(unsigned int addrL, unsigned char cmd, unsigned char *data, int size, unsigned char *retBuf, int retSize) {
  
  int lastTimeout = Timeout;
  unsigned char sendBuf[256];
  sendBuf[0] = 0x10;
  sendBuf[2] = 0x00;
  sendBuf[3] = 0x13;
  sendBuf[4] = 0xa2;
  sendBuf[5] = 0x00;
  sendBuf[6] = addrL >> 24;
  sendBuf[7] = addrL >> 16;
  sendBuf[8] = addrL >> 8;
  sendBuf[9] = addrL;
  sendBuf[10] = 0xff;
  sendBuf[11] = 0xfe;
  sendBuf[12] = 0x00;
  sendBuf[13] = 0x00;
  sendBuf[15] = cmd;
  if(size) memcpy(sendBuf + 16, data, size);
  Timeout = 3000;

  int retry = 0;
  int ret = 0;
  while(retry < 3) {
    retry++;
    sendBuf[1] = FrameID;
    sendBuf[14] = FrameID;
    int error = SendPacket(size + 16, sendBuf);
    if(error < 0) continue;
    
    unsigned char receiveBuf[256];
    struct timeval timeout;
    gettimeofday(&timeout, NULL);
    timeout.tv_sec++;
    while(1) {
      ret = error = ReceivePacket(receiveBuf);
      if(error < 0) break;

      if((receiveBuf[0] == 0x90) && (receiveBuf[12] == FrameID) && (receiveBuf[13] == cmd)) break;

      struct timeval now;
      gettimeofday(&now, NULL);
      if(timercmp(&now, &timeout, >)) {
        fprintf(stderr, "timeout\n");
        error = Error;
        break;
      }
    }
    FrameID++;
    if(FrameID > 255) FrameID = 1;
    if(error < 0) continue;
    
    if(retSize > ret - 14) retSize = ret - 14;
    if(retBuf) memcpy(retBuf, receiveBuf + 14, retSize);
    Timeout = lastTimeout;
    return ret - 14;
  }
  Timeout = lastTimeout;
  return Error;
}

void XBee::AddBufferWithEsc(unsigned char *buf, int *p, unsigned char d) {

  if(Mode == Mode_API2) {  
    if((d == 0x7e) || (d == 0x7d) || (d == 0x11) || (d == 0x13)) {
      d ^= 0x20;
      buf[(*p)++] = 0x7d;
    }
  }
  buf[(*p)++] = d;
}

int XBee::SendPacket(int len, unsigned char *buf) {
  
  if(len > 255) return Error;
  
  unsigned char sendBuf[520];
  int p = 0;
  sendBuf[p++] = 0x7e;
  AddBufferWithEsc(sendBuf, &p, len >> 8);
  AddBufferWithEsc(sendBuf, &p, len);
  unsigned char sum = 0;
  for(int i = 0; i < len; i++) {
    sum += buf[i];
    AddBufferWithEsc(sendBuf, &p, buf[i]);
  }
  AddBufferWithEsc(sendBuf, &p, 0xff - sum);

  int num = write(UartFd, sendBuf, p);
  if(num != p) return Error;
  
  if(LogEnable) {
    for(int i = 0; i < len; i++) fprintf(stderr, "<%02x>", buf[i]);
    fprintf(stderr, "\n");
  }
  return Success;
}

int XBee::ReceivePacket(unsigned char *buf) {
  
  int count = 0; 
  do {
    int c = GetByte();
    if(c < 0) return c;
    if(LogEnable) fprintf(stderr, "[%02x]", c & 0xff);
    if(c == 0x7e) break;
  } while(1);
  
  int len1 = GetByteWithEsc();
  if(len1 < 0) return len1;
  if(LogEnable) fprintf(stderr, "[%02x]", len1);
  
  int len = GetByteWithEsc();
  if(len < 0) return len;
  len |= len1 << 8;
  if(LogEnable) fprintf(stderr, "[%02x]", len);
  
  int sum = 0;
  for(int i = 0; i < len; i++) {
    int dat = GetByteWithEsc();
    if(dat < 0) return dat;
    if(LogEnable) fprintf(stderr, "[%02x]", dat);
    buf[i] = dat;
    sum += dat;
  }
  int csum = GetByteWithEsc();
  if(csum < 0) return csum;
  sum += csum;
  if(LogEnable) fprintf(stderr, "[%02x]\n", csum);
  
  if((sum & 0xff) != 0xff) {
    fprintf(stderr, "ReceivePacket sum error %02x\n", (sum & 0xff));
    return Error;
  }
  return len;
}

int XBee::EnterBootMode() {

  if(Mode == 3) return Success;
  if(Mode == 1) {
    char buf[256];
    SendText("AT%P\r");
    int size = ReceiveText(buf, 256);
    if(size < 0) return size;
  } else if(Mode == 2) {
    unsigned char res[256];
    int size = SendATCommand(0, "%P");
    if(size < 0) return size;
  }
  if(int error = SetBaudRate(B115200)) return error;
  sleep(2);
  return Success;
}

int XBee::LeaveBootMode() {

  if(int error = CheckBootMode()) return error;
  SendText("2\r");
  sleep(2);
  return Success;
}

int XBee::SendFirmware(const char *file) {
  
  int lastTimeout = Timeout;
  if(int error = CheckBootMode()) return error;
  
  SendText("1");
  struct stat st;
  int error = stat(file, &st);
  if(error < 0) {
    fprintf(stderr,"stat eror\n");
    return Error;
  }
  int blockMax = (int)(st.st_size + 127) / 128;
  
  FILE *fp = fopen(file, "r");
  if(fp == NULL) {
    fprintf(stderr, "Firmware file not found %s\n", file);
    return Error;
  }
  int retryCount = 0;
  do {
    int d = GetByte();
    if(d == NAKC) break;
    if(retryCount++ > 32) {
      fclose(fp);
      fprintf(stderr, "XMODEM Start Error\n");
      return Error;
    }
  } while(1);
  
  Timeout = 5000;
  retryCount = 0;
  unsigned char buf[256];
  int blockNum = 1;
  do {
    fprintf(stderr, "%d / %d\r", blockNum, blockMax);
    buf[0] = SOH;
    buf[1] = blockNum;
    buf[2] = (blockNum ^ 0xff) & 0xff;
    int size = fread(buf + 3, 1, 128, fp);
    if(size == 0) break;
    if(size < 128) {
      for(int i = size; i < 128; i++) buf[3 + i] = 0;
    }
    unsigned short crc = CalcCRC(buf + 3, 128, 0);
    buf[131] = crc >> 8;
    buf[132] = crc;
    int num = write(UartFd, buf, 133);
    if(num != 133) {
      fclose(fp);
      fprintf(stderr, "\nXMODEM write error\n");
      Timeout = lastTimeout;
      return Error;
    }
    
    int d = GetByte();
    if(d < 0) {
      fprintf(stderr, "Timeout\n");
    }
    if(d == ACK) {
      blockNum++;
      retryCount = 0;
    } else if(d == NAK) {
      fseek(fp, (blockNum - 1) * 128, SEEK_SET);
      if(retryCount++ > 32) {
        fclose(fp);
        fprintf(stderr, "\nXMODEM retry Error\n");
        buf[0] = CAN;
        write(UartFd, buf, 1);
        Timeout = lastTimeout;
        return Error;
      }
    } else {
      fprintf(stderr, "\nAck/Nack Error block %d %02x\n", blockNum, d & 0xff);
      fclose(fp);
      fprintf(stderr, "\nXMODEM protocol Error\n");
      buf[0] = CAN;
      write(UartFd, buf, 1);
      Timeout = 100;
      return Error;
    }
  } while(blockNum <= blockMax);
  buf[0] = EOT;
  write(UartFd, buf, 1);
  Timeout = lastTimeout;
  
  fclose(fp);
  fprintf(stderr, "\n");
  return Success;
}

int XBee::GetByte() {
  
  struct timeval now;
  struct timeval timeout;
  gettimeofday(&now, NULL);
  timeout.tv_sec = Timeout / 1000;
  timeout.tv_usec = (Timeout % 1000) * 1000;
  timeradd(&now, &timeout, &timeout);
  do {
    int c;
    int n = read(UartFd, &c, 1);
    if(n > 0) return (c & 0xff);
    gettimeofday(&now, NULL);
    if(timercmp(&now, &timeout, >)) return Error;
  } while(1);
}

int XBee::GetByteWithEsc() {

  int d = GetByte();
  if(Mode != Mode_API2) return d;
  if(d < 0) return d;
  if(d == 0x7d) {
    EscapedFlag = 0x20;
    d = GetByte();
    if(d < 0) return d;
  }
  d ^= EscapedFlag;
  EscapedFlag = 0;
  return d;
}
