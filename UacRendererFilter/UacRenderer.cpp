#include <streams.h>
#include "StdAfx.h"
#include <assert.h>
#include <avrt.h>
#include "UacRenderer.h"
#include "FormatUtils.h"
#define AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED AUDCLNT_ERR(0x019)

CUacRenderer::CUacRenderer(LPCWSTR pDevID) : 
    //_AudioClient(NULL),
    //_RenderClient(NULL),
    _RenderBufferQueue(NULL),
    _RenderThread(NULL),
    _ShutdownEvent(NULL),
    _ProcessSamplesInQueueEvent(NULL),
    _AudioBufferReadyEvent(NULL),
	_ExitFeederLoopEvent(NULL),
	_pCurrentMediaType(NULL),
	_pCurrentSample(NULL),
	_CurrentSampleOffset(0),
	CurrentSampleStart(NULL),
	CurrentSampleEnd(NULL),
	_ShareMode(AUDCLNT_SHAREMODE_SHARED),
	InitializedMode(-1)
{
	//IMMDeviceEnumerator *deviceEnumerator = NULL;
	IPropertyStore *pDeviceProperties=NULL;

	HRESULT hr=S_OK;
	DebugPrintf(L"Create instance...\n", hr);
 	long input, output;
	_AsioUac = new AsioUAC2;
	_pDeviceFormat = new WAVEFORMATEX;
	_pDeviceFormat->cbSize =0;
	_AsioUac->getChannels(&input,&output);
	_pDeviceFormat->nChannels = output;
	_pDeviceFormat->wFormatTag = WAVE_FORMAT_PCM;
	_pDeviceFormat->wBitsPerSample = _AsioUac->getOutputSampleSize()*8;
	double rate;
	_AsioUac->getSampleRate(&rate);
	_pDeviceFormat->nAvgBytesPerSec = rate*_pDeviceFormat->wBitsPerSample;
	_pDeviceFormat->nSamplesPerSec = rate;
	_pDeviceFormat->nBlockAlign = _pDeviceFormat->wBitsPerSample / 8 * _pDeviceFormat->nChannels;

    //  Create our shutdown and samples ready events- we want auto reset events that start in the not-signaled state.
    _ShutdownEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if (_ShutdownEvent == NULL)
    {
        DebugPrintf(L"CUacRenderer - Unable to create shutdown event: %d.\n", GetLastError());
        goto exit;
    }

    _AudioBufferReadyEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if (_AudioBufferReadyEvent == NULL)
    {
        DebugPrintf(L"CUacRenderer - Unable to create samples ready event: %d.\n", GetLastError());
        goto exit;
    }

	_ProcessSamplesInQueueEvent = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if (_ProcessSamplesInQueueEvent == NULL)
    {
        DebugPrintf(L"CUacRenderer - Unable to create samples ready event: %d.\n", GetLastError());
        goto exit;
    }

	//  Create our shutdown and samples ready events- we want auto reset events that start in the not-signaled state.
    _ExitFeederLoopEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    if (_ExitFeederLoopEvent == NULL)
    {
        DebugPrintf(L"CUacRenderer - Unable to create _ExitFeederLoopEvent event: %d.\n", GetLastError());
        //goto exit;
    }
exit:
	return;
}

CUacRenderer::~CUacRenderer(void) 
{
	Stop();
	ClearQueue();
    if (_ShutdownEvent)
    {
        CloseHandle(_ShutdownEvent);
        _ShutdownEvent = NULL;
    }
    if (_AudioBufferReadyEvent)
    {
        CloseHandle(_AudioBufferReadyEvent);
        _AudioBufferReadyEvent = NULL;
    }
	if (_ProcessSamplesInQueueEvent)
    {
        CloseHandle(_ProcessSamplesInQueueEvent);
        _ProcessSamplesInQueueEvent = NULL;
    }
	if (_ExitFeederLoopEvent)
    {
        CloseHandle(_ExitFeederLoopEvent);
        _ExitFeederLoopEvent = NULL;
    }
	if(_pDeviceFormat)
	{
		delete(_pDeviceFormat);
		_pDeviceFormat=NULL;
	}
	
}


