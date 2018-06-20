/*
* (C) Copyright 2018 Jaxo Systems.
*
* Author:  Pierre G. Richard
* Written: 06/3/2018
*
* An implementation example to demonstrate the Bmp280Device class.
*
* Compile with:
* g++ Bmp280Device.cpp Bmp280Test.cpp -o Bmp280Test
*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "Bmp280Device.h"

class MyInterface: public Bmp280Device::Interface {
public:
   MyInterface(int i2cAddr) {
      if (((m_fd = ::open("/dev/i2c-1", O_RDWR)) < 0) ||
         (ioctl(m_fd, I2C_SLAVE, i2cAddr) < 0)) {
         printf("Can't open I2C at address %d\n", i2cAddr);
         exit(2);
      }
   }
   virtual ~MyInterface() {
      if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
   }
   bool isSpi() const { return false; } // it's an I2C interface
   void sleep(int ms) { usleep(1000 * ms); }
   bool write(void const * buf, int len) {
      return (m_fd >= 0)? ::write(m_fd, buf, len) == len : false;
   }
   bool readReg(unsigned char reg, void * buf, int len) {
      return (
         ((m_fd >= 0) && (::write(m_fd, &reg, 1)==1)) ?
         (::read(m_fd, buf, len)==len) : false
      );
   }
private:
   int m_fd;
};

int main(int argc, char const * const * argv)
{
   // 0x76 is the I2C address; it can be 0x77, depending on the chip wiring
   MyInterface interface(0x76);
   Bmp280Device device(interface);
   int outDataPeriod;
   double temperature;
   double pressure;

   // do your own settings
   device.setMode(Bmp280Device::VAL_MODE_NORMAL);
   device.setFilter(Bmp280Device::VAL_FILTER_COEFF_2);
   device.setOversampPress(Bmp280Device::VAL_OVERSAMP_16X);
   device.setOversampTmprt(Bmp280Device::VAL_OVERSAMP_4X);
   device.setStandbyTime(Bmp280Device::VAL_STANDBY_1000_MS);
   outDataPeriod = device.getOutDataPeriod() * 1000; // in micro seconds

   // how fast do we do ?
   printf("Output Data Rate: %.3fHz\n", 1e6/outDataPeriod);

   // start a first measure to wake-up the device
   device.readValues(pressure, temperature);

   // read and display 10 samples
   for (int i=0; (i < 10) && device.isOperational(); ++i) {
      usleep(outDataPeriod);
      if (device.readValues(pressure, temperature)) {
         printf("T=%.2f\u00B0C, P=%.2fh\u33A9\n", temperature, pressure/100);
      }else {
         printf ("Error: readValues failure\n");
      }
   }
   return 0;
}
/*===========================================================================*/
