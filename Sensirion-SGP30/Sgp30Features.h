/*
* (C) Copyright 2018 Jaxo Systems.
*
* Author:  Pierre G. Richard
* Written: 06/09/2018
*
* SGP30 - Sensirion Multi-Pixel Gas Sensor - Version dependent features
*/
#ifndef _SGP30_FEATURE_H_
#define _SGP30_FEATURE_H_

class Sgp30Features {
public:
   enum ID {
      INIT_AIR_QUALITY,
      MEASURE_AIR_QUALITY,
      GET_BASELINE,
      SET_BASELINE,
      SET_HUMIDITY,
      MEASURE_TEST,
      GET_FEATURE_SET_VERSION,
      MEASURE_RAW_SIGNALS,
      GET_SERIAL_ID
   };
   class Command;
   class Value {
   friend class Command;
   public:
      Value(char const * name, unsigned short (*convert)(unsigned short));
   private:
      char const * m_name;
      unsigned short (* m_convert)(unsigned short);
   };
   class Command {
   public:
      Command(
         char const * name,
         ID id,
         unsigned short const code,
         unsigned long durationMicros,
         Value const ** values,
         unsigned short valuesCount
      );
      char const * const m_name;
      unsigned char const m_id;
      unsigned short const m_code;
      unsigned long const m_durationMicros;
      Value const * const * const m_values;
      unsigned short const m_valuesCount;

      void convertValues(unsigned short * buffer) const;
   };
   class Set {
   public:
      static Set const * makeSet(unsigned short version);
      Command const * getCommand(ID id) const;
   private:
      static Set const set9;
      static Set const set32;
      static Set const setNull;

      Command const ** m_commands;
      unsigned short m_commandsCount;
      unsigned short const * m_versions;
      unsigned short m_versionsCount;

      Set(
         Command const ** commands,
         unsigned short commandsCount,
         unsigned short const * versions,
         unsigned short versionsCount
      );
   };
};
#endif