WAVEFORMATEX* CUacRenderer::GetUacMixFormat()
{
	return _pDeviceFormat;
}


HRESULT CUacRenderer::GetDeviceId(LPWSTR *ppstrId)
{
	return (HRESULT)this;
}


//IsFormatSupported in exclusive mode queries the audio driver. Exact implementation may vary between drivers.
//Some prefer WaveFormatEx, some prefer WaveFormatExtensible.
bool CUacRenderer::CheckFormatExclusive( WAVEFORMATEX* requestedFormat, WAVEFORMATEX** ppSuggestedFormat)
{
	WAVEFORMATEX* pSuggested=NULL;

	bool retValue=false;

	int sizeRequestFormat=sizeof(WAVEFORMATEX) + requestedFormat->cbSize;
	WAVEFORMATEX* formatEx = NULL;

	formatEx = (WAVEFORMATEX*) CoTaskMemAlloc(sizeRequestFormat);;
	CopyMemory(formatEx, requestedFormat, sizeRequestFormat);

	//Adjust sampleformat (bitspersample) to same as deviceformat and check format again
	
	if (ASE_OK == _AsioUac->canSampleRate(requestedFormat->nSamplesPerSec))
	{
		formatEx->wFormatTag = _pDeviceFormat->wFormatTag;
		if (formatEx->nChannels > _pDeviceFormat->nChannels)
		{
			formatEx->nChannels = _pDeviceFormat->nChannels;
		}
		formatEx->wBitsPerSample = _pDeviceFormat->wBitsPerSample;
		formatEx->nSamplesPerSec = requestedFormat->nSamplesPerSec;
	}
	else
	{
		formatEx->wBitsPerSample=_pDeviceFormat->wBitsPerSample;
		formatEx->wFormatTag = _pDeviceFormat->wFormatTag;
		formatEx->nChannels=_pDeviceFormat->nChannels;
		formatEx->nSamplesPerSec = _pDeviceFormat->nSamplesPerSec;;
	}

	DWORD bytesPerSample = formatEx->wBitsPerSample / 8;
	formatEx->nAvgBytesPerSec = (DWORD)(formatEx->nSamplesPerSec * formatEx->nChannels * bytesPerSample);
	formatEx->nBlockAlign = formatEx->nChannels * bytesPerSample;
	
	//Adjust number of channels to same as deviceformat and check format again
	pSuggested = formatEx;
	CopyMemory(_pDeviceFormat, formatEx, sizeRequestFormat);
exit:
	if(formatEx!=pSuggested)
		CoTaskMemFree(formatEx);

	if(ppSuggestedFormat)
		*ppSuggestedFormat=pSuggested;

	return retValue;
}

//Returns true if format can be used in specified mode.
//Returns a suggested alternative format if requested format can not be used. The caller must release suggested format if it is set (non NULL).
bool CUacRenderer::CheckFormat(WAVEFORMATEX* requestedFormat, WAVEFORMATEX** ppSuggestedFormat, AUDCLNT_SHAREMODE shareMode)
{
	bool retValue=false;
	retValue=CheckFormatExclusive(requestedFormat,ppSuggestedFormat);
	return retValue;
}

void CUacRenderer::SetIsProcessing(bool isOK)
{

	CAutoLock lock(&m_QueueLock);
	m_bIsProcessing=isOK;
	UpdateProcessSamplesInQueueEvent();
	if(isOK)
		ResetEvent(_ExitFeederLoopEvent);
	else
		SetEvent(_ExitFeederLoopEvent);
}


