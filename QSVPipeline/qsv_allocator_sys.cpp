﻿// -----------------------------------------------------------------------------------------
// QSVEnc by rigaya
// -----------------------------------------------------------------------------------------
// The MIT License
//
// Copyright (c) 2011-2016 rigaya
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// --------------------------------------------------------------------------------------------

#include "qsv_allocator_sys.h"
#include "qsv_util.h"

#pragma warning(disable : 4100)

#define ID_BUFFER MFX_MAKEFOURCC('B','U','F','F')
#define ID_FRAME  MFX_MAKEFOURCC('F','R','M','E')

QSVBufferAllocatorSys::QSVBufferAllocatorSys() { }

QSVBufferAllocatorSys::~QSVBufferAllocatorSys() { }

mfxStatus QSVBufferAllocatorSys::BufAlloc(mfxU32 nbytes, mfxU16 type, mfxMemId *mid) {
    if (!mid)
        return MFX_ERR_NULL_PTR;

    if (0 == (type & MFX_MEMTYPE_SYSTEM_MEMORY))
        return MFX_ERR_UNSUPPORTED;

    mfxU32 header_size = ALIGN32(sizeof(sBuffer));
    void *buffer_ptr = _aligned_malloc(header_size + nbytes, 32);
    if (!buffer_ptr) {
        return MFX_ERR_MEMORY_ALLOC;
    }

    sBuffer *bs = (sBuffer *)buffer_ptr;
    bs->id = ID_BUFFER;
    bs->type = type;
    bs->nbytes = nbytes;
    *mid = (mfxHDL)bs;
    return MFX_ERR_NONE;
}

mfxStatus QSVBufferAllocatorSys::BufLock(mfxMemId mid, mfxU8 **ptr) {
    if (!ptr)
        return MFX_ERR_NULL_PTR;

    sBuffer *bs = (sBuffer *)mid;

    if (!bs || ID_BUFFER != bs->id) {
        return MFX_ERR_INVALID_HANDLE;
    }

    *ptr = ((mfxU8 *)bs) + ALIGN32(sizeof(sBuffer));
    return MFX_ERR_NONE;
}

mfxStatus QSVBufferAllocatorSys::BufUnlock(mfxMemId mid) {
    sBuffer *bs = (sBuffer *)mid;
    if (!bs || ID_BUFFER != bs->id) {
        return MFX_ERR_INVALID_HANDLE;
    }
    return MFX_ERR_NONE;
}

mfxStatus QSVBufferAllocatorSys::BufFree(mfxMemId mid) {
    sBuffer *bs = (sBuffer *)mid;
    if (!bs || ID_BUFFER != bs->id) {
        return MFX_ERR_INVALID_HANDLE;
    }
    _aligned_free(bs);
    return MFX_ERR_NONE;
}

QSVAllocatorSys::QSVAllocatorSys()
: m_pBufferAllocator() {
}

QSVAllocatorSys::~QSVAllocatorSys() {
    Close();
}

mfxStatus QSVAllocatorSys::Init(mfxAllocatorParams *pParams, shared_ptr<RGYLog> pQSVLog) {
    m_pQSVLog = pQSVLog;
    m_pBufferAllocator.reset(new QSVBufferAllocatorSys());
    return MFX_ERR_NONE;
}

mfxStatus QSVAllocatorSys::Close() {
    mfxStatus sts = QSVAllocator::Close();
    m_pBufferAllocator.reset();
    return sts;
}

