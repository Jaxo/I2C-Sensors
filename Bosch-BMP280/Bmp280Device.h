/*
* (C) Copyright 2018 Jaxo Systems.
*
* Author:  Pierre G. Richard
* Written: 05/31/2018
*
* BMP280 - Air Pressure and Temperature Sensor from Bosh Sensortec
*/
#ifndef _BMP280DEVICE_H_
#define _BMP280DEVICE_H_

class Bmp280Device {
public:
   class Interface {               // pure abstract class
   public:
      virtual bool isSpi() const = 0;    // true: SPI, false: I2C
      virtual void sleep(int ms) = 0;
      virtual bool write(void const * buf, int len) = 0;
      virtual bool readReg(unsigned char reg, void * buf, int len) = 0;
   };
   enum VAL_MODE {
      VAL_MODE_SLEEP = 0x00,       // (of no real use at the API level)
      VAL_MODE_FORCED = 0x01,      // on demand
      VAL_MODE_NORMAL = 0x03       // periodic
   };
   enum VAL_FILTER {               // IIR to smooth disturbances
      VAL_FILTER_OFF = 0x00,
      VAL_FILTER_COEFF_2 = 0x01,
      VAL_FILTER_COEFF_4 = 0x02,
      VAL_FILTER_COEFF_8 = 0x03,
      VAL_FILTER_COEFF_16 = 0x04
   };
   enum VAL_OVERSAMP {
      VAL_OVERSAMP_NONE = 0x00,    // no oversampling
      VAL_OVERSAMP_1X = 0x01,      // X 1
      VAL_OVERSAMP_2X = 0x02,      // X 2
      VAL_OVERSAMP_4X = 0x03,      // X 4
      VAL_OVERSAMP_8X = 0x04,      // X 8
      VAL_OVERSAMP_16X = 0x05      // X 16
   };
   enum VAL_STANDBY {
      VAL_STANDBY_0_5_MS = 0x00,   // 0.5 ms
      VAL_STANDBY_62_5_MS = 0x01,  // 62.5 ms
      VAL_STANDBY_125_MS = 0x02,   // 125 ms
      VAL_STANDBY_250_MS = 0x03,   // 250 ms
      VAL_STANDBY_500_MS = 0x04,   // 500 ms
      VAL_STANDBY_1000_MS = 0x05,  // 1000 ms
      VAL_STANDBY_2000_MS = 0x06,  // 2000 ms
      VAL_STANDBY_4000_MS = 0x07   // 4000 ms
   };
   enum VAL_SPI_WIRING {
      VAL_SPI_WIRING_4 = 0,        // 4-wire
      VAL_SPI_WIRING_3 = 1         // 3-wire
   };

   Bmp280Device(Interface & interface);
   bool isOperational();

   void setMode(VAL_MODE val);
   void setFilter(VAL_FILTER val);
   void setOversampPress(VAL_OVERSAMP val);
   void setOversampTmprt(VAL_OVERSAMP val);
   void setStandbyTime(VAL_STANDBY val);
   void setSpiWiring(VAL_SPI_WIRING val);
   int getOutDataPeriod();         // in milliseconds

   bool readValues(double & pressure, double & temperature);

private:
   enum REG {
      REG_CALIB = 0x88,
      REG_CHIP_ID = 0xD0,
      REG_SOFT_RESET = 0xE0,
      REG_STATUS = 0xF3,
      REG_CTRL_MEAS = 0xF4,
      REG_CONFIG = 0xF5,
      REG_VALUES = 0xF7
   };
   enum CHIP_ID {
      CHIP_ID_1 = 0x56,
      CHIP_ID_2 = 0x57,
      CHIP_ID_3 = 0x58
   };
   enum I2C_ADDR {
      I2C_ADDR_1 = 0x76,
      I2C_ADDR_2 = 0x77
   };
   enum BITS {
      BITS_FILTER_POS = 2,
      BITS_FILTER_MASK = 0x1C,
      BITS_OVERSAMP_TMPRT_POS = 5,
      BITS_OVERSAMP_TMPRT_MASK = 0xE0,
      BITS_OVERSAMP_PRESS_POS = 2,
      BITS_OVERSAMP_PRESS_MASK = 0x1C,
      BITS_STANDBY_TIME_POS = 5,
      BITS_STANDBY_TIME_MASK = 0xE0,
      BITS_SPI_WIRING_POS = 0,
      BITS_SPI_WIRING_MASK = 0x01,
      BITS_MODE_POS = 0,
      BITS_MODE_MASK = 0x03
   };

   class Calibration {
   public:
      enum { BUFLEN = 24 };
      void populate(unsigned char const * buf);
      void compensate(double & press, double & tmprt) const;
   private:
      double t1, t2, t3;
      double p1, p2, p3, p4, p5, p6, p7, p8, p9;
   } m_calibration;

   struct {                        // CTRL_MEAS + CONFIG registers
      unsigned char oversampTmprt; // see VAL_OVERSAMP
      unsigned char oversampPress; // see VAL_OVERSAMP
      unsigned char standbyTime;   // see VAL_STANDBY
      unsigned char filter;        // see VAL_FILTER
      unsigned char mode;          // see VAL_MODE
      unsigned char spiWiring;     // see VAL_SPI_WIRING
   } m_options;

   Interface & m_interface;
   bool m_isOk;
   bool m_isOptionsSet;
   unsigned char const m_orMaskRead;
   unsigned char const m_andMaskWrite;
   int m_outDataPeriod;            // in milliseconds

   bool softReset();
   bool setOptions();
};

/*--------+
| INLINES |
+--------*/
inline bool Bmp280Device::isOperational() {
   return m_isOk;
}
inline void Bmp280Device::setMode(VAL_MODE val) {
   m_isOptionsSet = false;
   m_options.mode = val;
}
inline void Bmp280Device::setFilter(VAL_FILTER val) {
   m_isOptionsSet = false;
   m_options.filter = val;
}
inline void Bmp280Device::setOversampPress(VAL_OVERSAMP val) {
   m_isOptionsSet = false;
   m_options.oversampPress = val;
}
inline void Bmp280Device::setOversampTmprt(VAL_OVERSAMP val) {
   m_isOptionsSet = false;
   m_options.oversampTmprt = val;
}
inline void Bmp280Device::setStandbyTime(VAL_STANDBY val) {
   m_isOptionsSet = false;
   m_options.standbyTime = val;
}
inline void Bmp280Device::setSpiWiring(VAL_SPI_WIRING val) {
   m_isOptionsSet = false;
   m_options.spiWiring = val;
}

#endif
/*===========================================================================*/
