/*
* (C) Copyright 2018 Jaxo Systems.
*
* Author:  Pierre G. Richard
* Written: 06/09/2018
*
* SGP30 - Sensirion Multi-Pixel Gas Sensor - Version dependent features
*/
#include "Sgp30Features.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ // Network Byte Order
#define NBO_16(s) (((unsigned short)(s) << 8)|(0xff & ((unsigned short)(s)) >> 8))
#else
#define NBO_16(s) (s)
#endif

static unsigned short networkByteOrder16(unsigned short v) { return NBO_16(v); }

static Sgp30Features::Value const CO2EQ("co2eq", &networkByteOrder16);
static Sgp30Features::Value const TVOC("tvoc", &networkByteOrder16);
static Sgp30Features::Value const H2("h2", &networkByteOrder16);
static Sgp30Features::Value const ETHANOL("ethanol", &networkByteOrder16);
static Sgp30Features::Value const ID_1("id1", &networkByteOrder16);
static Sgp30Features::Value const ID_2("id2", &networkByteOrder16);
static Sgp30Features::Value const ID_3("id3", &networkByteOrder16);
static Sgp30Features::Value const FEATURE_SET_VERSION("version", &networkByteOrder16);
static Sgp30Features::Value const MEASURE_TEST("test", &networkByteOrder16);

static const Sgp30Features::Value * MEASURE_AIR_QUALITY_VALUES[] = { &CO2EQ, &TVOC };
static const Sgp30Features::Value * MEASURE_RAW_SIGNALS_VALUES[] = { &H2, &ETHANOL };
static const Sgp30Features::Value * GET_SERIAL_ID_VALUES[] = { &ID_1, &ID_2, &ID_3 };
static const Sgp30Features::Value * FEATURE_SET_VERSION_VALUES[] = { &FEATURE_SET_VERSION };
static const Sgp30Features::Value * GET_BASELINE_VALUES[] = { &CO2EQ, &TVOC };
static const Sgp30Features::Value * MEASURE_TEST_VALUES[] = { &MEASURE_TEST };

static const Sgp30Features::Command COMMAND_INIT_AIR_QUALITY(
   "iaq_init", Sgp30Features::INIT_AIR_QUALITY, 0x2003, 10000,
   (Sgp30Features::Value const **)0, 0
);

static const Sgp30Features::Command COMMAND_MEASURE_AIR_QUALITY(
   "iaq_measure", Sgp30Features::MEASURE_AIR_QUALITY, 0x2008, 50000,
   MEASURE_AIR_QUALITY_VALUES, ARRAY_SIZE(MEASURE_AIR_QUALITY_VALUES)
);

static const Sgp30Features::Command COMMAND_GET_BASELINE(
   "iaq_get_baseline", Sgp30Features::GET_BASELINE, 0x2015, 10000,
   GET_BASELINE_VALUES, ARRAY_SIZE(GET_BASELINE_VALUES)
);

static const Sgp30Features::Command COMMAND_SET_BASELINE(
   "iaq_set_baseline", Sgp30Features::SET_BASELINE, 0x201e, 10000,
   (Sgp30Features::Value const **)0, 0
);

static Sgp30Features::Command const COMMAND_MEASURE_RAW_SIGNALS(
   "measure_signals", Sgp30Features::MEASURE_RAW_SIGNALS, 0x2050, 200000,
   MEASURE_RAW_SIGNALS_VALUES, ARRAY_SIZE(MEASURE_RAW_SIGNALS_VALUES)
);

static Sgp30Features::Command const COMMAND_SET_HUMIDITY(
   "set_absolute_humidity", Sgp30Features::SET_HUMIDITY, 0x2061, 10000,
   (Sgp30Features::Value const **)0, 0
);

static Sgp30Features::Command const COMMAND_GET_SERIAL_ID(
   "get_serial_id", Sgp30Features::GET_SERIAL_ID, 0x3682, 500,
   GET_SERIAL_ID_VALUES, ARRAY_SIZE(GET_SERIAL_ID_VALUES)
);

static Sgp30Features::Command const COMMAND_GET_FEATURE_SET_VERSION(
   "get_feature_set_version", Sgp30Features::GET_FEATURE_SET_VERSION, 0x202f, 1000,
   FEATURE_SET_VERSION_VALUES, ARRAY_SIZE(FEATURE_SET_VERSION_VALUES)
);

static Sgp30Features::Command const COMMAND_MEASURE_TEST(
   "measure_test", Sgp30Features::MEASURE_TEST, 0x2032, 220000,
   MEASURE_TEST_VALUES, ARRAY_SIZE(MEASURE_TEST_VALUES)
);

