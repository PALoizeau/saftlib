// @file saft-mcbm-ro.cpp
// @brief Command-line reader of mCBM timing events for saftlib.
// @author Pierre-Alain Loizeau  <p.-a.loizeau@gsi.de>
//
// Copyright (C) 2022-2023 Facility for Antiproton and Ion Research GmbH
//
// Derived from saft-ctl.cpp
//
//*****************************************************************************
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//*****************************************************************************
//

#define __STDC_FORMAT_MACROS
#define __STDC_CONSTANT_MACROS

#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>

#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

#include "interfaces/SAFTd.h"
#include "interfaces/TimingReceiver.h"
#include "interfaces/SoftwareActionSink.h"
#include "interfaces/SoftwareCondition.h"
#include "interfaces/iDevice.h"
#include "interfaces/iOwned.h"
#include "CommonFunctions.h"

using namespace std;

static const char* program;
static uint32_t pmode = PMODE_NONE;   // how data are printed (hex, dec, verbosity)
bool printJSON      = false;          // display values in JSON format
bool absoluteTime   = false;
bool UTC            = false;          // show UTC instead of TAI
bool UTCleap        = false;

// this will be called, in case we are snooping for events
static void on_action(uint64_t id, uint64_t param, saftlib::Time deadline, saftlib::Time executed, uint16_t flags)
{
/*
  if (printJSON)
  {
    std::cout << "{ ";
    std::cout << "\"Deadline\": \"" << tr_formatDate(deadline, pmode, printJSON) << "\",";
  }
  else
  {
    std::cout << "tDeadline: " << tr_formatDate(deadline, pmode, printJSON);
  }
  std::cout << tr_formatActionEvent(id, pmode, printJSON);
  std::cout << tr_formatActionParam(param, 0xFFFFFFFF, pmode, printJSON);
  std::cout << tr_formatActionFlags(flags, executed - deadline, pmode, printJSON);

  if (printJSON)
  {
    uint64_t time_tai = deadline.getTAI();
    std::cout << std::hex << std::setfill('0') << "\"EventRaw\": \"0x" << std::setw(16) << id << "\"" << ", ";
    std::cout << std::hex << std::setfill('0') << "\"ParameterRaw\": \"0x" << std::setw(16) << param << "\"" << ", ";
    std::cout << std::dec << "\"DeadlineRaw\": " << time_tai;
    std::cout << " }"<< std::endl;
  }
  else
  {
    std::cout << std::endl;
  }
*/
  if (pmode & PMODE_VERBOSE) {
    const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    std::cout << "=>  System time: " << now.time_since_epoch().count()
              << " (" << (now.time_since_epoch().count() - executed.getUTC()) << ")"
              << "\n";
  }
  std::cout << "Planned UTC: " << setw(20) << deadline.getUTC();
  std::cout << " TAI: " << setw(20) << deadline.getTAI();
  std::cout << " Raw:" << std::hex << std::setfill('0')
            << " 0x" << std::setw(16) << id
            << " 0x" << std::setw(16) << param
            << " 0x" << std::setw(4) << flags
            << std::dec << std::setfill(' ');
  std::cout << " exec UTC: " << setw(20) << executed.getUTC();
  std::cout << " TAI: " << setw(20) << executed.getTAI();

  /// Dec  Hex  Name                  Meaning
  ///
  /// Spill limits
  /// 46   2E   EVT_EXTR_START_SLOW   Start of extraction
  /// 51   33   EVT_EXTR_END 	        End of extraction
  /// 78   4E   EVT_EXTR_STOP_SLOW    End of slow extraction
  ///
  /// Cycle limits
  /// 32   20   EVT_START_CYCLE       First Event in a cycle
  /// 55   37   EVT_END_CYCLE         End of a cycle
  uint32_t uEventNb = ((id >> 36) & 0xfff);
  switch (uEventNb) {
    case 32:
      std::cout << " => EVT_START_CYCLE     ";
      break;
    case 55:
      std::cout << " => EVT_END_CYCLE       ";
      break;
    case 46:
      std::cout << " => EVT_EXTR_START_SLOW ";
      break;
    case 51:
      std::cout << " => EVT_EXTR_END        ";
      break;
    case 78:
      std::cout << " => EVT_EXTR_STOP_SLOW  ";
      break;
  }
  std::cout << tr_formatDate(deadline, pmode, printJSON);
  std::cout << tr_formatActionFlags(flags, executed - deadline, pmode, printJSON);
  std::cout << std::endl;
} // on_action


