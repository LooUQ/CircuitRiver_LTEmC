/** ****************************************************************************
  \file 
  \brief LTEmC LTEm Device Driver for LooUQ LTEm Series Modems
  \author Greg Terrell, LooUQ Incorporated

  \loouq

--------------------------------------------------------------------------------

    This project is released under the GPL-3.0 License.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 
***************************************************************************** */

#define SRCFILE "LTE"                       // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT
#define ENABLE_DIAGPRINT                    // expand DPRINT into debug output
//#define ENABLE_DIAGPRINT_VERBOSE            // expand DPRINT and DPRINT_V into debug output
#define ENABLE_ASSERT
#include <lqdiag.h>

#include "ltemc.h"
#include "ltemc-internal.h"

/* ------------------------------------------------------------------------------------------------
 * GLOBAL LTEm Device Objects, One LTEmX supported
 * --------------------------------------------------------------------------------------------- */
ltemDevice_t g_lqLTEM;

#define APPRDY_TIMEOUT 8000

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))


/* BGx module initialization commands (start script)
 * ---------------------------------------------------------------------------------------------
 * used in ltemc-quectel-bg.c for module initialization, declared here for convenience
 * LTEmC requires that no-echo is there, append any ADDITIONAL global module setting command in the list.
 * Ex: Radio setup (RAT search, IoT mode, etc.) 
 * ------------------------------------------------------------------------------------------------ */
const char* const qbg_initCmds[] = 
{ 
    "ATE0\r",                                       // don't echo AT commands on serial
    "AT+QURCCFG=\"urcport\",\"uart1\"\r"            // URC events are reported to UART1
};

// makes for compile time automatic sz determination
int8_t qbg_initCmdsCnt = sizeof(qbg_initCmds)/sizeof(const char* const);            


/* Static Local Function Declarations
------------------------------------------------------------------------------------------------ */
static bool S__initLTEmDevice();


#pragma region Public Functions
/*-----------------------------------------------------------------------------------------------*/

/**
 *	@brief Initialize the LTEm1 modem.
 */
void ltem_create(const ltemPinConfig_t ltem_config, yield_func yieldCallback, appEvntNotify_func eventNotifCallback)
{
    ASSERT(g_lqLTEM.atcmd == NULL);                 // prevent multiple calls, memory leak calloc()
    memset(&g_lqLTEM, 0, sizeof(ltemDevice_t));
    
	g_lqLTEM.pinConfig = ltem_config;
    #if (ARDUINO_ARCH_ESP32)
        g_lqLTEM.platformSpi = spi_createFromPins(g_lqLTEM.pinConfig.spiClkPin, g_lqLTEM.pinConfig.spiMisoPin, g_lqLTEM.pinConfig.spiMosiPin, g_lqLTEM.pinConfig.spiCsPin);
    #else
        g_lqLTEM.platformSpi = spi_createFromIndex(g_lqLTEM.pinConfig.spiIndx, g_lqLTEM.pinConfig.spiCsPin);
    #endif

    g_lqLTEM.modemSettings =  calloc(1, sizeof(modemSettings_t));
    ASSERT(g_lqLTEM.modemSettings != NULL);

    g_lqLTEM.modemInfo = calloc(1, sizeof(modemInfo_t));
    ASSERT(g_lqLTEM.modemInfo != NULL);

    IOP_create();
    
    g_lqLTEM.atcmd = calloc(1, sizeof(atcmd_t));
    ASSERT(g_lqLTEM.atcmd != NULL);
    atcmd_reset(true);

    g_lqLTEM.fileCtrl = calloc(1, sizeof(fileCtrl_t));
    ASSERT(g_lqLTEM.fileCtrl != NULL);

    ntwk_create();

    g_lqLTEM.cancellationRequest = false;
    g_lqLTEM.appEvntNotifyCB = eventNotifCallback;
}



/**
 *	@brief Uninitialize the LTEm device structures.
 */
