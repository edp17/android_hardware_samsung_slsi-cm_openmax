/*
 *
 * Copyright 2013 Samsung Electronics S.LSI Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * @file        Exynos_OMX_HEVCdec.c
 * @brief
 * @author      SeungBeom Kim (sbcrux.kim@samsung.com)
 *              Taehwan Kim (t_h.kim@samsung.com)
 * @version     2.0.0
 * @history
 *   2013.07.26 : Create
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Exynos_OMX_Macros.h"
#include "Exynos_OMX_Basecomponent.h"
#include "Exynos_OMX_Baseport.h"
#include "Exynos_OMX_Vdec.h"
#include "Exynos_OMX_VdecControl.h"
#include "Exynos_OSAL_ETC.h"
#include "Exynos_OSAL_Semaphore.h"
#include "Exynos_OSAL_Thread.h"
#include "library_register.h"
#include "Exynos_OMX_HEVCdec.h"
#include "ExynosVideoApi.h"
#include "Exynos_OSAL_SharedMemory.h"
#include "Exynos_OSAL_Event.h"

#ifdef USE_ANB
#include "Exynos_OSAL_Android.h"
#endif

/* To use CSC_METHOD_HW in EXYNOS OMX */
#include "csc.h"

#undef  EXYNOS_LOG_TAG
#define EXYNOS_LOG_TAG    "EXYNOS_HEVC_DEC"
#define EXYNOS_LOG_OFF
//#define EXYNOS_TRACE_ON
#include "Exynos_OSAL_Log.h"

#define HEVC_DEC_NUM_OF_EXTRA_BUFFERS 7

//#define FULL_FRAME_SEARCH /* Full frame search not support*/

/* HEVC Decoder Supported Levels & profiles */
EXYNOS_OMX_VIDEO_PROFILELEVEL supportedHEVCProfileLevels[] ={
    {OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCLevel1},
    {OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCLevel2},
    {OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCLevel21},
    {OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCLevel3},
    {OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCLevel31},
    {OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCLevel4},
    {OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCLevel41},
    {OMX_VIDEO_HEVCProfileMain, OMX_VIDEO_HEVCLevel5}};

static OMX_ERRORTYPE GetCodecInputPrivateData(
    OMX_PTR      codecBuffer,
    OMX_PTR     *pVirtAddr,
    OMX_U32     *dataSize)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

EXIT:
    return ret;
}

static OMX_ERRORTYPE GetCodecOutputPrivateData(
    OMX_PTR     codecBuffer,
    OMX_PTR     addr[],
    OMX_U32     size[])
{
    OMX_ERRORTYPE       ret             = OMX_ErrorNone;
    ExynosVideoBuffer  *pCodecBuffer    = NULL;

    if (codecBuffer == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pCodecBuffer = (ExynosVideoBuffer *)codecBuffer;

    if (addr != NULL) {
        addr[0] = pCodecBuffer->planes[0].addr;
        addr[1] = pCodecBuffer->planes[1].addr;
        addr[2] = pCodecBuffer->planes[2].addr;
    }

    if (size != NULL) {
        size[0] = pCodecBuffer->planes[0].allocSize;
        size[1] = pCodecBuffer->planes[1].allocSize;
        size[2] = pCodecBuffer->planes[2].allocSize;
    }

EXIT:
    return ret;
}

static int Check_HEVC_Frame(
    OMX_U8   *pInputStream,
    OMX_U32   buffSize,
    OMX_U32   flag,
    OMX_BOOL  bPreviousFrameEOF,
    OMX_BOOL *pbEndOfFrame)
{
    OMX_U32  preFourByte       = (OMX_U32)-1;
    int      accessUnitSize    = 0;
    int      frameTypeBoundary = 0;
    int      nextNaluSize      = 0;
    int      naluStart         = 0;
#if 0
    if (bPreviousFrameEOF == OMX_TRUE)
        naluStart = 0;
    else
        naluStart = 1;

    while (1) {
        int inputOneByte = 0;

        if (accessUnitSize == (int)buffSize)
            goto EXIT;

        inputOneByte = *(pInputStream++);
        accessUnitSize += 1;

        if (preFourByte == 0x00000001 || (preFourByte << 8) == 0x00000100) {
            int naluType = inputOneByte & 0x1F;

            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "NaluType : %d", naluType);
            if (naluStart == 0) {
#ifdef ADD_SPS_PPS_I_FRAME
                if (naluType == 1 || naluType == 5)
#else
                if (naluType == 1 || naluType == 5 || naluType == 7 || naluType == 8)
#endif
                    naluStart = 1;
            } else {
#ifdef OLD_DETECT
                frameTypeBoundary = (8 - naluType) & (naluType - 10); //AUD(9)
#else
                if (naluType == 9)
                    frameTypeBoundary = -2;
#endif
                if (naluType == 1 || naluType == 5) {
                    if (accessUnitSize == (int)buffSize) {
                        accessUnitSize--;
                        goto EXIT;
                    }
                    inputOneByte = *pInputStream++;
                    accessUnitSize += 1;

                    if (inputOneByte >= 0x80)
                        frameTypeBoundary = -1;
                }
                if (frameTypeBoundary < 0) {
                    break;
                }
            }

        }
        preFourByte = (preFourByte << 8) + inputOneByte;
    }

    *pbEndOfFrame = OMX_TRUE;
    nextNaluSize = -5;
    if (frameTypeBoundary == -1)
        nextNaluSize = -6;
    if (preFourByte != 0x00000001)
        nextNaluSize++;
    return (accessUnitSize + nextNaluSize);

EXIT:
    *pbEndOfFrame = OMX_FALSE;
#endif
    return accessUnitSize;
}

static OMX_BOOL Check_HEVC_StartCode(
    OMX_U8     *pInputStream,
    OMX_U32     streamSize)
{
    if (streamSize < 4) {
        return OMX_FALSE;
    }

    if ((pInputStream[0] == 0x00) &&
        (pInputStream[1] == 0x00) &&
        (pInputStream[2] == 0x00) &&
        (pInputStream[3] == 0x01))
        return OMX_TRUE;

    return OMX_FALSE;
}

OMX_BOOL CheckFormatHWSupport(
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent,
    OMX_COLOR_FORMATTYPE         eColorFormat)
{
    OMX_BOOL                         ret            = OMX_FALSE;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec      = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec       = NULL;
    ExynosVideoColorFormatType       eVideoFormat   = VIDEO_CODING_UNKNOWN;
    int i;

    FunctionIn();

    if (pExynosComponent == NULL)
        goto EXIT;

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoDec == NULL)
        goto EXIT;

    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec == NULL)
        goto EXIT;

    eVideoFormat = (ExynosVideoColorFormatType)Exynos_OSAL_OMX2VideoFormat(eColorFormat);

    for (i = 0; i < VIDEO_COLORFORMAT_MAX; i++) {
        if (pHevcDec->hMFCHevcHandle.videoInstInfo.supportFormat[i] == VIDEO_COLORFORMAT_UNKNOWN)
            break;

        if (pHevcDec->hMFCHevcHandle.videoInstInfo.supportFormat[i] == eVideoFormat) {
            ret = OMX_TRUE;
            break;
        }
    }

EXIT:
    return ret;
}

