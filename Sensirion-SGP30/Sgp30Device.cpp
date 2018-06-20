/*
* (C) Copyright 2018 Jaxo Systems.
*
* Author:  Pierre G. Richard
* Written: 06/09/2018
*
* SGP30 - Multi-Pixel Gas Sensor
*/

#include <math.h>
#include "Sgp30Device.h"

static unsigned int checksum(unsigned char const * data, int n);

/*---------------------------------------------------Sgp30Device::Sgp30Device-+
|                                                                             |
+----------------------------------------------------------------------------*/
Sgp30Device::Sgp30Device(Interface & interface) : m_interface(interface) {
   init();
}

/*PROTECTED------------------------------------------Sgp30Device::Sgp30Device-+
| For derived class for which the Interface's virtuals are still pure.        |
| init() should be called immediately after the construct finishes.           |
+----------------------------------------------------------------------------*/
Sgp30Device::Sgp30Device(Interface * interface) : m_interface(*interface) {}

/*PROTECTED-------------------------------------------------Sgp30Device::init-+
|                                                                             |
+----------------------------------------------------------------------------*/
bool Sgp30Device::init() {
   if (!run(Sgp30Features::GET_SERIAL_ID)) {
      return false;
   }else {
      m_id = (m_buffer[0] << 16) | (m_buffer[1] << 8) | m_buffer[2];
      if (!run(Sgp30Features::GET_FEATURE_SET_VERSION)) {
         return false;
      }else {
         m_version = m_buffer[0];
         m_featureSet = Sgp30Features::Set::makeSet(m_version);
         m_isOk = true;
         return true;
      }
   }
}

/*------------------------------------------------Sgp30Device::initAirQuality-+
| This resets the SGP30 baselines                                             |
+----------------------------------------------------------------------------*/
bool Sgp30Device::initAirQuality() {
   return run(Sgp30Features::INIT_AIR_QUALITY);
}

/*---------------------------------------------Sgp30Device::measureAirQuality-+
|                                                                             |
+----------------------------------------------------------------------------*/
bool Sgp30Device::measureAirQuality(
   unsigned short * co2eq,
   unsigned short * tvoc
) {
   if (!run(Sgp30Features::MEASURE_AIR_QUALITY)) {
      return false;
   }else {
      *co2eq = m_buffer[0];
      *tvoc = m_buffer[1];
      return true;
   }
}

/*---------------------------------------------Sgp30Device::measureAirQuality-+
|                                                                             |
+----------------------------------------------------------------------------*/
bool Sgp30Device::measureAirQuality() {
   return (start(Sgp30Features::MEASURE_AIR_QUALITY) != 0);
}

/*-------------------------------------------------Sgp30Device::getAirQuality-+
|                                                                             |
+----------------------------------------------------------------------------*/
bool Sgp30Device::getAirQuality(
   unsigned short * co2eq,
   unsigned short * tvoc
) {
   if (!getValues(Sgp30Features::MEASURE_AIR_QUALITY)) {
      return false;
   }else {
      *co2eq = m_buffer[0];
      *tvoc = m_buffer[1];
      return true;
   }
}

/*---------------------------------------------Sgp30Device::measureRawSignals-+
|                                                                             |
+----------------------------------------------------------------------------*/
bool Sgp30Device::measureRawSignals(
   unsigned short * h2,
   unsigned short * ethanol
) {
   if (!run(Sgp30Features::MEASURE_RAW_SIGNALS)) {
      return false;
   }else {
      *h2 = m_buffer[0];
      *ethanol = m_buffer[1];
      return true;
   }
}

/*---------------------------------------------Sgp30Device::measureRawSignals-+
|                                                                             |
+----------------------------------------------------------------------------*/
bool Sgp30Device::measureRawSignals() {
   return (start(Sgp30Features::MEASURE_RAW_SIGNALS) != 0);
}

/*-------------------------------------------------Sgp30Device::getRawSignals-+
|                                                                             |
+----------------------------------------------------------------------------*/
bool Sgp30Device::getRawSignals(
   unsigned short * h2,
   unsigned short * ethanol
) {
   if (!getValues(Sgp30Features::MEASURE_RAW_SIGNALS)) {
      return false;
   }else {
      *h2 = m_buffer[0];
      *ethanol = m_buffer[1];
      return true;
   }
}
/*---------------------------------------------------Sgp30Device::measureTest-+
|                                                                             |
+----------------------------------------------------------------------------*/
bool Sgp30Device::measureTest(unsigned short * result) {
   if (!run(Sgp30Features::MEASURE_TEST)) {
      return false;
   }else {
      *result = m_buffer[0];
      return (*result == 0xd400);
   }
}