//  Start rendering - Create and start the render thread 
//  Returns true and does nothing if already running
bool CUacRenderer::Start(UINT32 EngineLatency)
{
    HRESULT hr;
	if(_RenderThread)		//Already running
	{
		DebugPrintf(L"CUacRenderer::Start - Already running.\n");
		return true;
	}

	DebugPrintf(L"CUacRenderer::Start - Starting up.\n");
	//  Remember our configured latency in case we'll need it for a stream switch later.
    _EngineLatencyInMS = EngineLatency;

	//  Now create the thread which is going to drive the renderer.
    if (_ShutdownEvent)
    {
        ResetEvent(_ShutdownEvent);
    }
	

    _RenderThread = CreateThread(NULL, 0, UacRenderThread, this, 0, NULL);
	
    if (_RenderThread == NULL)
    {
        DebugPrintf(L"Unable to create transport thread: %x.", GetLastError());
        return false;
    }


	if (_AsioUac)
	{
		DWORD ret;
		_BufferSizePerPeriod = 128*2;
		buffer_info = new ASIOBufferInfo[_pDeviceFormat->nChannels];
		for (int i = 0; i < _pDeviceFormat->nChannels; i++)
		{
			buffer_info[i].channelNum = i;
			buffer_info[i].isInput = false;
		}
		ret = _AsioUac->createBuffers(buffer_info, _pDeviceFormat->nChannels, _BufferSizePerPeriod);
		ret = _AsioUac->setSampleRate(_pDeviceFormat->nSamplesPerSec);
		_AsioUac->start();
	}
    return true;
}

//
//  Stop the renderer.
//
void CUacRenderer::Stop()
{
    HRESULT hr;

    //  Tell the render thread to shut down, wait for the thread to complete then clean up all the stuff we 
    //  allocated in Start().
	if (_AsioUac)
	{
		_AsioUac->stop();
		if (buffer_info)
		{
			delete buffer_info; 
			buffer_info = NULL;
		}

	}

    if (_ShutdownEvent)
    {
        SetEvent(_ShutdownEvent);
    }

	if (_RenderThread)
    {
		DebugPrintf(L"CUacRenderer::Stop() - Stopping.\n");
        WaitForSingleObject(_RenderThread, INFINITE);
        CloseHandle(_RenderThread);
        _RenderThread = NULL;
    }
	else
	{
		DebugPrintf(L"CUacRenderer::Stop() - Already stopped.\n");
	}
}


void CUacRenderer::ClearQueue()
{
	DebugPrintf(L"CUacRenderer::ClearQueue()\n");
	CAutoLock lock(&m_QueueLock);
	while (_RenderBufferQueue != NULL)
    {
        RenderBuffer *node = _RenderBufferQueue;
		node->pSample->Release();
		node->pMediaType->Release();
        _RenderBufferQueue = node->_Next;
        delete node;
    }
	if(_pCurrentSample)
		_pCurrentSample->Release();
	if(_pCurrentMediaType)
		_pCurrentMediaType->Release();
	_pCurrentSample=NULL;
	_pCurrentMediaType=NULL;
	_CurrentSampleOffset=0;
	CurrentSampleStart=NULL;
	CurrentSampleEnd=NULL;
	UpdateProcessSamplesInQueueEvent();
}

void CUacRenderer::AddSampleToQueue(IMediaBufferEx *pSample, RefCountingWaveFormatEx *pMediaType, bool isExclusive)
{
	RenderBuffer *newNode = new RenderBuffer();
	newNode->pSample=pSample;
	newNode->pMediaType=pMediaType;
	newNode->ExclusiveMode=isExclusive;
	{
		CAutoLock lock(&m_QueueLock);
		if(_RenderBufferQueue==NULL)
		{
			_RenderBufferQueue=newNode;
			UpdateProcessSamplesInQueueEvent();
		}
		else
		{
			RenderBuffer *tailNode = _RenderBufferQueue;
			while(tailNode->_Next!=NULL)
				tailNode = tailNode->_Next;
			tailNode->_Next=newNode;
		}
	}
}

