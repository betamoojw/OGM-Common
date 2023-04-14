#pragma once
#include "../Helper.h"
#include "OpenKNX/Console.h"
#include "OpenKNX/FlashStorage.h"
#include "OpenKNX/Hardware.h"
#include "OpenKNX/Helper.h"
#include "OpenKNX/Information.h"
#include "OpenKNX/Logger.h"
#include "OpenKNX/Module.h"
#include "OpenKNX/TimerInterrupt.h"
#include "hardware.h"
#include "knxprod.h"
#include <knx.h>
#ifdef WATCHDOG
#include <Adafruit_SleepyDog.h>
#endif

#ifndef OPENKNX_MAX_MODULES
#define OPENKNX_MAX_MODULES 9
#endif

#ifndef OPENKNX_MAX_LOOPTIME
#define OPENKNX_MAX_LOOPTIME 4000
#endif

#ifndef KNX_SERIAL
#define KNX_SERIAL Serial1
#endif

namespace OpenKNX
{
#ifdef WATCHDOG
    struct WatchdogData
    {
        uint32_t timer = 0;
        uint8_t resetCause;
    };
#endif

    struct Modules
    {
        uint8_t count = 0;
        uint8_t ids[OPENKNX_MAX_MODULES];
        Module* list[OPENKNX_MAX_MODULES];
    };

    class Common : public Helper
    {
      private:
#ifdef DEBUG_LOOP_TIME
        uint32_t lastDebugTime = 0;
#endif
#ifdef WATCHDOG
#ifndef WATCHDOG_MAX_PERIOD_MS
#define WATCHDOG_MAX_PERIOD_MS 16384
#endif
        WatchdogData watchdog;
#endif
        uint8_t _currentModule = 0;
        uint32_t _loopMicros = 0;
        bool _usesDualCore = false;
        Modules _modules;

        uint32_t _savedPinProcessed = 0;
        bool _savePinTriggered = false;
        volatile int32_t _freeMemoryMin = 0x7FFFFFFF;
        char _diagnoseInput[15];
        char _diagnoseOutput[15] = {0};

        void initKnx();
        void appSetup();
        void appLoop();
        void appLoop2();
        void loopModule(uint8_t id);
        void processModulesLoop();
        void registerCallbacks();
        void processRestoreSavePin();
        void initMemoryTimerInterrupt();
        void debugWait();
#ifdef DEBUG_LOG
        void showDebugInfo();
#endif
#if defined(ARDUINO_ARCH_RP2040) && defined(OPENKNX_RECOVERY_ON)
        void processRecovery();
#endif
#ifdef WATCHDOG
        void watchdogSetup();
#endif
#ifdef LOG_HeartbeatDelayBase
        uint32_t _heartbeatDelay;
        void processHeartbeat();
#endif
        void processFunctionProperty(uint8_t objectIndex, uint8_t propertyId, uint8_t length, uint8_t *data, uint8_t *resultData, uint8_t &resultLength);
        void processFunctionPropertyState(uint8_t objectIndex, uint8_t propertyId, uint8_t length, uint8_t *data, uint8_t *resultData, uint8_t &resultLength);

      public:
        FlashStorage flash;
        Information info;
        Console console;
        Logger logger;
        TimerInterrupt timerInterrupt;
        Hardware hardware;

        static VersionCheckResult versionCheck(uint16_t manufacturerId, uint8_t* hardwareType, uint16_t firmwareVersion);

        void init(uint8_t firmwareRevision);
        void triggerSavePin();
        void setup();
        void loop();
        static void loop2();
        bool usesDualCore();
#ifdef LOG_StartupDelayBase
        uint32_t _startupDelay;
#endif
#ifdef WATCHDOG
        void watchdogLoop();
#endif
        bool _afterStartupDelay = false;
        bool afterStartupDelay();
        void processAfterStartupDelay();

        void addModule(uint8_t id, Module* module);
        void collectMemoryStats();
        uint freeMemoryMin();
        bool freeLoopTime();
        Module* getModule(uint8_t id);
        Modules* getModules();

        void processSavePin();
        void processBeforeRestart();
        void processBeforeTablesUnload();
        void processInputKo(GroupObject& iKo);
        void processDiagnoseCommand(GroupObject& iKo);
        std::string logPrefix();
    };
} // namespace OpenKNX

extern OpenKNX::Common openknx;