void ltem_destroy()
{
	ltem_stop(false);

	gpio_pinClose(g_lqLTEM.pinConfig.irqPin);
	gpio_pinClose(g_lqLTEM.pinConfig.powerkeyPin);
	gpio_pinClose(g_lqLTEM.pinConfig.resetPin);
	gpio_pinClose(g_lqLTEM.pinConfig.statusPin);

    ip_destroy();
    free(g_lqLTEM.atcmd);
    iop_destroy();
    spi_destroy(g_lqLTEM.platformSpi);
}


/**
 *	@brief Start the modem.
 */
bool ltem_start(resetAction_t resetAction)
{
    if (!g_lqLTEM.hostConfigured)
    {
        // on Arduino compatible, ensure pin is in default "logical" state prior to opening
        platform_writePin(g_lqLTEM.pinConfig.powerkeyPin, gpioValue_low);
        platform_writePin(g_lqLTEM.pinConfig.resetPin, gpioValue_low);
        platform_writePin(g_lqLTEM.pinConfig.spiCsPin, gpioValue_high);
        platform_writePin(g_lqLTEM.pinConfig.irqPin, gpioValue_high);

        platform_openPin(g_lqLTEM.pinConfig.powerkeyPin, gpioMode_output);		// powerKey: normal low
        platform_openPin(g_lqLTEM.pinConfig.resetPin, gpioMode_output);			// resetPin: normal low
        platform_openPin(g_lqLTEM.pinConfig.spiCsPin, gpioMode_output);			// spiCsPin: invert, normal gpioValue_high
        platform_openPin(g_lqLTEM.pinConfig.statusPin, gpioMode_input);
        platform_openPin(g_lqLTEM.pinConfig.irqPin, gpioMode_inputPullUp);
        DPRINT_V(PRNT_DEFAULT, "GPIO Configured\r\n");

        spi_start(g_lqLTEM.platformSpi);                                        // start host SPI
        DPRINT_V(PRNT_DEFAULT, "SPI Configured\r\n");
        g_lqLTEM.hostConfigured = true;
    }

    DPRINT(PRNT_CYAN, "LTEm reqst resetType=%d\r\n", resetAction);
    bool ltemWasReset = true;
    if (QBG_isPowerOn())
    {
        if (resetAction == resetAction_skipIfOn)
            ltemWasReset = false;
        else
        {
            if (resetAction == resetAction_swReset)
            {
                if (!SC16IS7xx_isAvailable())                               // fall back to power reset if UART not available
                    resetAction = resetAction_powerReset;
            }
            // DPRINT_V(PRNT_DEFAULT, "Ready to power off\r\n");
            // QBG_powerOff();
            // DPRINT_V(PRNT_DEFAULT, "Powered OFF\r\n");

            // // for (size_t i = 0; i < 5; i++)
            // // {
            // //     DPRINT_V(PRNT_DEFAULT, ".");
            // //     pDelay(1000);
            // // }
            // pDelay(1000);
            
            // DPRINT_V(PRNT_DEFAULT, "Ready to power on\r\n");
            // IOP_detachIrq();
            // QBG_powerOn();                                                       // turn on BGx
            // DPRINT_V(PRNT_DEFAULT, "Powered ON\r\n");

            IOP_detachIrq();
            QBG_reset(resetAction);                                         // do requested reset (sw, hw, pwrCycle)
        }
    }
    else 
    {
       QBG_powerOn();                                                       // turn on BGx
    }
    DPRINT_V(PRNT_DEFAULT, "LTEm was reset=%d\r\n", ltemWasReset);

    SC16IS7xx_start();                                                      // initialize NXP SPI-UART bridge base functions: FIFO, levels, baud, framing
    DPRINT_V(PRNT_CYAN, "UART started\r\n");
    SC16IS7xx_enableIrqMode();                                              // enable IRQ generation on SPI-UART bridge (IRQ mode)
    DPRINT_V(PRNT_CYAN, "UART set to IRQ mode\r\n");
    IOP_attachIrq();                                                        // attach I/O processor ISR to IRQ
    DPRINT_V(PRNT_CYAN, "UART IRQ attached\r\n");

    IOP_interruptCallbackISR();                                             // force ISR to run once to sync IRQ 

    uint32_t startAppRdy = pMillis();                                       // wait for BGx to signal internal ready
    while (bbffr_find(g_lqLTEM.iop->rxBffr, "APP RDY", 0, 0, true))
    {
        // pDelay(500);
        // SC16IS7xx_IIR iirVal;
        // uint8_t rxLevel;
        // char rxData[65] = {0};
        // iirVal.reg = SC16IS7xx_readReg(SC16IS7xx_IIR_regAddr);
        // DPRINT(PRNT_CYAN, "iirVal=%d\r\n", iirVal.reg);

        // rxLevel = SC16IS7xx_readReg(SC16IS7xx_RXLVL_regAddr);
        // DPRINT(PRNT_CYAN, "rxLevel=%d\r\n", rxLevel);

        // // SC16IS7xx_read(rxData, rxLevel);
        // // DPRINT(PRNT_CYAN, "rxData=%s\r\n", rxData);

        if (IS_ELAPSED(startAppRdy, APPRDY_TIMEOUT))
            return false;
    }
    DPRINT_V(PRNT_dCYAN, "AppRdy recv'd=%dms\r\n", pMillis() - startAppRdy);
    bbffr_reset(g_lqLTEM.iop->rxBffr);

    // if (!IOP_awaitAppReady())
    //     return;
    // S__initLTEmDevice();

    if (!QBG_setOptions())
    {
        ltem_notifyApp(appEvent_fault_hardFault, "BGx init cmd fault");     // send notification, maybe app can recover
        DPRINT(PRNT_DEFAULT, "\r");
    }
    else
        DPRINT_V(PRNT_CYAN, "BGx options set\r\n");

    // ntwk_setRatOptions();                                            // initialize BGx Radio Access Technology (RAT) options
    // DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): rat options set");

    ntwk_applyPpdNetworkConfig();                                       // configures default PDP context for likely autostart with provider attach
    DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): pdp ntwk configured\r\n");

    ntwk_awaitProvider(2);                                              // attempt to warm-up provider/PDP briefly. 
    DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): provider warmed up\r\n");     // If longer duration required, leave that to application

    return true;
}


