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

#define COLOR_RED "\033[31m"
#define COLOR_DEFAULT "\033[0m"

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
  if(!strcmp(cmd, "irrec")) avrcmd = CmdIRRecord;
  if(!strcmp(cmd, "hactrl")) avrcmd = CmdHACtrl;
  if(!strcmp(cmd, "hastat")) avrcmd = CmdHAStat;
  if(!strcmp(cmd, "adc")) avrcmd = CmdGetADC;
  if(!strcmp(cmd, "adcd")) avrcmd = CmdGetADCData;
  if(!strcmp(cmd, "i2cw")) avrcmd = CmdI2CWrite;
  if(!strcmp(cmd, "i2cr")) avrcmd = CmdI2CRead;
  if(!strcmp(cmd, "led")) avrcmd = CmdLEDTape;

  if(!strcmp(cmd, "getver")) avrcmd = CmdGetVer;
  if(!strcmp(cmd, "boot")) avrcmd = CmdBoot;
  if(!strcmp(cmd, "update")) avrcmd = CmdUpdate;
  if(!strcmp(cmd, "reboot")) avrcmd = CmdRebootFW;
  if(!strcmp(cmd, "readm")) avrcmd = CmdReadMemory;
  if(!strcmp(cmd, "readf")) avrcmd = CmdReadFlash;
  if(!strcmp(cmd, "validate")) avrcmd = CmdValidateHA2;

  if(!strcmp(cmd, "irrecv")) {
    XBee.DisableLog();
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
    int size = fread(buf + 1, 1, 999, fp);
    fprintf(stderr, "file %s size %d\n", argv[4], size);
    fclose(fp);
    buf[0] = 0;
    size++;
    if(argc >= 6) buf[0] = atoi(argv[5]); 
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
    XBee.DisableLog();
    
    int size = 16;
    int addr = strtoul(argv[4], NULL, 16);
    if(argc > 5) size = strtoul(argv[5], NULL, 16);
    while(size) {
      int sz = size;
      if(sz > 32) sz = 32;
      unsigned char buf[3];
      buf[0] = addr >> 8;
      buf[1] = addr & 0xff;
      buf[2] = sz;
      unsigned char retBuf[256];
      int ret = XBee.SendAVRCommand(addrl, avrcmd, buf, 3, retBuf, 256);
      if((ret < 0) || (retBuf[0] & 0x80)) {
        fprintf(stderr, "\nret=%02x\n", ret);
      } else {
        for(int i = 1; i < ret; i++) {
          if(((i - 1) % 16) == 0) fprintf(stderr, "%04x : ", addr - 1 + i);
          fprintf(stderr, "%02x ", retBuf[i]);
          if(((i - 1) % 16) == 15) fprintf(stderr, "\n");
        }
      }
      size -= sz;
      addr += sz;
    }
    fprintf(stderr, "\n");
    XBee.Finalize();
    fprintf(stderr, "Complete.\n");
    return Success;
  }    

  if(avrcmd == CmdLEDTape) { // led 
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
        fprintf(stderr, "%dmV ", ((retBuf[i] << 8) | retBuf[i + 1]) * 3300 / 4096);
      }
      fprintf(stderr, "\n");
    }
    XBee.Finalize();
    fprintf(stderr, "Complete.\n");
    return Success;
  }    
  
  if(avrcmd == CmdGetADCData) { // adcd
    if(argc < 5) {
      XBee.Finalize();
      fprintf(stderr, "Illegal Parameter\n");
      return Error;
    }
    XBee.DisableLog();
    
    int ch = strtoul(argv[4], NULL, 10);
    unsigned char buf[1];
    buf[0] = ch;
    unsigned char retBuf[256];
    int ret = XBee.SendAVRCommand(addrl, avrcmd, buf, 1, retBuf, 256);
    if((ret < 0) || (retBuf[0] & 0x80)) {
      fprintf(stderr, "\nret=%02x\n", ret);
    } else {
      for(int i = 1; i < ret; i += 2) {
        fprintf(stderr, "%d\n", ((retBuf[i] << 8) | retBuf[i + 1]) * 3300 / 4096);
      }
      fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
    XBee.Finalize();
    fprintf(stderr, "Complete.\n");
    return Success;
  }    

  if(avrcmd == CmdIRRecord) { // irrec
    if(argc < 5) {
      XBee.Finalize();
      fprintf(stderr, "Illegal Parameter\n");
      return Error;
    }
    
    unsigned char buf[1];
    buf[0] = 10;
    unsigned char retBuf[256];
    int ret = XBee.SendAVRCommand(addrl, avrcmd, buf, 1, retBuf, 256);
    if((ret < 0) || (retBuf[0] & 0x80)) {
      fprintf(stderr, "\nret=%02x\n", ret);
    } else {
      for(int i = 1; i < ret; i++) {
        if((i % 16) == 1) fprintf(stderr, "\n");
        fprintf(stderr, "%02x ", retBuf[i]);
      }
      fprintf(stderr, "\n");
      FILE *fp = fopen(argv[4], "w");
      if(fp) {
        fwrite(retBuf + 1, 1, ret - 1, fp);
        fclose(fp);
      }
    }
    XBee.Finalize();
    fprintf(stderr, "Complete.\n");
    return Success;
  }    
  
  if(avrcmd == CmdValidateHA2) { // validate
    XBee.DisableLog();

    int stat;
    unsigned char retBuf[256];

    buf[0] = 0;
    buf[1] = 0;
    int size = XBee.SendATCommand(addrl, "ic", buf, 2, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }

    int ret = XBee.SendAVRCommand(addrl, avrcmd, NULL, 0, retBuf, 256);
    if(ret < 0) {
      fprintf(stderr, "\nret=%02x\n", ret);
    } else {
      stat = (retBuf[1] << 8) | retBuf[2];
      if(stat & (1 << 0)) fprintf(stderr, COLOR_RED "GPIO1(PC4)=Hi, GPIO2(PC5)=Low -> AD1(ADC6)=%dmV\n" COLOR_DEFAULT, ((retBuf[3] << 8) | retBuf[4]) * 3300 / 4096); 
      if(stat & (1 << 1)) fprintf(stderr, COLOR_RED "GPIO1(PC4)=Hi, GPIO2(PC5)=Low -> AD2(ADC7)=%dmV\n" COLOR_DEFAULT, ((retBuf[5] << 8) | retBuf[6]) * 3300 / 4096); 
      if(stat & (1 << 2)) fprintf(stderr, COLOR_RED "GPIO1(PC4)=Low, GPIO2(PC5)=Hi -> AD1(ADC6)=%dmV\n" COLOR_DEFAULT, ((retBuf[7] << 8) | retBuf[8]) * 3300 / 4096); 
      if(stat & (1 << 3)) fprintf(stderr, COLOR_RED "GPIO1(PC4)=Low, GPIO2(PC5)=Hi -> AD2(ADC7)=%dmV\n" COLOR_DEFAULT, ((retBuf[9] << 8) | retBuf[10]) * 3300 / 4096);
      if(stat & (1 << 4)) fprintf(stderr, COLOR_RED "IRH,IRL(PD5)=Hi -> SW1(PB0)=Low\n" COLOR_DEFAULT);
      if(stat & (1 << 5)) fprintf(stderr, COLOR_RED "IRH,IRL(PD5)=Hi -> SW3(PB2)=Low\n" COLOR_DEFAULT);
      if(stat & (1 << 6)) fprintf(stderr, COLOR_RED "IRH,IRL(PD5)=Low -> SW1(PB0)=Hi\n" COLOR_DEFAULT);
      if(stat & (1 << 7)) fprintf(stderr, COLOR_RED "IRH,IRL(PD5)=Low -> SW3(PB2)=Hi\n" COLOR_DEFAULT);
      if(stat & (1 << 8)) fprintf(stderr, COLOR_RED "HA1C(PC0)=Hi, HA2C(PC2)=Low -> HA1M(PC1) = Low\n" COLOR_DEFAULT);
      if(stat & (1 << 9)) fprintf(stderr, COLOR_RED "HA1C(PC0)=Hi, HA2C(PC2)=Low -> HA2M(PC3) = Hi\n" COLOR_DEFAULT);
      if(stat & (1 << 10)) fprintf(stderr, COLOR_RED "HA1C(PC0)=Low, HA2C(PC2)=Hi -> HA1M(PC1) = Hi\n" COLOR_DEFAULT);
      if(stat & (1 << 11)) fprintf(stderr, COLOR_RED "HA1C(PC0)=Low, HA2C(PC2)=Hi -> HA2M(PC3) = Low\n" COLOR_DEFAULT);
      if(!stat) fprintf(stderr, "AVR Validate command -> OK\n");
    }

    // XBee Test
    // GPIO1(DIO3)=Hi, GPIO2(DIO4)=Low -> AD1=770mV, AD2=0mV
    stat = 0;
    buf[0] = 2;
    size = XBee.SendATCommand(addrl, "d1", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    size = XBee.SendATCommand(addrl, "d2", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 5;
    size = XBee.SendATCommand(addrl, "d3", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 4;
    size = XBee.SendATCommand(addrl, "d4", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    size = XBee.SendATCommand(addrl, "is", buf, 0, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    if((retBuf[4] & 6) != 6) {
      fprintf(stderr, "IS Error %02x\n", retBuf[4]);
      XBee.Finalize();
      return Error;
    }
    int ad1 = ((retBuf[7] << 8) | retBuf[8]) * 1200 / 1024;
    int ad2 = ((retBuf[9] << 8) | retBuf[10]) * 1200 / 1024;
    if((ad1 < 718) | (ad1 > 821)) {
      stat = 1;
      fprintf(stderr, COLOR_RED "GPIO1(DIO3)=Hi, GPIO2(DIO4)=Low -> AD1=%dmV\n" COLOR_DEFAULT, ad1);
    }
    if(ad2 > 22) {
      stat = 1;
      fprintf(stderr, COLOR_RED "GPIO1(DIO3)=Hi, GPIO2(DIO4)=Low -> AD2=%dmV\n" COLOR_DEFAULT, ad2);
    }
    
    //  GPIO1(DIO3)=Low, GPIO2(DIO4)=Hi -> AD1=0mV, AD2=770mV
    buf[0] = 4;
    size = XBee.SendATCommand(addrl, "d3", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 5;
    size = XBee.SendATCommand(addrl, "d4", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    size = XBee.SendATCommand(addrl, "is", buf, 0, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    if((retBuf[4] & 6) != 6) {
      fprintf(stderr, "IS Error %02x\n", retBuf[4]);
      XBee.Finalize();
      return Error;
    }
    ad1 = ((retBuf[7] << 8) | retBuf[8]) * 1200 / 1024;
    ad2 = ((retBuf[9] << 8) | retBuf[10]) * 1200 / 1024;
    if(ad1 > 22) {
      stat = 1;
      fprintf(stderr, COLOR_RED "GPIO1(DIO3)=Low, GPIO2(DIO4)=Hi -> AD1=%dmV\n" COLOR_DEFAULT, ad1);
    }
    if((ad2 < 718) | (ad2 > 821)) {
      stat = 1;
      fprintf(stderr, COLOR_RED "GPIO1(DIO3)=Low, GPIO2(DIO4)=Hi -> AD2=%dmV\n" COLOR_DEFAULT, ad2);
    }
    
    buf[0] = 3;
    size = XBee.SendATCommand(addrl, "d3", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 3;
    size = XBee.SendATCommand(addrl, "d4", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    if(!stat) fprintf(stderr, "XBee GPIO1(DIO3), GPIO2(DIO4), AD1, AD2 -> OK\n" COLOR_DEFAULT);
     
    //  SW1(DIO10)=Hi, SW2(DIO11)=Low, SW3(DIO12)=Low -> DIO5=Hi, DIO6=Low, DIO7=Low
    stat = 0;
    buf[0] = 3;
    size = XBee.SendATCommand(addrl, "d5", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 3;
    size = XBee.SendATCommand(addrl, "d6", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 3;
    size = XBee.SendATCommand(addrl, "d7", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 5;
    size = XBee.SendATCommand(addrl, "p0", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 4;
    size = XBee.SendATCommand(addrl, "p1", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 4;
    size = XBee.SendATCommand(addrl, "p2", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    size = XBee.SendATCommand(addrl, "is", buf, 0, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    if((retBuf[3] & 0xe0) != 0xe0) {
      fprintf(stderr, "IS Error %02x%02x\n", retBuf[2], retBuf[3]);
      XBee.Finalize();
      return Error;
    }
    if((retBuf[6] & 0x20) != 0x20) fprintf(stderr, COLOR_RED "SW1(DIO10)=Hi, SW2(DIO11)=Low, SW3(DIO12)=Low -> DIO5=Low\n" COLOR_DEFAULT);
    if((retBuf[6] & 0x40) != 0x00) fprintf(stderr, COLOR_RED "SW1(DIO10)=Hi, SW2(DIO11)=Low, SW3(DIO12)=Low -> DIO6=Hi\n" COLOR_DEFAULT);
    if((retBuf[6] & 0x80) != 0x00) fprintf(stderr, COLOR_RED "SW1(DIO10)=Hi, SW2(DIO11)=Low, SW3(DIO12)=Low -> DIO7=Hi\n" COLOR_DEFAULT);
    if((retBuf[6] & 0xe0) != 0x20) stat = 1;

    //  SW1(DIO10)=Low, SW2(DIO11)=Hi, SW3(DIO12)=Low -> DIO5=Low, DIO6=Hi, DIO7=Low
    buf[0] = 4;
    size = XBee.SendATCommand(addrl, "p0", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 5;
    size = XBee.SendATCommand(addrl, "p1", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 4;
    size = XBee.SendATCommand(addrl, "p2", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    size = XBee.SendATCommand(addrl, "is", buf, 0, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    if((retBuf[3] & 0xe0) != 0xe0) {
      fprintf(stderr, "IS Error %02x%02x\n", retBuf[2], retBuf[3]);
      XBee.Finalize();
      return Error;
    }
    if((retBuf[6] & 0x20) != 0x00) fprintf(stderr, COLOR_RED "SW1(DIO10)=Low, SW2(DIO11)=Hi, SW3(DIO12)=Low -> DIO5=Hi\n" COLOR_DEFAULT);
    if((retBuf[6] & 0x40) != 0x40) fprintf(stderr, COLOR_RED "SW1(DIO10)=Low, SW2(DIO11)=Hi, SW3(DIO12)=Low -> DIO6=Low\n" COLOR_DEFAULT);
    if((retBuf[6] & 0x80) != 0x00) fprintf(stderr, COLOR_RED "SW1(DIO10)=Low, SW2(DIO11)=Hi, SW3(DIO12)=Low -> DIO7=Hi\n" COLOR_DEFAULT);
    if((retBuf[6] & 0xe0) != 0x40) stat = 1;

    //  SW1(DIO10)=Low, SW2(DIO11)=Low, SW3(DIO12)=Hi -> DIO5=Low, DIO6=Low, DIO7=Hi
    buf[0] = 4;
    size = XBee.SendATCommand(addrl, "p0", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 4;
    size = XBee.SendATCommand(addrl, "p1", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 5;
    size = XBee.SendATCommand(addrl, "p2", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    size = XBee.SendATCommand(addrl, "is", buf, 0, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    if((retBuf[3] & 0xe0) != 0xe0) {
      fprintf(stderr, "IS Error %02x%02x\n", retBuf[2], retBuf[3]);
      XBee.Finalize();
      return Error;
    }
    if((retBuf[6] & 0x20) != 0x00) fprintf(stderr, COLOR_RED "SW1(DIO10)=Low, SW2(DIO11)=Low, SW3(DIO12)=Hi -> DIO5=Hi\n" COLOR_DEFAULT);
    if((retBuf[6] & 0x40) != 0x00) fprintf(stderr, COLOR_RED "SW1(DIO10)=Low, SW2(DIO11)=Low, SW3(DIO12)=Hi -> DIO6=Hi\n" COLOR_DEFAULT);
    if((retBuf[6] & 0x80) != 0x80) fprintf(stderr, COLOR_RED "SW1(DIO10)=Low, SW2(DIO11)=Low, SW3(DIO12)=Hi -> DIO7=Low\n" COLOR_DEFAULT);
    if((retBuf[6] & 0xe0) != 0x80) stat = 1;

    buf[0] = 4;
    size = XBee.SendATCommand(addrl, "p2", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    if(!stat) fprintf(stderr, "SW1(DIO10->DIO5), SW2(DIO11->DIO6), SW3(DIO12->DIO7) -> OK\n");
     
    //  AVRReset(DIO0)=Low , AVRReset(DIO0)=Hi -> GetVer = 0x8xxx
    stat = 0;
    buf[0] = 4;
    size = XBee.SendATCommand(addrl, "d0", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    buf[0] = 5;
    size = XBee.SendATCommand(addrl, "d0", buf, 1, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    
    usleep(500 * 1000);
    size = XBee.SendAVRCommand(addrl, CmdGetVer, buf, 0, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    if(!(retBuf[1] & 0x80)) {
      fprintf(stderr, COLOR_RED "AVR-Reset(DIO0) Low->Hi -> AVR boot mode error\n" COLOR_DEFAULT);
      stat = 1;
    }
    int bootVer = (retBuf[1] << 8) | retBuf[2];
   
    //  AVRFWBoot
    size = XBee.SendAVRCommand(addrl, CmdRebootFW, buf, 0, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    
    usleep(500 * 1000);
    size = XBee.SendAVRCommand(addrl, CmdGetVer, buf, 0, retBuf, 256);
    if((size <= 0) || retBuf[0]) {
      fprintf(stderr, "SendAT Error %02x\n", retBuf[0]);
      XBee.Finalize();
      return Error;
    }
    if((retBuf[1] & 0x80)) {
      fprintf(stderr, COLOR_RED "AVRFWBoot -> AVR boot mode error\n" COLOR_DEFAULT);
      stat = 1;
    }
    if(!stat) fprintf(stderr, "AVR-Reset(DIO0) -> OK\n");
    
    fprintf(stderr, "Boot Version %04x\n", bootVer);   
    fprintf(stderr, "FW Version %02x%02x SetID %02x%02x\n", retBuf[1], retBuf[2], retBuf[3], retBuf[4]);   

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
