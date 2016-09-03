#include <streams.h>
#include "UacRendererFilteruids.h"

#include "CUacFilter.h"
#include "CUacInputPin.h"
#include "UacRendererFilter.h"
#include "UacRenderer.h"
#include "UacHelper.h"
#include "stdafx.h"
#include "CUacFilterManager.h"
#include "CResampler.h"
#include "CUacUtils.h"

//
//  CUacFilterManager class
//
CUacFilterManager::CUacFilterManager(LPUNKNOWN pUnk, HRESULT *phr) :
    CBaseReferenceClock(NAME("CUacFilterManager"), pUnk, phr, NULL),
    m_pFilter(NULL),
    m_pPin(NULL),
    m_pPosition(NULL),
	m_pRenderer(NULL),
	m_pCurrentMediaTypeReceive(NULL),
	m_pCurrentMediaTypeResample(NULL),
	m_IsExclusive(true),
	m_pResampler(new CResampler()),
	m_currentMediaTypeSampleReceivedAction(ReceivedSampleActions_RejectLoud)
{
    ASSERT(phr);
    
    m_pFilter = new CUacFilter(this, GetOwner(), &m_Lock, phr);
    if (m_pFilter == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
        return;
    }

    m_pPin = new CUacInputPin(this,GetOwner(),
                               m_pFilter,
                               &m_Lock,
                               &m_ReceiveLock,
                               phr);
    if (m_pPin == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
        return;

    }

	m_pRenderer=new CUacRenderer(NULL);
}

bool CUacFilterManager::CheckFormat(WAVEFORMATEX* requestedFormat)
{
	bool retValue=false;

	_AUDCLNT_SHAREMODE shareMode=AUDCLNT_SHAREMODE_SHARED;
	if(m_IsExclusive)
		shareMode=AUDCLNT_SHAREMODE_EXCLUSIVE;

	WAVEFORMATEX* pSuggestedFormat=NULL;

	bool isSupportedNoResample = m_pRenderer->CheckFormat(requestedFormat, &pSuggestedFormat, shareMode);
	if(isSupportedNoResample) {
		retValue= true;
		goto exit;
	}

	if(CResampler::CanResample(requestedFormat, pSuggestedFormat)) {
		retValue= true;
		goto exit;
	}
exit:
	if(pSuggestedFormat) {
		CoTaskMemFree(pSuggestedFormat);
	}
	return retValue;
}

bool CUacFilterManager::StartRendering()
{
	HRESULT hr=S_OK;
	m_pRenderer->Start(20);
	m_pRenderer->SetIsProcessing(true);
	return true;
}

bool CUacFilterManager::PauseRendering()
{
	HRESULT hr=S_OK;
	m_pRenderer->Start(20);
	m_pRenderer->SetIsProcessing(false);
	return true;
}

bool CUacFilterManager::ClearQueue()
{
	m_pRenderer->ClearQueue();
	return true;
}

bool CUacFilterManager::StopRendering(bool clearQueue, bool clearFormats)
{
	m_pRenderer->SetIsProcessing(false);
	m_pRenderer->Stop();
	if(clearQueue)
		m_pRenderer->ClearQueue();
	if(clearFormats)
		SetFormatReceived(NULL);
	return true;
}

HRESULT CUacFilterManager::SetDevice(LPCWSTR pDevID)
{
	//Destroy Renderer and create new one.
	if(m_pRenderer)
		delete m_pRenderer;
	m_pRenderer=new CUacRenderer(pDevID);
	HRESULT hr=S_OK;
	return hr;
}

HRESULT CUacFilterManager::GetDevice(LPWSTR* ppDevID)
{
	//Destroy Renderer and create new one.
	if(!m_pRenderer) {
		*ppDevID=NULL;
		return S_FALSE;
	}
	return m_pRenderer->GetDeviceId((LPWSTR*)ppDevID);
}

HRESULT CUacFilterManager::GetUacMixFormat(WAVEFORMATEX** ppFormat)
{
	WAVEFORMATEX* pFormat=m_pRenderer->GetUacMixFormat();
	*ppFormat=pFormat;
    WAVEFORMATEXTENSIBLE* pFormatExt=(WAVEFORMATEXTENSIBLE*)pFormat;
	return S_OK;
}

HRESULT CUacFilterManager::GetCurrentInputFormat(RefCountingWaveFormatEx** ppFormat)
{
	CAutoLock lock(&m_MediaTypeLock);
	if(m_pCurrentMediaTypeReceive)
		m_pCurrentMediaTypeReceive->AddRef();
	*ppFormat=m_pCurrentMediaTypeReceive;
	return S_OK;
}

HRESULT CUacFilterManager::GetCurrentResampledFormat(RefCountingWaveFormatEx** ppFormat)
{
	CAutoLock lock(&m_MediaTypeLock);
	if(m_pCurrentMediaTypeResample)
		m_pCurrentMediaTypeResample->AddRef();
	*ppFormat=m_pCurrentMediaTypeResample;
	return S_OK;
}