// /**
//  *	@brief Static internal BGx initialization logic shared between start and reset
//  */
// bool S__initLTEmDevice()
// {
//     ASSERT(QBG_isPowerOn());
//     ASSERT(SC16IS7xx_isAvailable());

//     SC16IS7xx_start();                                                  // initialize NXP SPI-UART bridge base functions: FIFO, levels, baud, framing

//     if (g_lqLTEM.deviceState != deviceState_appReady)
//         return false;

//     if (!QBG_setOptions())
//     {
//         ltem_notifyApp(appEvent_fault_hardFault, "BGx init cmd fault"); // send notification, maybe app can recover
//         DPRINT(PRNT_DEFAULT, "\r");
//     }
//     else
//         DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): bgx options set");

//     // ntwk_setRatOptions();                                            // initialize BGx Radio Access Technology (RAT) options
//     // DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): rat options set");

//     ntwk_applyPpdNetworkConfig();                                       // configures default PDP context for likely autostart with provider attach
//     DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): pdp ntwk configured");

//     ntwk_awaitProvider(2);                                              // attempt to warm-up provider/PDP briefly. 
//     DPRINT_V(PRNT_CYAN, "S__initLTEmDevice(): provider warmed up");     // If longer duration required, leave that to application

//     return true;
// }


/**
 *	@brief Powers off the modem without destroying memory objects. Modem device will require ltem_start() to reinit HW.
 */
void ltem_stop()
{
    spi_stop(g_lqLTEM.platformSpi);
    IOP_stopIrq();
    g_lqLTEM.deviceState = deviceState_powerOff;
    QBG_powerOff();
}


/**
 *	@brief Performs a reset of LTEm.
 */
bool ltem_reset(bool hardReset)
{
    resetAction_t resetAction = hardReset ? resetAction_hwReset : resetAction_swReset;
    return ltem_start(resetAction);
}


/**
 *	@brief Turn modem power OFF
 */
void ltem_powerOff()
{
    QBG_powerOff();  
}


void ltem_enterPcm()
{
}

/**
 *	@brief Get RF priority on BG95/BG77 modules. 
 */