using namespace saftlib;
using namespace std;

// display help
static void help(void) {
  std::cout << std::endl << "Usage: " << program << " <device name> [OPTIONS] [command]" << std::endl;
  std::cout << std::endl;
  std::cout << "  -h                   display this help and exit" << std::endl;
  std::cout << "  -a                   use absolute time (UTC)" << std::endl;
  std::cout << "  -f                   use the first attached device (and ignore <device name>)" << std::endl;
  std::cout << "  -d                   display values in dec format" << std::endl;
  std::cout << "  -x                   display values in hex format" << std::endl;
  std::cout << "  -v                   more verbosity, usefull with command 'snoop'" << std::endl;
//  std::cout << "  -p                   used with command 'inject': <time> will be added to next full second (option -p) or current time (option unused)" << std::endl;
  std::cout << "  -i                   display saftlib info" << std::endl;
  std::cout << "  -j                   list all attached devices (hardware)" << std::endl;
  std::cout << "  -J                   display values in JSON format" << std::endl;
  std::cout << "  -k                   display gateware version (hardware)" << std::endl;
  std::cout << "  -s                   display actual status of software actions" << std::endl;
  std::cout << "  -t                   display the current temperature in Celsius (if sensor is available) " << std::endl;
  std::cout << "  -U                   display/inject absolute time in UTC instead of TAI" << std::endl;
  std::cout << "  -L                   used with command 'inject' and -U: if injected UTC second is ambiguous choose the later one" << std::endl;
  std::cout << std::endl;
//  std::cout << "  inject  <eventID> <param> <time>  inject event locally, time [ns] is relative (see option -p for precise timing)" << std::endl;
  std::cout << "  snoop   <eventID> <mask> <offset> [<seconds>] snoop events from DM, offset is in ns, " << std::endl;
  std::cout << "                                   snoop for <seconds> or use CTRL+C to exit (try 'snoop 0x0 0x0 0' for ALL)" << std::endl;
  std::cout << std::endl;
  std::cout << "  attach <path>                    instruct saftd to control a new device (admin only)" << std::endl;
  std::cout << "  remove                           remove the device from saftlib management (admin only)" << std::endl;
  std::cout << "  quit                             instructs the saftlib daemon to quit (admin only)" << std::endl << std::endl;
  std::cout << std::endl;
  std::cout << "This tool displays Timing Receiver and related saftlib status. It can also be used to list the ECA status for" << std::endl;
  std::cout << "software actions. Furthermore, one can do simple things with a Timing Receiver (snoop for events, inject messages)." <<std::endl;
  std::cout << std::endl;
  std::cout << "Tip: For using negative values with commands such as 'snoop', consider" << std::endl;
  std::cout << "using the special argument '--' to terminate option scanning." << std::endl << std::endl;

  std::cout << "Report bugs to <d.beck@gsi.de> !!!" << std::endl;
  std::cout << "Licensed under the GPL v3." << std::endl;
  std::cout << std::endl;
} // help


