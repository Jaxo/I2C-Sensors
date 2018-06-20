/*
* (C) Copyright 2018 Jaxo Systems.
*
* Author:  Pierre G. Richard
* Written: 06/13/2018
*
* An implementation example to demonstrate the Sgp30Device class.
*
* Compile with:
g++ -Wall -std=c++0x -pthread -o Sgp30Test \
   Sgp30Test.cpp Sgp30Device.cpp Sgp30Features.cpp
*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "Sgp30Device.h"

static char const * const timeStamp(time_t time = (time_t)-1);

static char const * const usage(
   "| Enter one of:\n"
   "|   %s\n"
   "|   %s <i2c-address>\n"
   "|   %s <co2-eq-compensate> <tvoc-compensate> <time-stamp>\n"
   "|   %s <i2c-address> <co2-eq-compensate> <tvoc-compensate> <time-stamp>\n"
);
static char const * const baselineUsage(
   "| You didn't enter any compensation arguments. For the next 15 seconds\n"
   "| the SGP30 will self-initialize, returning erroneous values.\n"
   "| After having run for one hour, a rough baseline will be established\n"
   "| and refined each following hours. To have enough accuracy, it is\n"
   "| advised to run this program during 12 hours.\n"
   "|\n"
   "| Before exiting, a specific command will be displayed to be used\n"
   "| at the next time this program is run.\n"
   "|\n"
   "| IMPORTANT:\n"
   "|   To exit, enter the letter \'q\', do *not* press Ctrl-C.\n"
   "|   If you do, you will loose all your precious compensation parameters.\n"
   "|   Also, note that the compensation parameters are valid only one week.\n"
);
static char const * const runUsage(
   "\n"
   "| Enter:\n"
   "|\t- the letter \"r\" (as in \"raw\") to get the H\u2082 and ethanol values\n"
   "|\t- the letter \"q\" (as in \"quit\") to exit this program\n"
   "|\t- anything else (including nothing) displays the Air Quality values\n"
);
static char const * const noNewBaseline(
   "| No new baseline could have been established since it requires\n"
   "| at least a one hour run to do so.\n"
);
static char const * const commandUsage(
   "| Please, jot the following command: \"%s %d %d %d 0x%lx\"\n"
   "| Use it at next time you restart this program, so that the SGP30 returns\n"
   "| accurate values 15s after it starts\n"
);

/*----------------------------------------------------------------class Args -+
|                                                                             |
+----------------------------------------------------------------------------*/
class Args {
public:
   Args(int argc, char const * const * argv);
   void usage() const;
   int i2cAddr;
   char const * const pgmName;
   unsigned short co2eqCompens;
   unsigned short tvocCompens;
   time_t stampCompens;
   bool isCompensated;
private:
   int getI2cAddr(char const * const buf);
};


/*--------------------------------------------------------------class Thread -+
|                                                                             |
+----------------------------------------------------------------------------*/
class Thread {
public:
   Thread();
   bool create(char const * name, void *(*func)(void *), void * arg);
   void terminate();
private:
   pthread_t m_tid;
   char const * m_name;
};

/*-------------------------------------------------------- class MyInterface -+
|                                                                             |
+----------------------------------------------------------------------------*/
class MyInterface: public Sgp30Device::Interface {
public:
   MyInterface(int i2cAddr) {
      if (((m_fd = ::open("/dev/i2c-1", O_RDWR)) < 0) ||
         (ioctl(m_fd, I2C_SLAVE, i2cAddr) < 0)) {
         printf("Can't open I2C at address %d - Exiting\n", i2cAddr);
         exit(2);
      }
   }
   virtual ~MyInterface() {
      if (m_fd >= 0) { ::close(m_fd); m_fd = -1; }
   }
   void sleep(int us) { usleep(us); }
   bool write(void const * buf, int len) {
      return (m_fd >= 0)? ::write(m_fd, buf, len) == len : false;
   }
   bool read(void * buf, int len) {
      return (m_fd >= 0)? ::read(m_fd, buf, len) == len : false;
   }
private:
   int m_fd;
};

/*------------------------------------------------------ class MySgp30Device -+
|                                                                             |
+----------------------------------------------------------------------------*/
class MySgp30Device : public Sgp30Device {
public:
   MySgp30Device(MyInterface & interface, Args & args);
   ~MySgp30Device();
   void acquireLock() {
      if (pthread_mutex_lock(&m_mutex) != 0) perror("mutex_lock");
   }
   void releaseLock() {
      if (pthread_mutex_unlock(&m_mutex) != 0) perror("mutex_unlock");
   }
private:
   Args & m_args;
   Thread m_tickler; // to send measureAirQuality each sec in the background
   pthread_mutex_t m_mutex;
   sigset_t m_sigset;
   static void * runTickler(void * p);
   void * tickle();
};

