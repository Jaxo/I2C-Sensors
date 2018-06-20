# Sensirion SGP30 CO2eq and TVOC Gas Sensor

## History

I'm enjoying Sensirion's hardware, but I must say
I was a bit disappointed at reading the Sensirion's document
*SGP Driver Integration Guide* -- the software implementation.

The more I was reading, the more nervous I got.
IMPLEMENT, IMPLEMENT, sgp_measure_iaq_blocking_read (what's that?).
Going forth and back to the SGP30 datasheet, trying to find a logical link.
I was expecting to spend an hour at most to have the little thing
ready to run. Alas...

I went to Github to download the code and it was even worst.
A hand full of directories, with (again) esoteric names. Sigh.
I spent a couple of hours to disentangle the maze -- I didn't wanting
to have to learn a wierd (not standard) implementation scheme.
Also, I found discrepancies in the methods wrt the use of
the feature's mechanism, or not.

It's useless to say more. Think positive. I decided to rewrite the API
from scratch, using the SGP30 datasheet as the primary official document.

My objective was:
- to reduce the package to only 2 files: Sgp30Device and Sgp30Test,
less than 50k.
Later, I added Sgp30Features separately, since FeatureSet is mentionned in
the SGP30 datasheet and that it might probably ease for adding new features.
- to have the end-user be able to run it 5 minutes after he/she downladed
the code
- running on Linux, C++, open-source under a permissive MIT License

## Instructions For Use

- download the package
- enter the command:
`g++ -Wall -std=c++0x -pthread -o Sgp30Test Sgp30Test.cpp Sgp30Device.cpp Sgp30Features.cpp`
- run it: `Sgp30Test`

You're done.  You can stop reading from here if, like me, you are impatient
to put at work this nice little SGP30 device.

## More details on the internals (going further for your own needs)

"sgp30Device" is the API, "sgp30Test" is a full, complete example of
a working implementation -- sorry if it takes more than 400 lines.

Below is what "Sgp30Test" does.

As most gas sensors do, the SGP30 needs to be calibrated,
but Sensirion engineering are smart enough to integrate the calibration
within the sensor itself, avoiding any external intervention.
Programmatically speaking, the code needs, at a period of 1 second,
to run a command in order to refine the calibration factors, aka "baseline".

Implementing this loop in the main thread is not practical: the main has
more to do, probably taking care of the other sensors, of asynchronous
environment request (actuators) and other tasks. The main doesn't want
to be disturbed by a 1 second tick-tick. Therefore, the SGP30 calibration has
to enter in an auxilliary "helper", that is, an independent thread
with (possibly) a lower priority (1 second, or 1.00000001 second, who cares?)
This is what the "Tickler" does: invisibly asking the SGP30 to measure
the atmosphere so to refine the "baseline" and reporting each hour
the main thread about it.

Then, the Sgp30Test main is just waiting at the door for asynchroneous
requests... Calibration is none of its business.

I2C I/O's are fully implemented in Sgp30Test. Seen from Sgp30Device,
it is a pure abstract interface realizing the read and write functions.
The Sgp30Device has no notion of I2C: it asks the interface to read or
write characters whatever the data comes from or goes to.

The rationale behind this approach is that an I2C bus is not dedicated
to rule a single device. Its management has to be at a higher level: a
Sgp30Device has no rights to open or close an I2C bus as it pleases.

For experts: if you centrally manage the I2C bus resource among several
devices, you probably have already implemented a class I2cBus that
federates the sharing. C++ can give you headaches if (logically) you derive
the Sgp30Interface class from your I2cBus class.
The "pure virtual call" will hit you. This is why SgpDevice has a protected
method allowing you to create the device, even before the virtual methods
of Sgp30Device::Interface have been resolved. It is at the cost of a call
to the (protected) Sgp30Device::init() occurring just after the construction.
