/*
* (C) Copyright 2018 Jaxo Systems.
*
* Author:  Pierre G. Richard
* Written: 06/09/2018
*
* SGP30 - Sensirion Multi-Pixel Gas Sensor
*/
#ifndef _SGP30_DEVICE_H_
#define _SGP30_DEVICE_H_

#include "Sgp30Features.h"

class Sgp30Device {
public:
   class Interface {
   public:
      virtual void sleep(int us) = 0;
      virtual bool write(void const * buf, int len) = 0;
      virtual bool read(void * buf, int len) = 0;
   };
public:
   Sgp30Device(Interface & interface);
   bool isOperational() const;

   static unsigned char getDefaultI2cAddr() { return 0x58; }
   static char const * getDriverVersion() { return "1.0.0"; }
   unsigned long long getSerialId() const;
   unsigned char getProductType() const;
   unsigned short getProductVersion() const;

   bool initAirQuality();

   bool measureAirQuality(unsigned short * co2eq, unsigned short * tvoc);
   bool measureAirQuality();
   bool getAirQuality(unsigned short * co2eq, unsigned short * tvoc);

   bool measureRawSignals(unsigned short * h2, unsigned short * ethanol);
   bool measureRawSignals();
   bool getRawSignals(unsigned short * h2, unsigned short * ethanol);

   bool measureTest(unsigned short * result);

   bool getBaseline(unsigned short * co2eq, unsigned short * tvoc);
   bool setBaseline(unsigned short co2eq, unsigned short tvoc);

   bool setHumidity(unsigned long humidity);
   bool setRelativeHumidity(double rh, double t);

protected:
   Sgp30Device(Interface * interface);
   bool init();

private:
   Interface & m_interface;
   alignas(unsigned long long) unsigned short m_buffer[10]; // large enough
   unsigned long long m_id;
   Sgp30Features::Command const * m_runningCommand;
   Sgp30Features::Set const * m_featureSet;
   unsigned short m_version;
   bool m_isOk;

   Sgp30Features::Command const * start(
      Sgp30Features::ID id, int argc=0, unsigned short const * argv=0
   );
   bool getValues(Sgp30Features::ID id);
   bool run(Sgp30Features::ID id, int argc=0, unsigned short const * argv=0);
};

/*--------+
| INLINES |
+--------*/
inline bool Sgp30Device::isOperational() const {
   return m_isOk;
}
inline unsigned long long Sgp30Device::getSerialId() const {
   return m_id;
}
inline unsigned char Sgp30Device::getProductType() const {
   return (unsigned char)((m_version & 0xF000) >> 12);
}
inline unsigned short Sgp30Device::getProductVersion() const {
   return m_version & 0x00FF;
}

#endif
/*===========================================================================*/