REFERENCE_TIME CUacRenderer::GetCurrentSampleTime()
{
	return CurrentSampleStart;
}

//Sets the event if we have samples (_RenderBufferQueue!=null && m_bIsProcessing)
//Caller should lock on QueueLock before calling
void CUacRenderer::UpdateProcessSamplesInQueueEvent()
{
	if(_RenderBufferQueue&&m_bIsProcessing)
		SetEvent(_ProcessSamplesInQueueEvent);
	else
		ResetEvent(_ProcessSamplesInQueueEvent);

}

//
//  Initialize Uac in event driven mode, associate the audio client with our samples ready event handle, and retrieve 
//  a render client for the transport.
//
bool CUacRenderer::InitializeAudioClient()
{
    REFERENCE_TIME bufferDuration = _EngineLatencyInMS*10000;
	REFERENCE_TIME period = bufferDuration;

	if(_ShareMode==AUDCLNT_SHAREMODE_SHARED)
		period=0;
	HRESULT hr = _AsioUac->SetEventHandle(_AudioBufferReadyEvent);
    return true;
}

//
//  Render thread - processes samples from the audio engine
DWORD CUacRenderer::UacRenderThread(LPVOID Context)
{
	CUacRenderer *renderer = static_cast<CUacRenderer *>(Context);
	HANDLE mmcssHandle = NULL;
    DWORD mmcssTaskIndex = 0;
	DWORD retValue=NULL;

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        DebugPrintf(L"Unable to initialize COM in render thread: %x\n", hr);
        return hr;
    }

    mmcssHandle = AvSetMmThreadCharacteristics(L"Audio", &mmcssTaskIndex);
    if (mmcssHandle == NULL)
    {
        DebugPrintf(L"Unable to enable MMCSS on render thread: %d\n", GetLastError());
    }

    retValue = renderer->DoRenderThread();

	AvRevertMmThreadCharacteristics(mmcssHandle);
    CoUninitialize();
	return retValue;

}


//Returns true if new mediatype is different from last mediatype
bool CUacRenderer::PopulateCurrentFromQueue()
{
	bool reportChange=false;
	RenderBuffer* pulledNode=NULL;
	{
		CAutoLock lock(&m_QueueLock);
		pulledNode=_RenderBufferQueue;
		if(_RenderBufferQueue!=NULL)
		{
			_RenderBufferQueue=_RenderBufferQueue->_Next;
		}
		if(!_RenderBufferQueue)		//Last sample was pulled
			UpdateProcessSamplesInQueueEvent();
	}

	if(_pCurrentMediaType)		//Current set has value. Needs to release before setting.
	{
		_pCurrentSample->Release();
		_pCurrentSample=NULL;
		_CurrentSampleOffset=0;
		if(!pulledNode)
		{
			//Changed from non-null to null, reportchange=false (buffer underrun, not reinit) (Reinit will occur next time we receive a sample)
			_pCurrentMediaType->Release();
			_pCurrentMediaType=NULL;
		}
		else	//Pulled node has value. Compare with current set before releasing currentmediatype.
		{
			reportChange = *(pulledNode->pMediaType) != *_pCurrentMediaType;		//Sjekker om innholdet i mediatypene er ulike.
			_pCurrentMediaType->Release();
			_pCurrentSample=pulledNode->pSample;
			_pCurrentMediaType=pulledNode->pMediaType;

		}
	}
	else								//No need for release
	{
		if(pulledNode)
		{
			reportChange=true;		//Changed from null to non-null
			_CurrentSampleOffset=0;
			_pCurrentSample=pulledNode->pSample;
			_pCurrentMediaType=pulledNode->pMediaType;
		}
	}
	
	if(_pCurrentSample)
	{
		CurrentSampleStart=_pCurrentSample->GetStartTime();
		CurrentSampleEnd=_pCurrentSample->GetEndTime();
	}

	if(pulledNode)
	{
		AUDCLNT_SHAREMODE newShareMode=pulledNode->ExclusiveMode ? AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
		if(newShareMode!=_ShareMode)		
		{
			//We can always switch from exclusive to shared. Trigger reinit by returning true.
			if(newShareMode==AUDCLNT_SHAREMODE_SHARED)
			{
				_ShareMode=newShareMode;
				reportChange=true;
				//_hasTriedExclusiveModeWithCurrentMediaType=false;
			}
			else //if(!_hasTriedExclusiveModeWithCurrentMediaType)
			//Check if we can switch to Exclusive Mode, if not we just want to continue in shared mode.
			{
				WAVEFORMATEX* formatNew=_pCurrentMediaType->GetFormat();
				WAVEFORMATEX* suggestedFormat=NULL;
				bool isOK=CheckFormat(formatNew,&suggestedFormat,newShareMode);
				if(isOK)
				{
					_ShareMode=newShareMode;
					reportChange=true;
				}
				if(suggestedFormat)
					CoTaskMemFree(suggestedFormat);
				//_hasTriedExclusiveModeWithCurrentMediaType=true;
			}
		}
		delete pulledNode;
	}
	return reportChange;
}


