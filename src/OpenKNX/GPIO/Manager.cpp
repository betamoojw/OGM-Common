#include "Manager.h"
#include "DriverEmbedded.h"
#include "DriverPCA9554.h"
#include "DriverPCA9557.h"
#include "DriverTCA6408.h"
#include "DriverTCA9555.h"
#include "OpenKNX.h"

namespace OpenKNX
{
    namespace GPIO
    {
#ifdef OPENKNX_GPIO_NUM
        const OPENKNX_GPIO_T GPIO_TYPES[OPENKNX_GPIO_NUM + 1] = {OPENKNX_GPIO_T_EMBEDDED, OPENKNX_GPIO_TYPES};
        const uint16_t GPIO_ADDRS[OPENKNX_GPIO_NUM + 1] = {0, OPENKNX_GPIO_ADDRS};
        const uint8_t GPIO_INTS[OPENKNX_GPIO_NUM + 1] = {0, OPENKNX_GPIO_INTS};

        std::vector<Manager::GpioQueueEntry> Manager::gpioQueue;

        void Manager::queueI2CAction(const Manager::GpioQueueEntry &entry)
        {
            noInterrupts();
            gpioQueue.push_back(entry);
            interrupts();
        }
#else
    #define OPENKNX_GPIO_NUM 0
        const OPENKNX_GPIO_T GPIO_TYPES[1] = {OPENKNX_GPIO_T_EMBEDDED};
#endif
        Base *GPIOExpanders[OPENKNX_GPIO_NUM + 1];

        Manager::Manager()
        {
        }

        Manager::~Manager()
        {
        }

        void Manager::init()
        {
#if OPENKNX_GPIO_NUM > 0
            OPENKNX_GPIO_WIRE.setSDA(OPENKNX_GPIO_SDA);
            OPENKNX_GPIO_WIRE.setSCL(OPENKNX_GPIO_SCL);
            OPENKNX_GPIO_WIRE.begin();
            OPENKNX_GPIO_WIRE.setClock(OPENKNX_GPIO_CLOCK);
#endif

            for (int i = 0; i < OPENKNX_GPIO_NUM + 1; i++)
            {
                switch (GPIO_TYPES[i])
                {
                    case OPENKNX_GPIO_T_EMBEDDED:
                    {
                        GPIOExpanders[i] = new DriverEmbedded();
                    }
                    break;
#if OPENKNX_GPIO_NUM > 0
                    case OPENKNX_GPIO_T_TCA9555:
                    {
                        GPIOExpanders[i] = new DriverTCA9555(GPIO_ADDRS[i], &OPENKNX_GPIO_WIRE);
                        GPIOExpanders[i]->init();
                    }
                    break;
                    case OPENKNX_GPIO_T_TCA6408:
                    {
                        GPIOExpanders[i] = new DriverTCA6408(GPIO_ADDRS[i], &OPENKNX_GPIO_WIRE, GPIO_INTS[i]);
                        GPIOExpanders[i]->init();
                    }
                    break;
                    case OPENKNX_GPIO_T_PCA9557:
                    {
                        GPIOExpanders[i] = new DriverPCA9557(GPIO_ADDRS[i], &OPENKNX_GPIO_WIRE);
                        GPIOExpanders[i]->init();
                    }
                    break;
                    case OPENKNX_GPIO_T_PCA9554:
                    {
                        GPIOExpanders[i] = new DriverPCA9554(GPIO_ADDRS[i], &OPENKNX_GPIO_WIRE);
                        GPIOExpanders[i]->init();
                    }
                    break;
                    default:
                        GPIOExpanders[i]->setInitState(GPIOInitState::TypeUnknown);
#endif
                }
            }
        }

        bool Manager::isInitialized(uint8_t expander)
        {
            return GPIOExpanders[expander]->getInitState() == GPIOInitState::OK;
        }

        void Manager::showInitResults()
        {
#if OPENKNX_GPIO_NUM > 0
            for (int i = 1; i < OPENKNX_GPIO_NUM + 1; i++) // skip embedded
            {
                switch (GPIOExpanders[i]->getInitState())
                {
                    case GPIOInitState::OK:
                        logInfoP("connected to GPIO Expander %u with address 0x%02x", i, GPIO_ADDRS[i]);
                        break;
                    case GPIOInitState::Failed:
                        logErrorP("no connection to GPIO Expander %u with address 0x%02X", i, GPIO_ADDRS[i]);
                        break;
                    case GPIOInitState::Uninitialized:
                        logErrorP("GPIO Expander %u with address 0x%02X not initialized", i, GPIO_ADDRS[i]);
                        break;
                    case GPIOInitState::TypeUnknown:
                        logErrorP("unknown GPIO Expander type for Expander %u with address 0x%02X", i, GPIO_ADDRS[i]);
                        break;
                    default:
                        logErrorP("unknown error for GPIO Expander %u with address 0x%02X", i, GPIO_ADDRS[i]);
                        break;
                }
            }
#endif // OPENKNX_GPIO_NUM > 0
        }