/*-----------------------------------------------MySgp30Device::MySgp30Device-+
|                                                                             |
+----------------------------------------------------------------------------*/
MySgp30Device::MySgp30Device(MyInterface & interface, Args & args) :
Sgp30Device(interface), m_args(args), m_mutex(PTHREAD_MUTEX_INITIALIZER)
{
   if (!isOperational()) {
      printf("No SGP30 device, or device not operational - Exiting\n");
      exit(3);
   }
   printf(
      "\n[%s] SGP30 serial #%lld found, type:%u, feature: %d\n",
      timeStamp(),
      getSerialId(),
      getProductType(),
      getProductVersion()
   );
   if (!initAirQuality()) {
      printf("Init Air Quality failure - Exiting\n");
      exit(4);
   }
   if (args.isCompensated && !setBaseline(args.co2eqCompens, args.tvocCompens)) {
      printf("Set Baseline failure - Exiting\n");
      exit(5);
   }

   // start tickling in the background at a period of 1s
   sigemptyset(&m_sigset);
   sigaddset(&m_sigset, SIGUSR1);
   if (
      (pthread_sigmask(SIG_BLOCK, &m_sigset, 0) != 0) ||
      (m_tickler.create("MySgp30Device.Tickler", runTickler, this))
   ) {
      perror("MySgp30Device.Tickler");
   }
}

/*--------------------------------------------------MySgp30Device::runTickler-+
|                                                                             |
+----------------------------------------------------------------------------*/
void * MySgp30Device::runTickler(void * p) {
   return ((MySgp30Device *)p)->tickle();
}

/*----------------------------------------------MySgp30Device::~MySgp30Device-+
|                                                                             |
+----------------------------------------------------------------------------*/
MySgp30Device::~MySgp30Device() {
   m_tickler.terminate();
   pthread_mutex_destroy(&m_mutex);
}

/*------------------------------------------------------MySgp30Device::tickle-+
| This is executed under a separate thread.                                   |
+----------------------------------------------------------------------------*/
void * MySgp30Device::tickle() {
   unsigned short ticks = 0;
   unsigned short hours = 0;
   struct timespec timeout;
   timeout.tv_sec = 1;           // tickeling period of 1s
   timeout.tv_nsec = 0L;
// ::setpriority(PRIO_PROCESS, syscall(SYS_gettid), 2); // lower the priority
   do {
      acquireLock();
      measureAirQuality();       // no arguments -> immediate return
      usleep(12000);             // sleep 12ms (table 10)
      if (++ticks % 3601 == 0) { // every hour, get the baseline
         ++hours;
         ticks = 1;
         if (getBaseline(&m_args.co2eqCompens, &m_args.tvocCompens)) {
            m_args.stampCompens = time(0);
            m_args.isCompensated = true;
            printf(
               "[%s] Hourly baseline established: %d/%d/0x%lx\n",
               timeStamp(), m_args.co2eqCompens, m_args.tvocCompens,
               m_args.stampCompens
            );
         }else {
            printf("Get Baseline failure - Exiting\n");
            exit(6);
         }
      }
      releaseLock();
      // printf("\u2022"); fflush(stdout);
   }while ((sigtimedwait(&m_sigset, 0, &timeout) == -1) && (errno == EAGAIN));

   printf("| This run durated %d hour(s) and %d seconds.\n", hours, ticks);
   if (hours == 0) {
      printf("%s|\n", noNewBaseline);
   }
   if (m_args.isCompensated) {
      printf(
         commandUsage,
         m_args.pgmName, m_args.i2cAddr,
         m_args.co2eqCompens, m_args.tvocCompens, m_args.stampCompens
      );
   }
   return 0;
}

/*-----------------------------------------------------------------Args::Args-+
|                                                                             |
+----------------------------------------------------------------------------*/
Args::Args(int argc, char const * const * argv) :
i2cAddr(0x58),
pgmName(argv[0]),
isCompensated(false)
{
   switch (argc) {
   case 1:
      break;
   case 2:
      if (*argv[1] == '?') {
         usage();
         exit(0);
      }
      i2cAddr = getI2cAddr(argv[1]);
      break;
   case 4:
      co2eqCompens = (unsigned short)strtod(argv[1], 0);
      tvocCompens = (unsigned short)strtod(argv[2], 0);
      stampCompens = (time_t)strtod(argv[3], 0);
      isCompensated = true;
      break;
   case 5:
      i2cAddr = getI2cAddr(argv[1]);
      co2eqCompens = (unsigned short)strtod(argv[2], 0);
      tvocCompens = (unsigned short)strtod(argv[3], 0);
      stampCompens = (time_t)strtod(argv[4], 0);
      isCompensated = true;
      break;
   default:
      usage();
      printf("Exiting\n");
      exit(1);
      break;
   }
   if (isCompensated) {
      time_t t;
      time(&t);
      if (stampCompens > t) {
         printf(
            "Baseline discarded: timestamp (%s) greather than current time\n",
            timeStamp(stampCompens)
         );
         isCompensated = false;
      }else if ((t - stampCompens) > 604800) {
         printf(
            "Baseline discarded: timestamp (%s) is older than a week\n",
            timeStamp(stampCompens)
         );
         isCompensated = false;
      }else {
         printf("Re-using baseline dated %s\n", timeStamp(stampCompens));
      }
   }
};

