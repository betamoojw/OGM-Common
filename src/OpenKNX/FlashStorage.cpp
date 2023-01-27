#include "OpenKNX/FlashStorage.h"
#include "OpenKNX/Common.h"

namespace OpenKNX
{
    FlashStorage::FlashStorage()
    {
    }

    void FlashStorage::load()
    {
        _flashSize = knx.platform().getNonVolatileMemorySize();
        _flashStart = knx.platform().getNonVolatileMemoryStart();
        uint32_t start = millis();
        loadedModules = new bool[openknx.getModules()->count];
        openknx.log("FlashStorage", "load");
        readData();
        initUnloadedModules();
        openknx.log("FlashStorage", "  complete (%i)", millis() - start);
    }

    void FlashStorage::initUnloadedModules()
    {
        Modules *modules = openknx.getModules();
        Module *module = nullptr;
        uint8_t moduleId = 0;
        uint16_t moduleSize = 0;
        for (uint8_t i = 1; i <= modules->count; i++)
        {
            // get data
            module = modules->list[i - 1];
            moduleId = modules->ids[i - 1];
            moduleSize = module->flashSize();

            if (moduleSize == 0)
                return;

            if (!loadedModules[moduleId])
            {
                openknx.log("FlashStorage", "  init module %s (%i)", module->name(), moduleId);
                module->readFlash(new uint8_t[0], 0);
            }
        }
    }

    void FlashStorage::readData()
    {
        uint8_t *currentPosition;
        uint8_t moduleId = 0;
        uint16_t moduleSize = 0;
        uint16_t dataSize = 0;
        uint16_t dataProcessed = 0;
        Module *module = nullptr;

        // check magicwords exists
        currentPosition = _flashStart + _flashSize - FLASH_DATA_META_LEN;
        if (FLASH_DATA_INIT != getInt(currentPosition + FLASH_DATA_META_LEN - FLASH_DATA_INIT_LEN))
        {
            openknx.log("FlashStorage", "   - Abort: No data found");
            return;
        }

        // read size
        dataSize = (currentPosition[FLASH_DATA_META_LEN - 8] << 8) + currentPosition[FLASH_DATA_META_LEN - 7];

        // read FirmwareVersion
        _lastOpenKnxId = currentPosition[0];
        _lastApplicationNumber = currentPosition[1];
        openknx.log("FlashStorage", "  ApplicationNumber: %02x", _lastApplicationNumber);
        _lastApplicationVersion = getWord(currentPosition + 2);
        openknx.log("FlashStorage", "  ApplicationVersion: %i", _lastApplicationVersion);

        // check
        currentPosition = (currentPosition - dataSize);
        if (!verifyChecksum(currentPosition, dataSize + FLASH_DATA_META_LEN - FLASH_DATA_INIT_LEN))
        {
            openknx.log("FlashStorage", "   - Abort: Checksum invalid!");
            openknx.logHex("FlashStorage", currentPosition, dataSize + FLASH_DATA_META_LEN - FLASH_DATA_INIT_LEN);
            return;
        }

        // check apliicationNumber
        if (_lastOpenKnxId != openknx.openKnxId() || _lastApplicationNumber != openknx.applicationNumber())
        {
            openknx.log("FlashStorage", "  - Abort: Data from other application");
            return;
        }

#ifdef FLASH_DATA_TRACE
        openknx.logHex("FlashStorage", currentPosition, dataSize + FLASH_DATA_META_LEN);
#endif

        while (dataProcessed < dataSize)
        {
            moduleId = currentPosition[0];
            moduleSize = getWord(currentPosition + 1);
            currentPosition = (currentPosition + FLASH_DATA_MODULE_ID_LEN + FLASH_DATA_SIZE_LEN);
            dataProcessed += FLASH_DATA_MODULE_ID_LEN + FLASH_DATA_SIZE_LEN + moduleSize;
            module = openknx.getModule(moduleId);
            if (module == nullptr)
            {
                openknx.log("FlashStorage", "  skip module with id %i (not found)", moduleId);
            }
            else
            {
                openknx.log("FlashStorage", "  restore module %s (%i) with %i bytes", module->name(), moduleId, moduleSize);
                _currentReadAddress = currentPosition;
#ifdef FLASH_DATA_TRACE
                openknx.logHex("FlashStorage", currentPosition, moduleSize);
#endif
                module->readFlash(currentPosition, moduleSize);
                loadedModules[moduleId] = true;
            }
            currentPosition = (currentPosition + moduleSize);
        }
    }