static Sgp30Features::Command const * commandsV9[] = {
   &COMMAND_INIT_AIR_QUALITY,
   &COMMAND_MEASURE_AIR_QUALITY,
   &COMMAND_GET_BASELINE,
   &COMMAND_SET_BASELINE,
   &COMMAND_MEASURE_RAW_SIGNALS
};

static Sgp30Features::Command const * commandsV32[] = {
   &COMMAND_INIT_AIR_QUALITY,
   &COMMAND_MEASURE_AIR_QUALITY,
   &COMMAND_GET_BASELINE,
   &COMMAND_SET_BASELINE,
   &COMMAND_MEASURE_RAW_SIGNALS,
   &COMMAND_SET_HUMIDITY
};

static unsigned short const m_versions_fs9[] = { 9 };
static unsigned short const m_versions_fs32[] = { 0x20 };

/*------------------------------------------------Sgp30Features::Value::Value-+
|                                                                             |
+----------------------------------------------------------------------------*/
Sgp30Features::Value::Value(
   char const * name,
   unsigned short (*convert)(unsigned short)
) :
   m_name(name), m_convert(convert) {
}

/*--------------------------------------------Sgp30Features::Command::Command-+
|                                                                             |
+----------------------------------------------------------------------------*/
Sgp30Features::Command::Command(
   char const * name,
   ID id,
   unsigned short code,
   unsigned long durationMicros,
   Value const ** values,
   unsigned short valuesCount
) :
   m_name(name),
   m_id(id),
   m_code((unsigned short)NBO_16(code)),
   m_durationMicros(durationMicros),
   m_values(values),
   m_valuesCount(valuesCount)
{}

/*--------------------------------------Sgp30Features::Command::convertValues-+
|                                                                             |
+----------------------------------------------------------------------------*/
void Sgp30Features::Command::convertValues(unsigned short * buffer) const {
   for (unsigned i=0; i < m_valuesCount; ++i) {
      buffer[i] = m_values[i]->m_convert(buffer[i]);
   }
}

/*----------------------------------------------------Sgp30Features::Set::Set-+
|                                                                             |
+----------------------------------------------------------------------------*/
Sgp30Features::Set::Set(
   Command const ** commands,
   unsigned short commandsCount,
   unsigned short const * versions,
   unsigned short versionsCount
) :
   m_commands(commands),
   m_commandsCount(commandsCount),
   m_versions(versions),
   m_versionsCount(versionsCount)
{}

Sgp30Features::Set const Sgp30Features::Set::set9(
   commandsV9,
   ARRAY_SIZE(commandsV9),
   (unsigned short *)m_versions_fs9,
   ARRAY_SIZE(m_versions_fs9)
);

Sgp30Features::Set const Sgp30Features::Set::set32(
   commandsV32,
   ARRAY_SIZE(commandsV32),
   (unsigned short *)m_versions_fs32,
   ARRAY_SIZE(m_versions_fs32)
);

Sgp30Features::Set const Sgp30Features::Set::setNull( 0, 0, 0, 0);

/*------------------------------------------------Sgp30Features::Set::makeSet-+
| List featureSets from the newest to the oldest to use the most recent       |
| compatible featureSet config, as multiple ones may apply for the same       |
| major version.                                                              |
+----------------------------------------------------------------------------*/
Sgp30Features::Set const * Sgp30Features::Set::makeSet(unsigned short version) {
   Set const * sets[] = { &set32, &set9 };
   for (unsigned int i=0; i < ARRAY_SIZE(sets); ++i) {
      Set const * set = sets[i];
      for (int j=0; j < set->m_versionsCount; ++j) {
         unsigned short const  fsVersion = set->m_versions[j];
         if (
            ((fsVersion & 0xF000) == (version & 0xF000)) &&
            ((version & 0x0100) == 0) &&
            ((fsVersion & 0x00E0) == (version & 0x00E0)) &&
            ((version & 0x001F) >= (fsVersion & 0x001F))
         ) {
            return set;
         }
      }
   }
   return &setNull;
}

/*---------------------------------------------Sgp30Features::Set::getCommand-+
|                                                                             |
+----------------------------------------------------------------------------*/
Sgp30Features::Command const * Sgp30Features::Set::getCommand(ID id) const {
   switch (id) {
   case GET_SERIAL_ID:
      return &COMMAND_GET_SERIAL_ID;
   case GET_FEATURE_SET_VERSION:
      return &COMMAND_GET_FEATURE_SET_VERSION;
   case MEASURE_TEST:
      return &COMMAND_MEASURE_TEST;
   default: // depends on the chip version
      for (int i=0; i < m_commandsCount; ++i) {
         Command const * command = m_commands[i];
         if (command->m_id == id) return command;
      }
   }
   return 0;  // not found
}
/*===========================================================================*/