mfxStatus QSVAllocatorSys::FrameLock(mfxMemId mid, mfxFrameData *ptr) {
    if (!m_pBufferAllocator) {
        return MFX_ERR_NOT_INITIALIZED;
    }
    if (!ptr) {
        return MFX_ERR_NULL_PTR;
    }

    sFrame *fs = 0;
    mfxStatus sts = m_pBufferAllocator->Lock(m_pBufferAllocator->pthis, mid, (mfxU8 **)&fs);
    if (MFX_ERR_NONE != sts) {
        m_pQSVLog->write(RGY_LOG_ERROR, _T("QSVAllocatorSys::FrameLock Failed to Lock frmame mid 0x%x: %s\n"), mid, get_err_mes(sts));
        return sts;
    }
    if (ID_FRAME != fs->id) {
        m_pBufferAllocator->Unlock(m_pBufferAllocator->pthis, mid);
        m_pQSVLog->write(RGY_LOG_ERROR, _T("QSVAllocatorSys::FrameLock Invalid mem handle\n"), mid, get_err_mes(sts));
        return MFX_ERR_INVALID_HANDLE;
    }

    uint32_t WidthAlign  = ALIGN32(fs->info.Width);
    uint32_t HeightAlign = ALIGN32(fs->info.Height);
    ptr->B = ptr->Y = (uint8_t *)fs + ALIGN32(sizeof(sFrame));

    switch (fs->info.FourCC) {
    case MFX_FOURCC_NV12:
        ptr->U = ptr->Y + WidthAlign * HeightAlign;
        ptr->V = ptr->U + 1;
        ptr->Pitch = (mfxU16)WidthAlign;
        break;
    case MFX_FOURCC_YV12:
        ptr->V = ptr->Y + WidthAlign * HeightAlign;
        ptr->U = ptr->V + (WidthAlign >> 1) * (HeightAlign >> 1);
        ptr->Pitch = (mfxU16)WidthAlign;
        break;
    case MFX_FOURCC_YUY2:
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;
        ptr->Pitch = 2 * (mfxU16)WidthAlign;
        break;
    case MFX_FOURCC_RGB3:
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->Pitch = 3 * (mfxU16)WidthAlign;
        break;
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_A2RGB10:
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        ptr->Pitch = 4 * (mfxU16)WidthAlign;
        break;
     case MFX_FOURCC_R16:
        ptr->Y16 = (mfxU16 *)ptr->B;
        ptr->Pitch = 2 * (mfxU16)WidthAlign;
        break;
    case MFX_FOURCC_P010:
        ptr->U = ptr->Y + WidthAlign * HeightAlign * 2;
        ptr->V = ptr->U + 2;
        ptr->Pitch = (mfxU16)WidthAlign * 2;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }
    m_pQSVLog->write(RGY_LOG_TRACE, _T("QSVAllocatorSys::FrameLock success mid 0x%x\n"), mid);
    return MFX_ERR_NONE;
}

