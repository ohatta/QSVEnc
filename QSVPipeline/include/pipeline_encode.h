﻿//  -----------------------------------------------------------------------------------------
//    QSVEnc by rigaya
//  -----------------------------------------------------------------------------------------
//   ソースコードについて
//   ・無保証です。
//   ・本ソースコードを使用したことによるいかなる損害・トラブルについてrigayaは責任を負いません。
//   以上に了解して頂ける場合、本ソースコードの使用、複製、改変、再頒布を行って頂いて構いません。
//  ---------------------------------------------------------------------------------------

#ifndef __PIPELINE_ENCODE_H__
#define __PIPELINE_ENCODE_H__

#include "qsv_version.h"
#include "qsv_osdep.h"
#include "qsv_util.h"
#include "qsv_prm.h"
#include "qsv_thread.h"
#include "qsv_event.h"
#include "qsv_log.h"

#include "hw_device.h"

#ifdef D3D_SURFACES_SUPPORT
#pragma warning(disable : 4201)
#include <d3d9.h>
#include <dxva2api.h>
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dxva2.lib")
#endif

#include "base_allocator.h"

#include "mfxmvc.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxplugin.h"
#include "mfxplugin++.h"

#ifndef QSVENC_AUO
#pragma comment(lib, "libmfx.lib")
#endif

#include "vpp_plugins.h"
#include "scene_change_detection.h"
#include "perf_monitor.h"
#include "qsv_plugin.h"
#include "qsv_input.h"
#include "qsv_output.h"
#include "qsv_task.h"

#include <vector>
#include <memory>
#include <string>
#include <iostream>

enum {
    MFX_PRM_EX_SCENE_CHANGE = 0x01,
    MFX_PRM_EX_VQP          = 0x02,
    MFX_PRM_EX_DEINT_NORMAL = 0x04,
    MFX_PRM_EX_DEINT_BOB    = 0x08
};
enum {
    SC_FIELDFLAG_INVALID_ALL  = 0xffffffff,
    SC_FIELDFLAG_INVALID_LOW  = 0x0000ffff,
    SC_FIELDFLAG_INVALID_HIGH = 0xffff0000,
};

class CEncodingPipeline
{
public:
    CEncodingPipeline();
    virtual ~CEncodingPipeline();

    virtual mfxStatus CheckParam(sInputParams *pParams);
    virtual mfxStatus Init(sInputParams *pParams);
    virtual mfxStatus Run();
    virtual mfxStatus Run(size_t SubThreadAffinityMask);
    virtual void Close();
    virtual mfxStatus ResetMFXComponents(sInputParams* pParams);
    virtual mfxStatus ResetDevice();
#if ENABLE_MVC_ENCODING
    void SetMultiView();
    void SetNumView(mfxU32 numViews) { m_nNumView = numViews; }
#endif
    virtual mfxStatus CheckCurrentVideoParam(TCHAR *buf = NULL, mfxU32 bufSize = 0);

    virtual void SetAbortFlagPointer(bool *abort);

    virtual mfxStatus GetEncodeStatusData(sEncodeStatusData *data);
    virtual void GetEncodeLibInfo(mfxVersion *ver, bool *hardware);
    virtual const TCHAR *GetInputMessage();
    virtual MemType GetMemType();

    virtual void PrintMes(int log_level, const TCHAR *format, ...);
    shared_ptr<CQSVLog> m_pQSVLog;

    virtual mfxStatus RunEncode();
    mfxStatus CheckSceneChange();
    static void RunEncThreadLauncher(void *pParam);
    static void RunSubThreadLauncher(void *pParam);
protected:
    mfxVersion m_mfxVer;
    shared_ptr<CEncodeStatusInfo> m_pEncSatusInfo;
    unique_ptr<CPerfMonitor> m_pPerfMonitor;
    CEncodingThread m_EncThread;

    bool m_bTimerPeriodTuning; //timeBeginPeriodを使ったかどうか記憶する

    CSceneChangeDetect m_SceneChange;
    mfxU32 m_nExPrm;
    CQSVFrameTypeSimulation m_frameTypeSim;

    vector<shared_ptr<CQSVOut>> m_pFileWriterListAudio;
    shared_ptr<CQSVOut> m_pFileWriter;
    vector<shared_ptr<CQSVInput>> m_AudioReaders;
    shared_ptr<CQSVInput> m_pFileReader;

    CQSVTaskControl m_TaskPool;
    mfxU16 m_nAsyncDepth; // depth of asynchronous pipeline, this number can be tuned to achieve better performance

    mfxInitParam m_InitParam;
    mfxExtBuffer *m_pInitParamExtBuf[1];
    mfxExtThreadsParam m_ThreadsParam;

