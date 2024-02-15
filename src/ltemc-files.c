/** ***************************************************************************
  @file 
  @brief Modem filesystem storage features/services.

  @author Greg Terrell, LooUQ Incorporated

  \loouq
-------------------------------------------------------------------------------

LooUQ-LTEmC // Software driver for the LooUQ LTEm series cellular modems.
Copyright (C) 2022-2023 LooUQ Incorporated

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

#include <lq-embed.h>
#define lqLOG_LEVEL lqLOGLEVEL_DBG
//#define DISABLE_ASSERTS                                   // ASSERT/ASSERT_W enabled by default, can be disabled 
#define LQ_SRCFILE "FIL"                                    // create SRCFILE (3 char) MACRO for lq-diagnostics ASSERT

#include "ltemc-internal.h"
#include "ltemc-files.h"

extern ltemDevice_t g_lqLTEM;


#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

/* Local Static Functions
------------------------------------------------------------------------------------------------------------------------- */
static cmdParseRslt_t S__writeStatusParser();


/**
 *	@brief Set the data callback function for filedata.
 */
void file_setAppReceiver(appRcvr_func fileReceiver)
{
    ASSERT(fileReceiver != NULL);                                           // assert user provided receiver function

    g_lqLTEM.fileCtrl->streamType = streamType_file;                        // init singleton fileCtrl
    g_lqLTEM.fileCtrl->dataHndlr = atcmd_rxHndlrWithLength;
    g_lqLTEM.fileCtrl->appRecvDataCB = fileReceiver;
}


/**
 *	@brief get filesystem information.
 */
resultCode_t file_getFSInfo(filesysInfo_t * fsInfo)
{
    RSLT;
    do
    {
        // first get file system info
        if (IS_NOTSUCCESS(atcmd_dispatch("AT+QFLDS=\"UFS\"")))
            break;

        // parse response >>  +QFLDS: <freesize>,<total_size>
        char * workPtr = atcmd_getResponse() + file__dataOffset_info;           // skip past +QFLDS: 
        fsInfo->freeSz = strtol(workPtr, &workPtr, 10);  
        fsInfo->totalSz = strtol(++workPtr, &workPtr, 10);                      // incr past comma, then grab total fileSystem size

        // now get file collection info
        if (IS_NOTSUCCESS(atcmd_dispatch("AT+QFLDS")))
            break;

        // parse response
        // +QFLDS: <freesize>,<total_size>
        workPtr = atcmd_getResponse() + file__dataOffset_info;                  // skip past +QFLDS: 
        fsInfo->filesSz = strtol(workPtr, &workPtr, 10);  
        fsInfo->filesCnt = strtol(++workPtr, &workPtr, 10);                     // incr past comma, then grab total fileSystem size
    } while (0);
    return rslt;
}


resultCode_t file_getFilelist(fileListResult_t *fileList, const char* filename)
{
    RSLT;
    do
    {
        if (strlen(filename) == 0)
        {
            fileList->namePattern[0] = '*';
            fileList->namePattern[1] = '\0';
            rslt = atcmd_dispatch("AT+QFLST ");
        }
        else
        {
            memset(fileList->namePattern, 0, sizeof(fileList->namePattern));
            strncpy(fileList->namePattern, filename, MIN(strlen(filename), file__filenameSz));
            rslt = atcmd_dispatch("AT+QFLST=\"%s\"", fileList->namePattern);
        }
        if (rslt != resultCode__success)
        {
            if (strstr(atcmd_getRawResponse(), "+CME ERROR: 417"))
                rslt = resultCode__notFound;
            break;
        }
        // parse response >>  +QFLST: <filename>,<file_size>
        uint8_t lineNm = 0;
        char *workPtr = atcmd_getResponse();

        for (size_t i = 0; i < file__fileListMaxCnt; i++)
        {
            workPtr += 9;
            uint8_t len = strchr(workPtr, '"') - workPtr;
            strncpy(fileList->files[i].filename, workPtr, len);
            workPtr += len + 2;
            fileList->files[i].fileSz = strtol(workPtr, &workPtr, 10);

            workPtr += 2;
            if (*workPtr != '+')
            {
                fileList->fileCnt = ++i;
                break;
            }
        }
    } while (0);
    return rslt;
}


