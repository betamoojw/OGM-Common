#include "OpenKNX/Console.h"
#include "OpenKNX/Facade.h"
#include "OpenKNX/Flash/Driver.h"
#include "OpenKNX/i2c/test/Benchmark.h"

#if OPENKNX_LITTLE_FS
    #include "LittleFS.h"
#endif

namespace OpenKNX
{
    void Console::loop()
    {
        if (_disableConsole)
            return;
        if (OPENKNX_LOGGER_DEVICE.available())
            processSerialInput();
    }

    void Console::disableConsole(bool disable)
    {
        _disableConsole = disable;
    }

#ifdef BASE_KoDiagnose
    void Console::writeDiagnoseKo(const char* message, ...)
    {
        va_list values;
        va_start(values, message);
        _diagnoseKoOutput = true;
        writeDpt16Ko(KoBASE_Diagnose, message, values);
        knx.loop();
        _diagnoseKoOutput = false;
        va_end(values);
    }

    // fallback for old spelling mistake in method name
    void Console::writeDiagenoseKo(const char* message, ...)
    {
        va_list values;
        va_start(values, message);
        _diagnoseKoOutput = true;
        writeDpt16Ko(KoBASE_Diagnose, message, values);
        knx.loop();
        _diagnoseKoOutput = false;
        va_end(values);
    }

    void Console::processDiagnoseKo(GroupObject& ko)
    {
        // prevent the nested call by the output on the diagnose ko
        if (_diagnoseKoOutput)
            return;

        // quick-fix to ensure \0 at end of 14 char strings
        // TODO cleanup implementation and read DPT16.001
        char cmdBuf[15] = {};
        memcpy(cmdBuf, ko.valueRef(), 14);

        // prevent empty command
        if (cmdBuf[0] == '\0')
            return;

        openknx.logger.logWithPrefixAndValues("DiagnoseKO", "command \"%s\" received", cmdBuf);
        logIndentUp();

        if (!processCommand(cmdBuf, true))
            openknx.logger.logWithPrefix("DiagnoseKO", "command not found");

        logIndentDown();
    }
#endif

    bool Console::processCommand(std::string cmd, bool diagnoseKo /* = false */)
    {
        openknx.common.skipLooptimeWarning();
#if MASK_VERSION == 0x07B0
        TpUartDataLinkLayer* dll = knx.bau().getDataLinkLayer();
#elif MASK_VERSION == 0x091A
        TpUartDataLinkLayer* dll = knx.bau().getSecondaryDataLinkLayer();
#endif

        if (!diagnoseKo && (cmd == "i" || cmd == "info"))
        {
            showInformations();
        }
        else if (!diagnoseKo && (cmd == "h" || cmd == "help"))
        {
            showHelp();
        }
        else if (!diagnoseKo && (cmd == "v" || cmd == "versions"))
        {
            showVersions();
        }
        else if (cmd == "m" || cmd == "mem" || cmd == "memory")
        {
            showMemory(diagnoseKo);
        }
        else if (!diagnoseKo && (cmd == "p" || cmd == "prog"))
        {
            knx.toggleProgMode();
        }
        else if ((cmd == "u" || cmd == "uptime"))
        {
            showUptime(diagnoseKo);
        }
        else if (!diagnoseKo && (cmd == "sleep"))
        {
            sleep();
        }
        else if (!diagnoseKo && (cmd == "r" || cmd == "restart"))
        {
            delay(20);
            openknx.restart();
        }
        else if (!diagnoseKo && (cmd == "fatal"))
        {
            openknx.hardware.fatalError(5, "Test with 5x blinking");
        }
        else if (!diagnoseKo && (cmd == "powerloss"))
        {
            openknx.common.triggerSavePin();
        }
        else if (!diagnoseKo && (cmd == "s" || cmd == "w" || cmd == "save"))
        {
            openknx.flash.save();
        }
        else if (cmd == "flash knx")
        {
            showMemoryContent(openknx.knxFlash.flashAddress(), openknx.knxFlash.size());
        }
        else if (cmd == "flash openknx")
        {
            showMemoryContent(openknx.openknxFlash.flashAddress(), openknx.openknxFlash.size());
        }
        else if (cmd.substr(0, 6) == "mem 0x" && cmd.length() > 6)
        {
            std::string addrstr = cmd.substr(6, cmd.length() - 6);
            uint32_t addr = std::stoi(addrstr, nullptr, 16);
            showMemoryContent((uint8_t*)addr, 0x40);
        }
#ifndef ARDUINO_ARCH_SAMD
        else if ((!diagnoseKo && (cmd.compare(0, 3, "dw ") == 0 || cmd.compare(0, 3, "aw ") == 0)) ||
                 (cmd.compare(0, 3, "dr ") == 0 || cmd.compare(0, 3, "ar ") == 0))
        {
            processPinCommand(cmd);
        }
        else if (!diagnoseKo && (cmd.rfind("dwon ", 0) == 0 || cmd.rfind("dwoff ", 0) == 0))
        {
            processPinCommand("dw " + cmd.substr(((cmd.rfind("dwon ", 0) == 0) ? 5 : 6)) + (cmd.rfind("dwon ", 0) == 0 ? " 1" : " 0"));
        }
        else if (!diagnoseKo && (cmd.compare(0, 4, "i2c ") == 0 || cmd.compare(0, 3, "i2c") == 0))
        {
            processI2cCommand(cmd);
        }
#endif

#ifdef OPENKNX_RUNTIME_STAT
        else if (!diagnoseKo && (cmd == "runtime"))
        {
            openknx.common.showRuntimeStat();
        }
        else if (!diagnoseKo && (cmd == "runtime hist"))
        {
            openknx.common.showRuntimeStat(false, true);
        }
        else if (!diagnoseKo && (cmd == "runtime full"))
        {
            openknx.common.showRuntimeStat(true, true);
        }
#endif
#ifdef OPENKNX_WATCHDOG
        else if (cmd == "watchdog")
        {
            showWatchdogResets(diagnoseKo);
        }
#endif
#if OPENKNX_LITTLE_FS
        else if (!diagnoseKo && (cmd == "fs" || cmd == "files"))
        {
            showFilesystem();
        }
        else if (!diagnoseKo && (cmd == "file dummy"))
        {
            const char* buffer = "DUMMY";
            File file = LittleFS.open("/dummy.dummy", "a");
            file.seek((uint32_t)random());
            file.write((const uint8_t*)buffer, strlen(buffer));
            file.close();
            showFilesystem();
        }
        else if (!diagnoseKo && cmd.rfind("fs dmp ", 0) == 0 && cmd.length() > 7)
        {
            auto fileName = cmd.substr(7, cmd.length() - 7);
            if (fileName[0] != '/')
                fileName = "/" + fileName;
            File file = LittleFS.open(fileName.c_str(), "r");
            if (file.available() && !file.isDirectory())
            {
                logInfo("Filesystem", "Dump of file %s (%u bytes):", fileName.c_str(), file.size());
                uint8_t buffer[16] = {};
                size_t readBytes;
                while ((readBytes = file.readBytes((char*)buffer, sizeof(buffer))) > 0)
                {
                    openknx.logger.logHexWithPrefix("Filesystem", buffer, readBytes);
                }
                openknx.logger.logDividingLine();
            }
            else
            {
                logError("Filesystem", "File %s not found", fileName.c_str());
            }
            file.close();
        }
        else if (!diagnoseKo && cmd.rfind("fs del ", 0) == 0 && cmd.length() > 7)
        {
            auto fileName = cmd.substr(7, cmd.length() - 7);
            if (fileName[0] != '/')
                fileName = "/" + fileName;
            if (LittleFS.remove(fileName.c_str()))
                logInfo("Filesystem", "File %s deleted", fileName.c_str());
            else
                logError("Filesystem", "File %s not found", fileName.c_str());
        }
#endif
#ifdef ARDUINO_ARCH_RP2040
        else if (!diagnoseKo && (cmd == "bootloader"))
        {
            resetToBootloader();
        }
#endif
        else if (!diagnoseKo && (cmd == "erase knx"))
        {
            erase(EraseMode::KnxFlash);
        }
        else if (!diagnoseKo && (cmd == "erase openknx"))
        {
            erase(EraseMode::OpenKnxFlash);
        }
#if OPENKNX_LITTLE_FS
        else if (!diagnoseKo && (cmd == "erase files"))
        {
            erase(EraseMode::Filesystem);
        }
#endif
        else if (!diagnoseKo && (cmd == "erase all"))
        {
            erase(EraseMode::All);
        }
#if MASK_VERSION == 0x07B0 || MASK_VERSION == 0x091A
        else if (cmd.compare("bcu") == 0)
        {
            logInfo("BCU<Status>", "%s", dll->getTPUart().getBcuStateInfo());
            TPUart::Statistics& statistics = dll->getTPUart().getStatistics();
            logInfo("BCU<Stats>", "TX Frames: %u | RX Frames: %u (%u B) | Discarded: %u B | Received: %u B | Load: %u B/s | Buffer: %u | Await %u | Repetitions %u | Overflow %u/%u/%u/%u\n",
                    statistics.getTxFrames(), statistics.getRxFrames(), statistics.getRxFrameBytes(), statistics.getRxDiscardedBytes(), statistics.getRxReceivedBytes(),
                    statistics.getBusLoad(), dll->getTPUart().getReceiver().getSearchBufferPosition(), dll->getTPUart().getReceiver().getAwaitBytes(), statistics.getRxRepetitions(),
                    statistics.getRxUartOverflow(), statistics.getRxSearchBufferOverflow(), statistics.getRxFrameBufferOverflow(), statistics.getTxOverflowFrameBuffer());

            return true;
        }
        else if (cmd.compare("bcu mon") == 0)
        {
            dll->monitor();
            return true;
        }
        else if (cmd.compare("bcu rst") == 0)
        {
            dll->reset();
            return true;
        }
        else if (cmd.compare("bcu poff") == 0)
        {
            dll->powerControl(false);
            return true;
        }
        else if (cmd.compare("bcu pon") == 0)
        {
            dll->powerControl(true);
            return true;
        }
        else if (cmd.compare("bcu debug") == 0)
        {
            if (_bcuDebug)
                _bcuDebug = false;
            else
                _bcuDebug = true;

            openknx.logger.logWithPrefix("BCU<Debug>", _bcuDebug ? "Enabled" : "Disabled");
            return true;
        }
#endif
        else if (openknx.time.processCommand(cmd, diagnoseKo))
        {
            return true;
        }
#ifdef ParamBASE_Latitude
        else if (openknx.sun.processCommand(cmd, diagnoseKo))
        {
            return true;
        }
#endif
        else
        {
            // check modules for command
            for (uint8_t i = 0; i < openknx.modules.count; i++)
                if (openknx.modules.list[i]->processCommand(cmd, diagnoseKo))
                    return true;
            return false;
        }
        return true;
    }

