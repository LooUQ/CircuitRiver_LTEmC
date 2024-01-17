/** ***************************************************************************
  @file 
  @details Cellular/packet data network support features and services

  @author Greg Terrell, LooUQ Incorporated

  \loouq
-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2017-2023 LooUQ Incorporated

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
Also add information on how to contact you by electronic and paper mail.

**************************************************************************** */


#ifndef __NETWORK_H__
#define __NETWORK_H__

#include "ltemc.h"


/* ------------------------------------------------------------------------------------------------
 * Network constants, enums, and structures are declared in the LooUQ global lq-network.h header
 * --------------------------------------------------------------------------------------------- */
//#include <lq-network.h>


#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

//const char *ltem_ltemcVersion();

/**
 *	@brief Initialize the IP network contexts structure.
 */
void ntwk_create();


/**
 * @brief Configure RAT searching sequence
 * @details Example: scanSequence = "020301" represents: search LTE-M1, then LTE-NB1, then GSM
 * @param [in] scanSequence Character string specifying the RAT scanning order; 00=Automatic[LTE-M1|LTE-NB1|GSM],01=GSM,02=LTE-M1,03=LTE-NB1
*/
void ntwk_setOperatorScanSeq(const char *sequence);


/** 
 * @brief Configure RAT(s) allowed to be searched
 * @param [in] scanMode Enum specifying what cell network to scan; 0=Automatic,1=GSM only,3=LTE only
*/
void ntwk_setOperatorScanMode(ntwkScanMode_t mode);


/** 
 * @brief Configure the network category to be searched under LTE RAT.
 * @param [in] iotMode Enum specifying the LTE LPWAN protocol(s) to scan; 0=LTE M1,1=LTE NB1,2=LTE M1 and NB1
 */
void ntwk_setIotMode(ntwkIotMode_t mode);


/**
 * @brief Build default data context configuration for modem to use on startup.
 * @param [in] cntxtId The context ID to operate on. Typically 0 or 1
 * @param [in] protoType The PDP protocol IPV4, IPV6, IPV4V6 (both).
 * @param [in] apn The APN name if required by network carrier.
 */
resultCode_t ntwk_setDefaultNetwork(uint8_t pdpContextId, pdpProtocol_t protoType, const char *apn);


/**
 * @brief Configure PDP Context.
 * @param [in] cntxtId The context ID to operate on. Typically 0 or 1
 * @param [in] protoType The PDP protocol IPV4, IPV6, IPV4V6 (both).
 * @param [in] apn The APN name if required by network carrier.
 *  
 */
resultCode_t ntwk_configPdpNetwork(dataCntxt_t pdpContextId, pdpProtocol_t protoType, const char *apn);


/**
 * @brief Configure PDP Context requiring authentication.
 * @details Only IPV4 is supported 
 * 
 * @param [in] pdpContextId The PDP context to work with, typically 0 or 1.
 * @param [in] apn The APN name if required by network carrier.
 * @param [in] userName String with user name
 * @param [in] pw String with password
 * @param [in] authMethod Enum specifying the type of authentication expected by network
 */
resultCode_t ntwk_configPdpNetworkWithAuth(uint8_t pdpContextId, const char *apn, const char *pUserName, const char *pPW, pdpCntxtAuthMethods_t authMethod);


/**
 * @brief Get network registration information.
 * 
 * @return const char* Preconfigured PDP network registration parameters text content.
 */
const char * ntwk_getNetworkConfig();


/**
 * @brief Wait for a network operator name and network mode. Can be cancelled in threaded env via g_ltem->cancellationRequest.
 * @param [in] waitSec Number of seconds to wait for a network. Supply 0 for no wait.
 * @return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
ntwkOperator_t *ntwk_awaitOperator(uint16_t waitSec);


/**
 *	@brief Set the default/data context number for provider. Default is 1 if not overridden here.
 * @param [in] defaultContext The data context to operate on. Typically 0 or 1, up to 15
 */
void ntwk_setOperatorDefaultContext(uint8_t defaultContext);