resultCode_t file_open(const char* filename, fileOpenMode_t openMode, uint16_t* fileHandle)
{
    ASSERT(strlen(filename) > 0);                                           // assert user provided a filename

    RSLT;
    do
    {
        if (IS_NOTSUCCESS_RSLT(atcmd_dispatch("AT+QFOPEN=\"%s\",%d", filename, openMode)))
        {
            if (rslt >= resultCode__extendedCodesBase)
            {
                // uint16_t errDetail = atcmd_getErrorDetailCode();
                // if (errDetail == fileErr__detail_fileAlreadyOpen)
                //     rslt = fileErr__result_fileAlreadyOpen;
                // else if (errDetail == fileErr__detail_fileAlreadyOpen)
                //     rslt = fileErr__result_fileAlreadyOpen;
            }
            break;
        }
        // parse response >> +QFOPEN: <filehandle>
        char * workPtr = atcmd_getResponse() + file__dataOffset_open;
        *fileHandle = strtol(workPtr, NULL, 10);
        break;
    } while (0);
    return rslt;
}


/**
 *	@brief Get a list of open files, including their mode and file handles.
 */
resultCode_t file_getOpenFiles(char *fileInfo, uint16_t fileInfoSz)
{
    RSLT;
    atcmd_ovrrdDCmpltTimeout(SEC_TO_MS(2));
    if (IS_SUCCESS_RSLT(atcmd_dispatch("AT+QFOPEN? ")))
    {
        memset(fileInfo, 0, fileInfoSz);

        char * workPtr = atcmd_getResponse();
        while (memcmp(workPtr, "+QFOPEN: ", file__dataOffset_open) == 0)
        {
            workPtr += file__dataOffset_open;
            char * eolPtr = strchr(workPtr, '\r');
            memcpy(fileInfo, workPtr, eolPtr - workPtr);
            fileInfo += eolPtr - workPtr;
            *fileInfo = '\r';
            fileInfo++;
            workPtr = eolPtr + 2;
        }
        return resultCode__success;
    }
    return rslt;
}


/**
 *	@brief Closes the file. 
 */
resultCode_t file_close(uint16_t fileHandle)
{
    return atcmd_dispatch("AT+QFCLOSE=%d", fileHandle);
}


/**
 *	@brief Closes all open files. 
 *  @return ResultCode=200 if successful, otherwise error code (HTTP status type).
 */
resultCode_t file_closeAll()
{
    char openList[file__openFileItemSz * file__openFileMaxCnt];

    if (IS_SUCCESS(file_getOpenFiles(openList, sizeof(openList))))
    {
        char *workPtr = openList;
        while (*workPtr != 0)
        {
            workPtr = memchr(workPtr, ',', file__openFileItemSz) + 1;
            uint8_t fHandle = strtol(workPtr, &workPtr, 10);
            if (fHandle == 0 || fHandle > file__openFileMaxCnt)
            {
                return resultCode__internalError;
            }
            file_close(fHandle);
            workPtr = memchr(workPtr, '\r', file__openFileItemSz) + 2;
        }
        return resultCode__success;
    }
    return resultCode__conflict;
}