    mfxExtVideoSignalInfo m_VideoSignalInfo;
    mfxExtCodingOption m_CodingOption;
    mfxExtCodingOption2 m_CodingOption2;
    mfxExtCodingOption3 m_CodingOption3;
    mfxExtVP8CodingOption m_ExtVP8CodingOption;
    mfxExtHEVCParam m_ExtHEVCParam;
    MFXVideoSession m_mfxSession;
    unique_ptr<MFXVideoDECODE> m_pmfxDEC;
    unique_ptr<MFXVideoENCODE> m_pmfxENC;
    unique_ptr<MFXVideoVPP>    m_pmfxVPP;

    unique_ptr<CSessionPlugins> m_SessionPlugins;
    vector<unique_ptr<CVPPPlugin>> m_VppPrePlugins;
    vector<unique_ptr<CVPPPlugin>> m_VppPostPlugins;

    const sTrimParam *m_pTrimParam;

    mfxVideoParam m_mfxDecParams;
    mfxVideoParam m_mfxEncParams;
    mfxVideoParam m_mfxVppParams;
    
    unique_ptr<MFXVideoUSER>  m_pUserModule;

    vector<mfxExtBuffer*> m_EncExtParams;
    vector<mfxExtBuffer*> m_VppExtParams;
    tstring VppExtMes;

    mfxExtVPPDoNotUse m_VppDoNotUse;
    mfxExtVPPDoNotUse m_VppDoUse;
    mfxExtVPPDenoise m_ExtDenoise;
    mfxExtVPPDetail m_ExtDetail;
    mfxExtVPPDeinterlacing m_ExtDeinterlacing;
    mfxExtVPPFrameRateConversion m_ExtFrameRateConv;
    mfxExtVPPRotation m_ExtRotate;
    mfxExtVPPVideoSignalInfo m_ExtVppVSI;
    mfxExtVPPImageStab m_ExtImageStab;
    vector<mfxU32> m_VppDoNotUseList;
    vector<mfxU32> m_VppDoUseList;

    unique_ptr<MFXFrameAllocator> m_pMFXAllocator;
    unique_ptr<mfxAllocatorParams> m_pmfxAllocatorParams;
    MemType m_memType;
    bool m_bd3dAlloc; // use d3d surfaces
    bool m_bExternalAlloc; // use memory allocator as external for Media SDK

    bool *m_pAbortByUser;

    mfxBitstream m_DecInputBitstream;

    vector<mfxFrameSurface1> m_pEncSurfaces; // frames array for encoder input (vpp output, decoder output)
    vector<mfxFrameSurface1> m_pVppSurfaces; // frames array for vpp input (decoder output)
    vector<mfxFrameSurface1> m_pDecSurfaces; // work area for decoder
    mfxFrameAllocResponse m_EncResponse;  // memory allocation response for encoder
    mfxFrameAllocResponse m_VppResponse;  // memory allocation response for vpp
    mfxFrameAllocResponse m_DecResponse;  // memory allocation response for decoder

#if ENABLE_MVC_ENCODING
    mfxU16 m_MVCflags; // MVC codec is in use
    mfxU32 m_nNumView;
    // for MVC encoder and VPP configuration
    mfxExtMVCSeqDesc m_MVCSeqDesc;
#endif
    // for disabling VPP algorithms
    //mfxExtVPPDoNotUse m_VppDoNotUse;

    shared_ptr<CHWDevice> m_hwdev;

    virtual mfxStatus DetermineMinimumRequiredVersion(const sInputParams &pParams, mfxVersion &version);

    virtual mfxStatus InitSessionInitParam(mfxU16 threads, mfxU16 priority);
    virtual mfxStatus InitLog(sInputParams *pParams);
    virtual mfxStatus InitInput(sInputParams *pParams);
    virtual mfxStatus InitOutput(sInputParams *pParams);
    virtual mfxStatus InitMfxDecParams();
    virtual mfxStatus InitMfxEncParams(sInputParams *pParams);
    virtual mfxStatus InitMfxVppParams(sInputParams *pParams);
    virtual mfxStatus InitVppPrePlugins(sInputParams *pParams);
    virtual mfxStatus InitVppPostPlugins(sInputParams *pParams);
    virtual mfxStatus InitSession(bool useHWLib, mfxU16 memType);
    //virtual void InitVppExtParam();
    virtual mfxStatus CreateVppExtBuffers(sInputParams *pParams);

    virtual mfxStatus AllocAndInitVppDoNotUse();
    virtual void FreeVppDoNotUse();
#if ENABLE_MVC_ENCODING
    virtual mfxStatus AllocAndInitMVCSeqDesc();
    virtual void FreeMVCSeqDesc();
#endif
    virtual mfxStatus CreateAllocator();
    virtual void DeleteAllocator();

    virtual mfxStatus CreateHWDevice();
    virtual void DeleteHWDevice();

    virtual mfxStatus AllocFrames();
    virtual void DeleteFrames();

    virtual mfxStatus AllocateSufficientBuffer(mfxBitstream* pBS);

    virtual mfxStatus GetFreeTask(QSVTask **ppTask);
    virtual mfxStatus SynchronizeFirstTask();
};

#endif // __PIPELINE_ENCODE_H__
