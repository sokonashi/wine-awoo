/*
 * Primary DirectDraw video stream
 *
 * Copyright 2005, 2008, 2012 Christian Costa
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define NONAMELESSUNION
#define COBJMACROS
#include "amstream_private.h"
#include "wine/debug.h"
#include "wine/strmbase.h"

WINE_DEFAULT_DEBUG_CHANNEL(amstream);

static HRESULT ddrawstreamsample_create(IDirectDrawMediaStream *parent, IDirectDrawSurface *surface,
    const RECT *rect, IDirectDrawStreamSample **ddraw_stream_sample);

struct DirectDrawMediaStreamImpl;

typedef struct {
    BaseInputPin pin;
    struct DirectDrawMediaStreamImpl *parent;
} DirectDrawMediaStreamInputPin;

typedef struct DirectDrawMediaStreamImpl {
    IAMMediaStream IAMMediaStream_iface;
    IDirectDrawMediaStream IDirectDrawMediaStream_iface;
    LONG ref;
    IMultiMediaStream* parent;
    MSPID purpose_id;
    STREAM_TYPE stream_type;
    IDirectDraw7 *ddraw;
    DirectDrawMediaStreamInputPin *input_pin;
    CRITICAL_SECTION critical_section;
} DirectDrawMediaStreamImpl;

static inline DirectDrawMediaStreamImpl *impl_from_DirectDrawMediaStream_IAMMediaStream(IAMMediaStream *iface)
{
    return CONTAINING_RECORD(iface, DirectDrawMediaStreamImpl, IAMMediaStream_iface);
}

/*** IUnknown methods ***/
static HRESULT WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_QueryInterface(IAMMediaStream *iface,
                                                        REFIID riid, void **ret_iface)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);

    TRACE("(%p/%p)->(%s,%p)\n", iface, This, debugstr_guid(riid), ret_iface);

    if (IsEqualGUID(riid, &IID_IUnknown) ||
        IsEqualGUID(riid, &IID_IMediaStream) ||
        IsEqualGUID(riid, &IID_IAMMediaStream))
    {
        IAMMediaStream_AddRef(iface);
        *ret_iface = iface;
        return S_OK;
    }
    else if (IsEqualGUID(riid, &IID_IDirectDrawMediaStream))
    {
        IAMMediaStream_AddRef(iface);
        *ret_iface = &This->IDirectDrawMediaStream_iface;
        return S_OK;
    }
    else if (IsEqualGUID(riid, &IID_IPin))
    {
        IAMMediaStream_AddRef(iface);
        *ret_iface = &This->input_pin->pin.pin.IPin_iface;
        return S_OK;
    }
    else if (IsEqualGUID(riid, &IID_IMemInputPin))
    {
        IAMMediaStream_AddRef(iface);
        *ret_iface = &This->input_pin->pin.IMemInputPin_iface;
        return S_OK;
    }

    ERR("(%p)->(%s,%p),not found\n", This, debugstr_guid(riid), ret_iface);
    return E_NOINTERFACE;
}

static ULONG WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_AddRef(IAMMediaStream *iface)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p/%p)->(): new ref = %u\n", iface, This, ref);

    return ref;
}

static ULONG WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_Release(IAMMediaStream *iface)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p/%p)->(): new ref = %u\n", iface, This, ref);

    if (!ref)
    {
        BaseInputPin_Destroy((BaseInputPin *)This->input_pin);
        DeleteCriticalSection(&This->critical_section);
        if (This->ddraw)
            IDirectDraw7_Release(This->ddraw);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

/*** IMediaStream methods ***/
static HRESULT WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_GetMultiMediaStream(IAMMediaStream *iface,
        IMultiMediaStream** multi_media_stream)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, multi_media_stream);

    if (!multi_media_stream)
        return E_POINTER;

    IMultiMediaStream_AddRef(This->parent);
    *multi_media_stream = This->parent;

    return S_OK;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_GetInformation(IAMMediaStream *iface,
        MSPID *purpose_id, STREAM_TYPE *type)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);

    TRACE("(%p/%p)->(%p,%p)\n", This, iface, purpose_id, type);

    if (purpose_id)
        *purpose_id = This->purpose_id;
    if (type)
        *type = This->stream_type;

    return S_OK;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_SetSameFormat(IAMMediaStream *iface,
        IMediaStream *pStreamThatHasDesiredFormat, DWORD flags)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);

    FIXME("(%p/%p)->(%p,%x) stub!\n", This, iface, pStreamThatHasDesiredFormat, flags);

    return S_FALSE;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_AllocateSample(IAMMediaStream *iface,
        DWORD flags, IStreamSample **sample)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);

    FIXME("(%p/%p)->(%x,%p) stub!\n", This, iface, flags, sample);

    return S_FALSE;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_CreateSharedSample(IAMMediaStream *iface,
        IStreamSample *existing_sample, DWORD flags, IStreamSample **sample)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);

    FIXME("(%p/%p)->(%p,%x,%p) stub!\n", This, iface, existing_sample, flags, sample);

    return S_FALSE;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_SendEndOfStream(IAMMediaStream *iface, DWORD flags)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);

    FIXME("(%p/%p)->(%x) stub!\n", This, iface, flags);

    return S_FALSE;
}