    void Console::processSerialInput()
    {
        const uint8_t current = OPENKNX_LOGGER_DEVICE.read();

        // Magic byte for save data during firmware upgrade
        if (current == 0x7)
        {
            OPENKNX_LOGGER_DEVICE.write(0x7);
            openknx.progLed.forceOn();
            openknx.flash.save(true);
            OPENKNX_LOGGER_DEVICE.write(0x7);
            delay(10000);
            openknx.restart();
        }

        if (current == '\r' || current == '\n')
        {
            if (_consoleCharLast == '\r' && current == '\n')
            {
                _consoleCharLast = current;
                return;
            }

            openknx.logger.log(prompt);
            if (strlen(prompt) > 0)
            {
                if (!processCommand(prompt))
                {
                    // Command not found
                    openknx.logger.logWithValues("%s: command not found", prompt);
                }
            }
            memset(prompt, 0, CONSOLE_INPUT_SIZE); // Reset Promptbuffer
        }

        if (current == '\b' && strlen(prompt) > 0)
            prompt[strlen(prompt) - 1] = 0x0;

        if (strlen(prompt) < CONSOLE_INPUT_SIZE && current >= 32 && current <= 126) // Max. printables chars allowed
            prompt[strlen(prompt)] = current;

        openknx.logger.printPrompt();
        _consoleCharLast = current;
    }