//  Returnerer ERROR_UNSUPPORTED_TYPE hvis den møter p?en ny mediatype. (_pCurrentSample er ny(ubrukt) sample, _pCurrentMediaType er ny mediatype)
//  Returner E_ABORT ved shutdown. 
//  Returnerer S_FALSE ved buffer underrun.
int ctr=0;
HRESULT CUacRenderer::FeedRenderer()
{
    HRESULT hrReturn=S_OK;

	while (hrReturn==S_OK)
    {
		HANDLE waitArray[2] = {_ExitFeederLoopEvent, _AudioBufferReadyEvent};
        DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
        switch (waitResult)
        {
			case WAIT_OBJECT_0 + 0:     // _ExitFeederLoopEvent
				hrReturn=E_ABORT;
				break;
			case WAIT_OBJECT_0 + 1:     // _AudioBufferReadyEvent
				//  We need to provide the next buffer of samples to the audio renderer.
				ctr++;
				BYTE *pSampleData;
				UINT32 numFramesPadding=0;
			
				UINT32 renderBufferSizeInBytes = _BufferSizePerPeriod*_FrameSize;
				int bytesCopied=0;
				int bytesToCopy=renderBufferSizeInBytes;
				bool mediaTypeChanged=false;
				INT32 *		pData0 = _AsioUac->GetToggle() ? (int *)buffer_info[0].buffers[1] : (int *)buffer_info[0].buffers[0];
				INT32 *		pData1 = _AsioUac->GetToggle() ? (int *)buffer_info[1].buffers[1] : (int *)buffer_info[1].buffers[0];
				while (_pCurrentSample != NULL && bytesCopied < bytesToCopy  && !mediaTypeChanged)
				{		
					DWORD sampleSize=0;
					_pCurrentSample->GetBufferAndLength(&pSampleData,&sampleSize);

					long offsetEnd=_CurrentSampleOffset + bytesToCopy-bytesCopied;
					if(offsetEnd>sampleSize)
						offsetEnd=sampleSize;
					INT32* pin = (INT32*)(pSampleData + _CurrentSampleOffset);
					//Copy fra sample+offset til sample+offsetend to pData;
					//CopyMemory(pData+bytesCopied, pSampleData+_CurrentSampleOffset, offsetEnd-_CurrentSampleOffset);
					for (int i = 0; i < (offsetEnd - _CurrentSampleOffset)/4; pData0++, pData1++,i++)
					{
						*pData0 = *pin++;
						*pData1 = *pin++;
					}

					//DebugPrintf(L"ctr:%d. Bytes to copy: %d, Copied %d bytes from current sample offset %d to buffer location %d.\n",ctr,bytesToCopy,offsetEnd-_CurrentSampleOffset,_CurrentSampleOffset,bytesCopied);
					bytesCopied +=offsetEnd-_CurrentSampleOffset;
					if(offsetEnd==sampleSize)
					{
						mediaTypeChanged=PopulateCurrentFromQueue();
						break;
					}
					else
					{
						_CurrentSampleOffset+=(offsetEnd-_CurrentSampleOffset);
					}
				}
				//TODO:? Hvis bytescopied<bytestocopy, zeromemory resten. 

				if(mediaTypeChanged)
					hrReturn=ERROR_UNSUPPORTED_TYPE;
				else if(bytesCopied<bytesToCopy)		//No change, but still not managed to fill buffer=underrun
					hrReturn=S_FALSE;
				break;
        }//Switch
    }//While

    return hrReturn;
}

