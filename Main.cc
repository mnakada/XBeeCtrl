/*
 XBeeCtrl Main.cc
 
 Copyright: Copyright (C) 2013,2014 Mitsuru Nakada All Rights Reserved.
 License: GNU GPL v2
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>

#include "XBee.h"
#include "Error.h"
#include "Command.h"

int main(int argc, char **argv) {

  if(argc < 4) {
    fprintf(stderr, "%s <dev> <addrL> <cmd> [<param> ...]\n", argv[0]);
    fprintf(stderr, "    addrL : -                 local\n");
    return -1;
  }

  unsigned int addrl = 0;
  if(strcmp(argv[2], "-")) addrl = strtoul(argv[2], NULL, 16);
  char *cmd = argv[3];
  unsigned char buf[256];
  for(int i = 4; i < argc; i++) {
    buf[i - 4] = strtoul(argv[i], NULL, 16);
  }

  XBee XBee;
  int error = XBee.Initialize(argv[1]);
  if(error < 0) return error;
  fprintf(stderr, "BaudRate %d\n", XBee.GetBaudRate());
  int mode = XBee.GetMode();
  if(mode != Mode_API) {
    fprintf(stderr, "Mode Error %d\n", mode);
    XBee.Finalize();
    return Error;
  }
  XBee.EnableLog();
  
  unsigned char avrcmd = 0;
  if(!strcmp(cmd, "irsend")) avrcmd = CmdIRSend;
  if(!strcmp(cmd, "hactrl")) avrcmd = CmdHACtrl;
  if(!strcmp(cmd, "hastat")) avrcmd = CmdHAStat;
  if(!strcmp(cmd, "adc")) avrcmd = CmdGetADC;
  if(!strcmp(cmd, "i2cw")) avrcmd = CmdI2CWrite;
  if(!strcmp(cmd, "i2cr")) avrcmd = CmdI2CRead;
  if(!strcmp(cmd, "led")) avrcmd = CmdLEDTape;

  if(!strcmp(cmd, "halt")) avrcmd = CmdHalt;
  if(!strcmp(cmd, "getver")) avrcmd = CmdGetVer;
  if(!strcmp(cmd, "boot")) avrcmd = CmdBoot;
  if(!strcmp(cmd, "update")) avrcmd = CmdUpdate;
  if(!strcmp(cmd, "reboot")) avrcmd = CmdRebootFW;
  if(!strcmp(cmd, "readm")) avrcmd = CmdReadMemory;
  if(!strcmp(cmd, "readf")) avrcmd = CmdReadFlash;
  if(!strcmp(cmd, "cal")) avrcmd = CmdOSCCalibration;

  if(!strcmp(cmd, "irrecv")) {
    while(1) {
      unsigned char retBuf[256];
      int size = XBee.ReceivePacket(retBuf);
      if((size > 0) && (retBuf[13] == CmdIRReceiveNotification)) {
        for(int i = 15; i < size; i++) fprintf(stderr, "%02x ", retBuf[i]);
        fprintf(stderr, "\n");
      }
    }
  }
 
  if(avrcmd == 0) { // AT command
    fprintf(stderr, "ATX command\n");
    unsigned char retBuf[256];
    XBee.EnableLog();
    int size = XBee.SendATCommand(addrl, cmd, buf, argc - 4, retBuf, 256);
    if(size > 0) {
      switch(retBuf[0]) {
        case 0:
          fprintf(stderr, "OK\n");
          break;
        case 1:
          fprintf(stderr, "error\n");
          break;
        case 2:
          fprintf(stderr, "Invalid Command\n");
          break;
        case 3:
          fprintf(stderr, "Invalid Parameter\n");
          break;
        case 4:
          fprintf(stderr, "Tx Failure\n");
          break;
        default:
          fprintf(stderr, "Unknown Error %02x\n", retBuf[0]);
          break;
      }
      for(int i = 1; i < size; i++) fprintf(stderr, "%02x ", retBuf[i]);
      fprintf(stderr, "\n");
    } else {
      fprintf(stderr, "SendATCommand Error\n");
      XBee.Finalize();
      return Error;
    }
    XBee.Finalize();
    fprintf(stderr, "Complete.\n");
    return Success;
  }
  
  if(avrcmd == CmdIRSend) { // irsend
    unsigned char *buf = (unsigned char *)malloc(1000);
    if(!buf) {
      fprintf(stderr, "malloc error\n");
      XBee.Finalize();
      return Error;
    }
    
    FILE *fp = fopen(argv[4], "r");
    if(fp == NULL) {
      fprintf(stderr, "file error %s\n", argv[4]);
      free(buf);
      return Error;
    }
    int size = fread(buf, 1, 1000, fp);
    fprintf(stderr, "file %s size %d\n", argv[4], size);
    fclose(fp);
    
    int retry = 0;
    int seq = 0;
    int offset = 0;
    while(size) {
      int sz = size;
      if(sz > 128) sz = 128;
      
      unsigned char buf2[129];
      memcpy(buf2 + 1, buf + offset, sz);
      buf2[0] = seq;
      if(size == sz) buf2[0] |= SeqFinal;
      unsigned char retBuf[1];
      int ret = XBee.SendAVRCommand(addrl, avrcmd, buf2, sz + 1, retBuf, 1);
      if((ret < 0) || (retBuf[0] & 0x80)) {
        fprintf(stderr, "\nret=%02x\n", retBuf[0]);
        retry++;
        if(retry > 5) {
          fprintf(stderr, "Retry Error\n");
          XBee.Finalize();
          free(buf);
          return Error;
        }
      } else {
        retry = 0;
        seq++;
        size -= sz;
        offset += sz;
      }
    }
    XBee.Finalize();
    free(buf);
    fprintf(stderr, "Complete.\n");
    return Success;
  }

  if(avrcmd == CmdUpdate) { // update
    unsigned char *buf = (unsigned char *)malloc(32768);
    if(!buf) {
      fprintf(stderr, "malloc error\n");
      XBee.Finalize();
      return Error;
    }
    
    FILE *fp = fopen(argv[4], "r");
    if(fp == NULL) {
      fprintf(stderr, "file error %s\n", argv[4]);
      XBee.Finalize();
      free(buf);
      return Error;
    }
    memset(buf, 0xff, 32768);
    int seqs = 0;
    while(!feof(fp)) {
      buf[seqs * 129] = seqs;
      int size = fread(buf + seqs * 129 + 1, 1, 128, fp);
      if(!size) break;
      seqs++;
      if(size != 128) break;
    }
    fclose(fp);
    
    int retry = 0;
    for(int i = 0; i < seqs; i++) {
      fprintf(stderr, "%d / %d\r", i, seqs);
      unsigned char retBuf[1];
      int ret = XBee.SendAVRCommand(addrl, avrcmd, buf + i * 129, 129, retBuf, 1);
      if((ret < 0) || (retBuf[0] & 0x80)) {
        fprintf(stderr, "\nret=%02x\n", ret);
        i--;
        retry++;
        if(retry > 5) {
          fprintf(stderr, "Update Error\n");
          XBee.Finalize();
          free(buf);
          return Error;
        }
      } else {
        retry = 0;
      }
    }
    XBee.Finalize();
    free(buf);
    fprintf(stderr, "Complete.\n");
    return Success;
  }

  if((avrcmd == CmdReadMemory) || (avrcmd == CmdReadFlash)) { // readm, readf
    if(argc < 5) {
      XBee.Finalize();
      fprintf(stderr, "Illegal Parameter\n");
      return Error;
    }
    unsigned char buf[3];
    int addr = strtoul(argv[4], NULL, 16);
    buf[0] = addr >> 8;
    buf[1] = addr & 0xff;
    int size = 16;
    if(argc > 5) size = strtoul(argv[5], NULL, 16);
    fprintf(stderr, "debug %d %d\n", size, argc);
    if(size > 32) size = 32;
    buf[2] = size;
    unsigned char retBuf[256];
    int ret = XBee.SendAVRCommand(addrl, avrcmd, buf, 3, retBuf, 256);
    if((ret < 0) || (retBuf[0] & 0x80)) {
      fprintf(stderr, "\nret=%02x\n", ret);
    } else {
      for(int i = 1; i < ret; i++) {
        if((i % 16) == 1) fprintf(stderr, "\n%04x : ", addr - 1 + i);
        fprintf(stderr, "%02x ", retBuf[i]);
      }
      fprintf(stderr, "\n");
    }
    XBee.Finalize();
    fprintf(stderr, "Complete.\n");
    return Success;
  }    

  if(avrcmd == CmdLEDTape) { // readm, readf
    if(argc < 6) {
      XBee.Finalize();
      fprintf(stderr, "Illegal Parameter\n");
      return Error;
    }
    unsigned char buf[4];
    buf[0] = strtoul(argv[4], NULL, 10);
    unsigned int color = strtoul(argv[5], NULL, 16);
    buf[1] = color >> 16;
    buf[2] = color >> 8;
    buf[3] = color;
    
    unsigned char retBuf[256];
    int ret = XBee.SendAVRCommand(addrl, avrcmd, buf, 4, retBuf, 256);
    if((ret < 0) || (retBuf[0] & 0x80)) {
      fprintf(stderr, "\nret=%02x\n", ret);
    } else {
      for(int i = 1; i < ret; i++) {
        fprintf(stderr, "%02x ", retBuf[i]);
      }
      fprintf(stderr, "\n");
    }
    XBee.Finalize();
    fprintf(stderr, "Complete.\n");
    return Success;
  }    
  
  if(avrcmd == CmdGetADC) { // adc
    unsigned char retBuf[256];
    int ret = XBee.SendAVRCommand(addrl, avrcmd, NULL, 0, retBuf, 256);
    if((ret < 0) || (retBuf[0] & 0x80)) {
      fprintf(stderr, "\nret=%02x\n", ret);
    } else {
      for(int i = 1; i < ret; i += 2) {
        fprintf(stderr, "%d ", (retBuf[i] << 8) | retBuf[i + 1]);
      }
      fprintf(stderr, "\n");
    }
    XBee.Finalize();
    fprintf(stderr, "Complete.\n");
    return Success;
  }    
  
  if(avrcmd == CmdOSCCalibration) { // cal

    XBee.DisableLog();
    unsigned char buf[256];
    int count = 5;
    unsigned char calib = 0x47;
    if(argc > 4) count = strtoul(argv[4], NULL, 10);
    if(argc > 5) calib = strtoul(argv[5], NULL, 16);
    
    buf[0] = count;
    buf[1] = calib;
    unsigned char retBuf[256];
    int ret = XBee.SendAVRCommand(addrl, avrcmd, buf, 2, retBuf, 256);
    if((ret < 0) || (retBuf[0] & 0x80)) {
      fprintf(stderr, "\nret=%02x\n", ret);
    }
    struct timeval last;
    gettimeofday(&last, NULL);
    int n = 0;
    int ave = 0;
    while(n < count) {
      int error = Error;
      while(1) {
        error = XBee.ReceivePacket(retBuf);
        if(error < 0) break;
        if((retBuf[0] == 0x90) && (retBuf[13] == CmdCalibrateNotification)) break;
      }
      if(error > 0) {
        struct timeval now;
        gettimeofday(&now, NULL);
        struct timeval dif;
        timersub(&now, &last, &dif);
        int ms = (dif.tv_usec + (dif.tv_sec * 1000 * 1000) + 500) / 1000;
        int tick = (retBuf[15] <<  8) | retBuf[16];
        if(n) ave += ms;
        fprintf(stderr, "CalbNotification %d %d\n", ms, tick);
        last = now;
        n++;
      }
    }
    fprintf(stderr, "Calib %d\n", ave / (count - 1));

    XBee.Finalize();
    fprintf(stderr, "Complete.\n");
    return Success;
  }
  
  {
    fprintf(stderr, "AVR command\n");
    // other AVR command
    unsigned char buf[256];
    for(int i = 4; i < argc; i++) {
      buf[i - 4] = strtoul(argv[i], NULL, 16);
    }
    unsigned char retBuf[256];
    int ret = XBee.SendAVRCommand(addrl, avrcmd, buf, argc - 4, retBuf, 256);
    if((ret < 0) || (retBuf[0] & 0x80)) {
      fprintf(stderr, "\nret=%02x\n", ret);
    } else {
      for(int i = 1; i < ret; i++) {
        fprintf(stderr, "%02x ", retBuf[i]);
      }
      fprintf(stderr, "\n");
    }
    XBee.Finalize();
    fprintf(stderr, "Complete.\n");
    return Success;
  }
}