/*** IAMMediaStream methods ***/
static HRESULT WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_Initialize(IAMMediaStream *iface, IUnknown *source_object, DWORD flags,
                                                    REFMSPID purpose_id, const STREAM_TYPE stream_type)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);

    FIXME("(%p/%p)->(%p,%x,%p,%u) stub!\n", This, iface, source_object, flags, purpose_id, stream_type);

    return S_FALSE;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_SetState(IAMMediaStream *iface, FILTER_STATE state)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);

    FIXME("(%p/%p)->(%u) stub!\n", This, iface, state);

    return S_FALSE;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_JoinAMMultiMediaStream(IAMMediaStream *iface, IAMMultiMediaStream *am_multi_media_stream)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);

    FIXME("(%p/%p)->(%p) stub!\n", This, iface, am_multi_media_stream);

    return S_FALSE;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_JoinFilter(IAMMediaStream *iface, IMediaStreamFilter *media_stream_filter)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, media_stream_filter);

    This->input_pin->pin.pin.pinInfo.pFilter = (IBaseFilter *)media_stream_filter;

    return S_OK;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IAMMediaStream_JoinFilterGraph(IAMMediaStream *iface, IFilterGraph *filtergraph)
{
    DirectDrawMediaStreamImpl *This = impl_from_DirectDrawMediaStream_IAMMediaStream(iface);

    FIXME("(%p/%p)->(%p) stub!\n", This, iface, filtergraph);

    return S_FALSE;
}

static const struct IAMMediaStreamVtbl DirectDrawMediaStreamImpl_IAMMediaStream_Vtbl =
{
    /*** IUnknown methods ***/
    DirectDrawMediaStreamImpl_IAMMediaStream_QueryInterface,
    DirectDrawMediaStreamImpl_IAMMediaStream_AddRef,
    DirectDrawMediaStreamImpl_IAMMediaStream_Release,
    /*** IMediaStream methods ***/
    DirectDrawMediaStreamImpl_IAMMediaStream_GetMultiMediaStream,
    DirectDrawMediaStreamImpl_IAMMediaStream_GetInformation,
    DirectDrawMediaStreamImpl_IAMMediaStream_SetSameFormat,
    DirectDrawMediaStreamImpl_IAMMediaStream_AllocateSample,
    DirectDrawMediaStreamImpl_IAMMediaStream_CreateSharedSample,
    DirectDrawMediaStreamImpl_IAMMediaStream_SendEndOfStream,
    /*** IAMMediaStream methods ***/
    DirectDrawMediaStreamImpl_IAMMediaStream_Initialize,
    DirectDrawMediaStreamImpl_IAMMediaStream_SetState,
    DirectDrawMediaStreamImpl_IAMMediaStream_JoinAMMultiMediaStream,
    DirectDrawMediaStreamImpl_IAMMediaStream_JoinFilter,
    DirectDrawMediaStreamImpl_IAMMediaStream_JoinFilterGraph
};

static inline DirectDrawMediaStreamImpl *impl_from_IDirectDrawMediaStream(IDirectDrawMediaStream *iface)
{
    return CONTAINING_RECORD(iface, DirectDrawMediaStreamImpl, IDirectDrawMediaStream_iface);
}

/*** IUnknown methods ***/
static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_QueryInterface(IDirectDrawMediaStream *iface,
        REFIID riid, void **ret_iface)
{
    DirectDrawMediaStreamImpl *This = impl_from_IDirectDrawMediaStream(iface);
    TRACE("(%p/%p)->(%s,%p)\n", iface, This, debugstr_guid(riid), ret_iface);
    return IAMMediaStream_QueryInterface(&This->IAMMediaStream_iface, riid, ret_iface);
}

