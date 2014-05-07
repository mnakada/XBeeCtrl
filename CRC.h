/*
 CRC.h

 Copyright: Copyright (C) 2013,2014 Mitsuru Nakada All Rights Reserved.
 License: GNU GPL v2
*/

#ifndef __CRC_h__
#define __CRC_h__

class CRC {
public:
  unsigned short CalcCRC(unsigned char* buf, int size, unsigned short init_crc);

  inline unsigned short CalcCRC16(unsigned char data, unsigned short crc) {
    return (crc << 8) ^ CRCTable[((crc >> 8) ^ data) & 0xff];
  }
  
private:
  static const unsigned short CRCTable[];
  
};

#endif /* __CRC_h__ */