ltemRfPriorityMode_t ltem_getRfPriorityMode()
{
    char* moduleType = ltem_getModuleType();
    if ((memcmp(moduleType, "BG95", 4) == 0) || 
        (memcmp(moduleType, "BG77", 4) == 0))
    {
        if (atcmd_tryInvoke("AT+QGPSCFG=\"priority\""))
        {
            if (IS_SUCCESS_RSLT(atcmd_awaitResult()))
            {
                char tkn[5] = {'\0'};
                atcmd_getToken(1, tkn, sizeof(tkn));
                return strtol(tkn, NULL, 10);
            }
        }
    }
    return ltemRfPriorityState_error;
}


/**
 *	@brief Set RF priority on BG95/BG77 modules. 
 */
resultCode_t ltem_setRfPriorityMode(ltemRfPriorityMode_t priority)
{
    if (memcmp(ltem_getModuleType(), "BG95", 4) == 0 || 
        memcmp(ltem_getModuleType(), "BG77", 4) == 0)
    {
        if (atcmd_tryInvoke("AT+QGPSCFG=\"priority\",%d", priority))
        {
            char tkState = (priority == ltemRfPriorityMode_gnss) ? '4' : '3';    // GNSS=4, WWAN=3
            if (IS_SUCCESS_RSLT(atcmd_awaitResult()))
            {
                char tkn[5] = {'\0'};
                while (tkn[0] != tkState)
                {
                    if (ltem_getRfPriorityMode() == ltemRfPriorityMode_error)
                        return resultCode__timeout;
                    atcmd_getToken(2, tkn, sizeof(tkn));
                }
                return resultCode__success;
            }
        }
        return resultCode__conflict;
    }
return resultCode__preConditionFailed;                                          //  only applicable to single-RF modules
}


// /**
//  *	@brief Set RF priority on BG95/BG77 modules. 
//  */
// resultCode_t ltem_setRfPriorityMode(ltemRfPriorityMode_t priority)
// {
//     if (lq_strnstr(ltem_getModuleType(), "BG95", 40) ||
//         lq_strnstr(ltem_getModuleType(), "BG77", 40))
//     {
//         if (atcmd_tryInvoke("AT+QGPSCFG=\"priority\",%d", priority))
//         {
//             return atcmd_awaitResult();
//         }
//         return resultCode__conflict;
//     }
//     return resultCode__preConditionFailed;
// }


// /**
//  *	@brief Get RF priority on BG95/BG77 modules. 
//  */
// ltemRfPriorityState_t ltem_getRfPriorityState()
// {
//     if (lq_strnstr(ltem_getModuleType(), "BG95", 40) ||
//         lq_strnstr(ltem_getModuleType(), "BG77", 40))
//     {
//         if (atcmd_tryInvoke("AT+QGPSCFG=\"priority\""))
//         {
//             if (atcmd_awaitResult() == resultCode__success)
//             {
//                 return atcmd_getValue();
//             }
//         }
//         return 99;
//     }
//     return 99;
// }


// /**
//  *	@brief Get the current UTC date and time.
//  */
// void ltem_getDateTimeUtc(char *dateTime)
// {
//     char* ts;
//     uint8_t len;
//     *dateTime = '\0';                                                       // dateTime is empty c-string now

//     if (dateTime != NULL && atcmd_tryInvoke("AT+CCLK?"))
//     {
//         if (atcmd_awaitResult() == resultCode__success)
//         {
//             if ((ts = memchr(atcmd_getResponse(), '"', 12)) != NULL)        // allowance for preceeding EOL
//             {
//                 ts++;
//                 if (*ts != '8')                                             // test for not initialized date/time, starts with 80 (aka 1980)
//                 {
//                     char* stop = memchr(ts, '-', 20);                       // strip UTC offset, safe stop in trailer somewhere
//                     if (stop != NULL)                                       // found expected - delimeter before TZ offset
//                     {
//                         *stop = '\0';
//                         strcpy(dateTime, ts);                               // safe strcpy to dateTime
//                         return;
//                     }
//                 }
//             }
//         }
//     }
// }


/**
 *	@brief Get the current local date and time.
 */