/**
 * @brief Activate a PDP data context for TCP/IP communications.
 * @note The BG9x supports a maximum of 3 contexts, BG7x supports a maximum of 2. Most network operators support 1 or 2 (VPN).
 * @param [in] cntxtId The PDP context ID to activate
 */
void ntwk_activatePdpContext(uint8_t cntxtId);


/**
 * @brief Deactivate a PDP (TCP/IP data communications) context.
 * @param [in] cntxtId The PDP context ID to deactivate
 */
void ntwk_deactivatePdpContext(uint8_t cntxtId);


/**
 * @brief Returns true if context is ready and updates LTEm internal network information for the context
 * @param [in] cntxtId The PDP context ID to deactivate
 * @return True if the context is active
 */
bool ntwk_getPdpContextState(uint8_t cntxtId);





/**
 * @brief Get current provider information. If not connected to a provider will be an empty providerInfo struct
 * @return Struct containing the network operator name (operName) and network mode (ntwkMode).
*/
ntwkOperator_t *ntwk_getOperatorInfo();


/**
 * @brief Get count of APN active data contexts from BGx.
 * @return Count of active data contexts (BGx max is 3).
 */
uint8_t ntwk_getActiveNetworkCount();


/**
 * @brief Get PDP network information
 * @param [in] cntxtId The PDP context ID/index to retreive.
 * @return Pointer to network (PDP) info in active network table, NULL if context ID not active
 */
packetNetwork_t* ntwk_getPacketNetwork(uint8_t pdpContextId);


/**
 * @brief Get information about the active operator network
 * @return Char array pointer describing operator network characteristics
 */
const char* ntwk_getNetworkInfo();


/**
 * @brief Get current network registration status.
 * @return The current network operator registration status.
 */
resultCode_t ntwk_getRegistrationStatus();


// /**
//  * @brief Set network operator.
//  * @details The characteristics of the selected operator are accessible using the atcmd_getResponse() function.

//  * @param [in] mode Action to be performed, set/clear/set default.
//  * @param [in] format The form for the ntwkOperator parameter value: long, short, numeric.
//  * @param [in] ntwkOperator Operator to select, presented in the "format". Not all modes require/act on this parameter.
//  * @return Current operator selection mode. Note:
//  */
// uint8_t ntwk_setOperator(uint8_t mode, uint8_t format, const char* ntwkOperator);


/**
 * @brief Check network ready condition (reads network operator info and checks signal strenght).
 * 
 * @return true network is fully established and ready for data transmission.
 * @return false network operator information is incomplete or no RF signal available.
 */
bool ntwk_isReady();


/**
 * @brief Check immediately with module for network condition (completes a module inquiry).
 * 
 * @return true, if network is fully established and ready for data transmission.
 * @return false network is not fully ready for data transmission; carrier info/signal/IP address.
 */
bool ntwk_validate();


/**
 *  @brief Get the signal strength reported by the LTEm device at a percent
 *  @return The radio signal strength in the range of 0 to 100 (0 is no signal)
*/
uint8_t ntwk_signalPercent();


/**
 *  @brief Get the signal strength reported by the LTEm device as RSSI reported
 *  @return The radio signal strength in the range of -51dBm to -113dBm (-999 is no signal)
*/
int16_t ntwk_signalRSSI();


/**
 *  @brief Get the signal strength reported by the LTEm device as RSSI reported
 *  @return The raw radio signal level reported by BGx
*/
uint8_t ntwk_signalRaw();


/** 
 *  @brief Get the signal strength, as a bar count for visualizations, (like on a smartphone) 
 *  @return The radio signal strength factored into a count of bars for UI display
 * */
uint8_t ntwk_signalBars(uint8_t displayBarCount);



/** 
 * @brief Development/diagnostic function to retrieve visible providers from radio.
 * @warning This command can take MINUTES to respond! It is generally considered a command used solely for diagnostics.
 * 
 * @param [out] operatorList  Pointer to char buffer to return operator list information retrieved from BGx.
 * @param [in] listSz Length of provided buffer.
 */
void ntwkDiagnostics_getOperators(char *operatorList, uint16_t listSz);


#ifdef __cplusplus
}
#endif // !__cplusplus

#endif  /* !__NETWORK_H__ */