mfxStatus QSVAllocatorSys::FrameUnlock(mfxMemId mid, mfxFrameData *ptr) {
    if (!m_pBufferAllocator) {
        return MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus sts = m_pBufferAllocator->Unlock(m_pBufferAllocator->pthis, mid);
    if (MFX_ERR_NONE != sts) {
        m_pQSVLog->write(RGY_LOG_ERROR, _T("QSVAllocatorSys::FrameUnlock failed to unlock frame mid 0x%x: %s\n"), mid, get_err_mes(sts));
        return sts;
    }

    if (NULL != ptr) {
        ptr->Pitch = 0;
        ptr->Y     = nullptr;
        ptr->U     = nullptr;
        ptr->V     = nullptr;
    }
    m_pQSVLog->write(RGY_LOG_TRACE, _T("QSVAllocatorSys::FrameUnlock success mid 0x%x\n"), mid);
    return MFX_ERR_NONE;
}

mfxStatus QSVAllocatorSys::GetFrameHDL(mfxMemId mid, mfxHDL *handle) {
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus QSVAllocatorSys::CheckRequestType(mfxFrameAllocRequest *request) {
    mfxStatus sts = QSVAllocator::CheckRequestType(request);
    if (MFX_ERR_NONE != sts) {
        return sts;
    }

    return ((request->Type & MFX_MEMTYPE_SYSTEM_MEMORY) != 0) ? MFX_ERR_NONE : MFX_ERR_UNSUPPORTED;
}

mfxStatus QSVAllocatorSys::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response) {
    if (!m_pBufferAllocator) {
        return MFX_ERR_NOT_INITIALIZED;
    }

    mfxU32 WidthAlign = ALIGN32(request->Info.Width);
    mfxU32 HeightAlign = ALIGN32(request->Info.Height);
    mfxU32 nbytes = 0;

    switch (request->Info.FourCC) {
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_NV12:
        nbytes = WidthAlign * HeightAlign * 3/2;
        break;
    case MFX_FOURCC_RGB3:
        nbytes = WidthAlign * HeightAlign * 3;
        break;
    case MFX_FOURCC_RGB4:
        nbytes = WidthAlign * HeightAlign * 4;
        break;
    case MFX_FOURCC_YUY2:
        nbytes = WidthAlign * HeightAlign * 2;
        break;
    case MFX_FOURCC_R16:
        nbytes = WidthAlign * HeightAlign * 2;
        break;
    case MFX_FOURCC_P010:
        nbytes = WidthAlign * HeightAlign * 3;
        break;
    case MFX_FOURCC_A2RGB10:
        nbytes = WidthAlign * HeightAlign * 4;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    unique_ptr<mfxMemId[]> mids(new mfxMemId[request->NumFrameSuggested]);
    if (!mids.get()) {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_pQSVLog->write(RGY_LOG_DEBUG, _T("QSVAllocatorSys::AllocImpl allocating %d frames...\n"), request->NumFrameSuggested);
    mfxU32 numAllocated = 0;
    for (numAllocated = 0; numAllocated < request->NumFrameSuggested; numAllocated++) {
        mfxStatus sts = m_pBufferAllocator->Alloc(m_pBufferAllocator->pthis,
            nbytes + ALIGN32(sizeof(sFrame)), request->Type, &(mids.get()[numAllocated]));
        if (sts != MFX_ERR_NONE) {
            m_pQSVLog->write(RGY_LOG_ERROR, _T("QSVAllocatorSys::AllocImpl failed to allocate frame #%d, size %d: %s\n"), numAllocated, nbytes + ALIGN32(sizeof(sFrame)), get_err_mes(sts));
            return MFX_ERR_MEMORY_ALLOC;
        }

        sFrame *fs;
        sts = m_pBufferAllocator->Lock(m_pBufferAllocator->pthis, mids.get()[numAllocated], (mfxU8 **)&fs);
        if (sts != MFX_ERR_NONE) {
            m_pQSVLog->write(RGY_LOG_ERROR, _T("QSVAllocatorSys::AllocImpl failed to unlock frame mid 0x%x: %s\n"), mids.get()[numAllocated], get_err_mes(sts));
            return MFX_ERR_MEMORY_ALLOC;
        }

        fs->id = ID_FRAME;
        fs->info = request->Info;
        sts = m_pBufferAllocator->Unlock(m_pBufferAllocator->pthis, mids.get()[numAllocated]);
        if (sts != MFX_ERR_NONE) {
            m_pQSVLog->write(RGY_LOG_ERROR, _T("QSVAllocatorSys::AllocImpl failed to unlock frame mid 0x%x: %s\n"), mids.get()[numAllocated], get_err_mes(sts));
            return MFX_ERR_MEMORY_ALLOC;
        }
    }

    response->NumFrameActual = (mfxU16)numAllocated;
    response->mids = mids.release();
    m_pQSVLog->write(RGY_LOG_DEBUG, _T("QSVAllocatorSys::AllocImpl Success.\n"));
    return MFX_ERR_NONE;
}

mfxStatus QSVAllocatorSys::ReleaseResponse(mfxFrameAllocResponse *response) {
    if (!response) {
        return MFX_ERR_NULL_PTR;
    }

    if (!m_pBufferAllocator) {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if (response->mids) {
        int nFrameCount = response->NumFrameActual;
        for (int i = 0; i < nFrameCount; i++) {
            if (response->mids[i]) {
                mfxStatus sts = m_pBufferAllocator->Free(m_pBufferAllocator->pthis, response->mids[i]);
                if (MFX_ERR_NONE != sts) return sts;
            }
        }
        delete [] response->mids;
    }
    response->mids = 0;
    m_pQSVLog->write(RGY_LOG_DEBUG, _T("QSVAllocatorSys::ReleaseResponse Success.\n"));
    return MFX_ERR_NONE;
}