const char* ltem_getLocalDateTime(char format)
{
    char* ts;
    uint8_t len;

    char* dateTime = &g_lqLTEM.statics.dateTimeBffr;                        // readability
    memset(dateTime, 0, ltem__dateTimeBffrSz);

    if (dateTime != NULL && atcmd_tryInvoke("AT+CCLK?"))
    {
        if (atcmd_awaitResult() == resultCode__success)
        {
            if ((ts = memchr(atcmd_getResponse(), '"', 12)) != NULL)        // allowance for preceeding EOL
            {
                ts++;
                if (*ts != '8')                                             // test for not initialized date/time, starts with 80 (aka 1980)
                {
                    char* tzDelim = memchr(ts, '-', 20);                       // strip UTC offset, safe stop in trailer somewhere
                    if (tzDelim != NULL)                                       // found expected - delimeter before TZ offset
                    {
                        if (format == 'v' || format == 'V')                     // "VERBOSE" format
                        {
                            *tzDelim = '\0';                                    // verbose displays local time, use ltem_getLocalTimezoneOffset() to get TZ
                            strcpy(dateTime, ts);                               // safe c-string strcpy to dateTime
                        }
                        else                                                    // default format ISO8601
                        {
                            if (format != 'c' && format != 'C')                 // not 'c'ompact format: 4 digit year
                                memcpy(dateTime, "20", 2);                      // convert to 4 digit year (ISO8601)
                            dateTime += 2;
                            memcpy(dateTime, ts, 2);                            // year
                            dateTime += 2;
                            memcpy(dateTime, ts + 3, 2);                        // month
                            dateTime += 2;
                            memcpy(dateTime, ts + 6, 2);                        // day
                            dateTime += 2;
                            *dateTime = 'T';                                    // delimiter
                            dateTime += 1;
                            memcpy(dateTime, ts + 9, 2);                        // hours
                            dateTime += 2;
                            memcpy(dateTime, ts + 12, 2);                       // minutes
                            dateTime += 2;
                            memcpy(dateTime, ts + 15, 2);                       // seconds
                            dateTime += 2;

                            if (format != 'c' && format != 'C')                 // not 'c'ompact format: include time zone offset
                            {
                                memcpy(dateTime, ts + 17, 1);                   // TZ delimiter (value sign)
                                dateTime += 1;
                                uint8_t tzOffset = strtol(ts + 18, NULL, 10);   // already have sign in output
                                uint8_t hours = tzOffset / 4;
                                uint8_t minutes = (tzOffset % 4) * 15;
                                snprintf(dateTime + 14, 4, "%02d%02d", hours, minutes);
                            }

                        }
                    }
                }
            }
        }
    }
    return (const char*)&g_lqLTEM.statics.dateTimeBffr;
}


/**
 *	@brief Get local time zone offset.
 */
int8_t ltem_getLocalTimezoneOffset(bool precise)
{
    char dateTime[30] = {0};
    char *ts;

    if (atcmd_tryInvoke("AT+CCLK?"))
    {
        if (IS_SUCCESS_RSLT(atcmd_awaitResult()))
        {
            if ((ts = memchr(atcmd_getResponse(), '"', 12)) != NULL)        // tolerate preceeding EOL
            {
                ts++;
                if (*ts != '8')                                             // test for not initialized date/time, starts with 80 (aka 1980)
                {
                    char* tzDelim = memchr(ts, '-', 20);                    // strip UTC offset, safe stop in trailer somewhere
                    if (tzDelim != NULL)                                    // found expected - delimeter before TZ offset
                    {
                        if (precise)
                            return strtol(tzDelim, NULL, 10);               // BGx reports 15min TZ offsets (supports 30, 45 minutes TZ offset regions)
                        else
                            return strtol(tzDelim, NULL, 10) / 4;           // BGx reports 15min TZ offsets (supports 30, 45 minutes TZ offset regions)
                    }
                }
            }
        }
    }
    return 0;
}


/**
 *  @brief Get the LTEm1 static device identification/provisioning information.
 */