    void Console::showInformations()
    {
#ifdef OPENKNX_DUALCORE
        const char* cpuMode = openknx.usesDualCore() ? "Dual-Core" : "Single-Core";
#else
        const char* cpuMode = "Single-Core";
#endif

        logBegin();
        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.logHeader("");
        openknx.logger.color(0);
        openknx.logger.log("");
        openknx.logger.log("        \x1B[90mOpen \x1B[32m#\x1B[0m           OpenKNX.de");
        openknx.logger.log("        \x1B[32m+----+\x1B[0m");
        openknx.logger.log("        \x1B[32m# \x1B[37mKNX\x1B[0m            wiki.openknx.de - forum.openknx.de");
        openknx.logger.log("");
        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.logHeader("Information");
        openknx.logger.color(0);

        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.log("Device");
        openknx.logger.color(0);
#ifdef DEVICE_ID
        openknx.logger.logWithPrefix("ID", DEVICE_ID);
#endif
#ifdef DEVICE_NAME
        openknx.logger.logWithPrefix("Name", DEVICE_NAME);
#elif defined(HARDWARE_NAME)
        openknx.logger.logWithPrefix("Name", HARDWARE_NAME);
#endif
        openknx.logger.logWithPrefix("Serial number", openknx.info.humanSerialNumber().c_str());

        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.log("Firmware");
        openknx.logger.color(0);
        openknx.logger.logWithPrefix("Name", openknx.info.firmwareName().c_str());
        openknx.logger.logWithPrefix("Version", openknx.info.humanFirmwareVersion().c_str());
        openknx.logger.logWithPrefix("Number", openknx.info.humanFirmwareNumber().c_str());
#if MASK_VERSION == 0x07B0
        openknx.logger.logWithPrefixAndValues("KNX-Type", "TP (%04X)", MASK_VERSION);
#elif MASK_VERSION == 0x57B0
        openknx.logger.logWithPrefixAndValues("KNX-Type", "IP (%04X)", MASK_VERSION);
#elif MASK_VERSION == 0x091A
        openknx.logger.logWithPrefixAndValues("KNX-Type", "Router (%04X)", MASK_VERSION);
#else
        openknx.logger.logWithPrefixAndValues("KNX-Type", "%04X", MASK_VERSION);
#endif
        if (openknx.hardware.cpuTemperature() > 0)
            openknx.logger.logWithPrefixAndValues("CPU-Mode", "%s (Temperature %.1f °C)", cpuMode, openknx.hardware.cpuTemperature());
        else
            openknx.logger.logWithPrefixAndValues("CPU-Mode", "%s", cpuMode);

        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.log("Programming");
        openknx.logger.color(0);
        openknx.logger.logWithPrefixAndValues("Address", "%s (%s)", openknx.info.humanIndividualAddress().c_str(), knx.configured() ? "Configured" : "Unconfigured");
        if (openknx.info.applicationNumber() > 0)
        {
            openknx.logger.logWithPrefix("Version", openknx.info.humanApplicationVersion().c_str());
            openknx.logger.logWithPrefix("Number", openknx.info.humanApplicationNumber().c_str());
        }
        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.log("Runtime");
        openknx.logger.color(0);

        showMemory();

#ifdef OPENKNX_WATCHDOG
        if (openknx.watchdog.active())
            openknx.logger.logWithPrefixAndValues("Watchdog", "Running (%is)", openknx.watchdog.maxPeriod());
        else
            openknx.logger.logWithPrefixAndValues("Watchdog", "Disabled");
#else
        openknx.logger.logWithPrefixAndValues("Watchdog", "Unsupported");
#endif

        for (uint8_t i = 0; i < openknx.modules.count; i++)
            openknx.modules.list[i]->showInformations();

        openknx.logger.logDividingLine();
        openknx.logger.log("");
        logEnd();
    }

#ifdef OPENKNX_WATCHDOG
    void Console::showWatchdogResets(bool diagnoseKo /* = false */)
    {
    #ifdef BASE_KoDiagnose
        if (diagnoseKo)
        {
            if (openknx.watchdog.active())
                openknx.console.writeDiagnoseKo("WD ON (%ix)", openknx.watchdog.resets());
            else
                openknx.console.writeDiagnoseKo("WD OFF");
        }
    #endif
        if (openknx.watchdog.active())
            openknx.logger.logWithPrefixAndValues("Watchdog", "Running (%i Resets)", openknx.watchdog.resets());
        else
            openknx.logger.logWithPrefixAndValues("Watchdog", "Disabled", openknx.watchdog.resets());
    }
#endif

#if OPENKNX_LITTLE_FS
    void Console::showFilesystem()
    {
        logBegin();
        openknx.logger.log("");
        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.logHeader("Filesystem");

        openknx.logger.color(0);
        showFilesystemDirectory("/");
        openknx.logger.logDividingLine();
        logEnd();
    }

    void Console::showFilesystemDirectory(std::string path)
    {
        logBegin();
        openknx.logger.logWithPrefixAndValues("Filesystem", "%s", path.c_str());

        File rootDir = LittleFS.open(path.c_str(), "r");
        File directory = rootDir.openNextFile();
        while (directory)
        {
            std::string full = path + directory.name();
            if (directory.isDirectory())
                showFilesystemDirectory(full + "/");
            else
            {
                openknx.logger.logWithPrefixAndValues("Filesystem", "%s (%i bytes)", full.c_str(), directory.size());
            }
            directory = rootDir.openNextFile();
        }
        logEnd();
    }
#endif

    void Console::showVersions()
    {
        logBegin();
        openknx.logger.log("");
        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.logHeader("Versions");
        openknx.logger.color(0);

        openknx.logger.logWithPrefix("This Firmware", openknx.info.humanFirmwareVersion(true));
        openknx.logger.logWithPrefix("KNX", KNX_Version);
        openknx.logger.logWithPrefix(openknx.common.logPrefix(), MODULE_Common_Version);
        for (uint8_t i = 0; i < openknx.modules.count; i++)
        {
            if (openknx.modules.list[i]->version().empty()) continue;

            openknx.logger.logWithPrefix(openknx.modules.list[i]->name().c_str(), openknx.modules.list[i]->version().c_str());
        }
        openknx.logger.logDividingLine();
        openknx.logger.logWithPrefix("Builddate", __DATE__);
        openknx.logger.logWithPrefix("Buildtime", __TIME__);
        openknx.logger.logDividingLine();
        logEnd();
    }

    void Console::showHelp()
    {
        logBegin();
        openknx.logger.log("");
        openknx.logger.color(CONSOLE_HEADLINE_COLOR);
        openknx.logger.logHeader("Help");
        openknx.logger.color(0);
        openknx.logger.log("Command(s)               Description");
        printHelpLine("help, h", "Show this help");
        printHelpLine("info, i", "Show general information");
        printHelpLine("uptime, u", "Show uptime");
        printHelpLine("version, v", "Show compiled versions");
        printHelpLine("memory, mem", "Show memory usage");
        printHelpLine("mem 0xXXXXXXXX", "Show memory content (64byte) starting at 0xXXXXXXXX");
        printHelpLine("flash knx", "Show knx flash content");
        printHelpLine("flash openknx", "Show openknx flash content");
#if OPENKNX_LITTLE_FS
        printHelpLine("files, fs", "Show files on filesystem");
        printHelpLine("fs del <file>", "Delete a file");
        printHelpLine("fs dmp <file>", "Dump a file");
#endif
#ifdef OPENKNX_RUNTIME_STAT
        printHelpLine("runtime", "Show runtime statistics (Short statistic)");
        printHelpLine("runtime hist", "Show runtime histogram");
        printHelpLine("runtime full", "Show runtime statistics and histogram");
#endif
        printHelpLine("restart, r", "Restart the device");
        printHelpLine("prog, p", "Toggle the ProgMode");
        printHelpLine("save, s, w", "Save data in Flash");
        printHelpLine("sleep", "Sleep for up to 20 seconds");
        printHelpLine("fatal", "Trigger a FatalError");
        printHelpLine("powerloss", "Trigger a PowerLoss (SavePin)");
#ifdef OPENKNX_WATCHDOG
        printHelpLine("watchdog", "Show restart count by watchdog");
#endif
        printHelpLine("erase knx", "Erase knx parameters");
        printHelpLine("erase openknx", "Erase openknx module data");
#if OPENKNX_LITTLE_FS
        printHelpLine("erase files", "Erase filesystem");
#endif
#ifdef ARDUINO_ARCH_RP2040
        printHelpLine("erase all", "Erase all");
#endif
#ifdef ARDUINO_ARCH_RP2040
        printHelpLine("bootloader", "Reset into Bootloader Mode");
#endif
#ifndef ARDUINO_ARCH_SAMD
        printHelpLine("dwon <pin>", "Write digital pin to HIGH");
        printHelpLine("dwoff <pin>", "Write digital pin to LOW");
        printHelpLine("dw <pin> 0-1", "Write digital pin");
        printHelpLine("dr <pin>", "Read digital pin");
        printHelpLine("aw <pin> 0-4095", "Write analog pin");
        printHelpLine("ar <pin>", "Read analog pin");
#endif
        printHelpLine("i2c", "I2C bus commands. Use 'i2c' for help");
#if MASK_VERSION == 0x07B0 || MASK_VERSION == 0x091A
        printHelpLine("bcu", "Show BCU status");
        printHelpLine("bcu mon", "Start BCU monitoring");
        printHelpLine("bcu rst", "Reset BCU");
#endif
#ifdef OPENKNX_TIME_DIGAGNOSTIC
        printHelpLine("tm ?", "Help for time related commands");
#else
        printHelpLine("tm", "Show time information");
    #ifdef OPENKNX_TIME_TESTCOMMAND
        printHelpLine("tm test", "Test some calculation)");
    #endif
#endif
#ifdef ParamBASE_Latitude
        printHelpLine("sun", "Shows sun information");
#endif

        for (uint8_t i = 0; i < openknx.modules.count; i++)
            openknx.modules.list[i]->showHelp();

        openknx.logger.logDividingLine();
        logEnd();
    }