// display status
static void displayStatus(std::shared_ptr<TimingReceiver_Proxy> receiver,
                          std::shared_ptr<SoftwareActionSink_Proxy> sink) {
  uint32_t       nFreeConditions;
  bool          wrLocked;
  saftlib::Time   wrTime;
  int           width;
  string        fmt;

  map<std::string, std::string> allSinks;
  std::shared_ptr<SoftwareActionSink_Proxy> aSink;

  map<std::string, std::string>::iterator i;
  vector<std::string>::iterator j;

  // display White Rabbit status
  wrLocked        = receiver->getLocked();
  if (wrLocked) {
    wrTime        = receiver->CurrentTime();
    if (absoluteTime) std::cout << "WR locked, time: " << tr_formatDate(wrTime, UTC?PMODE_UTC:PMODE_NONE, printJSON) <<std::endl;
    else std::cout << "WR locked, time: " << tr_formatDate(wrTime, pmode, printJSON) <<std::endl;
  }
  else std::cout << "no WR lock!!!" << std::endl;

  // display ECA status
  nFreeConditions  = receiver->getFree();
  std::cout << "receiver free conditions: " << nFreeConditions;

  std::cout << ", max (capacity of HW): " << sink->getMostFull()
            << "(" << sink->getCapacity() << ")"
            << ", early threshold: " << sink->getEarlyThreshold() << " ns"
            << ", latency: " << sink->getLatency() << " ns"
            << std::endl;

  // find software sinks and display their status
  allSinks = receiver->getSoftwareActionSinks();
  if (allSinks.size() > 0) {
    std::cout << "sinks instantiated on this host: " << allSinks.size() << std::endl;
    // get status of each sink
    for (i = allSinks.begin(); i != allSinks.end(); i++) {
      aSink = SoftwareActionSink_Proxy::create(i->second);
      std::cout << "  " << i->second
                << " (minOffset: " << aSink->getMinOffset() << " ns"
                << ", maxOffset: " << aSink->getMaxOffset() << " ns)"
                << std::endl;
      std::cout << "  -- actions: " << aSink->getActionCount()
                << ", delayed: "    << aSink->getDelayedCount()
                << ", conflict: "   << aSink->getConflictCount()
                << ", late: "       << aSink->getLateCount()
                << ", early: "      << aSink->getEarlyCount()
                << ", overflow: "   << aSink->getOverflowCount()
                << " (max signalRate: " << 1.0 / ((double)aSink->getSignalRate() / 1000000000.0) << "Hz)"
                << std::endl;
      // get all conditions for this sink
      vector< std::string > allConditions = aSink->getAllConditions();
      std::cout << "  -- conditions: " << allConditions.size() << std::endl;
      for (j = allConditions.begin(); j != allConditions.end(); j++ ) {
        std::shared_ptr<SoftwareCondition_Proxy> condition = SoftwareCondition_Proxy::create(*j);
        if (pmode & 1) {std::cout << std::dec; width = 20; fmt = "0d";}
        else           {std::cout << std::hex; width = 16; fmt = "0x";}
        // assemble accept flags config string
        char acceptFlagsConfigStr[] = "....";
        if (condition->getAcceptDelayed())  acceptFlagsConfigStr[0] = 'd';
        if (condition->getAcceptConflict()) acceptFlagsConfigStr[1] = 'c';
        if (condition->getAcceptEarly())    acceptFlagsConfigStr[2] = 'e';
        if (condition->getAcceptLate())     acceptFlagsConfigStr[3] = 'l';
        std::cout << "  ---- " << tr_formatActionEvent(condition->getID(), pmode, printJSON) //ID: "   << fmt << std::setw(width) << std::setfill('0') << condition->getID()
                  << ", mask: "         << fmt << std::setw(width) << std::setfill('0') << condition->getMask()
                  << ", offset: "       << fmt << std::setw(9)     << std::setfill('0') << condition->getOffset()
                  << ", accept: "       << acceptFlagsConfigStr
                  << ", active: "       << std::dec << condition->getActive()
                  << ", destructible: " << condition->getDestructible()
                  << ", owner: "        << condition->getOwner()
                  << std::endl;
      } // for all conditions
    } // for all sinks
  } // display all sinks
} // displayStatus