modemInfo_t *ltem_getModemInfo()
{
    if (ATCMD_awaitLock(atcmd__defaultTimeout))
    {
        if (g_lqLTEM.modemInfo->imei[0] == 0)
        {
            atcmd_invokeReuseLock("AT+GSN");
            if (atcmd_awaitResult() == resultCode__success)
            {
                strncpy(g_lqLTEM.modemInfo->imei, atcmd_getResponse(), ntwk__imeiSz);
            }
        }

        if (g_lqLTEM.modemInfo->fwver[0] == 0)
        {
            atcmd_invokeReuseLock("AT+QGMR");
            if (atcmd_awaitResult() == resultCode__success)
            {
                char *eol;
                if ((eol = strstr(atcmd_getResponse(), "\r\n")) != NULL)
                {
                    uint8_t sz = eol - atcmd_getResponse();
                    memcpy(g_lqLTEM.modemInfo->fwver, atcmd_getResponse(), MIN(sz, ntwk__dvcFwVerSz));
                }
            }
        }

        if (g_lqLTEM.modemInfo->mfg[0] == 0)
        {
            atcmd_invokeReuseLock("ATI");
            if (atcmd_awaitResult() == resultCode__success)
            {
                char* response = atcmd_getResponse();
                char* eol = strchr(response, '\r');
                memcpy(g_lqLTEM.modemInfo->mfg, response, eol - response);

                response = eol + 2;
                eol = strchr(response, '\r');
                memcpy(g_lqLTEM.modemInfo->model, response, eol - response);

                response = eol + 2;
                eol = strchr(response, ':');
                response = eol + 2;
                eol = strchr(response, '\r');
                memcpy(g_lqLTEM.modemInfo->fwver, response, eol - response);
            }
        }

        if (g_lqLTEM.modemInfo->iccid[0] == 0)
        {
            atcmd_invokeReuseLock("AT+ICCID");
            if (atcmd_awaitResultWithOptions(atcmd__defaultTimeout, S__iccidCompleteParser) == resultCode__success)
            {
                strncpy(g_lqLTEM.modemInfo->iccid, atcmd_getResponse(), ntwk__iccidSz);
            }
        }
        atcmd_close();
    }
    return (modemInfo_t *)(g_lqLTEM.modemInfo);
}

/**
 *  @brief Test for SIM ready
 */
bool ltem_isSimReady()
{
    bool cpinState = false;
    if (atcmd_tryInvoke("AT+CPIN?"))
    {
        if (atcmd_awaitResult() == resultCode__success)
        {
            cpinState = strstr(atcmd_getResponse(), "+CPIN: READY") != NULL;
        }
        atcmd_close();
    }
    return strlen(g_lqLTEM.modemInfo->iccid) > 0 && cpinState;
}


/**
 *  @brief Get the signal strenght as raw value returned from BGx.
 */
uint8_t ltem_signalRaw()
{
    uint8_t signal = 99;

    if (ltem_getDeviceState())
    {
        if (atcmd_tryInvoke("AT+CSQ"))
        {
            if (atcmd_awaitResult() == resultCode__success)
            {
                char *term;
                char *lastResponse = atcmd_getResponse();
                term = strstr(atcmd_getResponse(), "+CSQ");
                signal = strtol(term + 6, NULL, 10);
            }
            atcmd_close();
        }
    }
    return signal;
}

/**
 *  @brief Get the signal strength reported by the LTEm device at a percent
 */
uint8_t mdmInfo_signalPercent()
{
    double csq;
    uint8_t signal = 0;
    const double csqFactor = 3.23;

    csq = (double)ltem_signalRaw();
    signal = (csq == 99) ? 0 : (uint8_t)(csq * csqFactor);
    return signal;
}

/**
 *  @brief Get the signal strenght as RSSI (db).
 */
int16_t ltem_signalRSSI()
{
    const int8_t rssiBase = -113;
    const int8_t rssiRange = 113 - 51;

    uint8_t signalPercent = mdmInfo_signalPercent();
    return (signalPercent == 0) ? rssiBase : (signalPercent * 0.01 * rssiRange) + rssiBase;
}

/**
 *  @brief Get the signal strength, as a bar count for visualizations, (like on a smartphone)
 * */
