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

#include "XBee.h"
#include "Error.h"

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

  unsigned char avrcmd = 0;
  if(!strcmp(cmd, "irsend")) avrcmd = 0x10;
  if(!strcmp(cmd, "ha1off")) avrcmd = 0x20;
  if(!strcmp(cmd, "ha1on")) avrcmd = 0x21;
  if(!strcmp(cmd, "ha2off")) avrcmd = 0x24;
  if(!strcmp(cmd, "ha2on")) avrcmd = 0x25;
  if(!strcmp(cmd, "hastat")) avrcmd = 0x28;
  if(!strcmp(cmd, "halt")) avrcmd = 0x40;
  if(!strcmp(cmd, "readm")) avrcmd = 0x41;
  if(!strcmp(cmd, "readf")) avrcmd = 0x42;
  if(!strcmp(cmd, "getver")) avrcmd = 0x44;
  if(!strcmp(cmd, "boot")) avrcmd = 0x48;
  if(!strcmp(cmd, "update")) avrcmd = 0x4c;
  if(!strcmp(cmd, "reboot")) avrcmd = 0x4d;
  
  if(avrcmd == 0) { // AT command
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
  
  if(avrcmd == 0x10) { // irsend
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
    for(int i = 0; i < size; i+= 128) {
      int ret = XBee.SendAVRCommand(addrl, avrcmd, buf + i, 128);
      if((ret < 0) || (ret & 0x80)) {
        fprintf(stderr, "\nret=%02x\n", ret);
        i--;
        retry++;
        if(retry > 5) {
          fprintf(stderr, "Retry Error\n");
          XBee.Finalize();
          free(buf);
          return Error;
        }
      } else {
        retry = 0;
        avrcmd++;
      }
    }
    XBee.Finalize();
    free(buf);
    fprintf(stderr, "Complete.\n");
    return Success;
  }

  if(avrcmd == 0x4c) { // update
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
      int ret = XBee.SendAVRCommand(addrl, avrcmd, buf + i * 129, 129);
      if((ret < 0) || (ret & 0x80)) {
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

  if((avrcmd == 0x41) || (avrcmd == 0x42)) { // readm, readf
    unsigned char buf[2];
    int addr = strtoul(argv[4], NULL, 16);
    buf[0] = addr >> 8;
    buf[1] = addr & 0xff;
    unsigned char retBuf[256];
    int ret = XBee.SendAVRCommand(addrl, avrcmd, buf, 2, retBuf, 256);
    if((ret < 0) || (ret & 0x80)) {
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

  // other AVR command
  unsigned char retBuf[256];
  int ret = XBee.SendAVRCommand(addrl, avrcmd, NULL, 0, retBuf, 256);
  if((ret < 0) || (ret & 0x80)) {
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