resultCode_t file_read(uint16_t fileHandle, uint16_t requestSz, uint16_t* readSz)
{
    ASSERT(g_lqLTEM.fileCtrl->appRecvDataCB);                                   // assert that there is a app func registered to receive read data
    ASSERT(bbffr_getCapacity(g_lqLTEM.iop->rxBffr) > (requestSz + 128));        // ensure ample space in buffer for I/O

    // waiting for "CONNECT #### \r\n" response; dataMode will trim prefix, in handler buffer tail points at read length
    atcmd_configDataMode(g_lqLTEM.fileCtrl, "CONNECT ", atcmd_rxHndlrWithLength, NULL, 0, g_lqLTEM.fileCtrl->appRecvDataCB, false);
    g_lqLTEM.fileCtrl->fileHandle = fileHandle;
    DPRINT_V(0, "(file_read) dataMode configured fHandle=%d\r\n", g_lqLTEM.fileCtrl->fileHandle);

    RSLT;
    if (readSz > 0)
        rslt = atcmd_dispatch("AT+QFREAD=%d,%d", fileHandle, requestSz);
    else
        rslt = atcmd_dispatch("AT+QFREAD=%d", fileHandle);

    lqLOG_VRBS("(file_read) cmd rslt=%d\r\n", rslt);

    if (rslt == resultCode__success)
    {
        lqLOG_VRBS("(file_read) requestSz=%d, readSz=%d\r\n", requestSz, *readSz);

        *readSz = g_lqLTEM.atcmd->dataMode.rxDataSz;
        if (*readSz < requestSz)
            rslt = resultCode__partialContent;                                       // content returned, less than requested
    }
    else 
    {
        *readSz = 0;
        return resultCode__locked;
    }
    return rslt;
}


resultCode_t file_write(uint16_t fileHandle, const char* writeData, uint16_t writeSz, fileWriteResult_t *writeResult)
{
    RSLT;
    do
    {
        atcmd_configDataMode(0, "CONNECT\r\n", atcmd_txHndlrDefault, writeData, writeSz, NULL, false);
        atcmd_ovrrdDCmpltTimeout(SEC_TO_MS(2));
        if (IS_SUCCESS(atcmd_dispatch("AT+QFWRITE=%d,%d,1", fileHandle, writeSz)))
        {
            char* resultTrailer = strchr(atcmd_getRawResponse(), '+');
            if (memcmp(resultTrailer, "+QFWRITE: ", 10) == 0)
            {
                writeResult->writtenSz = strtol(resultTrailer + 10, &resultTrailer, 10);
                writeResult->fileSz = strtol(++resultTrailer, NULL, 10);
                rslt = resultCode__success;
            }
        }
    } while (0);

    return rslt;
}


/**
 *	@brief Set the position of the file pointer.
 */
resultCode_t file_seek(uint16_t fileHandle, uint32_t offset, fileSeekMode_t seekFrom)
{
    return atcmd_dispatch("AT+QFSEEK=%d,%d,%d", fileHandle, offset, seekFrom);
}


resultCode_t file_getPosition(uint16_t fileHandle, uint32_t* filePtr)
{
    RSLT;
    if (IS_SUCCESS_RSLT(atcmd_dispatch("AT+QFPOSITION=%d", fileHandle)))
    {
        char * workPtr = atcmd_getResponse() + file__dataOffset_pos;           // skip past preamble 
        *filePtr = strtol(workPtr, NULL, 10);
    }
    return rslt;
}


/**
 *	@brief Truncate all the data beyond the CURRENT position of the file pointer.
 */
resultCode_t file_truncate(uint16_t fileHandle)
{
    return atcmd_dispatch("AT+QFTUCAT=%d", fileHandle);
}


/**
 *	@brief Delete a file from the file system.
 */
resultCode_t file_delete(const char* filename)
{
    return atcmd_dispatch("AT+QFDEL=\"%s\"", filename);
}


void file_getTsFilename(char* tsFilename, uint8_t fnSize, const char* suffix)
{
    char* timestamp[30] = {0};
    char* srcPtr = timestamp;
    char* destPtr = tsFilename;

    ASSERT(fnSize >= strlen(suffix) + 13);                                  // ensure buffer can hold result

    memset(tsFilename, 0, fnSize);
    strcpy(tsFilename, ltem_getUtcDateTime('c'));

    if (strlen(suffix) > 0)                                                 // add suffix, if provided by caller
        strcat(destPtr,suffix);
}