uint8_t ltem_signalBars(uint8_t displayBarCount)
{
    const int8_t barOffset = 20; // adjust point for full-bar percent (20 = full bar count at 80%)

    uint8_t barSpan = 100 / displayBarCount;
    uint8_t signalPercent = MIN(mdmInfo_signalPercent() + barOffset, 100);
    return (uint8_t)(signalPercent / barSpan);
}



/**
 *	@brief Get the LTEmC software version.
 */
const char* ltem_getSwVersion()
{
    return LTEmC_VERSION;
}


/**
 *	@brief Get the LTEmC software version.
 */
const char* ltem_getModuleType()
{
    if (strlen(g_lqLTEM.modemInfo->model) == 0)
    {
        mdminfo_ltem();
    }
    return g_lqLTEM.modemInfo->model;
}


/**
 *	@brief Performs a HW reset of LTEm1 and optionally executes start sequence.
 */
deviceState_t ltem_getDeviceState()
{
    if (QBG_isPowerOn())             // ensure powered off device doesn't report otherwise
        g_lqLTEM.deviceState = MAX(g_lqLTEM.deviceState, deviceState_powerOn); 
    else
        g_lqLTEM.deviceState = deviceState_powerOff;

    return g_lqLTEM.deviceState;
}


/**
 *	@brief Test for responsive BGx.
 */
bool ltem_ping()
{
    resultCode_t rslt;
    if (atcmd_tryInvoke("AT"))
    {
        rslt = atcmd_awaitResult();
        return rslt != resultCode__timeout;
    }
    return false;
}


/**
 *	@brief Background work task runner. To be called in application Loop() periodically.
 */
void ltem_eventMgr()
{
    /* look for a new incoming URC 
     */
    int16_t urcPossible = bbffr_find(g_lqLTEM.iop->rxBffr, "+", 0, 0, false);       // look for prefix char in URC
    if (BBFFR_ISNOTFOUND(urcPossible))
    {
        return;
    }

    for (size_t i = 0; i < ltem__streamCnt; i++)                                    // potential URC in rxBffr, see if a data handler will service
    {
        resultCode_t serviceRslt;
        if (g_lqLTEM.streams[i] != NULL &&  g_lqLTEM.streams[i]->urcHndlr != NULL)  // URC event handler in this stream, offer the data to the handler
        {
            serviceRslt = g_lqLTEM.streams[i]->urcHndlr();
        }
        if (serviceRslt == resultCode__cancelled)                                   // not serviced, continue looking
        {
            continue;
        }
        break;                                                                      // service attempted (might have errored), so this event is over
    }

    // S__ltemUrcHandler();                                                            // always invoke system level URC validation/service
}


void ltem_addStream(streamCtrl_t *streamCtrl)
{
    DPRINT_V(PRNT_INFO, "Registering Stream\r\n");
    streamCtrl_t* prev = ltem_getStreamFromCntxt(streamCtrl->dataCntxt, streamType__ANY);

    if (prev != NULL)
        return;

    for (size_t i = 0; i < ltem__streamCnt; i++)
    {
        if (g_lqLTEM.streams[i] == NULL)
        {
            g_lqLTEM.streams[i] = streamCtrl;
            return;
        }
    }
}


void ltem_deleteStream(streamCtrl_t *streamCtrl)
{
    for (size_t i = 0; i < ltem__streamCnt; i++)
    {
        if (g_lqLTEM.streams[i]->dataCntxt == streamCtrl->dataCntxt)
        {
            ASSERT(memcmp(g_lqLTEM.streams[i], streamCtrl, sizeof(streamCtrl_t)) == 0);     // compare the common fields
            g_lqLTEM.streams[i] = NULL;
            return;
        }
    }
}


