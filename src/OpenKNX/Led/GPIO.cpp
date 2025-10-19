#include "OpenKNX/Led/GPIO.h"
#include "OpenKNX/Facade.h"

namespace OpenKNX
{
    namespace Led
    {
        GPIO::GPIO(long pin /*= -1*/, long activeOn /*= HIGH*/)
            : _pin(pin), _activeOn(activeOn)
        {
            _initialized = false;
            _currentLedBrightness = 0;
        }

        void GPIO::init()
        {
            // no valid pin
            if (_pin < 0)
                return;

            _initialized = true;
            openknx.gpio.pinMode(_pin, OUTPUT);
            openknx.gpio.digitalWrite(_pin, !_activeOn);
        }

        /*
         * write led state based on bool and _brightness
         */
        void GPIO::writeLed(uint8_t brightness)
        {
            // do nothing if not initialized
            if (!_initialized) return;

            uint8_t calcBrightness = (uint32_t)brightness * _maxBrightness / 100;

            if (calcBrightness == _currentLedBrightness)
                return;

            if (calcBrightness == 255)
            {
                if (!isI2C())
                    openknx.gpio.pinMode(_pin, OUTPUT);
                openknx.gpio.digitalWrite(_pin, _activeOn);
            }
            else if (calcBrightness == 0)
            {
                if (!isI2C())
                    openknx.gpio.pinMode(_pin, OUTPUT);
                openknx.gpio.digitalWrite(_pin, !_activeOn);
            }
            else
            {
                if (!isI2C())
                    analogWrite(_pin, _activeOn ? calcBrightness : (255 - calcBrightness));
            }

            _currentLedBrightness = calcBrightness;
        }

        bool GPIO::isI2C()
        {
            return (_pin > 0xff);
        }
    } // namespace Led
} // namespace OpenKNX