        void Manager::loop()
        {
#ifdef OPENKNX_GPIO_NUM
            noInterrupts();
            auto queueCopy = gpioQueue;
            gpioQueue.clear();
            interrupts();

            for (const auto &entry : queueCopy)
            {
                int8_t localpin = entry.pin & 0xff;
                uint8_t expander = entry.pin >> 8;

                switch (entry.type)
                {
                    case GpioActionType::DigitalWrite:
                        GPIOExpanders[expander]->GPIOdigitalWrite(localpin, entry.value);
                        break;
                    case GpioActionType::PinMode:
                        GPIOExpanders[expander]->GPIOpinMode(localpin, entry.value, entry.preset, entry.status);
                        break;
                    case GpioActionType::AttachInterrupt:
                        GPIOExpanders[expander]->GPIOattachInterrupt(localpin, entry.callback, entry.interruptMode);
                        break;
                }
            }
#endif
        }

        std::string Manager::logPrefix()
        {
            return openknx.logger.buildPrefix("GPIOHAL", 0);
        }

        void Manager::pinMode(openknx_gpio_number_t pin, int mode, bool preset, int status)
        {
            int8_t localpin = pin & 0xff;
            uint8_t expander = pin >> 8;
            if (expander > OPENKNX_GPIO_NUM)
            {
                logErrorP("GPIOModule::pinMode: invalid pin id %u", pin);
                return;
            }
            if (GPIO_TYPES[expander] == OPENKNX_GPIO_T_EMBEDDED)
            {
                GPIOExpanders[expander]->GPIOpinMode(localpin, mode, preset, status);
            }
            else
            {
                queueI2CAction({pin, GpioActionType::PinMode, mode, preset, status});
            }
        }

        void Manager::digitalWrite(openknx_gpio_number_t pin, int status)
        {
            int8_t localpin = pin & 0xff;
            uint8_t expander = pin >> 8;
            if (expander > OPENKNX_GPIO_NUM)
            {
                logErrorP("GPIOModule::digitalWrite: invalid pin id %u", pin);
                return;
            }
            if (GPIO_TYPES[expander] == OPENKNX_GPIO_T_EMBEDDED)
            {
                GPIOExpanders[expander]->GPIOdigitalWrite(localpin, status);
            }
            else
            {
                queueI2CAction({pin, GpioActionType::DigitalWrite, status});
            }
        }

        bool Manager::digitalRead(openknx_gpio_number_t pin)
        {
            int8_t localpin = pin & 0xff;
            uint8_t expander = pin >> 8;
            if (expander > OPENKNX_GPIO_NUM)
            {
                logErrorP("GPIOModule::digitalRead: invalid pin id %u", pin);
                return 0;
            }
            return GPIOExpanders[expander]->GPIOdigitalRead(localpin);
        }

        int Manager::analogRead(openknx_gpio_number_t pin)
        {
            int8_t localpin = pin & 0xff;
            uint8_t expander = pin >> 8;
            if (GPIO_TYPES[expander] != OPENKNX_GPIO_T_EMBEDDED)
            {
                logErrorP("GPIOModule::analogRead: Not supported for expanders");
                logErrorP("GPIOModule::analogRead: invalid pin id %u", pin);
                return -1;
            }
            return GPIOExpanders[expander]->GPIOanalogRead(localpin);
        }

        void Manager::analogWrite(openknx_gpio_number_t pin, int value)
        {
            int8_t localpin = pin & 0xff;
            uint8_t expander = pin >> 8;
            if (GPIO_TYPES[expander] != OPENKNX_GPIO_T_EMBEDDED)
            {
                logErrorP("GPIOModule::analogWrite: Not supported for expanders");
                logErrorP("GPIOModule::analogWrite: invalid pin id %u", pin);
                return;
            }
            GPIOExpanders[expander]->GPIOanalogWrite(localpin, value);
        }

        void Manager::attachInterrupt(openknx_gpio_number_t pin, std::function<void(openknx_gpio_number_t, bool)> callback, PinStatus mode)
        {
            int8_t localpin = pin & 0xff;
            uint8_t expander = pin >> 8;
            if (expander > OPENKNX_GPIO_NUM)
            {
                logErrorP("GPIOModule::attachInterrupt: invalid pin id %u", pin);
                return;
            }

            if (GPIO_TYPES[expander] == OPENKNX_GPIO_T_EMBEDDED)
            {
                GPIOExpanders[expander]->GPIOattachInterrupt(localpin, callback, mode);
            }
            else
            {
                queueI2CAction({pin, GpioActionType::AttachInterrupt, 0, false, 0, callback, mode});
            }
            return;
        }
    } // namespace GPIO
} // namespace OpenKNX