streamCtrl_t* ltem_getStreamFromCntxt(uint8_t context, streamType_t streamType)
{
    for (size_t i = 0; i < ltem__streamCnt; i++)
    {
        if (g_lqLTEM.streams[i] != NULL && g_lqLTEM.streams[i]->dataCntxt == context)
        {
            if (streamType == streamType__ANY)
            {
                return g_lqLTEM.streams[i];
            }
            else if (g_lqLTEM.streams[i]->streamType == streamType)
            {
                return g_lqLTEM.streams[i];
            }
            else if (streamType == streamType__SCKT)
            {
                if (g_lqLTEM.streams[i]->streamType == streamType_UDP ||
                    g_lqLTEM.streams[i]->streamType == streamType_TCP ||
                    g_lqLTEM.streams[i]->streamType == streamType_SSLTLS)
                {
                    return g_lqLTEM.streams[i];
                }
            }
        }
    }
    return NULL;
}

/**
 *	@brief Notify host application of significant events. Application may ignore, display, save status, whatever. 
 */
void ltem_notifyApp(uint8_t notifyType, const char *notifyMsg)
{
    if (g_lqLTEM.appEvntNotifyCB != NULL)                                       
        (g_lqLTEM.appEvntNotifyCB)(notifyType, notifyMsg);                                // if app handler registered, it may/may not return
}


/**
 *	@brief Registers the address (void*) of your application event nofication callback handler.
 */
void ltem_setEventNotifCallback(appEvntNotify_func eventNotifCallback)
{
    g_lqLTEM.appEvntNotifyCB = eventNotifCallback;
}

/**
 *	@brief Registers the address (void*) of your application yield callback handler.
 */
void ltem_setYieldCallback(platform_yieldCB_func_t yieldCallback)
{
    platform_yieldCB_func = yieldCallback;
}


#pragma endregion


#pragma region LTEmC Internal Functions (ltemc-internal.h)
/*-----------------------------------------------------------------------------------------------*/

// void LTEM_registerUrcHandler(urcHandler_func *urcHandler)
// {
//     bool registered = false;
//     for (size_t i = 0; i < ltem__urcHandlersCnt; i++)
//     {
//         if (g_lqLTEM.urcHandlers[i] == NULL)
//         {
//             registered = true;
//             g_lqLTEM.urcHandlers[i] = urcHandler;                               // add to "registered" handlers
//         }
//         else
//         {
//             if (g_lqLTEM.urcHandlers[i] == urcHandler)                          // previously registered
//                 registered = true;
//         }
//         ASSERT(registered);
//     }
// }

// uint8_t LTEM__getStreamIndx(dataCntxt_t dataCntxt)
// {
//     for (size_t indx = 0; indx < ltem__streamCnt; indx++)
//     {
//         if (g_lqLTEM.streams[indx]->dataContext == dataCntxt)
//         {
//             return indx;
//         }
//     }
// }

#pragma endregion


/*
 * --------------------------------------------------------------------------------------------- */
#pragma region Static Function Definitions


/**
 * @brief Global URC handler
 * @details Services URC events that are not specific to a stream/protocol
 */
static void S__ltemUrcHandler()                                                     
{
    bBuffer_t *rxBffr = g_lqLTEM.iop->rxBffr;                           // for convenience
    char parseBffr[30];

    // bool isQuectel = cbffr_find(rxBffr, "+Q", 0, 0, false) != CBFFR_NOFIND;
    // bool isCCITT = cbffr_find(rxBffr, "+C", 0, 0, false) != CBFFR_NOFIND;

    /* LTEm System URCs Handled Here
     *
     * +QIURC: "pdpdeact",<contextID>
    */

    /* PDP (packet network) deactivation/close
     ------------------------------------------------------------------------------------------- */
    // if (cbffr_find(rxBffr, "+QIURC: \"pdpdeact\"", 0, 0, true) >= 0)
    // {
    //     for (size_t i = 0; i < dataCntxt__cnt; i++)
    //     {
    //         if (g_lqLTEM.streams[i].dataCloseCB)
    //         {
    //             g_lqLTEM.streams[i].dataCloseCB(i);
    //         }
    //     }
    // }
 
}


/**
 *	@brief Action response parser for iccid value request.
 */
static cmdParseRslt_t S__iccidCompleteParser(ltemDevice_t *modem)
{
    return atcmd_stdResponseParser("+ICCID: ", true, "", 0, 0, "\r\n\r\nOK\r\n", 20);
}



#pragma endregion