DWORD CUacRenderer::DoRenderThread()
{
    bool stillPlaying = true;
    HANDLE waitArray[2] = {_ShutdownEvent, _ProcessSamplesInQueueEvent};
	bool isInited=false;
	while(stillPlaying)
	{
        HRESULT hr;
			//Vent p?samples (eller shutdown)
		DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
		switch (waitResult)
		{
			case WAIT_OBJECT_0 + 0:					// _ShutdownEvent
				stillPlaying = false;				// We're done, exit the loop.
				break;
			case WAIT_OBJECT_0 + 1:					// _ProcessSamplesInQueueEvent
				break;			
		}

		if(stillPlaying)
		{
			if(_pCurrentSample && _pCurrentMediaType)
			{
				DebugPrintf(L"CUacRenderer::DoRenderThread has samples. Entering feederloop. IsInited: %d\n",isInited);
				if(!isInited)
				{
					InitializeAudioClient();
					_FrameSize = _pCurrentMediaType->GetFormat()->nBlockAlign;
					isInited = true;
					DebugPrintf(L"CUacRenderer::DoRenderThread -  Inited audioengine. Result:%d \n",isInited);


				}
				if(!isInited)
				{
					DebugPrintf(L"CUacRenderer::DoRenderThread -  Init audioengine failed. Releasing current sample. \n",isInited);
					if(_pCurrentSample)
						_pCurrentSample->Release();
					if(_pCurrentMediaType)
						_pCurrentMediaType->Release();
					_pCurrentSample=NULL;
					_pCurrentMediaType=NULL;
					_CurrentSampleOffset=0;
					CurrentSampleStart=NULL;
					CurrentSampleEnd=NULL;
					_AsioUac->stop();
				}
				else
				{	//IsInited
					hr=FeedRenderer();
					switch(hr)
					{
						case ERROR_UNSUPPORTED_TYPE:	//ny mediatype. (_pCurrentSample er ny(ubrukt) sample, _pCurrentMediaType er ny mediatype)
							DebugPrintf(L"CUacRenderer::FeedRenderer returned unsupported type - Releasing AudioEngine. \n");
							isInited = FALSE;
							break;
						case S_FALSE:
							DebugPrintf(L"CUacRenderer::FeedRenderer returned S_FALSE (buffer underrun)\n");
							CurrentSampleStart=NULL;
							break;
						case E_ABORT:
							DebugPrintf(L"CUacRenderer::FeedRenderer returned E_ABORT (PauseState)-  Stops feederloop. \n");
							break;
						default:
							DebugPrintf(L"CUacRenderer::FeedRenderer returned %d - Releasing AudioEngine.  \n",hr);
							isInited = FALSE;
							break;
					}
				}
			}
			else	//_pCurrentSample==null || _pCurrentMediaType==null
			{
				bool mediaTypeChanged=PopulateCurrentFromQueue();
				if(mediaTypeChanged)
					isInited = FALSE;
			}
		}
	}
	isInited = FALSE;
    return 0;
}




