/******************************************************************************
 *  \file platform-pins.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020 LooUQ Incorporated.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 * Psuedo-Header referenced ONLY by application after defining host. This in turn
 * creates an initialized ltem_pinConfig variable.
 *****************************************************************************/


/* Add one of the host board defines from the list below to configure your LTEm1 connection. 
 * To extend, define your connections in a new ltem1PinConfig_t initialization and give it a name.

#define HOST_FEATHER_UXPLOR
#define HOST_FEATHER_BASIC
#define HOST_RASPI_UXPLOR
*/


#define STATUS_LOW_PULLDOWN

#ifdef HOST_FEATHER_UXPLOR_L
#define LTEM_PINS 1
const ltemPinConfig_t ltem_pinConfig =
{
    spiIndx : 0,
    spiCsPin : 13,
    spiClkPin : 0,
    spiMisoPin : 0,
    spiMosiPin : 0,
    irqPin : 12,
    statusPin : 6,                  ///< HIGH indicates ON
    powerkeyPin : 11,               ///< toggle HIGH to change state
    resetPin : 10,                  ///< reset active HIGH
    ringUrcPin : 0,             
    connected : 0,    
    wakePin : 0,
};
#endif

#ifdef HOST_FEATHER_UXPLOR
#define LTEM_PINS 1
const ltemPinConfig_t ltem_pinConfig =
{
    spiIndx : 0,
    spiCsPin : 13,
    spiClkPin : 0,
    spiMisoPin : 0,
    spiMosiPin : 0,
    irqPin : 12,
    statusPin : 19,                 ///< (aka A5) HIGH indicates ON 
    powerkeyPin : 11,               ///< toggle HIGH to change state
    resetPin : 10,                  ///< reset active HIGH
    ringUrcPin : 15,                /// (aka A2)
    connected : 17,                 /// (aka A3)
    wakePin : 18                    /// (aka A4)
};
#endif


#ifdef HOST_FEATHER_LTEM3F
#define LTEM_PINS 1
const ltemPinConfig_t ltem_pinConfig =
{
    spiIndx : 0,
    spiCsPin : 18,                  /// AKA A4
    spiClkPin : 0,
    spiMisoPin : 0,
    spiMosiPin : 0,
    irqPin : 19,                    /// AKA A5
    statusPin : 12,                 /// HIGH indicates ON
    powerkeyPin : 10,               /// toggle HIGH to change state
    resetPin : 11,                  /// reset active HIGH
    ringUrcPin : 0,                 
    connected : 0,
    wakePin : 0
};
#define LQLTE_MODULE BG77
#endif

#ifdef HOST_FEATHER_BASIC
#define LTEM_PINS 1
const ltemPinConfig_t ltem_pinConfig =
{
    spiIndx : 0,
    spiCsPin : 13,
    spiClkPin : 0,
    spiMisoPin : 0,
    spiMosiPin : 0,
    irqPin : 12,
    statusPin : 6,
    powerkeyPin : 11,
    resetPin : 19,
    ringUrcPin : 5,
    connected : 0,
    wakePin : 10
};
#endif

#ifdef HOST_RASPI_UXPLOR
#define LTEM_PINS 1
const ltemPinConfig_t ltem_pinConfig = 
{
    spiIndx : -1,
    spiCsPin = 0, 			//< J8_24
    spiClkPin : 0,
    spiMisoPin : 0,
    spiMosiPin : 0,
	irqPin = 22U,		    //< J8_15
	statusPin = 13U,		//< J8_22
	powerkeyPin = 24U,		//< J8_18
	resetPin = 23U,		//< J8_16
	ringUrcPin = 0,
    wakePin = 0
};
#endif

#ifdef HOST_ESP32_DEVMOD_BMS
#define LTEM_PINS 1
const ltemPinConfig_t ltem_pinConfig = 
{
    spiIndx : -1,
    spiCsPin : 18,
    spiClkPin : 15,
    spiMisoPin : 16,
    spiMosiPin : 17,
	irqPin : 8,
	statusPin : 47,
	powerkeyPin : 45,
	resetPin : 0,
	ringUrcPin : 0,
    wakePin : 48
};
#endif

#ifdef HOST_ESP32_DEVMOD_BMS2
#define LTEM_PINS 1
const ltemPinConfig_t ltem_pinConfig = 
{
    spiIndx : -1,
    spiCsPin : 8,
    spiClkPin : 16,
    spiMisoPin : 17,
    spiMosiPin : 18,
	irqPin : 3,
	statusPin : 47,
	powerkeyPin : 45,
	resetPin : 0,
	ringUrcPin : 0,
    wakePin : 48
};
#endif

#ifdef HOST_LOOUQ_REMOTENODE
#define LTEM_PINS 1
const ltemPinConfig_t ltem_pinConfig =
{
    spiIndx : 1,
    spiCsPin : 13,
    spiClkPin : 0,
    spiMisoPin : 0,
    spiMosiPin : 0,
    irqPin : 12,
    statusPin : 6,                  ///< HIGH indicates ON
    powerkeyPin : 11,               ///< toggle HIGH to change state
    resetPin : 10,                  ///< reset active HIGH
    ringUrcPin : 0,             
    connected : 0,    
    wakePin : 0,
};
#endif