// display information on the software environmet
static void displayInfoSW(std::shared_ptr<SAFTd_Proxy> saftd) {
  std::string sourceVersion;
  std::string buildInfo;

  sourceVersion   = saftd->getSourceVersion();
  buildInfo       = saftd->getBuildInfo();

  std::cout << "saftlib source version                  : " << sourceVersion << std::endl;
  std::cout << "saftlib build info                      : " << buildInfo << std::endl;
} // displayInfoSW


// display information on the hardware environmet
static void displayInfoHW(std::shared_ptr<SAFTd_Proxy> saftd) {
  std::string sourceVersion;
  std::string buildInfo;
  std::string ebDevice;
  std::string devName;
  map< std::string, std::string > allDevices;
  map<std::string, std::string>::iterator i;
  std::shared_ptr<TimingReceiver_Proxy> aDevice;

  map< std::string, std::string > gatewareInfo;
  map<std::string, std::string>::iterator j;

  allDevices      = saftd->getDevices();

  std::cout << "devices attached on this host   : " << allDevices.size() << std::endl;
  for (i = allDevices.begin(); i != allDevices.end(); i++ ) {
    aDevice =  TimingReceiver_Proxy::create(i->second);
    std::cout << "  device: " << i->second;
    std::cout << ", name: " << aDevice->getName();
    std::cout << ", path: " << aDevice->getEtherbonePath();
    std::cout << ", gatewareVersion : " << aDevice->getGatewareVersion();
    std::cout << std::endl;
    gatewareInfo = aDevice->getGatewareInfo();
    std::cout << "  --gateware version info:" << std::endl;
    for (j = gatewareInfo.begin(); j != gatewareInfo.end(); j++) {
      std::cout << "  ---- " << j->second << std::endl;
    } // for j
    if (pmode & PMODE_VERBOSE) {
      std::map<std::string, std::map<std::string, std::string> > interfaces = aDevice->getInterfaces();
      for (auto &interface: interfaces) {
        std::cout << "Interface: " << interface.first << std::endl;
        for (auto &name_objpath: interface.second) {
          std::cout << "   " << std::setw(20) << name_objpath.first << " " << name_objpath.second << std::endl;
        }
      }
    }
    std::cout <<std::endl;
  } //for i
} // displayInfoHW


static void displayInfoGW(std::shared_ptr<TimingReceiver_Proxy> receiver)
{
  std::cout << receiver->getGatewareVersion() << std::endl;
} // displayInfoGW

static void displayCurrentTemperature(std::shared_ptr<TimingReceiver_Proxy> receiver)
{
  if (receiver->getTemperatureSensorAvail())
    std::cout << "current temperature (Celsius): " << receiver->CurrentTemperature() << std::endl;
  else
    std::cout << "no temperature sensor is available in this device!" << std::endl;
}