/*---------------------------------------------------Sgp30Device::getBaseline-+
| See Sensirion Datasheet, v0.9, p8: Airquality Signals                       |
+----------------------------------------------------------------------------*/
bool Sgp30Device::getBaseline(unsigned short * co2eq, unsigned short * tvoc) {
   if (!run(Sgp30Features::GET_BASELINE)) {
      return false;
   }else {
      *co2eq = m_buffer[0];
      *tvoc = m_buffer[1];
      return true;
   }
}

/*---------------------------------------------------Sgp30Device::setBaseline-+
| Order: (tvoc, co2eq) - See datasheet, v0.9, p8, Air Quality, 2nd paragraph  |
+----------------------------------------------------------------------------*/
bool Sgp30Device::setBaseline(unsigned short co2eq, unsigned short tvoc) {
   unsigned short const args[] = { tvoc, co2eq };
   return run(Sgp30Features::SET_BASELINE, 2, args);
}

/*---------------------------------------------------Sgp30Device::setHumidity-+
| The humidity value is expressed in mg/m**3, and  0 < value < 256000.        |
| If zero, humidity compensation is disabled.                                 |
+----------------------------------------------------------------------------*/
bool Sgp30Device::setHumidity(unsigned long humidity) {
   if (humidity < 256000) {
      humidity <<= 8;
      humidity = (humidity/1000) | ((humidity % 1000)/1000);
      return run(Sgp30Features::SET_HUMIDITY, 1, (unsigned short *)&humidity);
   }else {
      return false;
   }
}

/*-------------------------------------------Sgp30Device::setRelativeHumidity-+
| rh: percentage of relative humidity, t: temperature in Celsius degrees      |
+----------------------------------------------------------------------------*/
bool Sgp30Device::setRelativeHumidity(double rh, double t) {
   return setHumidity(
      lround(
         216700.0*(((rh/100)*6.112*exp((17.62*t)/(243.12+t)))/(273.15+t))
      )
   );
}

/*---------------------------------------------------------Sgp30Device::start-+
| Start a command                                                             |
+----------------------------------------------------------------------------*/
Sgp30Features::Command const * Sgp30Device::start(
   Sgp30Features::ID id,
   int argc,
   unsigned short const * argv
) {
   Sgp30Features::Command const * command = m_featureSet->getCommand(id);
   if (command) {
      unsigned char *cp = (unsigned char *)(m_buffer + 1);
      while (argc--) {
         *cp++ = *argv >> 8;
         *cp++ = *(argv++);
         *cp = checksum(cp-2, 2);
         ++cp;
      }
      m_buffer[0] = command->m_code; // already in network byte order
      if (m_interface.write(m_buffer, cp-(unsigned char *)m_buffer)) {
         m_runningCommand = command;
         return command;
      }
   }
   return 0;
}

/*-----------------------------------------------------Sgp30Device::getValues-+
| Get the values of the last issued command                                   |
+----------------------------------------------------------------------------*/
bool Sgp30Device::getValues(Sgp30Features::ID id) {
   bool isOk = false;
   if (m_runningCommand && (m_runningCommand->m_id == id)) {
      Sgp30Features::Command const * command = m_runningCommand;
      unsigned short size = command->m_valuesCount * 3;
      if (!size) {
         m_runningCommand = 0;
         isOk = true;
      }else {
         unsigned char * data = (unsigned char *)m_buffer;
         if (m_interface.read(data, size)) {
            m_runningCommand = 0;
            for (int i=0, j=0; ; i += 3, j += 2) {
               if (i == size) {
                  command->convertValues(m_buffer);
                  isOk = true;
                  break;
               }else if (data[i+2] != checksum(data+i, 2)) {
                  break;
               }else {
                  ((unsigned char *)m_buffer)[j] = data[i];
                  ((unsigned char *)m_buffer)[j+1] = data[i+1];
               }
            }
         }
      }
   }
   return isOk;
}

/*-----------------------------------------------------------Sgp30Device::run-+
| Run a command                                                               |
+----------------------------------------------------------------------------*/
bool Sgp30Device::run(
   Sgp30Features::ID id,
   int argc, unsigned short const * argv
) {
   Sgp30Features::Command const * command = start(id, argc, argv);
   if (!command) {
      return false;
   }else {
      m_interface.sleep(command->m_durationMicros + 5);
      return getValues(id);
   }
}

/*STATIC-------------------------------------------------------------checksum-+
| Compute the checksum of 'n' bytes in 'data' (from 0xFF crc)                 |
| The polynomial is: P(x)=x^8+x^5+x^4+1, hence 100110001, aka 0x131           |
+----------------------------------------------------------------------------*/
unsigned int checksum(unsigned char const * data, int n) {
   unsigned short crc = 0xFF;
   for (int i=0; i < n; ++i) {
      crc ^= data[i];
      for (int j=8; j > 0; --j) {
         crc =  (crc & 0x80)? (crc<<1)^0x131 : (crc << 1);
      }
   }
   return crc;
}

/*===========================================================================*/
