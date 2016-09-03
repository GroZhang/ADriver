#include <streams.h>

#include "UacRendererFilteruids.h"
#include "UacRendererFilter.h"
#include "CUacInputPin.h"
#include "CUacFilter.h"
#include "UacRenderer.h"
#include "UacHelper.h"
#include "stdafx.h"
#include "CUacFilterManager.h"

//
//  Definition of CUacInputPin
//
CUacInputPin::CUacInputPin(CUacFilterManager *pDump,
                             LPUNKNOWN pUnk,
                             CBaseFilter *pFilter,
                             CCritSec *pLock,
                             CCritSec *pReceiveLock,
                             HRESULT *phr) :

    CRenderedInputPin(NAME("CUacInputPin"),
                  pFilter,                   // Filter
                  pLock,                     // Locking
                  phr,                       // Return code
                  L"Input"),                 // Pin name
    m_pReceiveLock(pReceiveLock),
    m_pManager(pDump),
    m_tLast(0),
	m_tSegmentStart(0)
{
}


//
// CheckMediaType
//
// Check if the pin can support this specific proposed type and format
//
HRESULT CUacInputPin::CheckMediaType(const CMediaType* mt)
{
	HRESULT hr=S_FALSE;
	if(mt->formattype==FORMAT_WaveFormatEx)
	{
		bool isOK=m_pManager->CheckFormat((WAVEFORMATEX*)mt->pbFormat);
		if(isOK)
			hr=S_OK;
		DebugPrintf(L"CUacInputPin::CheckMediaType - WaveFormatEx: ISOK: %d\n",isOK);
	}
	else if(mt->formattype==FORMAT_None)
	{
		hr=S_FALSE;
		DebugPrintf(L"CUacInputPin::CheckMediaType - FORMAT_None. OK.\n");
	}
	else
	{
		DebugPrintf(L"CUacInputPin::CheckMediaType - Unkown format. Not OK.\n");
	}
    return hr;
}

HRESULT CUacInputPin::SetMediaType(const CMediaType *pmt)
{
	HRESULT hr = CBasePin::SetMediaType(pmt);
	DebugPrintf(L"CUacInputPin::SetMediaType\n");
	m_pManager->SetFormatReceived((CMediaType*)pmt);
    return hr;
}

HRESULT CUacInputPin::NotifyAllocator(
                    IMemAllocator * pAllocator,
                    BOOL bReadOnly)
{
	return CBaseInputPin::NotifyAllocator(pAllocator,bReadOnly);
}

//
// BreakConnect
//
// Break a connection
//
HRESULT CUacInputPin::BreakConnect()
{
    //if (m_pManager->m_pPosition != NULL) {
    //    m_pManager->m_pPosition->ForceRefresh();
    //}

	DebugPrintf(L"CUacInputPin::BreakConnect \n");
	m_pManager->StopRendering(true,true);

    return CRenderedInputPin::BreakConnect();
}


HRESULT CUacInputPin::CompleteConnect(IPin *pReceivePin)
{
	return CRenderedInputPin::CompleteConnect(pReceivePin);
}

//
// ReceiveCanBlock
//
// We don't hold up source threads on Receive
//
STDMETHODIMP CUacInputPin::ReceiveCanBlock()
{
    return S_FALSE;
}

//
// Receive
//
// Do something with this media sample
//
STDMETHODIMP CUacInputPin::Receive(IMediaSample *pSample)
{
//	DebugPrintf(L"Receive\n");

	CAutoLock lock(m_pReceiveLock);
    CheckPointer(pSample,E_POINTER);
	bool mediaTypeFailed=false;

    HRESULT hr = CBaseInputPin::Receive(pSample);
	if(hr!=S_OK)
	{
		DebugPrintf(L"CUacInputPin::Receive CBaseInputPin returned error. %d \n",hr);
		goto exit;
	}

	CMediaType* mediaType=NULL;
	hr=pSample->GetMediaType((AM_MEDIA_TYPE**)&mediaType);
	if(hr==S_OK)
	{
		hr = CBasePin::SetMediaType(mediaType);
		DeleteMediaType(mediaType);
	}

	hr= m_pManager->SampleReceived(pSample);
	if(hr==S_FALSE)		//Media type rejected by Uac engine
	{
		DebugPrintf(L"CUacInputPin::Receive returning EC_ERRORABORT. \n");
		CBasePin::EndOfStream();
		m_pFilter->NotifyEvent(EC_ERRORABORT,NULL,NULL);
		goto exit;
	}
exit:
	return hr;
 }

HRESULT CUacInputPin::Active()
{
	DebugPrintf(L"CUacInputPin::Active \n");
	return CRenderedInputPin::Active();
}
HRESULT CUacInputPin::Inactive()
{
	DebugPrintf(L"CUacInputPin::Inactive \n");
	return CBaseInputPin::Inactive();
}

STDMETHODIMP CUacInputPin::BeginFlush(void)
{
	DebugPrintf(L"CUacInputPin::BeginFlush \n");
	HRESULT hr=CBaseInputPin::BeginFlush();
	m_pManager->ClearQueue();
	return hr;
}

// Todo: Also, if the filter processes Receive calls asynchronously, the pin should wait to send the EC_COMPLETE event until the filter has processed all pending samples.
// http://msdn.microsoft.com/en-us/library/dd375164%28VS.85%29.aspx, http://msdn.microsoft.com/en-us/library/dd388900%28VS.85%29.aspx
STDMETHODIMP CUacInputPin::EndOfStream(void)
{
    CAutoLock lock(m_pReceiveLock);
	DebugPrintf(L"CUacInputPin::EndOfStream \n");
    return CRenderedInputPin::EndOfStream();

} // EndOfStream


//
// NewSegment
//
// Called when we are seeked
//
STDMETHODIMP CUacInputPin::NewSegment(REFERENCE_TIME tStart,
                                       REFERENCE_TIME tStop,
                                       double dRate)
{
	DebugPrintf(L"CUacInputPin::NewSegment \n");
	m_tSegmentStart=tStart;
    m_tLast = 0;
    return S_OK;

} // NewSegment