    void Console::sleep()
    {
        openknx.logger.logWithValues("sleep %ims", sleepTime());
        delay(sleepTime());
    }

    uint32_t Console::sleepTime()
    {
#ifdef OPENKNX_WATCHDOG
        return MAX(openknx.watchdog.maxPeriod() + 1, 20000);
#else
        return 20000;
#endif
    }

    void Console::showUptime(bool diagnoseKo /* = false */)
    {
        std::string uptimeStr = openknx.logger.buildUptime();
#ifdef BASE_KoDiagnose
        if (diagnoseKo)
        {
            openknx.console.writeDiagnoseKo("%s", uptimeStr.c_str());
        }
#endif
        openknx.logger.logWithPrefixAndValues("Uptime", "%s", uptimeStr.c_str());
    }

    void Console::showMemory(bool diagnoseKo /* = false */)
    {

#ifdef BASE_KoDiagnose
        if (diagnoseKo)
        {
            openknx.console.writeDiagnoseKo("CUR %.3fKiB", ((float)freeMemory() / 1024));
            openknx.console.writeDiagnoseKo("MIN %.3fKiB", ((float)openknx.common.freeMemoryMin() / 1024));
        }
#endif
        openknx.logger.logWithPrefixAndValues("Free memory", "%.3f KiB (min. %.3f KiB)", ((float)freeMemory() / 1024), ((float)openknx.common.freeMemoryMin() / 1024));
#ifdef ARDUINO_ARCH_ESP32
    #if BOARD_HAS_PSRAM
        openknx.logger.logWithPrefixAndValues("Free PSRAM", "%.3f KiB (min. %.3f KiB)", ((float)ESP.getFreePsram() / 1024), ((float)ESP.getMinFreePsram() / 1024));
    #endif
    #ifdef OPENKNX_DUALCORE
        openknx.logger.logWithPrefixAndValues("Free stack size", "Loop0: %i bytes - Loop1: %i bytes", openknx.common.freeStackMin(), openknx.common.freeStackMin1());
    #else
        openknx.logger.logWithPrefixAndValues("Free stack size", "Loop0: %i bytes", openknx.common.freeStackMin());
    #endif
#endif
#ifdef ARDUINO_ARCH_RP2040

    #ifdef OPENKNX_DUALCORE
        if (openknx.common.freeStackMin() <= 0 || openknx.common.freeStackMin1() <= 0)
    #else
        if (openknx.common.freeStackMin() <= 0)
    #endif
            openknx.logger.color(31);

    #ifdef OPENKNX_DUALCORE
        openknx.logger.logWithPrefixAndValues("Free stack size", "Core0: %i bytes - Core1: %i bytes", openknx.common.freeStackMin(), openknx.common.freeStackMin1());
    #else
        openknx.logger.logWithPrefixAndValues("Free stack size", "Core0: %i bytes", openknx.common.freeStackMin());
    #endif
        openknx.logger.color(0);
#endif
    }

    void Console::showMemoryContent(uint8_t* start, uint32_t size)
    {
        const size_t lineLen = 16;
        uint8_t* end = start + size - (size % lineLen);

        logBegin();
        openknx.logger.logWithPrefixAndValues("Memory content", "Address 0x%08X - Size: 0x%04X (%d bytes)", start, size, size);
        for (uint8_t* linePtr = start; linePtr < end; linePtr += lineLen)
        {
            // normale output
            showMemoryLine(linePtr, lineLen, start);

            // skip repeated lines and show repetition count only
            int repeatCount = 0;
            while (linePtr + lineLen < end && memcmp(linePtr, linePtr + lineLen, lineLen) == 0)
            {
                repeatCount++;
                linePtr += lineLen;
            }
            if (repeatCount > 0)
            {
                openknx.logger.logWithPrefixAndValues("", "%ix (repetitions of previous line)", repeatCount);
            }
        }
        // incomplete last line (edge case)
        if (end != start + size)
        {
            showMemoryLine(end, (start + size) - end, start);
        }
        logEnd();
    }

    void Console::showMemoryLine(uint8_t* line, uint32_t length, uint8_t* memoryStart)
    {
        char prefix[24] = {};
        snprintf(prefix, 24, "0x%06X (0x%08X)", (uint)(line - memoryStart), (uint)line);
        openknx.logger.logHexWithPrefix(prefix, line, length);
    }

    void Console::printHelpLine(const char* command, const char* message)
    {
        // TODO Beautify
        openknx.logger.logWithPrefix(command, message);
    }