OMX_ERRORTYPE HevcCodecOpen(
    EXYNOS_HEVCDEC_HANDLE *pHevcDec,
    ExynosVideoInstInfo   *pVideoInstInfo)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if (pHevcDec == NULL) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }

    /* alloc ops structure */
    pDecOps    = (ExynosVideoDecOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoDecOps));
    pInbufOps  = (ExynosVideoDecBufferOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoDecBufferOps));
    pOutbufOps = (ExynosVideoDecBufferOps *)Exynos_OSAL_Malloc(sizeof(ExynosVideoDecBufferOps));

    if ((pDecOps == NULL) || (pInbufOps == NULL) || (pOutbufOps == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to allocate decoder ops buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pHevcDec->hMFCHevcHandle.pDecOps    = pDecOps;
    pHevcDec->hMFCHevcHandle.pInbufOps  = pInbufOps;
    pHevcDec->hMFCHevcHandle.pOutbufOps = pOutbufOps;

    /* function pointer mapping */
    pDecOps->nSize    = sizeof(ExynosVideoDecOps);
    pInbufOps->nSize  = sizeof(ExynosVideoDecBufferOps);
    pOutbufOps->nSize = sizeof(ExynosVideoDecBufferOps);

    Exynos_Video_Register_Decoder(pDecOps, pInbufOps, pOutbufOps);

    /* check mandatory functions for decoder ops */
    if ((pDecOps->Init == NULL) || (pDecOps->Finalize == NULL) ||
        (pDecOps->Get_ActualBufferCount == NULL) || (pDecOps->Set_FrameTag == NULL) ||
#ifdef USE_S3D_SUPPORT
        (pDecOps->Enable_SEIParsing == NULL) || (pDecOps->Get_FramePackingInfo == NULL) ||
#endif
        (pDecOps->Get_FrameTag == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Mandatory functions must be supplied");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* check mandatory functions for buffer ops */
    if ((pInbufOps->Setup == NULL) || (pOutbufOps->Setup == NULL) ||
        (pInbufOps->Run == NULL) || (pOutbufOps->Run == NULL) ||
        (pInbufOps->Stop == NULL) || (pOutbufOps->Stop == NULL) ||
        (pInbufOps->Enqueue == NULL) || (pOutbufOps->Enqueue == NULL) ||
        (pInbufOps->Dequeue == NULL) || (pOutbufOps->Dequeue == NULL)) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Mandatory functions must be supplied");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* alloc context, open, querycap */
#ifdef USE_DMA_BUF
    pVideoInstInfo->nMemoryType = V4L2_MEMORY_DMABUF;
#else
    pVideoInstInfo->nMemoryType = V4L2_MEMORY_USERPTR;
#endif
    pHevcDec->hMFCHevcHandle.hMFCHandle = pHevcDec->hMFCHevcHandle.pDecOps->Init(pVideoInstInfo);

    if (pHevcDec->hMFCHevcHandle.hMFCHandle == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to allocate context buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

#ifdef USE_S3D_SUPPORT
    /* S3D: Enable SEI parsing to check Frame Packing */
    if (pDecOps->Enable_SEIParsing(pHevcDec->hMFCHevcHandle.hMFCHandle) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to Enable SEI Parsing");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
#endif

    ret = OMX_ErrorNone;

EXIT:
    if (ret != OMX_ErrorNone) {
        if (pDecOps != NULL) {
            Exynos_OSAL_Free(pDecOps);
            pHevcDec->hMFCHevcHandle.pDecOps = NULL;
        }
        if (pInbufOps != NULL) {
            Exynos_OSAL_Free(pInbufOps);
            pHevcDec->hMFCHevcHandle.pInbufOps = NULL;
        }
        if (pOutbufOps != NULL) {
            Exynos_OSAL_Free(pOutbufOps);
            pHevcDec->hMFCHevcHandle.pOutbufOps = NULL;
        }
    }

    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecClose(EXYNOS_HEVCDEC_HANDLE *pHevcDec)
{
    OMX_ERRORTYPE            ret        = OMX_ErrorNone;
    void                    *hMFCHandle = NULL;
    ExynosVideoDecOps       *pDecOps    = NULL;
    ExynosVideoDecBufferOps *pInbufOps  = NULL;
    ExynosVideoDecBufferOps *pOutbufOps = NULL;

    FunctionIn();

    if (pHevcDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pHevcDec->hMFCHevcHandle.hMFCHandle;
    pDecOps    = pHevcDec->hMFCHevcHandle.pDecOps;
    pInbufOps  = pHevcDec->hMFCHevcHandle.pInbufOps;
    pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;

    if (hMFCHandle != NULL) {
        pDecOps->Finalize(hMFCHandle);
        pHevcDec->hMFCHevcHandle.hMFCHandle = NULL;
    }

    /* Unregister function pointers */
    Exynos_Video_Unregister_Decoder(pDecOps, pInbufOps, pOutbufOps);

    if (pOutbufOps != NULL) {
        Exynos_OSAL_Free(pOutbufOps);
        pHevcDec->hMFCHevcHandle.pOutbufOps = NULL;
    }
    if (pInbufOps != NULL) {
        Exynos_OSAL_Free(pInbufOps);
        pHevcDec->hMFCHevcHandle.pInbufOps = NULL;
    }
    if (pDecOps != NULL) {
        Exynos_OSAL_Free(pDecOps);
        pHevcDec->hMFCHevcHandle.pDecOps = NULL;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecStart(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_U32             nPortIndex)
{
    OMX_ERRORTYPE                    ret        = OMX_ErrorNone;
    void                            *hMFCHandle = NULL;
    ExynosVideoDecBufferOps         *pInbufOps  = NULL;
    ExynosVideoDecBufferOps         *pOutbufOps = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec  = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec   = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)((EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate)->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pHevcDec->hMFCHevcHandle.hMFCHandle;
    pInbufOps  = pHevcDec->hMFCHevcHandle.pInbufOps;
    pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;

    if (nPortIndex == INPUT_PORT_INDEX)
        pInbufOps->Run(hMFCHandle);
    else if (nPortIndex == OUTPUT_PORT_INDEX)
        pOutbufOps->Run(hMFCHandle);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecStop(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_U32             nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = NULL;
    void                            *hMFCHandle         = NULL;
    ExynosVideoDecBufferOps         *pInbufOps          = NULL;
    ExynosVideoDecBufferOps         *pOutbufOps         = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }


    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    hMFCHandle = pHevcDec->hMFCHevcHandle.hMFCHandle;
    pInbufOps  = pHevcDec->hMFCHevcHandle.pInbufOps;
    pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;

    if ((nPortIndex == INPUT_PORT_INDEX) && (pInbufOps != NULL)) {
        pInbufOps->Stop(hMFCHandle);
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) && (pOutbufOps != NULL)) {
        EXYNOS_OMX_BASEPORT *pOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

        pOutbufOps->Stop(hMFCHandle);

        if ((pOutputPort->bufferProcessType & BUFFER_SHARE) &&
            (pOutputPort->bDynamicDPBMode == OMX_TRUE))
            pOutbufOps->Clear_RegisteredBuffer(hMFCHandle);
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecOutputBufferProcessRun(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_U32             nPortIndex)
{
    OMX_ERRORTYPE                    ret        = OMX_ErrorNone;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec  = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec   = NULL;

    FunctionIn();

    if (pOMXComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)((EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate)->hComponentHandle;
    if (pVideoDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if (nPortIndex == INPUT_PORT_INDEX) {
        if (pHevcDec->bSourceStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pHevcDec->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    if (nPortIndex == OUTPUT_PORT_INDEX) {
        if (pHevcDec->bDestinationStart == OMX_FALSE) {
            Exynos_OSAL_SignalSet(pHevcDec->hDestinationStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecRegistCodecBuffers(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex,
    int                  nBufferCnt)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                            *hMFCHandle         = pHevcDec->hMFCHevcHandle.hMFCHandle;
    CODEC_DEC_BUFFER               **ppCodecBuffer      = NULL;
    ExynosVideoDecBufferOps         *pBufOps            = NULL;
    ExynosVideoPlane                *pPlanes            = NULL;

    int nPlaneCnt = 0;
    int i, j;

    FunctionIn();

    if (nPortIndex == INPUT_PORT_INDEX) {
        ppCodecBuffer   = &(pVideoDec->pMFCDecInputBuffer[0]);
        pBufOps         = pHevcDec->hMFCHevcHandle.pInbufOps;
    } else {
        ppCodecBuffer   = &(pVideoDec->pMFCDecOutputBuffer[0]);
        pBufOps         = pHevcDec->hMFCHevcHandle.pOutbufOps;
    }

    nPlaneCnt = Exynos_GetPlaneFromPort(&pExynosComponent->pExynosPort[nPortIndex]);
    pPlanes = (ExynosVideoPlane *)Exynos_OSAL_Malloc(sizeof(ExynosVideoPlane) * nPlaneCnt);
    if (pPlanes == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* Register buffer */
    for (i = 0; i < nBufferCnt; i++) {
        for (j = 0; j < nPlaneCnt; j++) {
            pPlanes[j].addr         = ppCodecBuffer[i]->pVirAddr[j];
            pPlanes[j].fd           = ppCodecBuffer[i]->fd[j];
            pPlanes[j].allocSize    = ppCodecBuffer[i]->bufferSize[j];
        }

        if (pBufOps->Register(hMFCHandle, pPlanes, nPlaneCnt) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "PORT[%d]: Failed to Register buffer", nPortIndex);
            ret = OMX_ErrorInsufficientResources;
            Exynos_OSAL_Free(pPlanes);
            goto EXIT;
        }
    }

    Exynos_OSAL_Free(pPlanes);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecReconfigAllBuffers(
    OMX_COMPONENTTYPE   *pOMXComponent,
    OMX_U32              nPortIndex)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = &pExynosComponent->pExynosPort[nPortIndex];
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                            *hMFCHandle         = pHevcDec->hMFCHevcHandle.hMFCHandle;
    ExynosVideoDecBufferOps         *pBufferOps         = NULL;

    FunctionIn();

    if ((nPortIndex == INPUT_PORT_INDEX) &&
        (pHevcDec->bSourceStart == OMX_TRUE)) {
        ret = OMX_ErrorNotImplemented;
        goto EXIT;
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) &&
               (pHevcDec->bDestinationStart == OMX_TRUE)) {
        pBufferOps  = pHevcDec->hMFCHevcHandle.pOutbufOps;

        if (pExynosPort->bufferProcessType & BUFFER_COPY) {
            /**********************************/
            /* Codec Buffer Free & Unregister */
            /**********************************/
            Exynos_Free_CodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX);
            Exynos_CodecBufferReset(pExynosComponent, OUTPUT_PORT_INDEX);
            pBufferOps->Clear_RegisteredBuffer(hMFCHandle);
            pBufferOps->Cleanup_Buffer(hMFCHandle);

            /******************************************************/
            /* V4L2 Destnation Setup for DPB Buffer Number Change */
            /******************************************************/
            HevcCodecDstSetup(pOMXComponent);

            pVideoDec->bReconfigDPB = OMX_FALSE;
        } else if (pExynosPort->bufferProcessType & BUFFER_SHARE) {
            /**********************************/
            /* Codec Buffer Unregister */
            /**********************************/
            pBufferOps->Clear_RegisteredBuffer(hMFCHandle);
            pBufferOps->Cleanup_Buffer(hMFCHandle);
        }

        Exynos_ResolutionUpdate(pOMXComponent);
    } else {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecEnQueueAllBuffer(
    OMX_COMPONENTTYPE  *pOMXComponent,
    OMX_U32             nPortIndex)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;
    int i;

    ExynosVideoDecOps       *pDecOps    = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pHevcDec->hMFCHevcHandle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;

    FunctionIn();

    if ((nPortIndex != INPUT_PORT_INDEX) && (nPortIndex != OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if ((nPortIndex == INPUT_PORT_INDEX) &&
        (pHevcDec->bSourceStart == OMX_TRUE)) {
        Exynos_CodecBufferReset(pExynosComponent, INPUT_PORT_INDEX);

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pVideoDec->pMFCDecInputBuffer[%d]: 0x%x", i, pVideoDec->pMFCDecInputBuffer[i]);
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pVideoDec->pMFCDecInputBuffer[%d]->pVirAddr[0]: 0x%x", i, pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[0]);

            Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, pVideoDec->pMFCDecInputBuffer[i]);
        }

        pInbufOps->Clear_Queue(hMFCHandle);
    } else if ((nPortIndex == OUTPUT_PORT_INDEX) &&
               (pHevcDec->bDestinationStart == OMX_TRUE)) {
        Exynos_CodecBufferReset(pExynosComponent, OUTPUT_PORT_INDEX);

        for (i = 0; i < pHevcDec->hMFCHevcHandle.maxDPBNum; i++) {
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pVideoDec->pMFCDecOutputBuffer[%d]: 0x%x", i, pVideoDec->pMFCDecOutputBuffer[i]);
            Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "pVideoDec->pMFCDecOutputBuffer[%d]->pVirAddr[0]: 0x%x", i, pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr[0]);

            Exynos_CodecBufferEnQueue(pExynosComponent, OUTPUT_PORT_INDEX, pVideoDec->pMFCDecOutputBuffer[i]);
        }
        pOutbufOps->Clear_Queue(hMFCHandle);
    }

EXIT:
    FunctionOut();

    return ret;
}

#ifdef USE_S3D_SUPPORT
OMX_BOOL HevcCodecCheckFramePacking(OMX_COMPONENTTYPE *pOMXComponent)
{
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    ExynosVideoDecOps             *pDecOps           = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoFramePacking        framePacking;
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;
    OMX_BOOL                       ret               = OMX_FALSE;

    /* Get Frame packing information*/
    if (pDecOps->Get_FramePackingInfo(pHevcDec->hMFCHevcHandle.hMFCHandle, &framePacking) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to Get Frame Packing Information");
        ret = OMX_FALSE;
        goto EXIT;
    }

    if (framePacking.available) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "arrangement ID: 0x%08x", framePacking.arrangement_id);
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "arrangement_type: %d", framePacking.arrangement_type);
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "content_interpretation_type: %d", framePacking.content_interpretation_type);
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "current_frame_is_frame0_flag: %d", framePacking.current_frame_is_frame0_flag);
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "spatial_flipping_flag: %d", framePacking.spatial_flipping_flag);
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "fr0X:%d fr0Y:%d fr0X:%d fr0Y:%d", framePacking.frame0_grid_pos_x,
            framePacking.frame0_grid_pos_y, framePacking.frame1_grid_pos_x, framePacking.frame1_grid_pos_y);

        pHevcDec->hMFCHevcHandle.S3DFPArgmtType = (EXYNOS_OMX_FPARGMT_TYPE) framePacking.arrangement_type;
        /** Send Port Settings changed call back - output color format change */
        (*(pExynosComponent->pCallbacks->EventHandler))
              (pOMXComponent,
               pExynosComponent->callbackData,
               OMX_EventS3DInformation,                             /* The command was completed */
               OMX_TRUE,                                            /* S3D is enabled */
               (OMX_S32)pHevcDec->hMFCHevcHandle.S3DFPArgmtType,    /* S3D FPArgmtType */
               NULL);

        Exynos_OSAL_SleepMillisec(0);
    } else {
        pHevcDec->hMFCHevcHandle.S3DFPArgmtType = OMX_SEC_FPARGMT_NONE;
    }

    ret = OMX_TRUE;

EXIT:
    return ret;
}
#endif

OMX_ERRORTYPE HevcCodecCheckResolutionChange(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec           = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle         = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pInputPort         = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pOutputPort        = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    ExynosVideoDecOps             *pDecOps            = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoDecBufferOps       *pOutbufOps         = pHevcDec->hMFCHevcHandle.pOutbufOps;
    ExynosVideoErrorType           codecRet           = VIDEO_ERROR_NONE;

    OMX_CONFIG_RECTTYPE          *pCropRectangle        = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE *pInputPortDefinition  = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE *pOutputPortDefinition = NULL;

    FunctionIn();

    /* get geometry for output */
    Exynos_OSAL_Memset(&pHevcDec->hMFCHevcHandle.codecOutbufConf, 0, sizeof(ExynosVideoGeometry));
    codecRet = pOutbufOps->Get_Geometry(hMFCHandle, &pHevcDec->hMFCHevcHandle.codecOutbufConf);
    if (codecRet ==  VIDEO_ERROR_HEADERINFO) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "parsed header info has only VPS");
        ret = OMX_ErrorNeedNextHeaderInfo;
        goto EXIT;
    } else if (codecRet != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to get geometry for parsed header info");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* get dpb count */
    pHevcDec->hMFCHevcHandle.maxDPBNum = pDecOps->Get_ActualBufferCount(hMFCHandle);
    if (pVideoDec->bThumbnailMode == OMX_FALSE)
        pHevcDec->hMFCHevcHandle.maxDPBNum += EXTRA_DPB_NUM;
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "HevcCodecSetup nOutbufs: %d", pHevcDec->hMFCHevcHandle.maxDPBNum);

    pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc = OMX_TRUE;

    if (pVideoDec->bReconfigDPB != OMX_TRUE) {
        pCropRectangle          = &(pOutputPort->cropRectangle);
        pInputPortDefinition    = &(pInputPort->portDefinition);
        pOutputPortDefinition   = &(pOutputPort->portDefinition);
    } else {
        pCropRectangle          = &(pOutputPort->newCropRectangle);
        pInputPortDefinition    = &(pInputPort->newPortDefinition);
        pOutputPortDefinition   = &(pOutputPort->newPortDefinition);
    }

    pCropRectangle->nTop     = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nTop;
    pCropRectangle->nLeft    = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nLeft;
    pCropRectangle->nWidth   = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nWidth;
    pCropRectangle->nHeight  = pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nHeight;

    if (pOutputPort->bufferProcessType & BUFFER_COPY) {
        if ((pVideoDec->bReconfigDPB) ||
            (pInputPort->portDefinition.format.video.nFrameWidth != pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth) ||
            (pInputPort->portDefinition.format.video.nFrameHeight != pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight)) {
            pOutputPort->exceptionFlag = NEED_PORT_DISABLE;

            pInputPortDefinition->format.video.nFrameWidth   = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth;
            pInputPortDefinition->format.video.nFrameHeight  = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight;
            pInputPortDefinition->format.video.nStride       = ((pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth + 15) & (~15));
            pInputPortDefinition->format.video.nSliceHeight  = ((pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight + 15) & (~15));

            pOutputPortDefinition->nBufferCountActual  = pOutputPort->portDefinition.nBufferCountActual;
            pOutputPortDefinition->nBufferCountMin     = pOutputPort->portDefinition.nBufferCountMin;

            if (pVideoDec->bReconfigDPB != OMX_TRUE)
                Exynos_UpdateFrameSize(pOMXComponent);

            /** Send Port Settings changed call back **/
            (*(pExynosComponent->pCallbacks->EventHandler))
                (pOMXComponent,
                 pExynosComponent->callbackData,
                 OMX_EventPortSettingsChanged, /* The command was completed */
                 OMX_DirOutput, /* This is the port index */
                 0,
                 NULL);
        }
    } else if (pOutputPort->bufferProcessType & BUFFER_SHARE) {
        if ((pVideoDec->bReconfigDPB) ||
            (pInputPort->portDefinition.format.video.nFrameWidth != pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth) ||
            (pInputPort->portDefinition.format.video.nFrameHeight != pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight) ||
            ((OMX_S32)pOutputPort->portDefinition.nBufferCountActual != pHevcDec->hMFCHevcHandle.maxDPBNum)) {
            pOutputPort->exceptionFlag = NEED_PORT_DISABLE;

            pInputPortDefinition->format.video.nFrameWidth   = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth;
            pInputPortDefinition->format.video.nFrameHeight  = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight;
            pInputPortDefinition->format.video.nStride       = ((pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth + 15) & (~15));
            pInputPortDefinition->format.video.nSliceHeight  = ((pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight + 15) & (~15));

            pOutputPortDefinition->nBufferCountActual    = pHevcDec->hMFCHevcHandle.maxDPBNum;
            pOutputPortDefinition->nBufferCountMin       = pHevcDec->hMFCHevcHandle.maxDPBNum;

            if (pVideoDec->bReconfigDPB != OMX_TRUE)
                Exynos_UpdateFrameSize(pOMXComponent);

            /** Send Port Settings changed call back **/
            (*(pExynosComponent->pCallbacks->EventHandler))
                (pOMXComponent,
                 pExynosComponent->callbackData,
                 OMX_EventPortSettingsChanged, /* The command was completed */
                 OMX_DirOutput, /* This is the port index */
                 0,
                 NULL);
        }
    }

    if ((pVideoDec->bReconfigDPB == OMX_TRUE) ||
        (pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth != pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nWidth) ||
        (pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight != pHevcDec->hMFCHevcHandle.codecOutbufConf.cropRect.nHeight)) {
        /* Check Crop */
        pInputPort->portDefinition.format.video.nFrameWidth     = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth;
        pInputPort->portDefinition.format.video.nFrameHeight    = pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight;
        pInputPort->portDefinition.format.video.nStride         = ((pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameWidth + 15) & (~15));
        pInputPort->portDefinition.format.video.nSliceHeight    = ((pHevcDec->hMFCHevcHandle.codecOutbufConf.nFrameHeight + 15) & (~15));

        if (pVideoDec->bReconfigDPB != OMX_TRUE)
            Exynos_UpdateFrameSize(pOMXComponent);

        /** Send crop info call back **/
        (*(pExynosComponent->pCallbacks->EventHandler))
            (pOMXComponent,
             pExynosComponent->callbackData,
             OMX_EventPortSettingsChanged, /* The command was completed */
             OMX_DirOutput, /* This is the port index */
             OMX_IndexConfigCommonOutputCrop,
             NULL);
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecSrcSetup(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    OMX_U32                        oneFrameSize      = pSrcInputData->dataLen;
    OMX_COLOR_FORMATTYPE           eOutputFormat     = pExynosOutputPort->portDefinition.format.video.eColorFormat;

    ExynosVideoDecOps       *pDecOps    = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps  = pHevcDec->hMFCHevcHandle.pInbufOps;
    ExynosVideoDecBufferOps *pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;
    ExynosVideoGeometry      bufferConf;

    OMX_U32  nInBufferCnt   = 0;
    OMX_BOOL bSupportFormat = OMX_FALSE;
    int i;

    FunctionIn();

    if ((oneFrameSize <= 0) &&
        (pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS)) {
        BYPASS_BUFFER_INFO *pBufferInfo = (BYPASS_BUFFER_INFO *)Exynos_OSAL_Malloc(sizeof(BYPASS_BUFFER_INFO));
        if (pBufferInfo == NULL) {
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        pBufferInfo->nFlags     = pSrcInputData->nFlags;
        pBufferInfo->timeStamp  = pSrcInputData->timeStamp;
        ret = Exynos_OSAL_Queue(&pHevcDec->bypassBufferInfoQ, (void *)pBufferInfo);
        Exynos_OSAL_SignalSet(pHevcDec->hDestinationStartEvent);

        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pVideoDec->bThumbnailMode == OMX_TRUE)
        pDecOps->Set_IFrameDecoding(hMFCHandle);

    if ((pDecOps->Enable_DTSMode != NULL) &&
        (pVideoDec->bDTSMode == OMX_TRUE))
        pDecOps->Enable_DTSMode(hMFCHandle);

    /* input buffer info */
    Exynos_OSAL_Memset(&bufferConf, 0, sizeof(bufferConf));
    bufferConf.eCompressionFormat = VIDEO_CODING_HEVC;
    pInbufOps->Set_Shareable(hMFCHandle);
    if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
        bufferConf.nSizeImage = pExynosInputPort->portDefinition.format.video.nFrameWidth
                                * pExynosInputPort->portDefinition.format.video.nFrameHeight * 3 / 2;
    } else if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        bufferConf.nSizeImage = DEFAULT_MFC_INPUT_BUFFER_SIZE;
    }
    bufferConf.nPlaneCnt = Exynos_GetPlaneFromPort(pExynosInputPort);
    nInBufferCnt = MAX_INPUTBUFFER_NUM_DYNAMIC;

    /* should be done before prepare input buffer */
    if (pInbufOps->Enable_Cacheable(hMFCHandle) != VIDEO_ERROR_NONE) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* set input buffer geometry */
    if (pInbufOps->Set_Geometry(hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to set geometry for input buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* setup input buffer */
    if (pInbufOps->Setup(hMFCHandle, nInBufferCnt) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to setup input buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* set output geometry */
    Exynos_OSAL_Memset(&bufferConf, 0, sizeof(bufferConf));

    bSupportFormat = CheckFormatHWSupport(pExynosComponent, eOutputFormat);
    if (bSupportFormat == OMX_TRUE) {  /* supported by H/W */
        bufferConf.eColorFormat = Exynos_OSAL_OMX2VideoFormat(eOutputFormat);
        Exynos_SetPlaneToPort(pExynosOutputPort, Exynos_OSAL_GetPlaneCount(eOutputFormat));
    } else {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Can not support this format (0x%x)", eOutputFormat);
        ret = OMX_ErrorNotImplemented;
        goto EXIT;
    }

    pHevcDec->hMFCHevcHandle.MFCOutputColorType = bufferConf.eColorFormat;
    bufferConf.nPlaneCnt = Exynos_GetPlaneFromPort(pExynosOutputPort);
    if (pOutbufOps->Set_Geometry(hMFCHandle, &bufferConf) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to set geometry for output buffer");
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* input buffer enqueue for header parsing */
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "oneFrameSize: %d", oneFrameSize);
    OMX_U32 nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    if (pExynosInputPort->bufferProcessType & BUFFER_SHARE)
        nAllocLen[0] = pSrcInputData->bufferHeader->nAllocLen;
    else if (pExynosInputPort->bufferProcessType & BUFFER_COPY)
        nAllocLen[0] = DEFAULT_MFC_INPUT_BUFFER_SIZE;

    if (pInbufOps->ExtensionEnqueue(hMFCHandle,
                            (void **)pSrcInputData->multiPlaneBuffer.dataBuffer,
                            (int *)pSrcInputData->multiPlaneBuffer.fd,
                            (unsigned long *)nAllocLen,
                            (unsigned long *)&oneFrameSize,
                            Exynos_GetPlaneFromPort(pExynosInputPort),
                            pSrcInputData->bufferHeader) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to enqueue input buffer for header parsing");
//        ret = OMX_ErrorInsufficientResources;
        ret = (OMX_ERRORTYPE)OMX_ErrorCodecInit;
        goto EXIT;
    }

    /* start header parsing */
    if (pInbufOps->Run(hMFCHandle) != VIDEO_ERROR_NONE) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to run input buffer for header parsing");
        ret = OMX_ErrorCodecInit;
        goto EXIT;
    }

    ret = HevcCodecCheckResolutionChange(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        HevcCodecStop(pOMXComponent, INPUT_PORT_INDEX);
        pInbufOps->Cleanup_Buffer(hMFCHandle);

        if ((EXYNOS_OMX_ERRORTYPE)ret == OMX_ErrorNeedNextHeaderInfo) {
            if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
                EXYNOS_OMX_DATABUFFER directReturnUseBuffer;
                Exynos_Shared_DataToBuffer(pSrcInputData, &directReturnUseBuffer);
                Exynos_InputBufferReturn(pOMXComponent, &directReturnUseBuffer);
            } else if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
                OMX_PTR codecBuffer;
                codecBuffer = pSrcInputData->pPrivate;
                if (codecBuffer != NULL)
                    Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, codecBuffer);
            }
        }

        goto EXIT;
    }

    Exynos_OSAL_SleepMillisec(0);
    /*  disable header info re-input scheme
      ret = OMX_ErrorInputDataDecodeYet;
      HevcCodecStop(pOMXComponent, INPUT_PORT_INDEX);
    */

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE HevcCodecDstSetup(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret                  = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent     = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec            = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec             = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle           = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort    = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    ExynosVideoDecOps           *pDecOps    = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoDecBufferOps     *pOutbufOps = pHevcDec->hMFCHevcHandle.pOutbufOps;

    int i, nOutbufs, nPlaneCnt;

    OMX_U32 nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    OMX_U32 dataLen[MAX_BUFFER_PLANE]   = {0, 0, 0};

    FunctionIn();

    nPlaneCnt = Exynos_GetPlaneFromPort(pExynosOutputPort);
    for (i = 0; i < nPlaneCnt; i++)
        nAllocLen[i] = pHevcDec->hMFCHevcHandle.codecOutbufConf.nAlignPlaneSize[i];

    if (pExynosOutputPort->bDynamicDPBMode == OMX_TRUE) {
        if (pDecOps->Enable_DynamicDPB(hMFCHandle) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to enable Dynamic DPB");
            ret = OMX_ErrorHardware;
            goto EXIT;
        }
    }

    pOutbufOps->Set_Shareable(hMFCHandle);

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        /* should be done before prepare output buffer */
        if (pOutbufOps->Enable_Cacheable(hMFCHandle) != VIDEO_ERROR_NONE) {
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        if (pExynosOutputPort->bDynamicDPBMode == OMX_FALSE) {
            /* get dpb count */
            nOutbufs = pHevcDec->hMFCHevcHandle.maxDPBNum;
            if (pOutbufOps->Setup(hMFCHandle, nOutbufs) != VIDEO_ERROR_NONE) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to setup output buffer");
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }

            ret = Exynos_Allocate_CodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX, nOutbufs, nAllocLen);
            if (ret != OMX_ErrorNone)
                goto EXIT;

            /* Register output buffer */
            ret = HevcCodecRegistCodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX, nOutbufs);
            if (ret != OMX_ErrorNone)
                goto EXIT;

            /* Enqueue output buffer */
            for (i = 0; i < nOutbufs; i++)
                pOutbufOps->Enqueue(hMFCHandle, (void **)pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr,
                                (unsigned long *)dataLen, nPlaneCnt, NULL);
        } else {
            if (pOutbufOps->Setup(hMFCHandle, MAX_OUTPUTBUFFER_NUM_DYNAMIC) != VIDEO_ERROR_NONE) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to setup output buffer");
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }

            /* get dpb count */
            nOutbufs = pHevcDec->hMFCHevcHandle.maxDPBNum;
            ret = Exynos_Allocate_CodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX, nOutbufs, nAllocLen);
            if (ret != OMX_ErrorNone)
                goto EXIT;

            /* without Register output buffer */

            /* Enqueue output buffer */
            for (i = 0; i < nOutbufs; i++) {
                pOutbufOps->ExtensionEnqueue(hMFCHandle,
                                (void **)pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr,
                                (int *)pVideoDec->pMFCDecOutputBuffer[i]->fd,
                                (unsigned long *)pVideoDec->pMFCDecOutputBuffer[i]->bufferSize,
                                (unsigned long *)dataLen, nPlaneCnt, NULL);
            }
        }

        if (pOutbufOps->Run(hMFCHandle) != VIDEO_ERROR_NONE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to run output buffer");
            ret = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    } else if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
#ifdef USE_ANB
        if (pExynosOutputPort->bDynamicDPBMode == OMX_FALSE) {
            ExynosVideoPlane planes[MAX_BUFFER_PLANE];
            int plane;

            Exynos_OSAL_Memset((OMX_PTR)planes, 0, sizeof(ExynosVideoPlane) * MAX_BUFFER_PLANE);

            /* get dpb count */
            nOutbufs = pExynosOutputPort->portDefinition.nBufferCountActual;
            if (pOutbufOps->Setup(hMFCHandle, nOutbufs) != VIDEO_ERROR_NONE) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to setup output buffer");
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }

            if ((pExynosOutputPort->bIsANBEnabled == OMX_TRUE) &&
                (pExynosOutputPort->bStoreMetaData == OMX_FALSE)) {
                for (i = 0; i < pExynosOutputPort->assignedBufferNum; i++) {
                    for (plane = 0; plane < nPlaneCnt; plane++) {
                        planes[plane].fd = pExynosOutputPort->extendBufferHeader[i].buf_fd[plane];
                        planes[plane].addr = pExynosOutputPort->extendBufferHeader[i].pYUVBuf[plane];
                        planes[plane].allocSize = nAllocLen[plane];
                    }

                    if (pOutbufOps->Register(hMFCHandle, planes, nPlaneCnt) != VIDEO_ERROR_NONE) {
                        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to Register output buffer");
                        ret = OMX_ErrorInsufficientResources;
                        goto EXIT;
                    }
                    pOutbufOps->Enqueue(hMFCHandle, (void **)pExynosOutputPort->extendBufferHeader[i].pYUVBuf,
                                   (unsigned long *)dataLen, nPlaneCnt, NULL);
                }

                if (pOutbufOps->Apply_RegisteredBuffer(hMFCHandle) != VIDEO_ERROR_NONE) {
                    Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to Apply output buffer");
                    ret = OMX_ErrorHardware;
                    goto EXIT;
                }
            } else {
                /*************/
                /*    TBD    */
                /*************/
                ret = OMX_ErrorNotImplemented;
                goto EXIT;
            }
        } else {
            /* get dpb count */
            nOutbufs = MAX_OUTPUTBUFFER_NUM_DYNAMIC;
            if (pOutbufOps->Setup(hMFCHandle, nOutbufs) != VIDEO_ERROR_NONE) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to setup output buffer");
                ret = OMX_ErrorInsufficientResources;
                goto EXIT;
            }

            if ((pExynosOutputPort->bIsANBEnabled == OMX_FALSE) &&
                (pExynosOutputPort->bStoreMetaData == OMX_FALSE)) {
                /*************/
                /*    TBD    */
                /*************/
                ret = OMX_ErrorNotImplemented;
                goto EXIT;
            }
        }
#else
        /*************/
        /*    TBD    */
        /*************/
        ret = OMX_ErrorNotImplemented;
        goto EXIT;
#endif
    }

    pHevcDec->hMFCHevcHandle.bConfiguredMFCDst = OMX_TRUE;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_GetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nParamIndex,
    OMX_INOUT OMX_PTR     pComponentParameterStructure)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE           *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch ((int)nParamIndex) {
    case OMX_IndexParamVideoHevc:
    {
        OMX_VIDEO_PARAM_HEVCTYPE *pDstHevcComponent = (OMX_VIDEO_PARAM_HEVCTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_HEVCTYPE *pSrcHevcComponent = NULL;
        EXYNOS_HEVCDEC_HANDLE    *pHevcDec          = NULL;
        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pDstHevcComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstHevcComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pSrcHevcComponent = &pHevcDec->HevcComponent[pDstHevcComponent->nPortIndex];

        Exynos_OSAL_Memcpy(((char *)pDstHevcComponent) + nOffset,
                           ((char *)pSrcHevcComponent) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_HEVCTYPE) - nOffset);
    }
        break;
    case OMX_IndexParamStandardComponentRole:
    {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE *)pComponentParameterStructure;
        ret = Exynos_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        Exynos_OSAL_Strcpy((char *)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_HEVC_DEC_ROLE);
    }
        break;
    case OMX_IndexParamVideoProfileLevelQuerySupported:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE    *pDstProfileLevel   = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        EXYNOS_OMX_VIDEO_PROFILELEVEL       *pProfileLevel      = NULL;
        OMX_U32                              maxProfileLevelNum = 0;

        ret = Exynos_OMX_Check_SizeVersion(pDstProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pProfileLevel = supportedHEVCProfileLevels;
        maxProfileLevelNum = sizeof(supportedHEVCProfileLevels) / sizeof(EXYNOS_OMX_VIDEO_PROFILELEVEL);

        if (pDstProfileLevel->nProfileIndex >= maxProfileLevelNum) {
            ret = OMX_ErrorNoMore;
            goto EXIT;
        }

        pProfileLevel += pDstProfileLevel->nProfileIndex;
        pDstProfileLevel->eProfile = pProfileLevel->profile;
        pDstProfileLevel->eLevel = pProfileLevel->level;
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pDstProfileLevel  = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_HEVCTYPE         *pSrcHevcComponent = NULL;
        EXYNOS_HEVCDEC_HANDLE            *pHevcDec          = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pSrcHevcComponent = &pHevcDec->HevcComponent[pDstProfileLevel->nPortIndex];

        pDstProfileLevel->eProfile = pSrcHevcComponent->eProfile;
        pDstProfileLevel->eLevel = pSrcHevcComponent->eLevel;
    }
        break;
    case OMX_IndexParamVideoErrorCorrection:
    {
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pDstErrorCorrectionType = (OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pSrcErrorCorrectionType = NULL;
        EXYNOS_HEVCDEC_HANDLE               *pHevcDec                = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pDstErrorCorrectionType, sizeof(OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pDstErrorCorrectionType->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pSrcErrorCorrectionType = &pHevcDec->errorCorrectionType[INPUT_PORT_INDEX];

        pDstErrorCorrectionType->bEnableHEC                 = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync              = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing      = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning    = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC                = pSrcErrorCorrectionType->bEnableRVLC;
    }
        break;
    case OMX_IndexParamVideoPortFormat:
    {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *pPortFormat         = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32                         nPortIndex          = pPortFormat->nPortIndex;
        OMX_U32                         nIndex              = pPortFormat->nIndex;
        EXYNOS_OMX_BASEPORT            *pExynosPort         = NULL;
        OMX_PARAM_PORTDEFINITIONTYPE   *pPortDef            = NULL;
        OMX_U32                         nSupportFormatCnt   = 0; /* supportFormatNum = N-1 */

        ret = Exynos_OMX_Check_SizeVersion(pPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((nPortIndex >= pExynosComponent->portParam.nPorts)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        if (nPortIndex == INPUT_PORT_INDEX) {
            nSupportFormatCnt = INPUT_PORT_SUPPORTFORMAT_NUM_MAX - 1;
            if (nIndex > nSupportFormatCnt) {
                ret = OMX_ErrorNoMore;
                goto EXIT;
            }

            pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
            pPortDef    = &pExynosPort->portDefinition;

            pPortFormat->eCompressionFormat = pPortDef->format.video.eCompressionFormat;
            pPortFormat->eColorFormat       = pPortDef->format.video.eColorFormat;
            pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
        } else if (nPortIndex == OUTPUT_PORT_INDEX) {
            pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
            pPortDef    = &pExynosPort->portDefinition;
#ifdef USE_ANB
            if (pExynosPort->bIsANBEnabled == OMX_FALSE)
#endif
            {
                switch (nIndex) {
                case supportFormat_0:
                    pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                    pPortFormat->eColorFormat       = OMX_COLOR_FormatYUV420Planar;
                    pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
                    break;
                case supportFormat_1:
                    pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                    pPortFormat->eColorFormat       = OMX_COLOR_FormatYUV420SemiPlanar;
                    pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
                    break;
                case supportFormat_3:
                    pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                    pPortFormat->eColorFormat       = OMX_SEC_COLOR_FormatNV21Linear;
                    pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
                    break;
                case supportFormat_4:
                    pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                    pPortFormat->eColorFormat       = OMX_SEC_COLOR_FormatYVU420Planar;
                    pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
                    break;
                default:
                    if (nIndex > supportFormat_0) {
                        ret = OMX_ErrorNoMore;
                        goto EXIT;
                    }
                    break;
                }
            }
#ifdef USE_ANB
             else {
                switch (nIndex) {
                case supportFormat_0:
                    pPortFormat->eCompressionFormat = OMX_VIDEO_CodingUnused;
                    pPortFormat->eColorFormat       = OMX_COLOR_FormatYUV420SemiPlanar;
                    pPortFormat->xFramerate         = pPortDef->format.video.xFramerate;
                    break;
                default:
                    if (nIndex > supportFormat_0) {
                        ret = OMX_ErrorNoMore;
                        goto EXIT;
                    }
                    break;
                }
            }
#endif
        }
        ret = OMX_ErrorNone;
    }
        break;
    default:
        ret = Exynos_OMX_VideoDecodeGetParameter(hComponent, nParamIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_SetParameter(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_IN OMX_INDEXTYPE  nIndex,
    OMX_IN OMX_PTR        pComponentParameterStructure)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent     = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentParameterStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch ((int)nIndex) {
    case OMX_IndexParamVideoHevc:
    {
        OMX_VIDEO_PARAM_HEVCTYPE *pDstHevcComponent = NULL;
        OMX_VIDEO_PARAM_HEVCTYPE *pSrcHevcComponent = (OMX_VIDEO_PARAM_HEVCTYPE *)pComponentParameterStructure;
        EXYNOS_HEVCDEC_HANDLE    *pHevcDec          = NULL;
        /* except nSize, nVersion and nPortIndex */
        int nOffset = sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE) + sizeof(OMX_U32);

        ret = Exynos_OMX_Check_SizeVersion(pSrcHevcComponent, sizeof(OMX_VIDEO_PARAM_HEVCTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pSrcHevcComponent->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pDstHevcComponent = &pHevcDec->HevcComponent[pSrcHevcComponent->nPortIndex];

        Exynos_OSAL_Memcpy(((char *)pDstHevcComponent) + nOffset,
                           ((char *)pSrcHevcComponent) + nOffset,
                           sizeof(OMX_VIDEO_PARAM_HEVCTYPE) - nOffset);
    }
        break;
    case OMX_IndexParamStandardComponentRole:
    {
        OMX_PARAM_COMPONENTROLETYPE *pComponentRole = (OMX_PARAM_COMPONENTROLETYPE*)pComponentParameterStructure;

        ret = Exynos_OMX_Check_SizeVersion(pComponentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if ((pExynosComponent->currentState != OMX_StateLoaded) && (pExynosComponent->currentState != OMX_StateWaitForResources)) {
            ret = OMX_ErrorIncorrectStateOperation;
            goto EXIT;
        }

        if (!Exynos_OSAL_Strcmp((char*)pComponentRole->cRole, EXYNOS_OMX_COMPONENT_HEVC_DEC_ROLE)) {
            pExynosComponent->pExynosPort[INPUT_PORT_INDEX].portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVendorHEVC;
        } else {
            ret = OMX_ErrorBadParameter;
            goto EXIT;
        }
    }
        break;
    case OMX_IndexParamVideoProfileLevelCurrent:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *pSrcProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_HEVCTYPE         *pDstHevcComponent = NULL;
        EXYNOS_HEVCDEC_HANDLE            *pHevcDec = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcProfileLevel, sizeof(OMX_VIDEO_PARAM_PROFILELEVELTYPE));
        if (ret != OMX_ErrorNone)
            goto EXIT;

        if (pSrcProfileLevel->nPortIndex >= ALL_PORT_NUM) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;

        pDstHevcComponent = &pHevcDec->HevcComponent[pSrcProfileLevel->nPortIndex];
        pDstHevcComponent->eProfile = pSrcProfileLevel->eProfile;
        pDstHevcComponent->eLevel   = pSrcProfileLevel->eLevel;
    }
        break;
    case OMX_IndexParamVideoErrorCorrection:
    {
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pSrcErrorCorrectionType = (OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *)pComponentParameterStructure;
        OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE *pDstErrorCorrectionType = NULL;
        EXYNOS_HEVCDEC_HANDLE               *pHevcDec                = NULL;

        ret = Exynos_OMX_Check_SizeVersion(pSrcErrorCorrectionType, sizeof(OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE));
        if (ret != OMX_ErrorNone) {
            goto EXIT;
        }

        if (pSrcErrorCorrectionType->nPortIndex != INPUT_PORT_INDEX) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
        pDstErrorCorrectionType = &pHevcDec->errorCorrectionType[INPUT_PORT_INDEX];

        pDstErrorCorrectionType->bEnableHEC                 = pSrcErrorCorrectionType->bEnableHEC;
        pDstErrorCorrectionType->bEnableResync              = pSrcErrorCorrectionType->bEnableResync;
        pDstErrorCorrectionType->nResynchMarkerSpacing      = pSrcErrorCorrectionType->nResynchMarkerSpacing;
        pDstErrorCorrectionType->bEnableDataPartitioning    = pSrcErrorCorrectionType->bEnableDataPartitioning;
        pDstErrorCorrectionType->bEnableRVLC                = pSrcErrorCorrectionType->bEnableRVLC;
    }
        break;
    default:
        ret = Exynos_OMX_VideoDecodeSetParameter(hComponent, nIndex, pComponentParameterStructure);
        break;
    }
EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_GetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nIndex,
    OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentConfigStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch ((int)nIndex) {
    case OMX_IndexConfigCommonOutputCrop:
    {
        EXYNOS_HEVCDEC_HANDLE  *pHevcDec     = NULL;
        OMX_CONFIG_RECTTYPE    *pSrcRectType = NULL;
        OMX_CONFIG_RECTTYPE    *pDstRectType = NULL;
        pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;

        if (pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc == OMX_FALSE) {
            ret = OMX_ErrorNotReady;
            break;
        }

        pDstRectType = (OMX_CONFIG_RECTTYPE *)pComponentConfigStructure;

        if ((pDstRectType->nPortIndex != INPUT_PORT_INDEX) &&
            (pDstRectType->nPortIndex != OUTPUT_PORT_INDEX)) {
            ret = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        EXYNOS_OMX_BASEPORT *pExynosPort = &pExynosComponent->pExynosPort[pDstRectType->nPortIndex];

        pSrcRectType = &(pExynosPort->cropRectangle);

        pDstRectType->nTop    = pSrcRectType->nTop;
        pDstRectType->nLeft   = pSrcRectType->nLeft;
        pDstRectType->nHeight = pSrcRectType->nHeight;
        pDstRectType->nWidth  = pSrcRectType->nWidth;
    }
        break;
#ifdef USE_S3D_SUPPORT
    case OMX_IndexVendorS3DMode:
    {
        EXYNOS_HEVCDEC_HANDLE  *pHevcDec = NULL;
        OMX_U32                *pS3DMode = NULL;
        pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;

        pS3DMode = (OMX_U32 *)pComponentConfigStructure;
        *pS3DMode = (OMX_U32) pHevcDec->hMFCHevcHandle.S3DFPArgmtType;
    }
        break;
#endif
    default:
        ret = Exynos_OMX_VideoDecodeGetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_SetConfig(
    OMX_HANDLETYPE hComponent,
    OMX_INDEXTYPE  nIndex,
    OMX_PTR        pComponentConfigStructure)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL || pComponentConfigStructure == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

    switch (nIndex) {
    default:
        ret = Exynos_OMX_VideoDecodeSetConfig(hComponent, nIndex, pComponentConfigStructure);
        break;
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_GetExtensionIndex(
    OMX_IN  OMX_HANDLETYPE  hComponent,
    OMX_IN  OMX_STRING      cParameterName,
    OMX_OUT OMX_INDEXTYPE  *pIndexType)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_Check_SizeVersion(pOMXComponent, sizeof(OMX_COMPONENTTYPE));
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }
    if (pOMXComponent->pComponentPrivate == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    if ((cParameterName == NULL) || (pIndexType == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (pExynosComponent->currentState == OMX_StateInvalid) {
        ret = OMX_ErrorInvalidState;
        goto EXIT;
    }

#ifdef USE_S3D_SUPPORT
    if (Exynos_OSAL_Strcmp(cParameterName, EXYNOS_INDEX_PARAM_GET_S3D) == 0) {
        *pIndexType = OMX_IndexVendorS3DMode;
        ret = OMX_ErrorNone;
        goto EXIT;
    }
#endif

    ret = Exynos_OMX_VideoDecodeGetExtensionIndex(hComponent, cParameterName, pIndexType);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_ComponentRoleEnum(
    OMX_HANDLETYPE hComponent,
    OMX_U8        *cRole,
    OMX_U32        nIndex)
{
    OMX_ERRORTYPE             ret              = OMX_ErrorNone;
    OMX_COMPONENTTYPE        *pOMXComponent    = NULL;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent = NULL;

    FunctionIn();

    if ((hComponent == NULL) || (cRole == NULL)) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if (nIndex == (MAX_COMPONENT_ROLE_NUM-1)) {
        Exynos_OSAL_Strcpy((char *)cRole, EXYNOS_OMX_COMPONENT_HEVC_DEC_ROLE);
        ret = OMX_ErrorNone;
    } else {
        ret = OMX_ErrorNoMore;
    }

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Init */
OMX_ERRORTYPE Exynos_HevcDec_Init(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;

    ExynosVideoInstInfo *pVideoInstInfo = &(pHevcDec->hMFCHevcHandle.videoInstInfo);
    CSC_METHOD csc_method = CSC_METHOD_SW;
    int i, plane;

    FunctionIn();

    pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc = OMX_FALSE;
    pHevcDec->hMFCHevcHandle.bConfiguredMFCDst = OMX_FALSE;
    pExynosComponent->bUseFlagEOF  = OMX_TRUE;
    pExynosComponent->bSaveFlagEOS = OMX_FALSE;
    pExynosComponent->bBehaviorEOS = OMX_FALSE;

    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, " CodecOpen W: %d H:%d  Bitrate:%d FPS:%d", pExynosInputPort->portDefinition.format.video.nFrameWidth,
                                                                                  pExynosInputPort->portDefinition.format.video.nFrameHeight,
                                                                                  pExynosInputPort->portDefinition.format.video.nBitrate,
                                                                                  pExynosInputPort->portDefinition.format.video.xFramerate);
    pVideoInstInfo->nSize         = sizeof(ExynosVideoInstInfo);
    pVideoInstInfo->nWidth        = pExynosInputPort->portDefinition.format.video.nFrameWidth;
    pVideoInstInfo->nHeight       = pExynosInputPort->portDefinition.format.video.nFrameHeight;
    pVideoInstInfo->nBitrate      = pExynosInputPort->portDefinition.format.video.nBitrate;
    pVideoInstInfo->xFramerate    = pExynosInputPort->portDefinition.format.video.xFramerate;

    /* HEVC Codec Open */
    ret = HevcCodecOpen(pHevcDec, pVideoInstInfo);
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    Exynos_SetPlaneToPort(pExynosInputPort, MFC_DEFAULT_INPUT_BUFFER_PLANE);
    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        OMX_U32 nPlaneSize[MAX_BUFFER_PLANE] = {DEFAULT_MFC_INPUT_BUFFER_SIZE, 0, 0};
        Exynos_OSAL_SemaphoreCreate(&pExynosInputPort->codecSemID);
        Exynos_OSAL_QueueCreate(&pExynosInputPort->codecBufferQ, MAX_QUEUE_ELEMENTS);

        ret = Exynos_Allocate_CodecBuffers(pOMXComponent, INPUT_PORT_INDEX, MFC_INPUT_BUFFER_NUM_MAX, nPlaneSize);
        if (ret != OMX_ErrorNone)
            goto EXIT;

        for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++)
            Exynos_CodecBufferEnQueue(pExynosComponent, INPUT_PORT_INDEX, pVideoDec->pMFCDecInputBuffer[i]);
    } else if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

    Exynos_SetPlaneToPort(pExynosOutputPort, MFC_DEFAULT_OUTPUT_BUFFER_PLANE);
    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        Exynos_OSAL_SemaphoreCreate(&pExynosOutputPort->codecSemID);
        Exynos_OSAL_QueueCreate(&pExynosOutputPort->codecBufferQ, MAX_QUEUE_ELEMENTS);
    } else if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

    pHevcDec->bSourceStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pHevcDec->hSourceStartEvent);
    pHevcDec->bDestinationStart = OMX_FALSE;
    Exynos_OSAL_SignalCreate(&pHevcDec->hDestinationStartEvent);

    Exynos_OSAL_Memset(pExynosComponent->timeStamp, -19771003, sizeof(OMX_TICKS) * MAX_TIMESTAMP);
    Exynos_OSAL_Memset(pExynosComponent->nFlags, 0, sizeof(OMX_U32) * MAX_FLAGS);
    pHevcDec->hMFCHevcHandle.indexTimestamp = 0;
    pHevcDec->hMFCHevcHandle.outputIndexTimestamp = 0;

    pExynosComponent->getAllDelayBuffer = OMX_FALSE;

    Exynos_OSAL_QueueCreate(&pHevcDec->bypassBufferInfoQ, QUEUE_ELEMENTS);

#ifdef USE_CSC_HW
    csc_method = CSC_METHOD_HW;
#endif
    pVideoDec->csc_handle = csc_init(csc_method);
    if (pVideoDec->csc_handle == NULL) {
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    pVideoDec->csc_set_format = OMX_FALSE;

EXIT:
    FunctionOut();

    return ret;
}

/* MFC Terminate */
OMX_ERRORTYPE Exynos_HevcDec_Terminate(OMX_COMPONENTTYPE *pOMXComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_OMX_BASEPORT             *pExynosInputPort   = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    EXYNOS_OMX_BASEPORT             *pExynosOutputPort  = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;

    FunctionIn();

    if (pVideoDec->csc_handle != NULL) {
        csc_deinit(pVideoDec->csc_handle);
        pVideoDec->csc_handle = NULL;
    }

    Exynos_OSAL_QueueTerminate(&pHevcDec->bypassBufferInfoQ);

    Exynos_OSAL_SignalTerminate(pHevcDec->hDestinationStartEvent);
    pHevcDec->hDestinationStartEvent = NULL;
    pHevcDec->bDestinationStart = OMX_FALSE;
    Exynos_OSAL_SignalTerminate(pHevcDec->hSourceStartEvent);
    pHevcDec->hSourceStartEvent = NULL;
    pHevcDec->bSourceStart = OMX_FALSE;

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        Exynos_Free_CodecBuffers(pOMXComponent, OUTPUT_PORT_INDEX);
        Exynos_OSAL_QueueTerminate(&pExynosOutputPort->codecBufferQ);
        Exynos_OSAL_SemaphoreTerminate(pExynosOutputPort->codecSemID);
    } else if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }

    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        Exynos_Free_CodecBuffers(pOMXComponent, INPUT_PORT_INDEX);
        Exynos_OSAL_QueueTerminate(&pExynosInputPort->codecBufferQ);
        Exynos_OSAL_SemaphoreTerminate(pExynosInputPort->codecSemID);
    } else if (pExynosInputPort->bufferProcessType & BUFFER_SHARE) {
        /*************/
        /*    TBD    */
        /*************/
        /* Does not require any actions. */
    }
    HevcCodecClose(pHevcDec);

    Exynos_ResetAllPortConfig(pOMXComponent);

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_SrcIn(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    OMX_BUFFERHEADERTYPE tempBufferHeader;
    void *pPrivate = NULL;

    OMX_U32  oneFrameSize = pSrcInputData->dataLen;
    OMX_BOOL bInStartCode = OMX_FALSE;

    ExynosVideoDecOps       *pDecOps     = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoDecBufferOps *pInbufOps   = pHevcDec->hMFCHevcHandle.pInbufOps;
    ExynosVideoErrorType     codecReturn = VIDEO_ERROR_NONE;

    FunctionIn();

    if (pHevcDec->hMFCHevcHandle.bConfiguredMFCSrc == OMX_FALSE) {
        ret = HevcCodecSrcSetup(pOMXComponent, pSrcInputData);
        goto EXIT;
    }
    if (pHevcDec->hMFCHevcHandle.bConfiguredMFCDst == OMX_FALSE) {
        ret = HevcCodecDstSetup(pOMXComponent);
        if (ret != OMX_ErrorNone)
            goto EXIT;
    }

    if (((bInStartCode = Check_HEVC_StartCode(pSrcInputData->multiPlaneBuffer.dataBuffer[0], oneFrameSize)) == OMX_TRUE) ||
        ((pSrcInputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
        pExynosComponent->timeStamp[pHevcDec->hMFCHevcHandle.indexTimestamp]    = pSrcInputData->timeStamp;
        pExynosComponent->nFlags[pHevcDec->hMFCHevcHandle.indexTimestamp]       = pSrcInputData->nFlags;

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "input timestamp %lld us (%.2f secs), Tag: %d, nFlags: 0x%x", pSrcInputData->timeStamp, pSrcInputData->timeStamp / 1E6, pHevcDec->hMFCHevcHandle.indexTimestamp, pSrcInputData->nFlags);
        pDecOps->Set_FrameTag(hMFCHandle, pHevcDec->hMFCHevcHandle.indexTimestamp);

        pHevcDec->hMFCHevcHandle.indexTimestamp++;
        pHevcDec->hMFCHevcHandle.indexTimestamp %= MAX_TIMESTAMP;
#ifdef USE_QOS_CTRL
        if ((pVideoDec->bQosChanged == OMX_TRUE) &&
            (pDecOps->Set_QosRatio != NULL)) {
            pDecOps->Set_QosRatio(hMFCHandle, pVideoDec->nQosRatio);
            pVideoDec->bQosChanged = OMX_FALSE;
        }
#endif
        /* queue work for input buffer */
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "oneFrameSize: %d, bufferHeader: 0x%x, dataBuffer: 0x%x", oneFrameSize, pSrcInputData->bufferHeader, pSrcInputData->multiPlaneBuffer.dataBuffer[0]);
        OMX_U32 nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};

        if (pExynosInputPort->bufferProcessType & BUFFER_SHARE)
            nAllocLen[0] = pSrcInputData->bufferHeader->nAllocLen;
        else if (pExynosInputPort->bufferProcessType & BUFFER_COPY)
            nAllocLen[0] = DEFAULT_MFC_INPUT_BUFFER_SIZE;

        if (pExynosInputPort->bufferProcessType == BUFFER_COPY) {
            tempBufferHeader.nFlags     = pSrcInputData->nFlags;
            tempBufferHeader.nTimeStamp = pSrcInputData->timeStamp;
            pPrivate = (void *)&tempBufferHeader;
        } else {
            pPrivate = (void *)pSrcInputData->bufferHeader;
        }

        codecReturn = pInbufOps->ExtensionEnqueue(hMFCHandle,
                                (void **)pSrcInputData->multiPlaneBuffer.dataBuffer,
                                (int *)pSrcInputData->multiPlaneBuffer.fd,
                                (unsigned long *)nAllocLen,
                                (unsigned long *)&oneFrameSize,
                                Exynos_GetPlaneFromPort(pExynosInputPort),
                                pPrivate);
        if (codecReturn != VIDEO_ERROR_NONE) {
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s : %d", __FUNCTION__, __LINE__);
            goto EXIT;
        }

        HevcCodecStart(pOMXComponent, INPUT_PORT_INDEX);
        if (pHevcDec->bSourceStart == OMX_FALSE) {
            pHevcDec->bSourceStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pHevcDec->hSourceStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
        if (pHevcDec->bDestinationStart == OMX_FALSE) {
            pHevcDec->bDestinationStart = OMX_TRUE;
            Exynos_OSAL_SignalSet(pHevcDec->hDestinationStartEvent);
            Exynos_OSAL_SleepMillisec(0);
        }
    } else if (bInStartCode == OMX_FALSE) {
        ret = OMX_ErrorCorruptedFrame;
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_SrcOut(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcOutputData)
{
    OMX_ERRORTYPE                  ret              = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec        = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec         = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle       = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosInputPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    ExynosVideoDecBufferOps *pInbufOps      = pHevcDec->hMFCHevcHandle.pInbufOps;
    ExynosVideoBuffer       *pVideoBuffer   = NULL;
    ExynosVideoBuffer        videoBuffer;

    FunctionIn();

    if (pInbufOps->ExtensionDequeue(hMFCHandle, &videoBuffer) == VIDEO_ERROR_NONE)
        pVideoBuffer = &videoBuffer;
    else
        pVideoBuffer = NULL;

    pSrcOutputData->dataLen       = 0;
    pSrcOutputData->usedDataLen   = 0;
    pSrcOutputData->remainDataLen = 0;
    pSrcOutputData->nFlags        = 0;
    pSrcOutputData->timeStamp     = 0;
    pSrcOutputData->bufferHeader  = NULL;

    if (pVideoBuffer == NULL) {
        pSrcOutputData->multiPlaneBuffer.dataBuffer[0] = NULL;
        pSrcOutputData->allocSize  = 0;
        pSrcOutputData->pPrivate = NULL;
    } else {
        pSrcOutputData->multiPlaneBuffer.dataBuffer[0] = pVideoBuffer->planes[0].addr;
        pSrcOutputData->multiPlaneBuffer.fd[0] = pVideoBuffer->planes[0].fd;
        pSrcOutputData->allocSize  = pVideoBuffer->planes[0].allocSize;

        if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
            int i;
            for (i = 0; i < MFC_INPUT_BUFFER_NUM_MAX; i++) {
                if (pSrcOutputData->multiPlaneBuffer.dataBuffer[0] ==
                        pVideoDec->pMFCDecInputBuffer[i]->pVirAddr[0]) {
                    pVideoDec->pMFCDecInputBuffer[i]->dataSize = 0;
                    pSrcOutputData->pPrivate = pVideoDec->pMFCDecInputBuffer[i];
                    break;
                }
            }

            if (i >= MFC_INPUT_BUFFER_NUM_MAX) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Can not find buffer");
                ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
                goto EXIT;
            }
        }

        /* For Share Buffer */
        if (pExynosInputPort->bufferProcessType == BUFFER_SHARE)
            pSrcOutputData->bufferHeader = (OMX_BUFFERHEADERTYPE*)pVideoBuffer->pPrivate;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_DstIn(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstInputData)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                            *hMFCHandle         = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT             *pExynosOutputPort  = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    ExynosVideoDecBufferOps *pOutbufOps  = pHevcDec->hMFCHevcHandle.pOutbufOps;
    ExynosVideoErrorType     codecReturn = VIDEO_ERROR_NONE;

    OMX_U32 nAllocLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    OMX_U32 dataLen[MAX_BUFFER_PLANE] = {0, 0, 0};
    int i, nPlaneCnt;

    FunctionIn();

    if (pDstInputData->multiPlaneBuffer.dataBuffer[0] == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Failed to find input buffer");
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }

    nPlaneCnt = Exynos_GetPlaneFromPort(pExynosOutputPort);
    for (i = 0; i < nPlaneCnt; i++) {
        nAllocLen[i] = pHevcDec->hMFCHevcHandle.codecOutbufConf.nAlignPlaneSize[i];

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "%s : %d => ADDR[%d]: 0x%x", __FUNCTION__, __LINE__, i,
                                        pDstInputData->multiPlaneBuffer.dataBuffer[i]);
    }

    if ((pVideoDec->bReconfigDPB == OMX_TRUE) &&
        (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) &&
        (pExynosOutputPort->exceptionFlag == GENERAL_STATE)) {
        ret = HevcCodecDstSetup(pOMXComponent);
        if (ret != OMX_ErrorNone)
            goto EXIT;
        pVideoDec->bReconfigDPB = OMX_FALSE;
    }

    if (pExynosOutputPort->bDynamicDPBMode == OMX_FALSE) {
        codecReturn = pOutbufOps->Enqueue(hMFCHandle, (void **)pDstInputData->multiPlaneBuffer.dataBuffer,
                         (unsigned long *)dataLen, nPlaneCnt, pDstInputData->bufferHeader);
    } else {
        codecReturn = pOutbufOps->ExtensionEnqueue(hMFCHandle,
                                    (void **)pDstInputData->multiPlaneBuffer.dataBuffer,
                                    (int *)pDstInputData->multiPlaneBuffer.fd,
                                    (unsigned long *)nAllocLen, (unsigned long *)dataLen,
                                    nPlaneCnt, pDstInputData->bufferHeader);
    }

    if (codecReturn != VIDEO_ERROR_NONE) {
        if (codecReturn != VIDEO_ERROR_WRONGBUFFERSIZE) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "%s : %d", __FUNCTION__, __LINE__);
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
        }
        goto EXIT;
    }
    HevcCodecStart(pOMXComponent, OUTPUT_PORT_INDEX);

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_DstOut(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstOutputData)
{
    OMX_ERRORTYPE                  ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT      *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_VIDEODEC_COMPONENT *pVideoDec         = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    EXYNOS_HEVCDEC_HANDLE         *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    void                          *hMFCHandle        = pHevcDec->hMFCHevcHandle.hMFCHandle;
    EXYNOS_OMX_BASEPORT           *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    DECODE_CODEC_EXTRA_BUFFERINFO *pBufferInfo       = NULL;

    ExynosVideoDecOps           *pDecOps        = pHevcDec->hMFCHevcHandle.pDecOps;
    ExynosVideoDecBufferOps     *pOutbufOps     = pHevcDec->hMFCHevcHandle.pOutbufOps;
    ExynosVideoBuffer           *pVideoBuffer   = NULL;
    ExynosVideoBuffer            videoBuffer;
    ExynosVideoFrameStatusType   displayStatus  = VIDEO_FRAME_STATUS_UNKNOWN;
    ExynosVideoGeometry         *bufferGeometry = NULL;
    ExynosVideoErrorType         codecReturn    = VIDEO_ERROR_NONE;

    OMX_S32 indexTimestamp = 0;
    int plane, nPlaneCnt;

    FunctionIn();

    if (pHevcDec->bDestinationStart == OMX_FALSE) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    while (1) {
        if (pExynosOutputPort->bDynamicDPBMode == OMX_FALSE) {
            pVideoBuffer = pOutbufOps->Dequeue(hMFCHandle);
            if (pVideoBuffer == (ExynosVideoBuffer *)VIDEO_ERROR_DQBUF_EIO) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "HW is not available");
                ret = OMX_ErrorHardware;
                goto EXIT;
            }

            if (pVideoBuffer == NULL) {
                ret = OMX_ErrorNone;
                goto EXIT;
            }
        } else {
            Exynos_OSAL_Memset(&videoBuffer, 0, sizeof(ExynosVideoBuffer));

            codecReturn = pOutbufOps->ExtensionDequeue(hMFCHandle, &videoBuffer);
            if (codecReturn == VIDEO_ERROR_NONE) {
                pVideoBuffer = &videoBuffer;
            } else if (codecReturn == VIDEO_ERROR_DQBUF_EIO) {
                Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "HW is not available");
                pVideoBuffer = NULL;
                ret = OMX_ErrorHardware;
                goto EXIT;
            } else {
                pVideoBuffer = NULL;
                ret = OMX_ErrorNone;
                goto EXIT;
            }
        }

        displayStatus = pVideoBuffer->displayStatus;
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "displayStatus: 0x%x", displayStatus);

        if ((displayStatus == VIDEO_FRAME_STATUS_DISPLAY_DECODING) ||
            (displayStatus == VIDEO_FRAME_STATUS_DISPLAY_ONLY) ||
            (displayStatus == VIDEO_FRAME_STATUS_CHANGE_RESOL) ||
            (displayStatus == VIDEO_FRAME_STATUS_DECODING_FINISHED) ||
            (displayStatus == VIDEO_FRAME_STATUS_ENABLED_S3D) ||
            (CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
            ret = OMX_ErrorNone;
            break;
        }
    }

#ifdef USE_S3D_SUPPORT
    /* Check Whether frame packing information is available */
    if ((pHevcDec->hMFCHevcHandle.S3DFPArgmtType == OMX_SEC_FPARGMT_INVALID) &&
        (pVideoDec->bThumbnailMode == OMX_FALSE) &&
        ((displayStatus == VIDEO_FRAME_STATUS_DISPLAY_ONLY) ||
         (displayStatus == VIDEO_FRAME_STATUS_DISPLAY_DECODING) ||
         (displayStatus == VIDEO_FRAME_STATUS_ENABLED_S3D))) {
        if (HevcCodecCheckFramePacking(pOMXComponent) != OMX_TRUE) {
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
            goto EXIT;
        }
    }
#endif

    if ((pVideoDec->bThumbnailMode == OMX_FALSE) &&
        ((displayStatus == VIDEO_FRAME_STATUS_CHANGE_RESOL) ||
         (displayStatus == VIDEO_FRAME_STATUS_ENABLED_S3D))) {
        if (pVideoDec->bReconfigDPB != OMX_TRUE) {
            pExynosOutputPort->exceptionFlag = NEED_PORT_FLUSH;
            pVideoDec->bReconfigDPB = OMX_TRUE;
            HevcCodecCheckResolutionChange(pOMXComponent);
            pVideoDec->csc_set_format = OMX_FALSE;
#ifdef USE_S3D_SUPPORT
            pHevcDec->hMFCHevcHandle.S3DFPArgmtType = OMX_SEC_FPARGMT_INVALID;
#endif
        }
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    pHevcDec->hMFCHevcHandle.outputIndexTimestamp++;
    pHevcDec->hMFCHevcHandle.outputIndexTimestamp %= MAX_TIMESTAMP;

    pDstOutputData->allocSize = pDstOutputData->dataLen = 0;
    nPlaneCnt = Exynos_GetPlaneFromPort(pExynosOutputPort);
    for (plane = 0; plane < nPlaneCnt; plane++) {
        pDstOutputData->multiPlaneBuffer.dataBuffer[plane] = pVideoBuffer->planes[plane].addr;
        pDstOutputData->multiPlaneBuffer.fd[plane] = pVideoBuffer->planes[plane].fd;
        pDstOutputData->allocSize += pVideoBuffer->planes[plane].allocSize;
        pDstOutputData->dataLen +=  pVideoBuffer->planes[plane].dataSize;
    }
    pDstOutputData->usedDataLen = 0;
    pDstOutputData->pPrivate = pVideoBuffer;

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        int i = 0;
        pDstOutputData->pPrivate = NULL;
        for (i = 0; i < MFC_OUTPUT_BUFFER_NUM_MAX; i++) {
            if (pDstOutputData->multiPlaneBuffer.dataBuffer[0] ==
                pVideoDec->pMFCDecOutputBuffer[i]->pVirAddr[0]) {
                pDstOutputData->pPrivate = pVideoDec->pMFCDecOutputBuffer[i];
                break;
            }
        }

        if (pDstOutputData->pPrivate == NULL) {
            Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "Can not find buffer");
            ret = (OMX_ERRORTYPE)OMX_ErrorCodecDecode;
            goto EXIT;
        }
    }

    /* For Share Buffer */
    pDstOutputData->bufferHeader = (OMX_BUFFERHEADERTYPE *)pVideoBuffer->pPrivate;

    pBufferInfo     = (DECODE_CODEC_EXTRA_BUFFERINFO *)pDstOutputData->extInfo;
    bufferGeometry  = &pHevcDec->hMFCHevcHandle.codecOutbufConf;
    pBufferInfo->imageWidth  = bufferGeometry->nFrameWidth;
    pBufferInfo->imageHeight = bufferGeometry->nFrameHeight;
    pBufferInfo->ColorFormat = Exynos_OSAL_Video2OMXFormat((int)bufferGeometry->eColorFormat);
    Exynos_OSAL_Memcpy(&pBufferInfo->PDSB, &pVideoBuffer->PDSB, sizeof(PrivateDataShareBuffer));

    indexTimestamp = pDecOps->Get_FrameTag(hMFCHandle);
    Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "out indexTimestamp: %d", indexTimestamp);
    if ((indexTimestamp < 0) || (indexTimestamp >= MAX_TIMESTAMP)) {
        if ((pExynosComponent->checkTimeStamp.needSetStartTimeStamp != OMX_TRUE) &&
            (pExynosComponent->checkTimeStamp.needCheckStartTimeStamp != OMX_TRUE)) {
            if (indexTimestamp == INDEX_AFTER_EOS) {
                pDstOutputData->timeStamp = 0x00;
                pDstOutputData->nFlags = 0x00;
            } else {
                pDstOutputData->timeStamp = pExynosComponent->timeStamp[pHevcDec->hMFCHevcHandle.outputIndexTimestamp];
                pDstOutputData->nFlags = pExynosComponent->nFlags[pHevcDec->hMFCHevcHandle.outputIndexTimestamp];
                Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "missing out indexTimestamp: %d", indexTimestamp);
            }
        } else {
            pDstOutputData->timeStamp = 0x00;
            pDstOutputData->nFlags = 0x00;
        }
    } else {
        /* For timestamp correction. if mfc support frametype detect */
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "disp_pic_frame_type: %d", pVideoBuffer->frameType);

        /* NEED TIMESTAMP REORDER */
        if (pVideoDec->bDTSMode == OMX_TRUE) {
            if ((pVideoBuffer->frameType == VIDEO_FRAME_I) ||
                ((pVideoBuffer->frameType == VIDEO_FRAME_OTHERS) &&
                    ((pExynosComponent->nFlags[indexTimestamp] & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) ||
                (pExynosComponent->checkTimeStamp.needCheckStartTimeStamp == OMX_TRUE))
               pHevcDec->hMFCHevcHandle.outputIndexTimestamp = indexTimestamp;
            else
               indexTimestamp = pHevcDec->hMFCHevcHandle.outputIndexTimestamp;
        }

        pDstOutputData->timeStamp = pExynosComponent->timeStamp[indexTimestamp];
        pDstOutputData->nFlags = pExynosComponent->nFlags[indexTimestamp];

        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "timestamp %lld us (%.2f secs), indexTimestamp: %d, nFlags: 0x%x", pDstOutputData->timeStamp, pDstOutputData->timeStamp / 1E6, indexTimestamp, pDstOutputData->nFlags);
    }

    if ((displayStatus == VIDEO_FRAME_STATUS_DECODING_FINISHED) ||
        ((pDstOutputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS)) {
        Exynos_OSAL_Log(EXYNOS_LOG_TRACE, "displayStatus:%d, nFlags0x%x", displayStatus, pDstOutputData->nFlags);
        pDstOutputData->remainDataLen = 0;

        if (((pDstOutputData->nFlags & OMX_BUFFERFLAG_EOS) == OMX_BUFFERFLAG_EOS) &&
            (pExynosComponent->bBehaviorEOS == OMX_TRUE)) {
            pDstOutputData->remainDataLen = bufferGeometry->nFrameWidth * bufferGeometry->nFrameHeight * 3 / 2;
            pExynosComponent->bBehaviorEOS = OMX_FALSE;
        }
    } else {
        pDstOutputData->remainDataLen = bufferGeometry->nFrameWidth * bufferGeometry->nFrameHeight * 3 / 2;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_srcInputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcInputData)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_OMX_BASEPORT      *pExynosInputPort  = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosInputPort)) || (!CHECK_PORT_POPULATED(pExynosInputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }
    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    ret = Exynos_HevcDec_SrcIn(pOMXComponent, pSrcInputData);
    if ((ret != OMX_ErrorNone) &&
        ((EXYNOS_OMX_ERRORTYPE)ret != OMX_ErrorInputDataDecodeYet) &&
        ((EXYNOS_OMX_ERRORTYPE)ret != OMX_ErrorNeedNextHeaderInfo) &&
        ((EXYNOS_OMX_ERRORTYPE)ret != OMX_ErrorCorruptedFrame)) {
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_srcOutputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pSrcOutputData)
{
    OMX_ERRORTYPE                ret                = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT    *pExynosComponent   = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_HEVCDEC_HANDLE       *pHevcDec           = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    EXYNOS_OMX_BASEPORT         *pExynosInputPort   = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosInputPort)) || (!CHECK_PORT_POPULATED(pExynosInputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pExynosInputPort->bufferProcessType & BUFFER_COPY) {
        if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, INPUT_PORT_INDEX)) {
            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }
    if ((pHevcDec->bSourceStart == OMX_FALSE) &&
       (!CHECK_PORT_BEING_FLUSHED(pExynosInputPort))) {
        Exynos_OSAL_SignalWait(pHevcDec->hSourceStartEvent, DEF_MAX_WAIT_TIME);
        Exynos_OSAL_SignalReset(pHevcDec->hSourceStartEvent);
    }

    ret = Exynos_HevcDec_SrcOut(pOMXComponent, pSrcOutputData);
    if ((ret != OMX_ErrorNone) &&
        (pExynosComponent->currentState == OMX_StateExecuting)) {
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_dstInputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstInputData)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_HEVCDEC_HANDLE    *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    EXYNOS_OMX_BASEPORT      *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosOutputPort)) || (!CHECK_PORT_POPULATED(pExynosOutputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }
    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pExynosOutputPort->bufferProcessType & BUFFER_SHARE) {
        if ((pHevcDec->bDestinationStart == OMX_FALSE) &&
           (!CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
            Exynos_OSAL_SignalWait(pHevcDec->hDestinationStartEvent, DEF_MAX_WAIT_TIME);
            Exynos_OSAL_SignalReset(pHevcDec->hDestinationStartEvent);
        }
        if (Exynos_OSAL_GetElemNum(&pHevcDec->bypassBufferInfoQ) > 0) {
            BYPASS_BUFFER_INFO *pBufferInfo = (BYPASS_BUFFER_INFO *)Exynos_OSAL_Dequeue(&pHevcDec->bypassBufferInfoQ);
            if (pBufferInfo == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }

            pDstInputData->bufferHeader->nFlags     = pBufferInfo->nFlags;
            pDstInputData->bufferHeader->nTimeStamp = pBufferInfo->timeStamp;
            Exynos_OMX_OutputBufferReturn(pOMXComponent, pDstInputData->bufferHeader);
            Exynos_OSAL_Free(pBufferInfo);

            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }
    if (pHevcDec->hMFCHevcHandle.bConfiguredMFCDst == OMX_TRUE) {
        ret = Exynos_HevcDec_DstIn(pOMXComponent, pDstInputData);
        if (ret != OMX_ErrorNone) {
            pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
        }
    }

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_HevcDec_dstOutputBufferProcess(
    OMX_COMPONENTTYPE   *pOMXComponent,
    EXYNOS_OMX_DATA     *pDstOutputData)
{
    OMX_ERRORTYPE             ret               = OMX_ErrorNone;
    EXYNOS_OMX_BASECOMPONENT *pExynosComponent  = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    EXYNOS_HEVCDEC_HANDLE    *pHevcDec          = (EXYNOS_HEVCDEC_HANDLE *)((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle;
    EXYNOS_OMX_BASEPORT      *pExynosOutputPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];

    FunctionIn();

    if ((!CHECK_PORT_ENABLED(pExynosOutputPort)) || (!CHECK_PORT_POPULATED(pExynosOutputPort))) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }
    if (OMX_FALSE == Exynos_Check_BufferProcess_State(pExynosComponent, OUTPUT_PORT_INDEX)) {
        ret = OMX_ErrorNone;
        goto EXIT;
    }

    if (pExynosOutputPort->bufferProcessType & BUFFER_COPY) {
        if ((pHevcDec->bDestinationStart == OMX_FALSE) &&
           (!CHECK_PORT_BEING_FLUSHED(pExynosOutputPort))) {
            Exynos_OSAL_SignalWait(pHevcDec->hDestinationStartEvent, DEF_MAX_WAIT_TIME);
            Exynos_OSAL_SignalReset(pHevcDec->hDestinationStartEvent);
        }
        if (Exynos_OSAL_GetElemNum(&pHevcDec->bypassBufferInfoQ) > 0) {
            EXYNOS_OMX_DATABUFFER *dstOutputUseBuffer   = &pExynosOutputPort->way.port2WayDataBuffer.outputDataBuffer;
            OMX_BUFFERHEADERTYPE  *pOMXBuffer           = NULL;
            BYPASS_BUFFER_INFO    *pBufferInfo          = NULL;

            if (dstOutputUseBuffer->dataValid == OMX_FALSE) {
                pOMXBuffer = Exynos_OutputBufferGetQueue_Direct(pExynosComponent);
                if (pOMXBuffer == NULL) {
                    ret = OMX_ErrorUndefined;
                    goto EXIT;
                }
            } else {
                pOMXBuffer = dstOutputUseBuffer->bufferHeader;
            }

            pBufferInfo = Exynos_OSAL_Dequeue(&pHevcDec->bypassBufferInfoQ);
            if (pBufferInfo == NULL) {
                ret = OMX_ErrorUndefined;
                goto EXIT;
            }

            pOMXBuffer->nFlags      = pBufferInfo->nFlags;
            pOMXBuffer->nTimeStamp  = pBufferInfo->timeStamp;
            Exynos_OMX_OutputBufferReturn(pOMXComponent, pOMXBuffer);
            Exynos_OSAL_Free(pBufferInfo);

            dstOutputUseBuffer->dataValid = OMX_FALSE;

            ret = OMX_ErrorNone;
            goto EXIT;
        }
    }

    ret = Exynos_HevcDec_DstOut(pOMXComponent, pDstOutputData);
    if ((ret != OMX_ErrorNone) &&
        (pExynosComponent->currentState == OMX_StateExecuting)) {
        pExynosComponent->pCallbacks->EventHandler((OMX_HANDLETYPE)pOMXComponent,
                                                pExynosComponent->callbackData,
                                                OMX_EventError, ret, 0, NULL);
    }

EXIT:
    FunctionOut();

    return ret;
}

OSCL_EXPORT_REF OMX_ERRORTYPE Exynos_OMX_ComponentInit(
    OMX_HANDLETYPE  hComponent,
    OMX_STRING      componentName)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_BASEPORT             *pExynosPort        = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = NULL;
    int i = 0;

    FunctionIn();

    if ((hComponent == NULL) || (componentName == NULL)) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorBadParameter, Line:%d", __LINE__);
        goto EXIT;
    }
    if (Exynos_OSAL_Strcmp(EXYNOS_OMX_COMPONENT_HEVC_DEC, componentName) != 0) {
        ret = OMX_ErrorBadParameter;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorBadParameter, componentName:%s, Line:%d", componentName, __LINE__);
        goto EXIT;
    }

    pOMXComponent = (OMX_COMPONENTTYPE *)hComponent;
    ret = Exynos_OMX_VideoDecodeComponentInit(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_Error, Line:%d", __LINE__);
        goto EXIT;
    }
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pExynosComponent->codecType = HW_VIDEO_DEC_CODEC;

    pExynosComponent->componentName = (OMX_STRING)Exynos_OSAL_Malloc(MAX_OMX_COMPONENT_NAME_SIZE);
    if (pExynosComponent->componentName == NULL) {
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pExynosComponent->componentName, 0, MAX_OMX_COMPONENT_NAME_SIZE);

    pHevcDec = Exynos_OSAL_Malloc(sizeof(EXYNOS_HEVCDEC_HANDLE));
    if (pHevcDec == NULL) {
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        goto EXIT;
    }
    Exynos_OSAL_Memset(pHevcDec, 0, sizeof(EXYNOS_HEVCDEC_HANDLE));
    pVideoDec = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;
    pVideoDec->hCodecHandle = (OMX_HANDLETYPE)pHevcDec;

    Exynos_OSAL_Strcpy(pExynosComponent->componentName, EXYNOS_OMX_COMPONENT_HEVC_DEC);

#ifdef USE_S3D_SUPPORT
    pHevcDec->hMFCHevcHandle.S3DFPArgmtType = OMX_SEC_FPARGMT_INVALID;
#endif

    /* Set componentVersion */
    pExynosComponent->componentVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pExynosComponent->componentVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pExynosComponent->componentVersion.s.nRevision     = REVISION_NUMBER;
    pExynosComponent->componentVersion.s.nStep         = STEP_NUMBER;
    /* Set specVersion */
    pExynosComponent->specVersion.s.nVersionMajor = VERSIONMAJOR_NUMBER;
    pExynosComponent->specVersion.s.nVersionMinor = VERSIONMINOR_NUMBER;
    pExynosComponent->specVersion.s.nRevision     = REVISION_NUMBER;
    pExynosComponent->specVersion.s.nStep         = STEP_NUMBER;

    /* Input port */
    pExynosPort = &pExynosComponent->pExynosPort[INPUT_PORT_INDEX];
    pExynosPort->portDefinition.format.video.nFrameWidth  = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride      = 0; /*DEFAULT_FRAME_WIDTH;*/
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_INPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE)OMX_VIDEO_CodingVendorHEVC;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "video/hevc");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    pExynosPort->bufferProcessType = BUFFER_SHARE;
    pExynosPort->portWayType = WAY2_PORT;

    /* Output port */
    pExynosPort = &pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX];
    pExynosPort->portDefinition.format.video.nFrameWidth  = DEFAULT_FRAME_WIDTH;
    pExynosPort->portDefinition.format.video.nFrameHeight = DEFAULT_FRAME_HEIGHT;
    pExynosPort->portDefinition.format.video.nStride      = 0; /*DEFAULT_FRAME_WIDTH;*/
    pExynosPort->portDefinition.format.video.nSliceHeight = 0;
    pExynosPort->portDefinition.nBufferSize = DEFAULT_VIDEO_OUTPUT_BUFFER_SIZE;
    pExynosPort->portDefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    Exynos_OSAL_Memset(pExynosPort->portDefinition.format.video.cMIMEType, 0, MAX_OMX_MIMETYPE_SIZE);
    Exynos_OSAL_Strcpy(pExynosPort->portDefinition.format.video.cMIMEType, "raw/video");
    pExynosPort->portDefinition.format.video.pNativeRender = 0;
    pExynosPort->portDefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    pExynosPort->portDefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420Planar;
    pExynosPort->portDefinition.bEnabled = OMX_TRUE;
    pExynosPort->bufferProcessType = BUFFER_COPY | BUFFER_ANBSHARE_NV12L;
    pExynosPort->portWayType = WAY2_PORT;

    for(i = 0; i < ALL_PORT_NUM; i++) {
        INIT_SET_SIZE_VERSION(&pHevcDec->HevcComponent[i], OMX_VIDEO_PARAM_HEVCTYPE);
        pHevcDec->HevcComponent[i].nPortIndex = i;
        pHevcDec->HevcComponent[i].eProfile   = OMX_VIDEO_HEVCProfileMain;
        pHevcDec->HevcComponent[i].eLevel     = OMX_VIDEO_HEVCLevel5;
    }

    pOMXComponent->GetParameter      = &Exynos_HevcDec_GetParameter;
    pOMXComponent->SetParameter      = &Exynos_HevcDec_SetParameter;
    pOMXComponent->GetConfig         = &Exynos_HevcDec_GetConfig;
    pOMXComponent->SetConfig         = &Exynos_HevcDec_SetConfig;
    pOMXComponent->GetExtensionIndex = &Exynos_HevcDec_GetExtensionIndex;
    pOMXComponent->ComponentRoleEnum = &Exynos_HevcDec_ComponentRoleEnum;
    pOMXComponent->ComponentDeInit   = &Exynos_OMX_ComponentDeinit;

    pExynosComponent->exynos_codec_componentInit      = &Exynos_HevcDec_Init;
    pExynosComponent->exynos_codec_componentTerminate = &Exynos_HevcDec_Terminate;

    pVideoDec->exynos_codec_srcInputProcess  = &Exynos_HevcDec_srcInputBufferProcess;
    pVideoDec->exynos_codec_srcOutputProcess = &Exynos_HevcDec_srcOutputBufferProcess;
    pVideoDec->exynos_codec_dstInputProcess  = &Exynos_HevcDec_dstInputBufferProcess;
    pVideoDec->exynos_codec_dstOutputProcess = &Exynos_HevcDec_dstOutputBufferProcess;

    pVideoDec->exynos_codec_start            = &HevcCodecStart;
    pVideoDec->exynos_codec_stop             = &HevcCodecStop;
    pVideoDec->exynos_codec_bufferProcessRun = &HevcCodecOutputBufferProcessRun;
    pVideoDec->exynos_codec_enqueueAllBuffer = &HevcCodecEnQueueAllBuffer;

    pVideoDec->exynos_checkInputFrame                 = &Check_HEVC_Frame;
    pVideoDec->exynos_codec_getCodecInputPrivateData  = &GetCodecInputPrivateData;
    pVideoDec->exynos_codec_getCodecOutputPrivateData = &GetCodecOutputPrivateData;
    pVideoDec->exynos_codec_reconfigAllBuffers        = &HevcCodecReconfigAllBuffers;

    pVideoDec->exynos_codec_checkFormatSupport = &CheckFormatHWSupport;

    pVideoDec->hSharedMemory = Exynos_OSAL_SharedMemory_Open();
    if (pVideoDec->hSharedMemory == NULL) {
        Exynos_OSAL_Log(EXYNOS_LOG_ERROR, "OMX_ErrorInsufficientResources, Line:%d", __LINE__);
        Exynos_OSAL_Free(pHevcDec);
        pHevcDec = ((EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle)->hCodecHandle = NULL;
        Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pHevcDec->hMFCHevcHandle.videoInstInfo.eCodecType = VIDEO_CODING_HEVC;
    if (pVideoDec->bDRMPlayerMode == OMX_TRUE)
        pHevcDec->hMFCHevcHandle.videoInstInfo.eSecurityType = VIDEO_SECURE;
    else
        pHevcDec->hMFCHevcHandle.videoInstInfo.eSecurityType = VIDEO_NORMAL;

    if (Exynos_Video_GetInstInfo(&(pHevcDec->hMFCHevcHandle.videoInstInfo), VIDEO_TRUE /* dec */) != VIDEO_ERROR_NONE) {
		Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
        ret = OMX_ErrorUndefined;
        goto EXIT;
    }

    if (pHevcDec->hMFCHevcHandle.videoInstInfo.specificInfo.dec.bDynamicDPBSupport == VIDEO_TRUE)
        pExynosComponent->pExynosPort[OUTPUT_PORT_INDEX].bDynamicDPBMode = OMX_TRUE;

    Exynos_Output_SetSupportFormat(pExynosComponent);

    pExynosComponent->currentState = OMX_StateLoaded;

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}

OMX_ERRORTYPE Exynos_OMX_ComponentDeinit(
    OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE                    ret                = OMX_ErrorNone;
    OMX_COMPONENTTYPE               *pOMXComponent      = NULL;
    EXYNOS_OMX_BASECOMPONENT        *pExynosComponent   = NULL;
    EXYNOS_OMX_VIDEODEC_COMPONENT   *pVideoDec          = NULL;
    EXYNOS_HEVCDEC_HANDLE           *pHevcDec           = NULL;

    FunctionIn();

    if (hComponent == NULL) {
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pOMXComponent    = (OMX_COMPONENTTYPE *)hComponent;
    pExynosComponent = (EXYNOS_OMX_BASECOMPONENT *)pOMXComponent->pComponentPrivate;
    pVideoDec        = (EXYNOS_OMX_VIDEODEC_COMPONENT *)pExynosComponent->hComponentHandle;

    Exynos_OSAL_SharedMemory_Close(pVideoDec->hSharedMemory);

    Exynos_OSAL_Free(pExynosComponent->componentName);
    pExynosComponent->componentName = NULL;

    pHevcDec = (EXYNOS_HEVCDEC_HANDLE *)pVideoDec->hCodecHandle;
    if (pHevcDec != NULL) {
        Exynos_OSAL_Free(pHevcDec);
        pHevcDec = pVideoDec->hCodecHandle = NULL;
    }

    ret = Exynos_OMX_VideoDecodeComponentDeinit(pOMXComponent);
    if (ret != OMX_ErrorNone) {
        goto EXIT;
    }

    ret = OMX_ErrorNone;

EXIT:
    FunctionOut();

    return ret;
}
