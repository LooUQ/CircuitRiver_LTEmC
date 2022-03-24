/******************************************************************************
 *  \file ltemc-mdminfo.h
 *  \author Greg Terrell
 *  \license MIT License
 *
 *  Copyright (c) 2020,2021 LooUQ Incorporated.
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
 ******************************************************************************
 * Obtain basic modem identification and operational information 
 *****************************************************************************/

#ifndef _MODEMINFO_H_
#define _MODEMINFO_H_

#include "ltemc.h"
#include "lq-network.h"


// #include <stddef.h>
// #include <stdint.h>

#define MDMINFO_IMEI_SZ = 16
#define MDMINFO_ICCID_SZ = 21
#define MDMINFO_FWVER_SZ = 41
#define MDMINFO_MFGINFO_SZ 41

// /** 
//  *  \brief Struct holding information about the physical BGx module.
// */
// typedef struct modemInfo_tag
// {
// 	char imei[16];          ///< IMEI (15 digits) International Mobile Equipment Identity, set in the BGx itself at manufacture.
// 	char iccid [21];        ///< ICCID (20 digits) Integrated Circuit Card ID. Set in the SIM card at manufacture.
// 	char mfgmodel [21];     ///< The Quectel model number of the BGx device.
// 	char fwver [41];        ///< Firmware version of the BGx device.
// } modemInfo_t;


#ifdef __cplusplus
extern "C" {
#endif


/**
 *  @brief Get the LTEm1 static device identification/provisioning information.
 *  @return Modem information struct, see mdminfo.h for details.
*/
modemInfo_t *mdminfo_ltem();


/**
 *  @brief Get the signal strength reported by the LTEm device as RSSI reported
 *  @return The radio signal strength in the range of -51dBm to -113dBm (-999 is no signal)
*/
int16_t mdminfo_signalRSSI();


/**
 *  @brief Get the signal strength reported by the LTEm device at a percent
 *  @return The radio signal strength in the range of 0 to 100 (0 is no signal)
*/
uint8_t mdmInfo_signalPercent();

#ifdef __cplusplus
}
#endif

#endif  //!_MODEMINFO_H_