    void Console::erase(EraseMode mode)
    {
        openknx.watchdog.deactivate();

        openknx.progLed.blinking();
#ifdef INFO1_LED_PIN
        openknx.info1Led.off();
#endif
#ifdef INFO2_LED_PIN
        openknx.info2Led.off();
#endif
#ifdef INFO3_LED_PIN
        openknx.info3Led.off();
#endif

        if (mode == EraseMode::All || mode == EraseMode::KnxFlash)
        {
            openknx.logger.logWithPrefix("Erase", "KNX parameters");
            openknx.knxFlash.erase();
        }

        if (mode == EraseMode::All || mode == EraseMode::OpenKnxFlash)
        {
            openknx.logger.logWithPrefix("Erase", "OpenKNX save data");
            openknx.openknxFlash.erase();
        }

#if OPENKNX_LITTLE_FS
        if (mode == EraseMode::All || mode == EraseMode::Filesystem)
        {
            openknx.logger.logWithPrefix("Erase", "Format Filesystem");
            if (LittleFS.format())
            {
                openknx.logger.logWithPrefix("Erase", "Succesful");
            }
        }
#endif
#ifdef ARDUINO_ARCH_RP2040
        if (mode == EraseMode::All)
        {
            openknx.logger.logWithPrefix("Erase", "First bytes of Firmware");
            if (!__nukeFlash(0, 4096))
                openknx.logger.log("Fatal: nuke paramters invalid");
        }
#endif // ARDUINO_ARCH_RP2040

        openknx.progLed.forceOn();
        openknx.logger.logWithPrefix("Erase", "Completed");
        delay(1000);
        openknx.restart();
    }

#ifdef ARDUINO_ARCH_RP2040
    void Console::resetToBootloader()
    {
        reset_usb_boot(0, 0);
    }
#endif // ARDUINO_ARCH_RP2040

#ifndef ARDUINO_ARCH_SAMD
    void Console::processPinCommand(const std::string& cmd)
    {
        auto _pos = cmd.find(' ');
        if (_pos != std::string::npos)
        {
            uint16_t pin;
            if (cmd.length() <= 7)
                pin = std::stoi(cmd.substr(_pos + 1));
            else
                pin = std::stoi(cmd.substr(_pos + 1), nullptr, 16);

            if (cmd.compare(0, 2, "dw") == 0 || cmd.compare(0, 2, "aw") == 0)
            {
                auto __pos = cmd.find(' ', _pos + 1);
                if (__pos != std::string::npos)
                {
                    int value = std::stoi(cmd.substr(__pos + 1));
                    if (cmd.compare(0, 2, "dw") == 0 && value <= HIGH)
                    {
                        openknx.gpio.digitalWrite((pin_size_t)pin, value);
                        openknx.logger.logWithPrefixAndValues("PinCommand", "Write pin %i to %i", pin, value);
                    }
                    else if (cmd.compare(0, 2, "aw") == 0 && value <= 4095)
                    {
                        analogWrite((pin_size_t)pin, value);
                        openknx.logger.logWithPrefixAndValues("PinCommand", "Write pin %i to %i", pin, value);
                    }
                }
            }
            else if (cmd.compare(0, 2, "dr") == 0)
            {
                openknx.logger.logWithPrefixAndValues("PinCommand", "Read pin %i: %i", pin, openknx.gpio.digitalRead((openknx_gpio_number_t)pin));
            }
            else if (cmd.compare(0, 2, "ar") == 0)
            {
                openknx.logger.logWithPrefixAndValues("PinCommand", "Read pin %i: %i", pin, analogRead((pin_size_t)pin));
            }
        }
    } // processPinCommand
#endif // ARDUINO_ARCH_SAMD