static ULONG WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_AddRef(IDirectDrawMediaStream *iface)
{
    DirectDrawMediaStreamImpl *This = impl_from_IDirectDrawMediaStream(iface);
    TRACE("(%p/%p)\n", iface, This);
    return IAMMediaStream_AddRef(&This->IAMMediaStream_iface);
}

static ULONG WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_Release(IDirectDrawMediaStream *iface)
{
    DirectDrawMediaStreamImpl *This = impl_from_IDirectDrawMediaStream(iface);
    TRACE("(%p/%p)\n", iface, This);
    return IAMMediaStream_Release(&This->IAMMediaStream_iface);
}

/*** IMediaStream methods ***/
static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_GetMultiMediaStream(IDirectDrawMediaStream *iface,
        IMultiMediaStream **multi_media_stream)
{
    DirectDrawMediaStreamImpl *This = impl_from_IDirectDrawMediaStream(iface);

    TRACE("(%p/%p)->(%p)\n", This, iface, multi_media_stream);

    if (!multi_media_stream)
        return E_POINTER;

    IMultiMediaStream_AddRef(This->parent);
    *multi_media_stream = This->parent;

    return S_OK;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_GetInformation(IDirectDrawMediaStream *iface,
        MSPID *purpose_id, STREAM_TYPE *type)
{
    DirectDrawMediaStreamImpl *This = impl_from_IDirectDrawMediaStream(iface);

    TRACE("(%p/%p)->(%p,%p)\n", This, iface, purpose_id, type);

    if (purpose_id)
        *purpose_id = This->purpose_id;
    if (type)
        *type = This->stream_type;

    return S_OK;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_SetSameFormat(IDirectDrawMediaStream *iface,
        IMediaStream *pStreamThatHasDesiredFormat, DWORD dwFlags)
{
    DirectDrawMediaStreamImpl *This = impl_from_IDirectDrawMediaStream(iface);

    FIXME("(%p/%p)->(%p,%x) stub!\n", This, iface, pStreamThatHasDesiredFormat, dwFlags);

    return S_FALSE;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_AllocateSample(IDirectDrawMediaStream *iface,
        DWORD dwFlags, IStreamSample **ppSample)
{
    DirectDrawMediaStreamImpl *This = impl_from_IDirectDrawMediaStream(iface);

    FIXME("(%p/%p)->(%x,%p) stub!\n", This, iface, dwFlags, ppSample);

    return S_FALSE;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_CreateSharedSample(IDirectDrawMediaStream *iface,
        IStreamSample *pExistingSample, DWORD dwFlags, IStreamSample **ppSample)
{
    DirectDrawMediaStreamImpl *This = impl_from_IDirectDrawMediaStream(iface);

    FIXME("(%p/%p)->(%p,%x,%p) stub!\n", This, iface, pExistingSample, dwFlags, ppSample);

    return S_FALSE;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_SendEndOfStream(IDirectDrawMediaStream *iface,
        DWORD dwFlags)
{
    DirectDrawMediaStreamImpl *This = impl_from_IDirectDrawMediaStream(iface);

    FIXME("(%p/%p)->(%x) stub!\n", This, iface, dwFlags);

    return S_FALSE;
}

/*** IDirectDrawMediaStream methods ***/
static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_GetFormat(IDirectDrawMediaStream *iface,
        DDSURFACEDESC *current_format, IDirectDrawPalette **palette,
        DDSURFACEDESC *desired_format, DWORD *flags)
{
    FIXME("(%p)->(%p,%p,%p,%p) stub!\n", iface, current_format, palette, desired_format,
            flags);

    return MS_E_NOSTREAM;

}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_SetFormat(IDirectDrawMediaStream *iface,
        const DDSURFACEDESC *pDDSurfaceDesc, IDirectDrawPalette *pDirectDrawPalette)
{
    FIXME("(%p)->(%p,%p) stub!\n", iface, pDDSurfaceDesc, pDirectDrawPalette);

    return E_NOTIMPL;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_GetDirectDraw(IDirectDrawMediaStream *iface,
        IDirectDraw **ddraw)
{
    DirectDrawMediaStreamImpl *This = impl_from_IDirectDrawMediaStream(iface);

    TRACE("(%p)->(%p)\n", iface, ddraw);

    *ddraw = NULL;
    if (!This->ddraw)
    {
        HRESULT hr = DirectDrawCreateEx(NULL, (void**)&This->ddraw, &IID_IDirectDraw7, NULL);
        if (FAILED(hr))
            return hr;
        IDirectDraw7_SetCooperativeLevel(This->ddraw, NULL, DDSCL_NORMAL);
    }

    return IDirectDraw7_QueryInterface(This->ddraw, &IID_IDirectDraw, (void**)ddraw);
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_SetDirectDraw(IDirectDrawMediaStream *iface,
        IDirectDraw *pDirectDraw)
{
    FIXME("(%p)->(%p) stub!\n", iface, pDirectDraw);

    return E_NOTIMPL;
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_CreateSample(IDirectDrawMediaStream *iface,
        IDirectDrawSurface *surface, const RECT *rect, DWORD dwFlags,
        IDirectDrawStreamSample **ppSample)
{
    TRACE("(%p)->(%p,%s,%x,%p)\n", iface, surface, wine_dbgstr_rect(rect), dwFlags, ppSample);

    return ddrawstreamsample_create(iface, surface, rect, ppSample);
}

static HRESULT WINAPI DirectDrawMediaStreamImpl_IDirectDrawMediaStream_GetTimePerFrame(IDirectDrawMediaStream *iface,
        STREAM_TIME *pFrameTime)
{
    FIXME("(%p)->(%p) stub!\n", iface, pFrameTime);

    return E_NOTIMPL;
}

static const struct IDirectDrawMediaStreamVtbl DirectDrawMediaStreamImpl_IDirectDrawMediaStream_Vtbl =
{
    /*** IUnknown methods ***/
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_QueryInterface,
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_AddRef,
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_Release,
    /*** IMediaStream methods ***/
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_GetMultiMediaStream,
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_GetInformation,
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_SetSameFormat,
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_AllocateSample,
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_CreateSharedSample,
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_SendEndOfStream,
    /*** IDirectDrawMediaStream methods ***/
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_GetFormat,
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_SetFormat,
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_GetDirectDraw,
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_SetDirectDraw,
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_CreateSample,
    DirectDrawMediaStreamImpl_IDirectDrawMediaStream_GetTimePerFrame
};

struct enum_media_types
{
    IEnumMediaTypes IEnumMediaTypes_iface;
    LONG refcount;
    unsigned int index;
};

static const IEnumMediaTypesVtbl enum_media_types_vtbl;

static struct enum_media_types *impl_from_IEnumMediaTypes(IEnumMediaTypes *iface)
{
    return CONTAINING_RECORD(iface, struct enum_media_types, IEnumMediaTypes_iface);
}

static HRESULT WINAPI enum_media_types_QueryInterface(IEnumMediaTypes *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IEnumMediaTypes))
    {
        IEnumMediaTypes_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI enum_media_types_AddRef(IEnumMediaTypes *iface)
{
    struct enum_media_types *enum_media_types = impl_from_IEnumMediaTypes(iface);
    ULONG refcount = InterlockedIncrement(&enum_media_types->refcount);
    TRACE("%p increasing refcount to %u.\n", enum_media_types, refcount);
    return refcount;
}

static ULONG WINAPI enum_media_types_Release(IEnumMediaTypes *iface)
{
    struct enum_media_types *enum_media_types = impl_from_IEnumMediaTypes(iface);
    ULONG refcount = InterlockedDecrement(&enum_media_types->refcount);
    TRACE("%p decreasing refcount to %u.\n", enum_media_types, refcount);
    if (!refcount)
        heap_free(enum_media_types);
    return refcount;
}

static HRESULT WINAPI enum_media_types_Next(IEnumMediaTypes *iface, ULONG count, AM_MEDIA_TYPE **mts, ULONG *ret_count)
{
    struct enum_media_types *enum_media_types = impl_from_IEnumMediaTypes(iface);

    TRACE("iface %p, count %u, mts %p, ret_count %p.\n", iface, count, mts, ret_count);

    if (!ret_count)
        return E_POINTER;

    if (count && !enum_media_types->index)
    {
        mts[0] = CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
        memset(mts[0], 0, sizeof(AM_MEDIA_TYPE));
        mts[0]->majortype = MEDIATYPE_Video;
        mts[0]->subtype = MEDIASUBTYPE_RGB8;
        mts[0]->bFixedSizeSamples = TRUE;
        mts[0]->lSampleSize = 10000;
        ++enum_media_types->index;
        *ret_count = 1;
        return count == 1 ? S_OK : S_FALSE;
    }

    *ret_count = 0;
    return count ? S_FALSE : S_OK;
}

static HRESULT WINAPI enum_media_types_Skip(IEnumMediaTypes *iface, ULONG count)
{
    struct enum_media_types *enum_media_types = impl_from_IEnumMediaTypes(iface);

    TRACE("iface %p, count %u.\n", iface, count);

    enum_media_types->index += count;

    return S_OK;
}

static HRESULT WINAPI enum_media_types_Reset(IEnumMediaTypes *iface)
{
    struct enum_media_types *enum_media_types = impl_from_IEnumMediaTypes(iface);

    TRACE("iface %p.\n", iface);

    enum_media_types->index = 0;
    return S_OK;
}

static HRESULT WINAPI enum_media_types_Clone(IEnumMediaTypes *iface, IEnumMediaTypes **out)
{
    struct enum_media_types *enum_media_types = impl_from_IEnumMediaTypes(iface);
    struct enum_media_types *object;

    TRACE("iface %p, out %p.\n", iface, out);

    if (!(object = heap_alloc(sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IEnumMediaTypes_iface.lpVtbl = &enum_media_types_vtbl;
    object->refcount = 1;
    object->index = enum_media_types->index;

    *out = &object->IEnumMediaTypes_iface;
    return S_OK;
}

static const IEnumMediaTypesVtbl enum_media_types_vtbl =
{
    enum_media_types_QueryInterface,
    enum_media_types_AddRef,
    enum_media_types_Release,
    enum_media_types_Next,
    enum_media_types_Skip,
    enum_media_types_Reset,
    enum_media_types_Clone,
};

static inline DirectDrawMediaStreamInputPin *impl_from_DirectDrawMediaStreamInputPin_IPin(IPin *iface)
{
    return CONTAINING_RECORD(iface, DirectDrawMediaStreamInputPin, pin.pin.IPin_iface);
}

/*** IUnknown methods ***/
static HRESULT WINAPI DirectDrawMediaStreamInputPin_IPin_QueryInterface(IPin *iface, REFIID riid, void **ret_iface)
{
    DirectDrawMediaStreamInputPin *This = impl_from_DirectDrawMediaStreamInputPin_IPin(iface);

    return IAMMediaStream_QueryInterface(&This->parent->IAMMediaStream_iface, riid, ret_iface);
}

static ULONG WINAPI DirectDrawMediaStreamInputPin_IPin_AddRef(IPin *iface)
{
    DirectDrawMediaStreamInputPin *This = impl_from_DirectDrawMediaStreamInputPin_IPin(iface);

    return IAMMediaStream_AddRef(&This->parent->IAMMediaStream_iface);
}

static ULONG WINAPI DirectDrawMediaStreamInputPin_IPin_Release(IPin *iface)
{
    DirectDrawMediaStreamInputPin *This = impl_from_DirectDrawMediaStreamInputPin_IPin(iface);

    return IAMMediaStream_Release(&This->parent->IAMMediaStream_iface);
}

static HRESULT WINAPI ddraw_pin_EnumMediaTypes(IPin *iface, IEnumMediaTypes **enum_media_types)
{
    struct enum_media_types *object;

    TRACE("iface %p, enum_media_types %p.\n", iface, enum_media_types);

    if (!enum_media_types)
        return E_POINTER;

    if (!(object = heap_alloc(sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IEnumMediaTypes_iface.lpVtbl = &enum_media_types_vtbl;
    object->refcount = 1;
    object->index = 0;

    *enum_media_types = &object->IEnumMediaTypes_iface;
    return S_OK;
}

static const IPinVtbl DirectDrawMediaStreamInputPin_IPin_Vtbl =
{
    DirectDrawMediaStreamInputPin_IPin_QueryInterface,
    DirectDrawMediaStreamInputPin_IPin_AddRef,
    DirectDrawMediaStreamInputPin_IPin_Release,
    BaseInputPinImpl_Connect,
    BaseInputPinImpl_ReceiveConnection,
    BasePinImpl_Disconnect,
    BasePinImpl_ConnectedTo,
    BasePinImpl_ConnectionMediaType,
    BasePinImpl_QueryPinInfo,
    BasePinImpl_QueryDirection,
    BasePinImpl_QueryId,
    BasePinImpl_QueryAccept,
    ddraw_pin_EnumMediaTypes,
    BasePinImpl_QueryInternalConnections,
    BaseInputPinImpl_EndOfStream,
    BaseInputPinImpl_BeginFlush,
    BaseInputPinImpl_EndFlush,
    BaseInputPinImpl_NewSegment,
};

static HRESULT WINAPI DirectDrawMediaStreamInputPin_CheckMediaType(BasePin *base, const AM_MEDIA_TYPE *media_type)
{
    DirectDrawMediaStreamInputPin *This = impl_from_DirectDrawMediaStreamInputPin_IPin(&base->IPin_iface);

    TRACE("(%p)->(%p)\n", This, media_type);

    if (IsEqualGUID(&media_type->majortype, &MEDIATYPE_Video))
    {
        if (IsEqualGUID(&media_type->subtype, &MEDIASUBTYPE_RGB1) ||
            IsEqualGUID(&media_type->subtype, &MEDIASUBTYPE_RGB4) ||
            IsEqualGUID(&media_type->subtype, &MEDIASUBTYPE_RGB8)  ||
            IsEqualGUID(&media_type->subtype, &MEDIASUBTYPE_RGB565) ||
            IsEqualGUID(&media_type->subtype, &MEDIASUBTYPE_RGB555) ||
            IsEqualGUID(&media_type->subtype, &MEDIASUBTYPE_RGB24) ||
            IsEqualGUID(&media_type->subtype, &MEDIASUBTYPE_RGB32))
        {
            TRACE("Video sub-type %s matches\n", debugstr_guid(&media_type->subtype));
            return S_OK;
        }
    }

    return S_FALSE;
}

static HRESULT WINAPI DirectDrawMediaStreamInputPin_GetMediaType(BasePin *base, int index, AM_MEDIA_TYPE *media_type)
{
    DirectDrawMediaStreamInputPin *This = impl_from_DirectDrawMediaStreamInputPin_IPin(&base->IPin_iface);

    TRACE("(%p)->(%d,%p)\n", This, index, media_type);

    /* FIXME: Reset structure as we only fill majortype and minortype for now */
    ZeroMemory(media_type, sizeof(*media_type));

    media_type->majortype = MEDIATYPE_Video;

    switch (index)
    {
        case 0:
            media_type->subtype = MEDIASUBTYPE_RGB1;
            break;
        case 1:
            media_type->subtype = MEDIASUBTYPE_RGB4;
            break;
        case 2:
            media_type->subtype = MEDIASUBTYPE_RGB8;
            break;
        case 3:
            media_type->subtype = MEDIASUBTYPE_RGB565;
            break;
        case 4:
            media_type->subtype = MEDIASUBTYPE_RGB555;
            break;
        case 5:
            media_type->subtype = MEDIASUBTYPE_RGB24;
            break;
        case 6:
            media_type->subtype = MEDIASUBTYPE_RGB32;
            break;
        default:
            return S_FALSE;
    }

    return S_OK;
}

static HRESULT WINAPI DirectDrawMediaStreamInputPin_Receive(BaseInputPin *base, IMediaSample *sample)
{
    DirectDrawMediaStreamInputPin *This = impl_from_DirectDrawMediaStreamInputPin_IPin(&base->pin.IPin_iface);

    FIXME("(%p)->(%p) stub!\n", This, sample);

    return E_NOTIMPL;
}

static const BaseInputPinFuncTable DirectDrawMediaStreamInputPin_FuncTable =
{
    {
        DirectDrawMediaStreamInputPin_CheckMediaType,
        DirectDrawMediaStreamInputPin_GetMediaType,
    },
    DirectDrawMediaStreamInputPin_Receive,
};

HRESULT ddrawmediastream_create(IMultiMediaStream *parent, const MSPID *purpose_id,
        IUnknown *stream_object, STREAM_TYPE stream_type, IAMMediaStream **media_stream)
{
    DirectDrawMediaStreamImpl *object;
    PIN_INFO pin_info;
    HRESULT hr;

    TRACE("(%p,%s,%p,%p)\n", parent, debugstr_guid(purpose_id), stream_object, media_stream);

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->IAMMediaStream_iface.lpVtbl = &DirectDrawMediaStreamImpl_IAMMediaStream_Vtbl;
    object->IDirectDrawMediaStream_iface.lpVtbl = &DirectDrawMediaStreamImpl_IDirectDrawMediaStream_Vtbl;
    object->ref = 1;

    InitializeCriticalSection(&object->critical_section);

    pin_info.pFilter = NULL;
    pin_info.dir = PINDIR_INPUT;
    pin_info.achName[0] = 'I';
    StringFromGUID2(purpose_id, pin_info.achName + 1, MAX_PIN_NAME - 1);
    hr = BaseInputPin_Construct(&DirectDrawMediaStreamInputPin_IPin_Vtbl,
        sizeof(DirectDrawMediaStreamInputPin), &pin_info, &DirectDrawMediaStreamInputPin_FuncTable,
        &object->critical_section, NULL, (IPin **)&object->input_pin);
    if (FAILED(hr))
        goto out_object;

    object->input_pin->parent = object;

    object->parent = parent;
    object->purpose_id = *purpose_id;
    object->stream_type = stream_type;

    if (stream_object
            && FAILED(hr = IUnknown_QueryInterface(stream_object, &IID_IDirectDraw7, (void **)&object->ddraw)))
        FIXME("Stream object doesn't implement IDirectDraw7 interface, hr %#x.\n", hr);

    *media_stream = &object->IAMMediaStream_iface;

    return S_OK;

out_object:
    HeapFree(GetProcessHeap(), 0, object);

    return hr;
}

typedef struct {
    IDirectDrawStreamSample IDirectDrawStreamSample_iface;
    LONG ref;
    IMediaStream *parent;
    IDirectDrawSurface *surface;
    RECT rect;
} IDirectDrawStreamSampleImpl;

static inline IDirectDrawStreamSampleImpl *impl_from_IDirectDrawStreamSample(IDirectDrawStreamSample *iface)
{
    return CONTAINING_RECORD(iface, IDirectDrawStreamSampleImpl, IDirectDrawStreamSample_iface);
}

/*** IUnknown methods ***/
static HRESULT WINAPI IDirectDrawStreamSampleImpl_QueryInterface(IDirectDrawStreamSample *iface,
        REFIID riid, void **ret_iface)
{
    TRACE("(%p)->(%s,%p)\n", iface, debugstr_guid(riid), ret_iface);

    if (IsEqualGUID(riid, &IID_IUnknown) ||
        IsEqualGUID(riid, &IID_IStreamSample) ||
        IsEqualGUID(riid, &IID_IDirectDrawStreamSample))
    {
        IDirectDrawStreamSample_AddRef(iface);
        *ret_iface = iface;
        return S_OK;
    }

    *ret_iface = NULL;

    ERR("(%p)->(%s,%p),not found\n", iface, debugstr_guid(riid), ret_iface);
    return E_NOINTERFACE;
}

static ULONG WINAPI IDirectDrawStreamSampleImpl_AddRef(IDirectDrawStreamSample *iface)
{
    IDirectDrawStreamSampleImpl *This = impl_from_IDirectDrawStreamSample(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p)->(): new ref = %u\n", iface, ref);

    return ref;
}

static ULONG WINAPI IDirectDrawStreamSampleImpl_Release(IDirectDrawStreamSample *iface)
{
    IDirectDrawStreamSampleImpl *This = impl_from_IDirectDrawStreamSample(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p)->(): new ref = %u\n", iface, ref);

    if (!ref)
    {
        if (This->surface)
            IDirectDrawSurface_Release(This->surface);
        IMediaStream_Release(This->parent);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

/*** IStreamSample methods ***/
static HRESULT WINAPI IDirectDrawStreamSampleImpl_GetMediaStream(IDirectDrawStreamSample *iface, IMediaStream **media_stream)
{
    FIXME("(%p)->(%p): stub\n", iface, media_stream);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirectDrawStreamSampleImpl_GetSampleTimes(IDirectDrawStreamSample *iface, STREAM_TIME *start_time,
                                                                 STREAM_TIME *end_time, STREAM_TIME *current_time)
{
    FIXME("(%p)->(%p,%p,%p): stub\n", iface, start_time, end_time, current_time);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirectDrawStreamSampleImpl_SetSampleTimes(IDirectDrawStreamSample *iface, const STREAM_TIME *start_time,
                                                                 const STREAM_TIME *end_time)
{
    FIXME("(%p)->(%p,%p): stub\n", iface, start_time, end_time);

    return E_NOTIMPL;
}

static HRESULT WINAPI IDirectDrawStreamSampleImpl_Update(IDirectDrawStreamSample *iface, DWORD flags, HANDLE event,
                                                         PAPCFUNC func_APC, DWORD APC_data)
{
    FIXME("(%p)->(%x,%p,%p,%u): stub\n", iface, flags, event, func_APC, APC_data);

    return S_OK;
}

static HRESULT WINAPI IDirectDrawStreamSampleImpl_CompletionStatus(IDirectDrawStreamSample *iface, DWORD flags, DWORD milliseconds)
{
    FIXME("(%p)->(%x,%u): stub\n", iface, flags, milliseconds);

    return E_NOTIMPL;
}

/*** IDirectDrawStreamSample methods ***/
static HRESULT WINAPI IDirectDrawStreamSampleImpl_GetSurface(IDirectDrawStreamSample *iface, IDirectDrawSurface **ddraw_surface,
                                                             RECT *rect)
{
    IDirectDrawStreamSampleImpl *This = impl_from_IDirectDrawStreamSample(iface);

    TRACE("(%p)->(%p,%p)\n", iface, ddraw_surface, rect);

    if (ddraw_surface)
    {
        *ddraw_surface = This->surface;
        if (*ddraw_surface)
            IDirectDrawSurface_AddRef(*ddraw_surface);
    }

    if (rect)
        *rect = This->rect;

    return S_OK;
}

static HRESULT WINAPI IDirectDrawStreamSampleImpl_SetRect(IDirectDrawStreamSample *iface, const RECT *rect)
{
    FIXME("(%p)->(%p): stub\n", iface, rect);

    return E_NOTIMPL;
}

static const struct IDirectDrawStreamSampleVtbl DirectDrawStreamSample_Vtbl =
{
    /*** IUnknown methods ***/
    IDirectDrawStreamSampleImpl_QueryInterface,
    IDirectDrawStreamSampleImpl_AddRef,
    IDirectDrawStreamSampleImpl_Release,
    /*** IStreamSample methods ***/
    IDirectDrawStreamSampleImpl_GetMediaStream,
    IDirectDrawStreamSampleImpl_GetSampleTimes,
    IDirectDrawStreamSampleImpl_SetSampleTimes,
    IDirectDrawStreamSampleImpl_Update,
    IDirectDrawStreamSampleImpl_CompletionStatus,
    /*** IDirectDrawStreamSample methods ***/
    IDirectDrawStreamSampleImpl_GetSurface,
    IDirectDrawStreamSampleImpl_SetRect
};

static HRESULT ddrawstreamsample_create(IDirectDrawMediaStream *parent, IDirectDrawSurface *surface,
    const RECT *rect, IDirectDrawStreamSample **ddraw_stream_sample)
{
    IDirectDrawStreamSampleImpl *object;
    HRESULT hr;

    TRACE("(%p)\n", ddraw_stream_sample);

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->IDirectDrawStreamSample_iface.lpVtbl = &DirectDrawStreamSample_Vtbl;
    object->ref = 1;
    object->parent = (IMediaStream*)parent;
    IMediaStream_AddRef(object->parent);

    if (surface)
    {
        object->surface = surface;
        IDirectDrawSurface_AddRef(surface);
    }
    else
    {
        DDSURFACEDESC desc;
        IDirectDraw *ddraw;

        hr = IDirectDrawMediaStream_GetDirectDraw(parent, &ddraw);
        if (FAILED(hr))
        {
            IDirectDrawStreamSample_Release(&object->IDirectDrawStreamSample_iface);
            return hr;
        }

        desc.dwSize = sizeof(desc);
        desc.dwFlags = DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH|DDSD_PIXELFORMAT;
        desc.dwHeight = 100;
        desc.dwWidth = 100;
        desc.ddpfPixelFormat.dwSize = sizeof(desc.ddpfPixelFormat);
        desc.ddpfPixelFormat.dwFlags = DDPF_RGB;
        desc.ddpfPixelFormat.u1.dwRGBBitCount = 32;
        desc.ddpfPixelFormat.u2.dwRBitMask = 0xff0000;
        desc.ddpfPixelFormat.u3.dwGBitMask = 0x00ff00;
        desc.ddpfPixelFormat.u4.dwBBitMask = 0x0000ff;
        desc.ddpfPixelFormat.u5.dwRGBAlphaBitMask = 0;
        desc.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY|DDSCAPS_OFFSCREENPLAIN;
        desc.lpSurface = NULL;

        hr = IDirectDraw_CreateSurface(ddraw, &desc, &object->surface, NULL);
        IDirectDraw_Release(ddraw);
        if (FAILED(hr))
        {
            ERR("failed to create surface, 0x%08x\n", hr);
            IDirectDrawStreamSample_Release(&object->IDirectDrawStreamSample_iface);
            return hr;
        }
    }

    if (rect)
        object->rect = *rect;
    else if (object->surface)
    {
        DDSURFACEDESC desc = { sizeof(desc) };
        hr = IDirectDrawSurface_GetSurfaceDesc(object->surface, &desc);
        if (hr == S_OK)
            SetRect(&object->rect, 0, 0, desc.dwWidth, desc.dwHeight);
    }

    *ddraw_stream_sample = &object->IDirectDrawStreamSample_iface;

    return S_OK;
}