/*----------------------------------------------------------------Args::usage-+
|                                                                             |
+----------------------------------------------------------------------------*/
void Args::usage() const {
   printf(::usage, pgmName, pgmName, pgmName, pgmName);
}

/*-----------------------------------------------------------Args::getI2cAddr-+
|                                                                             |
+----------------------------------------------------------------------------*/
int Args::getI2cAddr(char const * buf) {
   int addr = (int)strtod(buf, 0);
   if ((addr < 0x3) | (addr > 0x77)) {
      usage();
      printf("%d is a bad I2C address - exiting\n", addr);
      exit(1);
   }
   return addr;
}

/*-------------------------------------------------------------Thread::Thread-+
|                                                                             |
+----------------------------------------------------------------------------*/
Thread::Thread() : m_tid((pthread_t)0L) {}

/*-------------------------------------------------------------Thread::create-+
|                                                                             |
+----------------------------------------------------------------------------*/
bool Thread::create(
   char const * name,
   void *(*func)(void *),
   void * arg
) {
   m_name = name;
   return (
     (pthread_create(&m_tid, 0, func, arg) == 0) &&
     (pthread_setname_np(m_tid, name) == 0)
   );
}

/*----------------------------------------------------------Thread::terminate-+
|                                                                             |
+----------------------------------------------------------------------------*/
void Thread::terminate() {
   if (m_tid != 0) {
      void * status;
      int rc = 100;
      struct timespec timeout;
      timeout.tv_sec = 10;  // give 10 seconds for the child to finish
      timeout.tv_nsec = 0;
      pthread_kill(m_tid, SIGUSR1);
      if (clock_gettime(CLOCK_REALTIME, &timeout) == 0) {
         timeout.tv_sec += 10;  // give 10 seconds for the child to finish
         rc = pthread_timedjoin_np(m_tid, &status, &timeout);
      }
      if (rc != 0) {
         printf("*** %s didn't terminate properly: %d\n", m_name, rc);
      }
   }
}

/*------------------------------------------------------------------timeStamp-+
|                                                                             |
+----------------------------------------------------------------------------*/
static char const * const timeStamp(time_t t) {
   static char stamp[30];
   if (t == (time_t)-1) time(&t);
   strftime(stamp, 30, "%b %d, %Y %H:%M:%S", localtime(&t));
   return stamp;
}

/*-----------------------------------------------------------------------main-+
|                                                                             |
+----------------------------------------------------------------------------*/
int main(int argc, char const * const * argv) {
   Args args(argc, argv);
   MyInterface interface(args.i2cAddr);
   MySgp30Device device(interface, args);
   char ch;
   bool isDone = false;

   printf("%s", runUsage);
   if (!args.isCompensated) {
      printf("|\n%s", baselineUsage);
   }
   printf("\n");
   while (!isDone && ((ch=fgetc(stdin)) >= 0)) {
      device.acquireLock();
      switch (ch) {
      case 'q':    // quit
         isDone = true;
         break;
      case 'r':    // raw
         {
            unsigned short h2;
            unsigned short ethanol;
            if (!device.measureRawSignals(&h2, &ethanol)) {
               printf("[%s] Error: measureRawSignals\n", timeStamp());
            }else {
               printf(
                  "[%s] H\u2082: %.2f, Ethanol: %.2f\n",
                  timeStamp(), (double)h2/512, (double)ethanol/512
               );
            }
         }
         break;
      case 'b':    // baseline (undocumented)
         {
            unsigned short co2eq;
            unsigned short tvoc;
            if (!device.getBaseline(&co2eq, &tvoc)) {
               printf("[%s] Error: getBaseline\n", timeStamp());
            }else {
               printf(
                  "[%s] Baseline: CO\u2082 Eq: %huppm, TVOC: %huppb\n",
                  timeStamp(), co2eq, tvoc
               );
            }
         }
         break;
      default:
         {
            unsigned short co2eq;
            unsigned short tvoc;
            device.measureAirQuality(&co2eq, &tvoc);
            printf(
               "[%s] CO\u2082 Eq: %huppm, TVOC: %huppb\n",
               timeStamp(), co2eq, tvoc
            );
         }
      }
      device.releaseLock();
      while (ch != '\n') ch=fgetc(stdin); // flush the rest...
   }
   return 0;
}
/*===========================================================================*/