HRESULT CUacFilterManager::GetDeviceInfos(bool includeDisconnected, UacDeviceInfo** ppDestInfos, int* pInfoCount, int* pIndexDefault)
{
	return CUacUtils::GetDeviceInfos(includeDisconnected, ppDestInfos, pInfoCount, pIndexDefault);
}

HRESULT CUacFilterManager::GetExclusiveMode(bool* pIsExclusive)
{
	HRESULT hr=S_OK;
	*pIsExclusive=m_IsExclusive;
	return hr;

}

HRESULT CUacFilterManager::SetExclusiveMode(bool pIsExclusive)
{
	HRESULT hr=S_FALSE;

	if(m_IsExclusive!=pIsExclusive)
	{
		m_IsExclusive=pIsExclusive;	
		return SetFormatProcessed();
	}
	return hr;
}

HRESULT CUacFilterManager::GetActiveMode(int* pMode)
{
	HRESULT hr=S_OK;
	*pMode=	m_pRenderer->InitializedMode;
	return hr;
}

//Invoked:
//- When "user" switches exclusive mode on/off
//- When mediatype has changed
//Sets m_pCurrentMediaTypeResample and initializes resampler based on m_pCurrentMediaTypeReceive, exclusive/shared mode and Uac engines suggestion.
//m_pCurrentMediaTypeResample is set to NULL if resample is not required (or resampling not possible)
HRESULT CUacFilterManager::SetFormatProcessed()
{
	CAutoLock lock(&m_MediaTypeLock);

	if(m_pCurrentMediaTypeResample)
	{
		m_pCurrentMediaTypeResample->Release();
		m_pCurrentMediaTypeResample=NULL;
	}

	if(!m_pCurrentMediaTypeReceive)
		return S_FALSE;

	WAVEFORMATEX* pSrcFormat=m_pCurrentMediaTypeReceive->GetFormat();

	if(!pSrcFormat)
		return S_FALSE;


	WAVEFORMATEX* pSuggestedFormat=NULL;

	AUDCLNT_SHAREMODE shareMode=AUDCLNT_SHAREMODE_SHARED;
	if(m_IsExclusive)
		shareMode=AUDCLNT_SHAREMODE_EXCLUSIVE;

	bool isSupportedNoResample = m_pRenderer->CheckFormat(m_pCurrentMediaTypeReceive->GetFormat(), &pSuggestedFormat, shareMode);
	
	if(pSuggestedFormat)
	{
		if(!isSupportedNoResample)
		{
			m_pCurrentMediaTypeResample = RefCountingWaveFormatEx::CopyAndCreate(pSuggestedFormat);
		}
		CoTaskMemFree(pSuggestedFormat);
	}

	if(isSupportedNoResample)
	{
		return S_OK;
	}

	if(!m_pCurrentMediaTypeResample)			//Source format not supported, no suggested format (if this happens in real life we should try to
	{
		return S_FALSE;
	}


	if(CResampler::CanResample(pSrcFormat, m_pCurrentMediaTypeResample->GetFormat()))
	{
		return S_OK;
	}

	return S_FALSE;			//Can not resample to suggested format
}

//Invoked from InputsPin->SetMediaType (Graph control thread) and from SampleReceived (parser/decoder thread)
//Sets m_pCurrentMediaTypeReceive based on the CMediaType parameter
HRESULT CUacFilterManager::SetFormatReceived(CMediaType* pmt)
{
	ReceivedSampleActions newAction=ReceivedSampleActions_RejectLoud;
	HRESULT hr=S_OK;

	CAutoLock lock(&m_MediaTypeLock);
	if(m_pCurrentMediaTypeReceive)
	{
		m_pCurrentMediaTypeReceive->Release();
		m_pCurrentMediaTypeReceive=NULL;
	}

	if(pmt==NULL) {
		SetFormatProcessed();		//Will clear processed format when source format is NULL;
		return hr;
	}

	if(pmt->formattype==FORMAT_WaveFormatEx)
	{
		WAVEFORMATEX* formatNew=(WAVEFORMATEX*)pmt->pbFormat;
		m_pCurrentMediaTypeReceive = RefCountingWaveFormatEx::CopyAndCreate(formatNew);

		if(SetFormatProcessed()==S_OK)
		{
			newAction=ReceivedSampleActions_Accept;
		}
	}
	else if(pmt->formattype==FORMAT_None)
	{
		DebugPrintf(L"SampleReceived - FORMAT_None format. Rejecting loud. \n");
	}
	else
	{
		DebugPrintf(L"SampleReceived - Unkown format. Rejecting loud.\n");
	}

	m_currentMediaTypeSampleReceivedAction=newAction;
	return hr;
}


