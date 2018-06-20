/*
* (C) Copyright 2018 Jaxo Systems.
*
* Author:  Pierre G. Richard
* Written: 05/31/2018
*
* BMP280 - Air Pressure and Temperature Sensor from Bosh Sensortec
*/
#include <stdio.h>
#include "Bmp280Device.h"

#define GET_VALUE(name, in) \
   ((in & name##_MASK) >> name##_POS)
#define SET_VALUE(name, in, to) \
   in = ((in & ~name##_MASK) | ((to << name##_POS) & name##_MASK))

/*-------------------------------------------------Bmp280Device::Bmp280Device-+
|                                                                             |
+----------------------------------------------------------------------------*/
Bmp280Device::Bmp280Device(Interface & interface) :
m_interface(interface),
m_isOk(false),
m_isOptionsSet(false),
m_orMaskRead(interface.isSpi()? 0x80 : 0x00),
m_andMaskWrite(interface.isSpi()? 0x7F : 0xFF)
{
   for (int tries=5; ; m_interface.sleep(10)) {
      unsigned char id;
      if (tries-- == 0) {
         printf("No BMP280 device found\n");
         return;
      }
      if (
         m_interface.readReg(REG_CHIP_ID | m_orMaskRead, &id, 1) &&
         ((id==CHIP_ID_1) || (id==CHIP_ID_2) || (id==CHIP_ID_3))
      ) {
         printf("BMP280 id:0x%02x found after %d try(ies)\n", id, 5-tries);
         break;
      }
   }
   if (!softReset()) return;

   // fill-up the calibration struct
   {
      unsigned char buf[Calibration::BUFLEN];  // auto-increment read

      if (!m_interface.readReg(REG_CALIB | m_orMaskRead, buf, sizeof buf)) {
         printf("Can't get calibration values\n");
         return;
      }
      m_calibration.populate(buf);
   }

   // fill up the options struct
   {
      unsigned char buf[2] = {};

      if (!m_interface.readReg(REG_CTRL_MEAS | m_orMaskRead, buf, sizeof buf)) {
         printf("Can't get current options");
         return;
      }
      m_options.oversampTmprt = GET_VALUE(BITS_OVERSAMP_TMPRT, buf[0]);
      m_options.oversampPress = GET_VALUE(BITS_OVERSAMP_PRESS, buf[0]);
      m_options.mode = GET_VALUE(BITS_MODE, buf[0]);
      m_options.standbyTime = GET_VALUE(BITS_STANDBY_TIME, buf[1]);
      m_options.filter = GET_VALUE(BITS_FILTER, buf[1]);
      m_options.spiWiring = GET_VALUE(BITS_SPI_WIRING, buf[1]);
   }
   m_isOk = true; // although options are not yet set on the device (lazy set);
}

/*---------------------------------------------------Bmp280Device::setOptions-+
|                                                                             |
+----------------------------------------------------------------------------*/
bool Bmp280Device::setOptions() {
   unsigned char buf1[2] = {};

   m_isOptionsSet = false;
   if (
      softReset() &&
      m_interface.readReg(REG_CTRL_MEAS | m_orMaskRead, buf1, sizeof buf1)
   ) {
      unsigned char buf2[4] = {};

      SET_VALUE(BITS_OVERSAMP_TMPRT, buf1[0], m_options.oversampTmprt);
      SET_VALUE(BITS_OVERSAMP_PRESS, buf1[0], m_options.oversampPress);
      // power mode (currently VAL_MODE_SLEEP) is set at the end
      SET_VALUE(BITS_STANDBY_TIME, buf1[1], m_options.standbyTime);
      SET_VALUE(BITS_FILTER, buf1[1], m_options.filter);
      SET_VALUE(BITS_SPI_WIRING, buf1[1], m_options.spiWiring);

      buf2[0] = REG_CTRL_MEAS & m_andMaskWrite;
      buf2[1] = buf1[0];
      buf2[2] = REG_CONFIG & m_andMaskWrite;
      buf2[3] = buf1[1];
      if (m_interface.write(buf2, sizeof buf2)) { // write 2 pairs
         // compute the minimal time required for a measure
         /*
         | For temperature and pressure,
         | - 2000 us is the duration of the 1 x oversampling
         | - oversampling factor is: (1 << options.oversampXxxxx) >> 1
         | - 1000 us to start the measuring process
         | - add 500 us to start the pressure oversampling (hence, if not 0)
         | - add the standby time
         | - divide by 1000 (us -> ms) and round
         */
         m_outDataPeriod = (
            1000 + 500 + ( // 500 added for rounding it up
               2000 * (
                  ((1<<m_options.oversampPress) >> 1) +
                  ((1<<m_options.oversampTmprt) >> 1)
               )
            ) + (
               m_options.oversampPress? 500 : 0
            ) + (
               m_options.standbyTime? (62500<<(m_options.standbyTime-1)) : 500
            )
         ) / 1000;

         // set the power mode (if not VAL_MODE_SLEEP) in a separate write
         if (m_options.mode != VAL_MODE_SLEEP) { // softReset sets MODE_SLEEP
            SET_VALUE(BITS_MODE, buf2[1], m_options.mode);
            m_isOptionsSet = m_interface.write(buf2, 2); // single pair
         }else {
            m_isOptionsSet = true;
         }
      }
   }
   if (!m_isOptionsSet) printf("Can't set options");
   return m_isOptionsSet;
}

/*---------------------------------------------Bmp280Device::getOutDataPeriod-+
|                                                                             |
+----------------------------------------------------------------------------*/
int Bmp280Device::getOutDataPeriod() {
   if (m_isOk && (m_isOptionsSet || setOptions())) {
      return m_outDataPeriod;
   }else {
      printf("Can't get data period\n");
      return -1;
   }
}

/*----------------------------------------------------Bmp280Device::softReset-+
|                                                                             |
+----------------------------------------------------------------------------*/
bool Bmp280Device::softReset() {
   unsigned char buf[2] = {
      (unsigned char)(REG_SOFT_RESET & m_andMaskWrite), 0xB6
   };
   if (!m_interface.write(buf, sizeof buf)) {
      printf("Soft reset failure\n");
      return false;
   }else {
      m_interface.sleep(2);
      return true;
   }
}

/*---------------------------------------------------Bmp280Device::readValues-+
|                                                                             |
+----------------------------------------------------------------------------*/
bool Bmp280Device::readValues(double & pressure, double & temperature)
{
   unsigned char buf[6];

   if (m_options.mode == VAL_MODE_SLEEP) m_options.mode = VAL_MODE_FORCED;
   if (
      !m_isOk || (
        (!m_isOptionsSet || (m_options.mode != VAL_MODE_NORMAL)) &&
        !setOptions()
      ) || // auto-increment read
      !m_interface.readReg(REG_VALUES | m_orMaskRead, buf, sizeof buf)
   ) {
      return false;
   }else {
      pressure = (buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4);
      temperature = (buf[3] << 12) | (buf[4] << 4) | (buf[5] >> 4);
      m_calibration.compensate(pressure, temperature);
      return true;
   }
}

/*----------------------------------------Bmp280Device::Calibration::populate-+
|                                                                             |
+----------------------------------------------------------------------------*/
void Bmp280Device::Calibration::populate(unsigned char const * buf) {
   t1 = (double)((unsigned short)(buf[0]|(buf[1+0]<<8))) / 0x400;
   t2 = ((short)(buf[2]|(buf[3]<<8)));
   t3 = (double)((short)(buf[4]|(buf[5]<<8))) / 0x40;
   p1 = ((unsigned short)(buf[6]|(buf[7]<<8)));
   p2 = ((double)((short)(buf[8]|(buf[9]<<8)))) * (p1 / 0x400000000L);
   p3 = ((double)((short)(buf[10]|(buf[11]<<8)))) * (p1 / 0x20000000000000L);
   p4 = ((short)(buf[12]|(buf[13]<<8))) << 4;
   p5 = ((double)((short)(buf[14]|(buf[15]<<8)))) / 0x2000;
   p6 = ((double)((short)(buf[16]|(buf[17]<<8)))) /  0x40000;
   p7 = ((double)((short)(buf[18]|(buf[19]<<8)))) / 0x10;
   p8 = ((double)((short)(buf[20]|(buf[21]<<8)))) / 0x80000;
   p9 = ((double)((short)(buf[22]|(buf[23]<<8)))) / 0x800000000L;
}

/*--------------------------------------Bmp280Device::Calibration::compensate-+
|                                                                             |
+----------------------------------------------------------------------------*/
void Bmp280Device::Calibration::compensate(double & press, double & tmprt) const
{
   double d1;
   double d2;

   // compensate temperature
   d1 = (tmprt/0x4000) - t1;
   d1 = (t3*d1*d1) + (t2*d1);
   tmprt = d1 / 5120;

   // compensate pressure
   d1 = (d1/2) - 64000;
   d2 = (p6*d1*d1) + (p5*d1) + p4;
   d1 = (p3*d1*d1) + (p2*d1) + p1;
   if (d1 != 0) {  // to avoid zero-divide
      press = ((p9*d1*d1) + (p8*d1) + p7 + ((((1048576-press)-d2)*6250)/d1));
   }
}
/*===========================================================================*/