    void FlashStorage::save(bool force /* = false */)
    {
        _checksum = 0;
        _flashSize = knx.platform().getNonVolatileMemorySize();
        _flashStart = knx.platform().getNonVolatileMemoryStart();

        uint32_t start = millis();
        uint8_t moduleId = 0;
        uint16_t dataSize = 0;
        uint16_t moduleSize = 0;
        Module *module = nullptr;

        // table is not loaded (ets prog running) and save is not possible
        if (!knx.configured())
            return;

        // we have to ensure, that save is not called too often, because flash memory
        // does not survive too many writes
        if (!force && _lastWrite > 0 && !delayCheck(_lastWrite, FLASH_DATA_WRITE_LIMIT))
            return;

        openknx.log("FlashStorage", "save <%i>", force);

        // determine some values
        Modules *modules = openknx.getModules();
        dataSize = 0;
        for (uint8_t i = 1; i <= modules->count; i++)
        {
            moduleSize = modules->list[i - 1]->flashSize();
            if (moduleSize == 0)
                continue;

            dataSize += moduleSize +
                        FLASH_DATA_MODULE_ID_LEN +
                        FLASH_DATA_SIZE_LEN;
        }

#ifdef FLASH_DATA_TRACE
        openknx.log("FlashStorage", "  dataSize: %i", dataSize);
#endif

        // start point
        _currentWriteAddress = _flashSize -
                               dataSize -
                               FLASH_DATA_META_LEN;

#ifdef FLASH_DATA_TRACE
        openknx.log("FlashStorage", "  startPosition: %i", _currentWriteAddress);
#endif

        for (uint8_t i = 1; i <= modules->count; i++)
        {
            // get data
            module = modules->list[i - 1];
            moduleSize = module->flashSize();
            moduleId = modules->ids[i - 1];

            if (moduleSize == 0)
                continue;

            // write data
            _maxWriteAddress = _currentWriteAddress +
                               FLASH_DATA_MODULE_ID_LEN +
                               FLASH_DATA_SIZE_LEN;
            writeByte(moduleId);
            writeWord(moduleSize);

            _maxWriteAddress = _currentWriteAddress + moduleSize;

            openknx.log("FlashStorage", "  save module %s (%i) with %i bytes", module->name(), moduleId, moduleSize);
            module->writeFlash();
            zeroize();
        }

        // write magicword
        _maxWriteAddress = _currentWriteAddress + FLASH_DATA_META_LEN;

        // application info
        writeByte(openknx.openKnxId());
        writeByte(openknx.applicationNumber());
        writeWord(openknx.applicationVersion());

        // write size
        writeWord(dataSize);

        // write checksum
        writeWord(_checksum);

        // write init
        writeInt(FLASH_DATA_INIT);

        knx.platform().commitNonVolatileMemory();
#ifdef FLASH_DATA_TRACE
        openknx.logHex("FlashStorage", _flashStart + _flashSize - dataSize - FLASH_DATA_META_LEN, dataSize + FLASH_DATA_META_LEN);
#endif

        _lastWrite = millis();
        openknx.log("FlashStorage", "  complete (%i)", _lastWrite - start);
    }

    uint16_t FlashStorage::calcChecksum(uint16_t data)
    {
        return (data >> 8) + (data & 0xff);
    }

    uint16_t FlashStorage::calcChecksum(uint8_t *data, uint16_t size)
    {
        uint16_t sum = 0;

        for (uint16_t i = 0; i < size; i++)
            sum = sum + data[i];

        return sum;
    }

    bool FlashStorage::verifyChecksum(uint8_t *data, uint16_t size)
    {
        //openknx.log("FlashStorage", "verifyChecksum %i == %i", ((data[size - 2] << 8) + data[size - 1]), calcChecksum(data, size - 2));
        return ((data[size - 2] << 8) + data[size - 1]) == calcChecksum(data, size - 2);
    }

    void FlashStorage::write(uint8_t *buffer, uint16_t size)
    {
        if ((_currentWriteAddress + size) > _maxWriteAddress)
        {
            openknx.log("FlashStorage", "write not allow");
            return;
        }

        for (uint16_t i = 0; i < size; i++)
            _checksum += buffer[i];

        _currentWriteAddress = knx.platform().writeNonVolatileMemory(_currentWriteAddress, buffer, size);
    }

    void FlashStorage::write(uint8_t value, uint16_t size)
    {
        if ((_currentWriteAddress + size) > _maxWriteAddress)
        {
            openknx.log("FlashStorage", "write not allow");
            return;
        }

        for (uint16_t i = 0; i < size; i++)
            _checksum += value;

        _currentWriteAddress = knx.platform().writeNonVolatileMemory(_currentWriteAddress, value, size);
    }

    void FlashStorage::writeByte(uint8_t value)
    {
        uint8_t *buffer = new uint8_t[1];
        buffer[0] = value;
        write(buffer);
        delete buffer;
    }

    void FlashStorage::writeWord(uint16_t value)
    {
        uint8_t *buffer = new uint8_t[2];
        buffer[0] = ((value >> 8) & 0xff);
        buffer[1] = (value & 0xff);
        write(buffer, 2);
        delete buffer;
    }

    void FlashStorage::writeInt(uint32_t value)
    {
        uint8_t *buffer = new uint8_t[4];
        buffer[0] = ((value >> 24) & 0xff);
        buffer[1] = ((value >> 16) & 0xff);
        buffer[2] = ((value >> 8) & 0xff);
        buffer[3] = (value & 0xff);
        write(buffer, 4);
        delete buffer;
    }

    void FlashStorage::zeroize()
    {
        uint16_t fillSize = (_maxWriteAddress - _currentWriteAddress);
        if (fillSize == 0)
            return;

#ifdef FLASH_DATA_TRACE
        openknx.log("FlashStorage", "    zeroize %i", fillSize);
#endif
        write((uint8_t)0xFF, fillSize);
    }

    uint8_t *FlashStorage::read(uint16_t size /* = 1 */)
    {
        uint8_t *address = _currentReadAddress;
        _currentReadAddress += size;
        return address;
    }
    uint8_t FlashStorage::readByte()
    {
        return read(1)[0];
    }
    uint16_t FlashStorage::writeWord()
    {
        return 0;
    }
    uint32_t FlashStorage::readInt()
    {
        return 0;
    }

    uint16_t FlashStorage::applicationVersion()
    {
        return _lastApplicationVersion;
    }
} // namespace OpenKNX