//Returns S_FALSE if sampleformat is not supported
HRESULT CUacFilterManager::SampleReceived(IMediaSample *pSample)
{
	int hr=S_OK;
	CMediaType* mediaType=NULL;
	
	//Check if mediatype has changed (sample usually only contains mediatype if changed)
	hr=pSample->GetMediaType((AM_MEDIA_TYPE**)&mediaType);
	if(hr==S_OK)
	{	
		DebugPrintf(L"SampleReceived - Mediatype has changed.\n");
		SetFormatReceived(mediaType);	//Makes a ref counting copy
	}
	hr=S_OK;
	//CurrentMediaType is either a accepted waveformat or FORMAT_None. 
	//If FORMAT_None, just ignore the sample.
	if(m_currentMediaTypeSampleReceivedAction==ReceivedSampleActions_Accept)
	{
		hr=S_OK;

		RefCountingWaveFormatEx* srcRefType=m_pCurrentMediaTypeReceive;
		srcRefType->AddRef();
		RefCountingWaveFormatEx* destRefType=(m_pCurrentMediaTypeResample == NULL ?  srcRefType : m_pCurrentMediaTypeResample);
		destRefType->AddRef();

		IMediaBufferEx* pSimple = m_pResampler->CreateSample(pSample,srcRefType->GetFormat(),destRefType->GetFormat());
		srcRefType->Release();

		m_pRenderer->AddSampleToQueue(pSimple,destRefType,m_IsExclusive);  //Renderer will release sample and mediaType after they are pulled/cleared from the queue.
	}
	else if(m_currentMediaTypeSampleReceivedAction==ReceivedSampleActions_RejectLoud)
	{
		hr=S_FALSE;
	}
exit:
	if(mediaType)
		DeleteMediaType(mediaType);
    return hr;
}

// Destructor
CUacFilterManager::~CUacFilterManager()
{
    delete m_pPin;
    delete m_pFilter;
    delete m_pPosition;
	if(m_pRenderer)
		delete m_pRenderer;
	m_pRenderer=NULL;

	delete m_pResampler;
}

REFERENCE_TIME CUacFilterManager::GetPrivateTime()
{
    CAutoLock cObjectLock(this);
 
 
    /* If the clock has wrapped then the current time will be less than
     * the last time we were notified so add on the extra milliseconds
     *
     * The time period is long enough so that the likelihood of
     * successive calls spanning the clock cycle is not considered.
     */
	REFERENCE_TIME clockFromSampleTime=NULL;
	REFERENCE_TIME clockFromSystemTime=NULL;
	DWORD dwTime = timeGetTime();
	if(m_pFilter->GetState()==State_Running)
	{
		REFERENCE_TIME sampleTime=m_pRenderer->GetCurrentSampleTime();
		REFERENCE_TIME startTime=m_pFilter->GetStartTime();
		//dwTime=(sampleTime+startTime)/10000;
		if(sampleTime>NULL)
			clockFromSampleTime=sampleTime+startTime;
		//REFERENCE_TIME PrivateTime = 
	}
	clockFromSystemTime=Int32x32To64(UNITS / MILLISECONDS, (DWORD)dwTime);
	//DebugPrintf(L"GetPrivateTime NULL - %lld, %lld\n",clockFromSampleTime,clockFromSystemTime);
	return clockFromSampleTime!=NULL ? clockFromSampleTime : clockFromSystemTime;
}


// CreateInstance
// Provide the way for COM to create the filter
CUnknown * WINAPI CUacFilterManager::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    ASSERT(phr);
    CUacFilterManager *pNewObject = new CUacFilterManager(punk, phr);
    if (pNewObject == NULL) {
        if (phr)
            *phr = E_OUTOFMEMORY;
    }

    return pNewObject;

} // CreateInstance

// NonDelegatingQueryInterface
// Override this to say what interfaces we support where
STDMETHODIMP CUacFilterManager::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
    CheckPointer(ppv,E_POINTER);
    CAutoLock lock(&m_Lock);

    // Do we have this interface

    if (riid == __uuidof(IRendererFilterUac)) 
	{
        return GetInterface((IRendererFilterUac *) this, ppv);
    } 
 if (riid == IID_ISpecifyPropertyPages)
    {
        return GetInterface(static_cast<ISpecifyPropertyPages*>(this), ppv);
    }
    else if (riid == IID_IBaseFilter || riid == IID_IMediaFilter || riid == IID_IPersist) 
	{
		return ((CBaseFilter*)m_pFilter)->NonDelegatingQueryInterface(riid, ppv);
    } 
	else if (riid == IID_IReferenceClock || riid == IID_IReferenceClockTimerControl) 
	{
		return GetInterface((IReferenceClock  *) this, ppv);
    } 
    else if (riid == IID_IMediaPosition || riid == IID_IMediaSeeking) {
        if (m_pPosition == NULL) 
        {

            HRESULT hr = S_OK;
            m_pPosition = new CPosPassThru(NAME("Dump Pass Through"),
                                           (IUnknown *) GetOwner(),
                                           (HRESULT *) &hr, m_pPin);
            if (m_pPosition == NULL) 
                return E_OUTOFMEMORY;

            if (FAILED(hr)) 
            {
                delete m_pPosition;
                m_pPosition = NULL;
                return hr;
            }
        }

        return m_pPosition->NonDelegatingQueryInterface(riid, ppv);
    } 

    return CUnknown::NonDelegatingQueryInterface(riid, ppv);

} // NonDelegatingQueryInterface