int main(int argc, char** argv)
{
  // variables and flags for command line parsing
  int  opt;
  bool eventSnoop     = false;
  bool statusDisp     = false;
  bool infoDispSW     = false;
  bool infoDispHW     = false;
  bool infoDispGW     = false;
  bool ppsAlign       = false;
  bool eventInject    = false;
  bool deviceAttach   = false;
  bool deviceRemove   = false;
  bool useFirstDev    = false;
  bool saftdQuit      = false;
  bool currentTemp    = false;
  char *value_end;

  // variables snoop event
  uint64_t snoopID     = 0x0;
  uint64_t snoopMask   = 0x0;
  int64_t  snoopOffset = 0x0;
  int64_t snoopSeconds = 0x7FFFFFFFFFFFFFFF; // maximum value

  // variables inject event
  uint64_t eventID     = 0x0;     // full 64 bit EventID contained in the timing message
  uint64_t eventParam  = 0x0;     // full 64 bit parameter contained in the timing message
  uint64_t eventTNext  = 0x0;     // time for next event (this value is added to the current time or the next PPS, see option -p
  saftlib::Time eventTime;     // time for next event in PTP time
  saftlib::Time ppsNext;     // time for next PPS
  saftlib::Time wrTime;     // current WR time

  // variables attach, remove
  char    *deviceName = NULL;
  char    *devicePath = NULL;

  const char *command;

  pmode       = PMODE_NONE;

  // parse for options
  program = argv[0];
  while ((opt = getopt(argc, argv, "dxsvapijJkhftUL")) != -1) {
    switch (opt) {
      case 'f' :
        useFirstDev = true;
        break;
      case 's':
        statusDisp = true;
        break;
      case 't':
        currentTemp = true;
        break;
      case 'i':
        infoDispSW = true;
        break;
      case 'a':
        absoluteTime = true;
        break;
      case 'j':
        infoDispHW = true;
        break;
      case 'J':
        printJSON = true;
        break;
      case 'k':
        infoDispGW = true;
        break;
/*
      case 'p':
        ppsAlign = true;
        break;
      case 'U':
        UTC = true;
        pmode = pmode + PMODE_UTC;
        break;
      case 'L':
        if (UTC) {
          UTCleap = true;
        } else {
          std::cerr << "-L only works with -U" << std::endl;
          return -1;
        } // else 'L'
        break;
*/
      case 'd':
        pmode = pmode + PMODE_DEC;
        break;
      case 'x':
        pmode = pmode + PMODE_HEX;
        break;
      case 'v':
        pmode = pmode + PMODE_VERBOSE;
        break;
      case 'h':
        help();
        return 0;
      default:
        std::cerr << program << ": bad getopt result" << std::endl;
        return 1;
    } // switch opt
  }   // while opt

  if (optind >= argc) {
    std::cerr << program << " expecting one non-optional argument: <device name>" << std::endl;
    help();
    return 1;
  }

  deviceName = argv[optind];

  try {
    // initialize required stuff
    std::shared_ptr<SAFTd_Proxy> saftd = SAFTd_Proxy::create();

    // do display information that is INDEPENDANT of a specific device
    if (infoDispSW) {
      displayInfoSW(saftd);
    }
    if (infoDispHW) {
      displayInfoHW(saftd);
    }

    // get a specific device
    map<std::string, std::string> devices = SAFTd_Proxy::create()->getDevices();
    std::shared_ptr<TimingReceiver_Proxy> receiver;
    if (useFirstDev) {
      if (devices.empty()) {
          std::cerr << "No devices attached to saftd" << std::endl;
          return -1;
      }
      receiver = TimingReceiver_Proxy::create(devices.begin()->second);
    } else {
      if (devices.find(deviceName) == devices.end()) {
        if (deviceRemove) {
          std::cerr << "Device '" << deviceName << "' was removed" << std::endl;
        } else {
          std::cerr << "Device '" << deviceName << "' does not exist" << std::endl;
        }
        return -1;
      } // find device
      receiver = TimingReceiver_Proxy::create(devices[deviceName]);
    } //if(useFirstDevice);

    if (infoDispGW) {
      displayInfoGW(receiver);
    }

    if (currentTemp) {
      displayCurrentTemperature(receiver);
    }

    std::shared_ptr<SoftwareActionSink_Proxy> sink = SoftwareActionSink_Proxy::create(receiver->NewSoftwareActionSink(""));

    // display status of software actions
    if (statusDisp) {
      displayStatus(receiver, sink);
    }

    // snoop
///    if (eventSnoop) {
///      std::shared_ptr<SoftwareCondition_Proxy> condition
///        = SoftwareCondition_Proxy::create(sink->NewCondition(false, snoopID, snoopMask, snoopOffset));
///      // Accept all errors
///      condition->setAcceptLate(true);
///      condition->setAcceptEarly(true);
///      condition->setAcceptConflict(true);
///      condition->setAcceptDelayed(true);
///      condition->SigAction.connect(sigc::ptr_fun(&on_action));
///      condition->setActive(true);
///      // set up new thread to snoop for the given number of seconds
///      bool runSnoop = true;
///      std::thread tSnoop( [snoopSeconds, &runSnoop]()
///        {
///          sleep(snoopSeconds);
///          runSnoop = false;
///        }
///      );
///      int64_t snoopMilliSeconds;
///      if (snoopSeconds < (0x7FFFFFFFFFFFFFFF / 1000)) {
///        snoopMilliSeconds = snoopSeconds * 1000;
///      } else {
///        // This is a workaround to avoid an overflow when calculating snoopSeconds * 1000.
///        snoopMilliSeconds = snoopSeconds;
///      }
///      while(runSnoop) {
///        saftlib::wait_for_signal(snoopMilliSeconds);
///      }
///      tSnoop.join();
///    } // eventSnoop (without UNILAC option)


/// Dec  Hex  Name                  Meaning
///
/// Spill limits
/// 46   2E   EVT_EXTR_START_SLOW   Start of extraction
/// 51   33   EVT_EXTR_END 	        End of extraction
/// 78   4E   EVT_EXTR_STOP_SLOW    End of slow extraction
///
/// Cycle limits
/// 32   20   EVT_START_CYCLE       First Event in a cycle
/// 55   37   EVT_END_CYCLE         End of a cycle

    ///                            Format           Group = SIS18      Event ID
    snoopMask                   = (0xFull << 60) + (0xFFFull << 48) + (0xFFFull << 36);  // <= 0xFFFF FFF0 0000 0000
    uint64_t snoopIdTesting     = (0x1ull << 60) + (0x0D2ull << 48) + (0x101ull << 36);  // <= CRYRING CMD_SEQ_START
    uint64_t snoopIdExtStart    = (0x1ull << 60) + (0x12Cull << 48) + (0x02Eull << 36);  // <= 46
    uint64_t snoopIdExtStop     = (0x1ull << 60) + (0x12Cull << 48) + (0x033ull << 36);  // <= 51
    uint64_t snoopIdExtStopSlow = (0x1ull << 60) + (0x12Cull << 48) + (0x04Eull << 36);  // <= 78
    uint64_t snoopIdCycleStart  = (0x1ull << 60) + (0x12Cull << 48) + (0x020ull << 36);  // <= 32
    uint64_t snoopIdCycleStop   = (0x1ull << 60) + (0x12Cull << 48) + (0x037ull << 36);  // <= 55
/*
    std::cout << std::hex << std::setfill('0')
              << "Snoop Mask            0x" << std::setw(16) << snoopMask << std::endl
              << "Testing       snoopId 0x" << std::setw(16) << snoopIdTesting << std::endl
              << "Ext Start     snoopId 0x" << std::setw(16) << snoopIdExtStart << std::endl
              << "Ext Stop      snoopId 0x" << std::setw(16) << snoopIdExtStop << std::endl
              << "Ext Stop Slow snoopId 0x" << std::setw(16) << snoopIdExtStopSlow << std::endl
              << "Cycle Start   snoopId 0x" << std::setw(16) << snoopIdCycleStart << std::endl
              << "Cycle Start   snoopId 0x" << std::setw(16) << snoopIdCycleStop << std::endl
              << std::setfill(' ') << std::dec;
*/
/*
    std::shared_ptr<SoftwareCondition_Proxy> conditionTesting
      = SoftwareCondition_Proxy::create(sink->NewCondition(false, snoopIdTesting, snoopMask, snoopOffset));
    // Accept all errors
    conditionTesting->setAcceptLate(true);
    conditionTesting->setAcceptEarly(true);
    conditionTesting->setAcceptConflict(true);
    conditionTesting->setAcceptDelayed(true);
    conditionTesting->SigAction.connect(sigc::ptr_fun(&on_action));
    conditionTesting->setActive(true);
*/
    std::shared_ptr<SoftwareCondition_Proxy> conditionExtStart
      = SoftwareCondition_Proxy::create(sink->NewCondition(false, snoopIdExtStart, snoopMask, snoopOffset));
    // Accept all errors
    conditionExtStart->setAcceptLate(true);
    conditionExtStart->setAcceptEarly(true);
    conditionExtStart->setAcceptConflict(true);
    conditionExtStart->setAcceptDelayed(true);
    conditionExtStart->SigAction.connect(sigc::ptr_fun(&on_action));
    conditionExtStart->setActive(true);

    std::shared_ptr<SoftwareCondition_Proxy> conditionExtStop
      = SoftwareCondition_Proxy::create(sink->NewCondition(false, snoopIdExtStop, snoopMask, snoopOffset));
    // Accept all errors
    conditionExtStop->setAcceptLate(true);
    conditionExtStop->setAcceptEarly(true);
    conditionExtStop->setAcceptConflict(true);
    conditionExtStop->setAcceptDelayed(true);
    conditionExtStop->SigAction.connect(sigc::ptr_fun(&on_action));
    conditionExtStop->setActive(true);

    std::shared_ptr<SoftwareCondition_Proxy> conditionExtStopSlow
      = SoftwareCondition_Proxy::create(sink->NewCondition(false, snoopIdExtStopSlow, snoopMask, snoopOffset));
    // Accept all errors
    conditionExtStopSlow->setAcceptLate(true);
    conditionExtStopSlow->setAcceptEarly(true);
    conditionExtStopSlow->setAcceptConflict(true);
    conditionExtStopSlow->setAcceptDelayed(true);
    conditionExtStopSlow->SigAction.connect(sigc::ptr_fun(&on_action));
    conditionExtStopSlow->setActive(true);

    std::shared_ptr<SoftwareCondition_Proxy> conditionCycleStart
      = SoftwareCondition_Proxy::create(sink->NewCondition(false, snoopIdCycleStart, snoopMask, snoopOffset));
    // Accept all errors
    conditionCycleStart->setAcceptLate(true);
    conditionCycleStart->setAcceptEarly(true);
    conditionCycleStart->setAcceptConflict(true);
    conditionCycleStart->setAcceptDelayed(true);
    conditionCycleStart->SigAction.connect(sigc::ptr_fun(&on_action));
    conditionCycleStart->setActive(true);

    std::shared_ptr<SoftwareCondition_Proxy> conditionCycleStop
      = SoftwareCondition_Proxy::create(sink->NewCondition(false, snoopIdCycleStop, snoopMask, snoopOffset));
    // Accept all errors
    conditionCycleStop->setAcceptLate(true);
    conditionCycleStop->setAcceptEarly(true);
    conditionCycleStop->setAcceptConflict(true);
    conditionCycleStop->setAcceptDelayed(true);
    conditionCycleStop->SigAction.connect(sigc::ptr_fun(&on_action));
    conditionCycleStop->setActive(true);

    // set up new thread to snoop for the given number of seconds
    bool runSnoop = true;
    std::thread tSnoop( [snoopSeconds, &runSnoop]()
      {
        sleep(snoopSeconds);
        runSnoop = false;
      }
    );
    int64_t snoopMilliSeconds;
    if (snoopSeconds < (0x7FFFFFFFFFFFFFFF / 1000)) {
      snoopMilliSeconds = snoopSeconds * 1000;
    } else {
      // This is a workaround to avoid an overflow when calculating snoopSeconds * 1000.
      snoopMilliSeconds = snoopSeconds;
    }
    while(runSnoop) {
      saftlib::wait_for_signal(snoopMilliSeconds);
    }
    tSnoop.join();

  } catch (const saftbus::Error& error) {
    std::string msg(error.what());
    if (saftdQuit && msg == "object path \"/de/gsi/saftlib\" not found") {
      std::cerr << "Quit SAFTd service" << std::endl;
    } else {
      std::cerr << "Failed to invoke method: \'" << error.what() << "\'" << std::endl;
    }
  }

  return 0;
}