/* Possible future API methods
*/
// fileUploadResult_t file_upload(const char* filename) {}
// fileDownloadResult_t file_download(const char* filename) {}


#pragma Static Helpers and Response Parsers
/*
 * --------------------------------------------------------------------------------------------- */


static cmdParseRslt_t S__writeStatusParser() 
{
    // +QFWRITE: <written_length>,<total_length>
    return atcmd_stdResponseParser("+QFWRITE: ", true, ",", 0, 1, "\r\n", 0);
}


// /**
//  * @brief Stream RX data handler accepting data length at RX buffer tail.
//  * 
//  * @return resultCode from operation
//  */
// resultCode_t atcmd_rxHndlrWithLength()
// {
//     char wrkBffr[32] = {0};
//     uint16_t lengthEOLAt;
//     // resultCode_t rsltNoError = resultCode__success;                                                         // stage full-read result, but could be partial/no content
//     // fileCtrl_t* fileCtrl = (fileCtrl_t*)g_lqLTEM.atcmd->dataMode.ctrlStruct;                                // cast for convenience
    
//     uint32_t trailerWaitStart = pMillis();
//     do
//     {
//         lengthEOLAt = bbffr_find(g_lqLTEM.iop->rxBffr, "\r", 0, 0, false);                                  // find EOL from CONNECT response

//         if (IS_ELAPSED(trailerWaitStart, streams__lengthWaitDuration))
//             return resultCode__timeout;

//     } while (BBFFR_ISFOUND(lengthEOLAt));
    
   
//     bbffr_pop(g_lqLTEM.iop->rxBffr, wrkBffr, lengthEOLAt + 2);                                              // pop data length and EOL from RX buffer
//     uint16_t readLen = strtol(wrkBffr, NULL, 10);
//     g_lqLTEM.atcmd->retValue = readLen;                                                                     // stash reported read length
//     DPRINT_V(PRNT_CYAN, "(atcmd_rxHndlrWithLength) fHandle=%d available=%d\r", fileCtrl->fileHandle, readLen);

//     uint32_t readTimeout = pMillis();
//     uint16_t bffrOccupiedCnt;
//     do
//     {
//         bffrOccupiedCnt = bbffr_getOccupied(g_lqLTEM.iop->rxBffr);
//         if (pMillis() - readTimeout > g_lqLTEM.atcmd->timeout)
//         {
//             g_lqLTEM.atcmd->retValue = 0;
//             DPRINT(PRNT_WARN, "(atcmd_rxHndlrWithLength) bffr timeout: %d rcvd\r\n", bffrOccupiedCnt);
//             return resultCode__timeout;                                                                     // return timeout waiting for bffr fill
//         }
//     } while (bffrOccupiedCnt < readLen + file__readTrailerSz);
    
//     do                                                                                                      // *NOTE* depending on buffer wrap may take 2 ops
//     {
//         char* streamPtr;
//         uint16_t blockSz = bbffr_popBlock(g_lqLTEM.iop->rxBffr, &streamPtr, readLen);                       // get contiguous block size from rxBffr
//         DPRINT_V(PRNT_CYAN, "(atcmd_rxHndlrWithLength) ptr=%p, bSz=%d, rSz=%d\r", streamPtr, blockSz, readSz);
//         uint8_t fileHandle = ((streamCtrl_t*)g_lqLTEM.atcmd->dataMode.ctrlStruct)->dataCntxt;
//         (*g_lqLTEM.fileCtrl->appRecvDataCB)(fileHandle, streamPtr, blockSz);                                // forward to application
//         bbffr_popBlockFinalize(g_lqLTEM.iop->rxBffr, true);                                                 // commit POP
//         readLen -= blockSz;
//     } while (readLen > 0);

//     if (bbffr_getOccupied(g_lqLTEM.iop->rxBffr) >= file__readTrailerSz)                                     // cleanup, remove trailer
//     {
//         bbffr_skipTail(g_lqLTEM.iop->rxBffr, file__readTrailerSz);
//     }
//     return resultCode__success;
// }


#pragma endregion