    void Console::processI2cCommand(const std::string& cmd)
    {
        std::string args = (cmd.length() > 4) ? cmd.substr(4) : "";

        // Trim
        auto trimStart = args.find_first_not_of(" ");
        if (trimStart != std::string::npos)
        {
            auto trimEnd = args.find_last_not_of(" ");
            args = args.substr(trimStart, trimEnd - trimStart + 1);
        }
        else
            args = "";

        // Show help if empty or "?"
        if (args.empty() || args == "?")
        {
            openknx.logger.begin();
            openknx.logger.log("");
            openknx.logger.color(CONSOLE_HEADLINE_COLOR);
            openknx.logger.log("================================ Help: I2C Control =============================="); // 88 characters
            openknx.logger.color(0);
            openknx.logger.log("Command(s)               Description");
            openknx.console.printHelpLine("i2c status", "Show all configured I2C buses");
            openknx.console.printHelpLine("i2c scan [bus]", "Scan bus for devices (all or specific)");
            openknx.console.printHelpLine("i2c info <bus>", "Show detailed bus information");
#ifdef OPENKNX_I2C_BENCHMARK
            openknx.console.printHelpLine("i2c bench <type> [bus] [addr]", "Run benchmarks");
#endif
            openknx.logger.log("");
            openknx.logger.color(CONSOLE_HEADLINE_COLOR);
            openknx.logger.log("Available Buses:");
            openknx.logger.color(0);
#ifdef OPENKNX_WIRE_PIO
            openknx.console.printHelpLine("  wire_pio", "WIRE_PIO (PIO I2C)");
#endif
#ifdef OPENKNX_WIRE1_PIO
            openknx.console.printHelpLine("  wire1_pio", "WIRE1_PIO (PIO I2C)");
#endif
#ifdef OPENKNX_WIRE
            openknx.console.printHelpLine("  wire", "WIRE (Hardware I2C)");
#endif
#ifdef OPENKNX_WIRE1
            openknx.console.printHelpLine("  wire1", "WIRE1 (Hardware I2C)");
#endif
            openknx.console.printHelpLine("  all", "All configured buses");

#ifdef OPENKNX_I2C_BENCHMARK
            openknx.logger.log("");
            openknx.logger.color(CONSOLE_HEADLINE_COLOR);
            openknx.logger.log("Benchmark Types:");
            openknx.logger.color(0);
            openknx.console.printHelpLine("  quick", "Quick performance test (throughput + latency)");
            openknx.console.printHelpLine("  full", "Full benchmark suite (all tests)");
            openknx.console.printHelpLine("  speed", "Speed comparison at different frequencies");
            openknx.console.printHelpLine("  stability", "Error rate testing");
            openknx.console.printHelpLine("  display", "Display update simulation");
            openknx.console.printHelpLine("  restart", "Repeated start test");
    #if defined(OPENKNX_WIRE_PIO) && defined(OPENKNX_WIRE)
            openknx.console.printHelpLine("  compare", "PIO vs Hardware comparison");
    #endif
#endif

            openknx.logger.log("");
            openknx.logger.color(CONSOLE_HEADLINE_COLOR);
            openknx.logger.log("Examples:");
            openknx.logger.color(0);
            openknx.console.printHelpLine("  i2c status", "Show all I2C buses configuration");
            openknx.console.printHelpLine("  i2c scan wire_pio", "Scan WIRE_PIO for devices");
            openknx.console.printHelpLine("  i2c scan all", "Scan all buses");
            openknx.console.printHelpLine("  i2c info wire", "Show WIRE bus details");
#ifdef OPENKNX_I2C_BENCHMARK
            openknx.console.printHelpLine("  i2c bench quick wire_pio", "Quick benchmark on WIRE_PIO");
            openknx.console.printHelpLine("  i2c bench full all", "Full benchmark on all buses");
            openknx.console.printHelpLine("  i2c bench speed wire 0x3C", "Speed test on WIRE at address 0x3C");
    #if defined(OPENKNX_WIRE_PIO) && defined(OPENKNX_WIRE)
            openknx.console.printHelpLine("  i2c bench compare", "Compare PIO vs Hardware");
    #endif
#endif
            openknx.logger.color(CONSOLE_HEADLINE_COLOR);
            openknx.logger.log("---------------------------------------------------------------------------------"); // 88 characters
            openknx.logger.log(" WARNING: Running these commands will interfere with any ongoing I2C operations!");
            openknx.logger.log(" !! DO NOT !! use on a production system. Those are for testing purposes only.");
            openknx.logger.log("---------------------------------------------------------------------------------"); // 88 characters
            openknx.logger.color(0);
            openknx.logger.end();
            return;
        }

        // Split args into tokens
        std::vector<std::string> tokens;
        size_t pos = 0;
        while (pos < args.length())
        {
            while (pos < args.length() && args[pos] == ' ')
                pos++;
            if (pos >= args.length()) break;

            size_t tokenStart = pos;
            while (pos < args.length() && args[pos] != ' ')
                pos++;
            tokens.push_back(args.substr(tokenStart, pos - tokenStart));
        }

        if (tokens.size() == 0)
        {
            openknx.logger.logWithPrefix("i2c", "Error: Missing command");
            openknx.logger.logWithPrefix("i2c", "Type 'i2c ?' for help");
            return;
        }

        std::string command = tokens[0];

        // ========================================
        // STATUS COMMAND
        // ========================================
        if (command == "status")
        {
            openknx.logger.begin();
            openknx.logger.log("");
            openknx.logger.color(CONSOLE_HEADLINE_COLOR);
            openknx.logger.log("========================================");
            openknx.logger.log("  I2C Bus Configuration");
            openknx.logger.log("========================================");
            openknx.logger.color(0);

            int busCount = 0;

#ifdef OPENKNX_WIRE_PIO
            busCount++;
            openknx.logger.log("");
            openknx.logger.logWithValues("[%d] WIRE_PIO (PIO I2C)", busCount);
            openknx.logger.logWithValues("    PIO:    PIO%d", WIRE_PIO.getPioIndex());
            openknx.logger.logWithValues("    SM:     %d", WIRE_PIO.getStateMachineNumber());
            openknx.logger.logWithValues("    Offset: 0x%02X", WIRE_PIO.getOffset());
            openknx.logger.logWithValues("    SDA:    GPIO %d", OPENKNX_WIRE_PIO_SDA_PIN);
            openknx.logger.logWithValues("    SCL:    GPIO %d", OPENKNX_WIRE_PIO_SCL_PIN);
            openknx.logger.logWithValues("    Clock:  %d kHz", OPENKNX_WIRE_PIO_CLOCK / 1000);
#endif

#ifdef OPENKNX_WIRE1_PIO
            busCount++;
            openknx.logger.log("");
            openknx.logger.logWithValues("[%d] WIRE1_PIO (PIO I2C)", busCount);
            openknx.logger.logWithValues("    PIO:    PIO%d", WIRE1_PIO.getPioIndex());
            openknx.logger.logWithValues("    SM:     %d", WIRE1_PIO.getStateMachineNumber());
            openknx.logger.logWithValues("    Offset: 0x%02X", WIRE1_PIO.getOffset());
            openknx.logger.logWithValues("    SDA:    GPIO %d", OPENKNX_WIRE1_PIO_SDA_PIN);
            openknx.logger.logWithValues("    SCL:    GPIO %d", OPENKNX_WIRE1_PIO_SCL_PIN);
            openknx.logger.logWithValues("    Clock:  %d kHz", OPENKNX_WIRE1_PIO_CLOCK / 1000);
#endif

#ifdef OPENKNX_WIRE
            busCount++;
            openknx.logger.log("");
            openknx.logger.logWithValues("[%d] WIRE (Hardware I2C)", busCount);
            openknx.logger.logWithValues("    Type:   Hardware I2C0");
            openknx.logger.logWithValues("    SDA:    GPIO %d", OPENKNX_WIRE_SDA_PIN);
            openknx.logger.logWithValues("    SCL:    GPIO %d", OPENKNX_WIRE_SCL_PIN);
            openknx.logger.logWithValues("    Clock:  %d kHz", OPENKNX_WIRE_CLOCK / 1000);
#endif

#ifdef OPENKNX_WIRE1
            busCount++;
            openknx.logger.log("");
            openknx.logger.logWithValues("[%d] WIRE1 (Hardware I2C)", busCount);
            openknx.logger.logWithValues("    Type:   Hardware I2C1");
            openknx.logger.logWithValues("    SDA:    GPIO %d", OPENKNX_WIRE1_SDA_PIN);
            openknx.logger.logWithValues("    SCL:    GPIO %d", OPENKNX_WIRE1_SCL_PIN);
            openknx.logger.logWithValues("    Clock:  %d kHz", OPENKNX_WIRE1_CLOCK / 1000);
#endif

            openknx.logger.log("");
            openknx.logger.logWithValues("Total buses: %d", busCount);
            openknx.logger.color(CONSOLE_HEADLINE_COLOR);
            openknx.logger.log("========================================");
            openknx.logger.color(0);
            openknx.logger.end();
            return;
        }

#ifdef OPENKNX_I2C_BENCHMARK
        OpenKNX::I2C::Benchmark bench;
#endif
        // ========================================
        // SCAN COMMAND
        // ========================================
        if (command == "scan")
        {
            std::string busName = tokens.size() > 1 ? tokens[1] : "all";

            auto scanBus = [this](TwoWire& wire, const char* name) {
                openknx.logger.begin();
                openknx.logger.logWithValues("Scanning %s...", name);
                openknx.logger.log("");
                openknx.logger.log("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");

                int found = 0;
                char line[100];

                for (uint8_t addr = 0; addr < 0x80; addr++)
                {
                    if (addr % 16 == 0)
                    {
                        snprintf(line, sizeof(line), "%02X: ", addr);
                    }

                    // Skip reserved
                    if ((addr & 0x78) == 0 || (addr & 0x78) == 0x78)
                    {
                        strncat(line, " . ", sizeof(line) - strlen(line) - 1);
                    }
                    else
                    {
                        wire.beginTransmission(addr);
                        uint8_t error = wire.endTransmission(false);

                        char temp[5];
                        if (error == 0)
                        {
                            snprintf(temp, sizeof(temp), " %02X", addr);
                            found++;
                        }
                        else
                        {
                            snprintf(temp, sizeof(temp), " --");
                        }
                        strncat(line, temp, sizeof(line) - strlen(line) - 1);
                    }

                    if (addr % 16 == 15)
                    {
                        openknx.logger.log(line);
                        line[0] = '\0'; // Reset line
                    }
                }

                openknx.logger.log("");
                openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                openknx.logger.logWithValues("Found %d device(s) on %s", found, name);
                openknx.logger.color(0);
                openknx.logger.log("");
                openknx.logger.end();
            };

            if (busName == "all")
            {
                openknx.logger.begin();
                openknx.logger.log("");
                openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                openknx.logger.log("========================================");
                openknx.logger.log("  Scanning all I2C buses");
                openknx.logger.log("========================================");
                openknx.logger.color(0);
                

#ifdef OPENKNX_WIRE_PIO
                //bench.scanBusDetailed(WIRE_PIO, "WIRE_PIO");
                scanBus(WIRE_PIO, "WIRE_PIO");
#endif
#ifdef OPENKNX_WIRE1_PIO
                //bench.scanBusDetailed(WIRE1_PIO, "WIRE1_PIO");
                scanBus(WIRE1_PIO, "WIRE1_PIO");
#endif
#ifdef OPENKNX_WIRE
                //bench.scanBusDetailed(WIRE, "WIRE");
                scanBus(WIRE, "WIRE");
#endif
#ifdef OPENKNX_WIRE1
                //bench.scanBusDetailed(WIRE1, "WIRE1");
                scanBus(WIRE1, "WIRE1");
#endif
            }
            else
            {
#ifdef OPENKNX_WIRE_PIO
                if (busName == "wire_pio")
                {
                    scanBus(WIRE_PIO, "WIRE_PIO");
                    return;
                }
#endif
#ifdef OPENKNX_WIRE1_PIO
                if (busName == "wire1_pio")
                {
                    scanBus(WIRE1_PIO, "WIRE1_PIO");
                    return;
                }
#endif
#ifdef OPENKNX_WIRE
                if (busName == "wire")
                {
                    scanBus(WIRE, "WIRE");
                    return;
                }
#endif
#ifdef OPENKNX_WIRE1
                if (busName == "wire1")
                {
                    scanBus(WIRE1, "WIRE1");
                    return;
                }
#endif

                openknx.logger.logWithValues("Error: Unknown bus '%s'", busName.c_str());
                openknx.logger.log("Available: all, wire_pio, wire1_pio, wire, wire1");
                openknx.logger.end();
            }
            return;
        }

        // ========================================
        // INFO COMMAND
        // ========================================
        if (command == "info")
        {
            if (tokens.size() < 2)
            {
                openknx.logger.logWithPrefix("i2c", "Error: Missing bus name");
                openknx.logger.logWithPrefix("i2c", "Usage: i2c info <bus>");
                return;
            }

            std::string busName = tokens[1];

#ifdef OPENKNX_WIRE_PIO
            if (busName == "wire_pio")
            {
                openknx.logger.begin();
                openknx.logger.log("");
                openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                openknx.logger.log("========================================");
                openknx.logger.log("  WIRE_PIO Details");
                openknx.logger.log("========================================");
                openknx.logger.color(0);
                openknx.logger.log("Type:      PIO I2C");
                openknx.logger.logWithValues("PIO:       PIO%d", WIRE_PIO.getPioIndex());
                openknx.logger.logWithValues("SM:        %d", WIRE_PIO.getStateMachineNumber());
                openknx.logger.logWithValues("Offset:    0x%02X", WIRE_PIO.getOffset());
                openknx.logger.logWithValues("SDA:       GPIO %d", OPENKNX_WIRE_PIO_SDA_PIN);
                openknx.logger.logWithValues("SCL:       GPIO %d", OPENKNX_WIRE_PIO_SCL_PIN);
                openknx.logger.logWithValues("Clock:     %d Hz", OPENKNX_WIRE_PIO_CLOCK);
                openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                openknx.logger.log("========================================");
                openknx.logger.color(0);
                openknx.logger.end();
                return;
            }
#endif

#ifdef OPENKNX_WIRE1_PIO
            if (busName == "wire1_pio")
            {
                openknx.logger.begin();
                openknx.logger.log("");
                openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                openknx.logger.log("========================================");
                openknx.logger.log("  WIRE1_PIO Details");
                openknx.logger.log("========================================");
                openknx.logger.color(0);
                openknx.logger.logWithPrefix("i2c", "Type:      PIO I2C");
                openknx.logger.logWithPrefixAndValues("i2c", "PIO:       PIO%d", WIRE1_PIO.getPioIndex());
                openknx.logger.logWithPrefixAndValues("i2c", "SM:        %d", WIRE1_PIO.getStateMachineNumber());
                openknx.logger.logWithPrefixAndValues("i2c", "Offset:    0x%02X", WIRE1_PIO.getOffset());
                openknx.logger.logWithPrefixAndValues("i2c", "SDA:       GPIO %d", OPENKNX_WIRE1_PIO_SDA_PIN);
                openknx.logger.logWithPrefixAndValues("i2c", "SCL:       GPIO %d", OPENKNX_WIRE1_PIO_SCL_PIN);
                openknx.logger.logWithPrefixAndValues("i2c", "Clock:     %d Hz", OPENKNX_WIRE1_PIO_CLOCK);
                openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                openknx.logger.log("========================================");
                openknx.logger.color(0);
                openknx.logger.end();
                return;
            }
#endif

#ifdef OPENKNX_WIRE
            if (busName == "wire")
            {
                openknx.logger.begin();
                openknx.logger.log("");
                openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                openknx.logger.log("========================================");
                openknx.logger.log("  WIRE Details");
                openknx.logger.log("========================================");
                openknx.logger.color(0);
                openknx.logger.log("Type:      Hardware I2C0");
                openknx.logger.logWithValues("SDA:       GPIO %d", OPENKNX_WIRE_SDA_PIN);
                openknx.logger.logWithValues("SCL:       GPIO %d", OPENKNX_WIRE_SCL_PIN);
                openknx.logger.logWithValues("Clock:     %d Hz", OPENKNX_WIRE_CLOCK);
                openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                openknx.logger.log("========================================");
                openknx.logger.color(0);
                openknx.logger.end();
                return;
            }
#endif

#ifdef OPENKNX_WIRE1
            if (busName == "wire1")
            {
                openknx.logger.begin();
                openknx.logger.log("");
                openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                openknx.logger.log("========================================");
                openknx.logger.log("  WIRE1 Details");
                openknx.logger.log("========================================");
                openknx.logger.color(0);
                openknx.logger.log("Type:      Hardware I2C1");
                openknx.logger.logWithValues("SDA:       GPIO %d", OPENKNX_WIRE1_SDA_PIN);
                openknx.logger.logWithValues("SCL:       GPIO %d", OPENKNX_WIRE1_SCL_PIN);
                openknx.logger.logWithValues("Clock:     %d Hz", OPENKNX_WIRE1_CLOCK);
                openknx.logger.color(CONSOLE_HEADLINE_COLOR);
                openknx.logger.log("========================================");
                openknx.logger.color(0);
                openknx.logger.end();
                return;
            }
#endif

            openknx.logger.logWithPrefixAndValues("i2c", "Error: Unknown bus '%s'", busName.c_str());
            return;
        }

#ifdef OPENKNX_I2C_BENCHMARK
        // ========================================
        // BENCH COMMAND
        // ========================================
        if (command == "bench")
        {
            if (tokens.size() < 2)
            {
                openknx.logger.logWithPrefix("i2c", "Error: Missing benchmark type");
                openknx.logger.logWithPrefix("i2c", "Usage: i2c bench <type> [bus] [addr]");
                openknx.logger.logWithPrefix("i2c", "Types: quick, full, speed, stability, display, restart, compare");
                return;
            }

            std::string benchType = tokens[1];
            std::string busName = tokens.size() > 2 ? tokens[2] : "all";
            uint8_t addr = 0x3C; // Default

            if (tokens.size() > 3)
            {
                addr = (uint8_t)strtol(tokens[3].c_str(), NULL, 0);
            }

            // Quick benchmark
            if (benchType == "quick")
            {
                auto quickBench = [addr, &bench](TwoWire& wire, const char* name) {
                    openknx.logger.logWithPrefixAndValues("i2c", "Quick benchmark: %s (addr 0x%02X)", name, addr);

                    bench.benchmarkThroughput(wire, name, addr, 128, 100);
                    bench.benchmarkLatency(wire, name, addr);
                };

                if (busName == "all")
                {
    #ifdef OPENKNX_WIRE_PIO
                    quickBench(WIRE_PIO, "WIRE_PIO");
    #endif
    #ifdef OPENKNX_WIRE1_PIO
                    quickBench(WIRE1_PIO, "WIRE1_PIO");
    #endif
    #ifdef OPENKNX_WIRE
                    quickBench(WIRE, "WIRE");
    #endif
    #ifdef OPENKNX_WIRE1
                    quickBench(WIRE1, "WIRE1");
    #endif
                }
                else
                {
    #ifdef OPENKNX_WIRE_PIO
                    if (busName == "wire_pio")
                    {
                        quickBench(WIRE_PIO, "WIRE_PIO");
                        return;
                    }
    #endif
    #ifdef OPENKNX_WIRE1_PIO
                    if (busName == "wire1_pio")
                    {
                        quickBench(WIRE1_PIO, "WIRE1_PIO");
                        return;
                    }
    #endif
    #ifdef OPENKNX_WIRE
                    if (busName == "wire")
                    {
                        quickBench(WIRE, "WIRE");
                        return;
                    }
    #endif
    #ifdef OPENKNX_WIRE1
                    if (busName == "wire1")
                    {
                        quickBench(WIRE1, "WIRE1");
                        return;
                    }
    #endif
                    openknx.logger.logWithPrefixAndValues("i2c", "Error: Unknown bus '%s'", busName.c_str());
                }
                return;
            }

            // Full benchmark
            if (benchType == "full")
            {
                if (busName == "all")
                {
                    openknx.logger.logWithPrefix("i2c", "Running full benchmark on all buses...");
                    openknx.logger.logWithPrefix("i2c", "This will take several minutes!");
                    bench.runAllBenchmarks();
                }
                else
                {
    #ifdef OPENKNX_WIRE_PIO
                    if (busName == "wire_pio")
                    {
                        bench.runBenchmark(WIRE_PIO, "WIRE_PIO", addr);
                        return;
                    }
    #endif
    #ifdef OPENKNX_WIRE1_PIO
                    if (busName == "wire1_pio")
                    {
                        bench.runBenchmark(WIRE1_PIO, "WIRE1_PIO", addr);
                        return;
                    }
    #endif
    #ifdef OPENKNX_WIRE
                    if (busName == "wire")
                    {
                        bench.runBenchmark(WIRE, "WIRE", addr);
                        return;
                    }
    #endif
    #ifdef OPENKNX_WIRE1
                    if (busName == "wire1")
                    {
                        bench.runBenchmark(WIRE1, "WIRE1", addr);
                        return;
                    }
    #endif
                    openknx.logger.logWithPrefixAndValues("i2c", "Error: Unknown bus '%s'", busName.c_str());
                }
                return;
            }

            // Speed comparison
            if (benchType == "speed")
            {
    #ifdef OPENKNX_WIRE_PIO
                if (busName == "wire_pio")
                {
                    bench.benchmarkSpeedComparison(WIRE_PIO, "WIRE_PIO", addr);
                    return;
                }
    #endif
    #ifdef OPENKNX_WIRE1_PIO
                if (busName == "wire1_pio")
                {
                    bench.benchmarkSpeedComparison(WIRE1_PIO, "WIRE1_PIO", addr);
                    return;
                }
    #endif
    #ifdef OPENKNX_WIRE
                if (busName == "wire")
                {
                    bench.benchmarkSpeedComparison(WIRE, "WIRE", addr);
                    return;
                }
    #endif
    #ifdef OPENKNX_WIRE1
                if (busName == "wire1")
                {
                    bench.benchmarkSpeedComparison(WIRE1, "WIRE1", addr);
                    return;
                }
    #endif
                openknx.logger.logWithPrefixAndValues("i2c", "Error: Unknown bus '%s'", busName.c_str());
                return;
            }

            // Stability
            if (benchType == "stability")
            {
    #ifdef OPENKNX_WIRE_PIO
                if (busName == "wire_pio")
                {
                    bench.benchmarkStability(WIRE_PIO, "WIRE_PIO", addr);
                    return;
                }
    #endif
    #ifdef OPENKNX_WIRE1_PIO
                if (busName == "wire1_pio")
                {
                    bench.benchmarkStability(WIRE1_PIO, "WIRE1_PIO", addr);
                    return;
                }
    #endif
    #ifdef OPENKNX_WIRE
                if (busName == "wire")
                {
                    bench.benchmarkStability(WIRE, "WIRE", addr);
                    return;
                }
    #endif
    #ifdef OPENKNX_WIRE1
                if (busName == "wire1")
                {
                    bench.benchmarkStability(WIRE1, "WIRE1", addr);
                    return;
                }
    #endif
                openknx.logger.logWithPrefixAndValues("i2c", "Error: Unknown bus '%s'", busName.c_str());
                return;
            }

            // Display update
            if (benchType == "display")
            {
    #ifdef OPENKNX_WIRE_PIO
                if (busName == "wire_pio")
                {
                    bench.benchmarkDisplayUpdate(WIRE_PIO, "WIRE_PIO", addr);
                    return;
                }
    #endif
    #ifdef OPENKNX_WIRE1_PIO
                if (busName == "wire1_pio")
                {
                    bench.benchmarkDisplayUpdate(WIRE1_PIO, "WIRE1_PIO", addr);
                    return;
                }
    #endif
    #ifdef OPENKNX_WIRE
                if (busName == "wire")
                {
                    bench.benchmarkDisplayUpdate(WIRE, "WIRE", addr);
                    return;
                }
    #endif
    #ifdef OPENKNX_WIRE1
                if (busName == "wire1")
                {
                    bench.benchmarkDisplayUpdate(WIRE1, "WIRE1", addr);
                    return;
                }
    #endif
                openknx.logger.logWithPrefixAndValues("i2c", "Error: Unknown bus '%s'", busName.c_str());
                return;
            }

            // Repeated start
            if (benchType == "restart")
            {
    #ifdef OPENKNX_WIRE_PIO
                if (busName == "wire_pio")
                {
                    bench.testRepeatedStart(WIRE_PIO, "WIRE_PIO", addr);
                    return;
                }
    #endif
    #ifdef OPENKNX_WIRE1_PIO
                if (busName == "wire1_pio")
                {
                    bench.testRepeatedStart(WIRE1_PIO, "WIRE1_PIO", addr);
                    return;
                }
    #endif
    #ifdef OPENKNX_WIRE
                if (busName == "wire")
                {
                    bench.testRepeatedStart(WIRE, "WIRE", addr);
                    return;
                }
    #endif
    #ifdef OPENKNX_WIRE1
                if (busName == "wire1")
                {
                    bench.testRepeatedStart(WIRE1, "WIRE1", addr);
                    return;
                }
    #endif
                openknx.logger.logWithPrefixAndValues("i2c", "Error: Unknown bus '%s'", busName.c_str());
                return;
            }

    #if defined(OPENKNX_WIRE_PIO) && defined(OPENKNX_WIRE)
            // Compare PIO vs Hardware
            if (benchType == "compare")
            {
                openknx.logger.logWithPrefix("i2c", "Comparing PIO vs Hardware...");
                bench.comparePIOvsHardware(addr);
                return;
            }
    #endif

            openknx.logger.logWithPrefixAndValues("i2c", "Error: Unknown benchmark type '%s'", benchType.c_str());
            openknx.logger.logWithPrefix("i2c", "Available: quick, full, speed, stability, display, restart, compare");
            return;
        }
#endif // OPENKNX_I2C_BENCHMARK

        openknx.logger.logWithPrefixAndValues("i2c", "Error: Unknown command '%s'", command.c_str());
        openknx.logger.logWithPrefix("i2c", "Type 'i2c ?' for help");
    } // processI2cCommand
} // namespace OpenKNX