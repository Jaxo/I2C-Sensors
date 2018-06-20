# Bosch BMP280 Driver

This package contains a C++ interface to the Bosh-Sensortec
BMP280 Air Pressure and Temperature Sensor.

Two files: `Bmp280Device.cpp` and `Bmp280Device.h` compose the API *per se*.

A 3rd file: `BmpTest.cpp` is an example of I2C implementation of the API.

- Edit and change it in order to match the I2C address of your BMP280,
to use SPI, etc...
- Compile with: `g++ Bmp280Device.cpp Bmp280Test.cpp -o Bmp280Test`
- Run it: `Bmp280Test`

I've done this work on my spare time.
Feel free to use and modify it at your will (and at your own risks!)

