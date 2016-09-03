/*!
#
# Win-Widget. Windows related software for Audio-Widget/SDR-Widget (http://code.google.com/p/sdr-widget/)
# Copyright (C) 2012 Nikolay Kovbasa
#
# Permission to copy, use, modify, sell and distribute this software 
# is granted provided this copyright notice appears in all copies. 
# This software is provided "as is" without express or implied
# warranty, and with no claim as to its suitability for any purpose.
#
#----------------------------------------------------------------------------
# Contact: nikkov@gmail.com
#----------------------------------------------------------------------------
*/
// Based on samples from ASIO SDK
/*
	Steinberg Audio Stream I/O API
	(c) 1996, Steinberg Soft- und Hardware GmbH
	charlie (May 1996)
*/

#include "DeviceRender.h"


#define DEFAULT_BLOCK_SIZE 512
#define _ENABLE_TRACE
//------------------------------------------------------------------------------------------
// extern
const double twoRaisedTo32 = 4294967296.;
void getNanoSeconds(ASIOTimeStamp* ts)
{
	double nanoSeconds = (double)((unsigned long)timeGetTime()) * 1000000.;
	ts->hi = (unsigned long)(nanoSeconds / twoRaisedTo32);
	ts->lo = (unsigned long)(nanoSeconds - (ts->hi * twoRaisedTo32));
}
// local

double AsioUAC2s2double (ASIOSamples* samples);


static const double twoRaisedTo32Reciprocal = 1. / twoRaisedTo32;

//---------------------------------------------------------------------------------------------
double AsioUAC2s2double (ASIOSamples* samples)
{
	double a = (double)(samples->lo);
	if (samples->hi)
		a += (double)(samples->hi) * twoRaisedTo32;
	return a;
}

//------------------------------------------------------------------------------------------
// on windows, we do the COM stuff.


// when not on windows, we derive from AsioDriver
AsioUAC2::AsioUAC2 () : inputBuffers(NULL), outputBuffers(NULL), 
	inMap(NULL), outMap(NULL), m_device(NULL), m_AsioSyncEvent(NULL), m_BufferSwitchEvent(NULL), m_StopInProgress(false)


{
	blockFrames = DEFAULT_BLOCK_SIZE;
	inputLatency = blockFrames;		// typically
	outputLatency = blockFrames * 2;
	
	// typically blockFrames * 2; try to get 1 by offering direct buffer
	// access, and using asioPostOutput for lower latency
	samplePosition = 0;
	sampleRate = 44100;
	milliSeconds = (long)((double)(blockFrames * 1000) / sampleRate);
	active = false;
	started = false;
	timeInfoMode = false;
	tcRead = false;

	m_NumInputs = m_NumOutputs = 0;
	activeInputs = activeOutputs = 0;
	m_inputSampleSize = m_outputSampleSize = 0;

	toggle = 0;
	//read position in buffer
	currentOutBufferPosition = 0;
	currentInBufferPosition = 0;
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: AsioUAC2::AsioUAC2()\n");
#endif
	init(NULL);

}

//------------------------------------------------------------------------------------------
AsioUAC2::~AsioUAC2 ()
{
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: AsioUAC2::~AsioUAC2() begin\n");
#endif
	stop ();
	outputClose ();
	inputClose ();
	disposeBuffers ();
	delete m_device;

	if(m_AsioSyncEvent)
		CloseHandle(m_AsioSyncEvent);
	if(m_BufferSwitchEvent)
		CloseHandle(m_BufferSwitchEvent);
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: AsioUAC2::~AsioUAC2()\n");
#endif
}

//------------------------------------------------------------------------------------------
bool AsioUAC2::init (void* sysRef)
{
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: AsioUAC2::init...\n");
#endif
	m_device = USBAudioDevice::GetAudioInstance();
	m_device->InitDevice();

	if (inputOpen ())
	{
		if (outputOpen ())
		{
			active = true;
			return true;
		}
		else
		{
#ifdef _ENABLE_TRACE
			DebugPrintf(L"ASIOUAC: Output open error!!\n");
#endif
		}
	}
	else
	{
#ifdef _ENABLE_TRACE
		DebugPrintf(L"ASIOUAC: Input open error!!\n");
#endif
	}

	if(m_device)
		m_device->Stop();

	delete m_device;
	m_device = NULL;
	
	outputClose ();
	inputClose ();
	return false;
}

//------------------------------------------------------------------------------------------
int AsioUAC2::start ()
{
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: Try start device...\n");
#endif
	if(started)
	{
#ifdef _ENABLE_TRACE
		DebugPrintf(L"ASIOUAC: Can't start device: already started!\n");
#endif
		return ASE_OK; //ASE_InvalidMode;
	}
	if(!m_device)
	{
#ifdef _ENABLE_TRACE
		DebugPrintf(L"ASIOUAC: Can't start device: m_device==NULL!\n");
#endif
		return ASE_HWMalfunction;
	}

	//if (callbacks)
	{
		started = false;
		samplePosition = 0;
		theSystemTime.lo = theSystemTime.hi = 0;
		toggle = 0;
		currentOutBufferPosition = 0;
		currentInBufferPosition = 0;

#ifdef EMULATION_HARDWARE
		timerOn ();		
		started = true;
		return ASE_OK;
#else
		// activate hardware
		m_StopInProgress = false;
#ifdef _ENABLE_TRACE
		DebugPrintf(L"ASIOUAC: Exit flag cleared\n");
#endif
		int retVal = m_device->Start() ? ASE_OK : ASE_HWMalfunction;
		if(retVal == ASE_OK)
		{
#ifdef _ENABLE_TRACE
			DebugPrintf(L"ASIOUAC: Device started successfully!\n");
#endif
			m_device->SetNotifyCallback(AsioUAC2::sDeviceNotify, this);
			started = true;
		}
		return retVal;
#endif
	}
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: Can't start device: callbacks is NULL!\n");
#endif
	return ASE_NotPresent;
}

//------------------------------------------------------------------------------------------
int AsioUAC2::stop ()
{
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: Try stop device...\n");
#endif
	if(!started)
	{
#ifdef _ENABLE_TRACE
		DebugPrintf(L"ASIOUAC: Can't stop device: already stoped!\n");
#endif
		return ASE_OK;//ASE_InvalidMode;
	}
	if(!m_device)
	{
#ifdef _ENABLE_TRACE
		DebugPrintf(L"ASIOUAC: Can't stop device: m_device==NULL!\n");
#endif
		return ASE_HWMalfunction;
	}

#ifdef EMULATION_HARDWARE
	timerOff ();		
	started = false;
	return ASE_OK;
#else
	// de-activate hardware
	m_StopInProgress = true;
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: Exit flag is set\n");
#endif
	if(m_AsioSyncEvent)
		SetEvent(m_AsioSyncEvent);
	if(m_BufferSwitchEvent)
		SetEvent(m_BufferSwitchEvent);

	int retVal = m_device->Stop() ? ASE_OK : ASE_HWMalfunction;
	
	if(retVal == ASE_OK)
	{
		m_device->SetNotifyCallback(NULL, this);
#ifdef _ENABLE_TRACE
		DebugPrintf(L"ASIOUAC: Device stoped successfully!\n");
#endif
		started = false;
	}
	return retVal;
#endif
}

//------------------------------------------------------------------------------------------
int AsioUAC2::getChannels (long *numInputChannels, long *numOutputChannels)
{
	*numInputChannels = m_NumInputs;
	*numOutputChannels = m_NumOutputs;
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: getChannels request. Number inputs = %d, number outputs = %d\n", m_NumInputs, m_NumOutputs);
#endif
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
int AsioUAC2::getLatencies (long *_inputLatency, long *_outputLatency)
{
	*_inputLatency = inputLatency;
	*_outputLatency = outputLatency;
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: getLatencies request. Input Latency = %d, Output Latency = %d\n", inputLatency, outputLatency);
#endif
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
int AsioUAC2::getBufferSize (long *minSize, long *maxSize,
	long *preferredSize, long *granularity)
{
	*minSize = 128;
	*maxSize = *preferredSize = blockFrames;		// allow this size only
	*granularity = 0;
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: getBufferSize request. MaxSize=maxSize=preferredSize=%d\n", blockFrames);
#endif
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
int AsioUAC2::canSampleRate (double sampleRate)
{
	if(!m_device)
		return ASE_HWMalfunction;
	int iSampleRate = (int)sampleRate;
	if (iSampleRate == 44100 || iSampleRate == 48000)
	{
#ifdef _ENABLE_TRACE
		DebugPrintf(L"ASIOUAC: canSampleRate request for samplerate %d OK\n", iSampleRate);
#endif
		return ASE_OK;
	}
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: canSampleRate request for samplerate %d error!\n", iSampleRate);
#endif
	return ASE_NoClock;
}

//------------------------------------------------------------------------------------------
int AsioUAC2::getSampleRate(double *sampleRate)
{
	*sampleRate = this->sampleRate;
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: getSampleRate request. Current samplerate %d\n", (int)*sampleRate);
#endif
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
int AsioUAC2::setSampleRate(double sampleRate)
{
	if(!m_device)
		return ASE_HWMalfunction;
	int iSampleRate = (int)sampleRate;
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: Try change samplerate to %d\n", iSampleRate);
#endif

	//if (sampleRate != this->sampleRate)
	{
		if(m_device->SetSampleRate(iSampleRate))
		{
			this->sampleRate = sampleRate;
			asioTime.timeInfo.sampleRate = sampleRate;
			asioTime.timeInfo.flags |= kSampleRateChanged;
			milliSeconds = (long)((double)(blockFrames * 1000) / this->sampleRate);
//			if (callbacks && callbacks->sampleRateDidChange)
//				callbacks->sampleRateDidChange (this->sampleRate);

#ifdef _ENABLE_TRACE
			DebugPrintf(L"ASIOUAC: Samplerate changed to %d\n", (int)this->sampleRate);
#endif
		}
		else
		{
#ifdef _ENABLE_TRACE
			DebugPrintf(L"ASIOUAC: Samplerate not changed to %d\n", iSampleRate);
#endif
			return ASE_NoClock;
		}

	}
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
int AsioUAC2::getClockSources (ASIOClockSource *clocks, long *numSources)
{
	// internal
	clocks->index = 0;
	clocks->associatedChannel = -1;
	clocks->associatedGroup = -1;
	clocks->isCurrentSource = ASIOTrue;
	strcpy(clocks->name, "Internal");
	*numSources = 1;
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: Get clock source req\n");
#endif
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
int AsioUAC2::setClockSource (long index)
{
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: Set clock source by index %d\n", (int)index);
#endif
	if (!index)
	{
		asioTime.timeInfo.flags |= kClockSourceChanged;
		return ASE_OK;
	}
	return ASE_NotPresent;
}

//------------------------------------------------------------------------------------------
int AsioUAC2::getSamplePosition (ASIOSamples *sPos, ASIOTimeStamp *tStamp)
{
	tStamp->lo = theSystemTime.lo;
	tStamp->hi = theSystemTime.hi;
	if (samplePosition >= twoRaisedTo32)
	{
		sPos->hi = (unsigned long)(samplePosition * twoRaisedTo32Reciprocal);
		sPos->lo = (unsigned long)(samplePosition - (sPos->hi * twoRaisedTo32));
	}
	else
	{
		sPos->hi = 0;
		sPos->lo = (unsigned long)samplePosition;
	}

#ifdef _ENABLE_TRACE
	//DebugPrintf(L"ASIOUAC: Retrieve sample position %d", (int)samplePosition);
#endif
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
int AsioUAC2::getChannelInfo (ASIOChannelInfo *info)
{
	if(!m_device)
		return ASE_HWMalfunction;

	if (info->channel < 0 || (info->isInput ? 
								info->channel >= m_NumInputs 
									: info->channel >= m_NumOutputs))
		return ASE_InvalidParameter;

	int slotSize = info->isInput ? 	m_inputSampleSize : m_outputSampleSize;
	int bitResolution = info->isInput ? m_device->GetADCBitResolution() : m_device->GetDACBitResolution();
	switch(slotSize)
	{
		case 2:
		{
			//TODO: check bit resolution
			info->type = ASIOSTInt16LSB;
		}
			break;
		case 3:
			{
				//TODO: check bit resolution
				info->type = ASIOSTInt24LSB;
			}
			break;
		default:
			{
				//TODO: check bit resolution
				info->type = ASIOSTInt32LSB;
			}
	}
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: getChannelInfo request. Channel %d, type %d, slot size %d\n", info->channel, info->type, slotSize);
#endif

	info->channelGroup = 0;
	info->isActive = ASIOFalse;

	long i;
	if (info->isInput)
	{
		for (i = 0; i < activeInputs; i++)
		{
			if (inMap[i] == info->channel)
			{
				info->isActive = ASIOTrue;
				break;
			}
		}
	}
	else
	{
		for (i = 0; i < activeOutputs; i++)
		{
			if (outMap[i] == info->channel)
			{
				info->isActive = ASIOTrue;
				break;
			}
		}
	}
	sprintf(info->name, "UAC2 %d", info->channel);
	return ASE_OK;
}

//------------------------------------------------------------------------------------------
int AsioUAC2::createBuffers (ASIOBufferInfo *bufferInfos, long numChannels,	long bufferSize)
{
	ASIOBufferInfo *info = bufferInfos;
	long i;
	bool notEnoughMem = false;

	activeInputs = 0;
	activeOutputs = 0;
	blockFrames = bufferSize;

	if (started)
	{
		return ASE_OK;
	}
	for (i = 0; i < numChannels; i++, info++)
	{
		if (info->isInput)
		{
			if (info->channelNum < 0 || info->channelNum >= m_NumInputs)
				goto error;
			inMap[activeInputs] = info->channelNum;
			inputBuffers[activeInputs] = new char[m_inputSampleSize * blockFrames * 2];	// double buffer
			memset(inputBuffers[activeInputs], 0, blockFrames * 2 * m_inputSampleSize);
#ifdef _ENABLE_TRACE
			DebugPrintf(L"ASIOUAC: Buffer for input channel %d allocated with size %d\n", activeInputs, (int)blockFrames * 2 * m_inputSampleSize);
#endif
			if (inputBuffers[activeInputs])
			{
				info->buffers[0] = inputBuffers[activeInputs];
				info->buffers[1] = inputBuffers[activeInputs] + m_inputSampleSize * blockFrames;
			}
			else
			{
				info->buffers[0] = info->buffers[1] = 0;
				notEnoughMem = true;
			}
			activeInputs++;
			if (activeInputs > m_NumInputs)
				goto error;
		}
		else	// output			
		{
			if (info->channelNum < 0 || info->channelNum >= m_NumOutputs)
				goto error;
			outMap[activeOutputs] = info->channelNum;
			outputBuffers[activeOutputs] = new char[m_outputSampleSize * blockFrames * 2];	// double buffer
			memset(outputBuffers[activeOutputs], 0, blockFrames * 2 * m_outputSampleSize);
#ifdef _ENABLE_TRACE
			DebugPrintf(L"ASIOUAC: Buffer for output channel %d allocated with size %d\n", activeOutputs, (long)blockFrames * 2 * m_outputSampleSize);
#endif
			if (outputBuffers[activeOutputs])
			{
				info->buffers[0] = outputBuffers[activeOutputs];
				info->buffers[1] = outputBuffers[activeOutputs] + m_outputSampleSize * blockFrames;
			}
			else
			{
				info->buffers[0] = info->buffers[1] = 0;
				notEnoughMem = true;
			}
			activeOutputs++;
			if (activeOutputs > m_NumOutputs)
			{
				activeOutputs--;
				disposeBuffers();
				return ASE_InvalidParameter;
			}
		}
	}		
	if (notEnoughMem)
	{
		disposeBuffers();
		return ASE_NoMemory;
	}

	timeInfoMode = true;
#ifdef _ENABLE_TRACE
		DebugPrintf(L"ASIOUAC: timeInfoMode = true\n");
#endif
		asioTime.timeInfo.speed = 1.;
		asioTime.timeInfo.systemTime.hi = asioTime.timeInfo.systemTime.lo = 0;
		asioTime.timeInfo.samplePosition.hi = asioTime.timeInfo.samplePosition.lo = 0;
		asioTime.timeInfo.sampleRate = sampleRate;
		asioTime.timeInfo.flags = kSystemTimeValid | kSamplePositionValid | kSampleRateValid;

		asioTime.timeCode.speed = 1.;
		asioTime.timeCode.timeCodeSamples.lo = asioTime.timeCode.timeCodeSamples.hi = 0;
		asioTime.timeCode.flags = kTcValid | kTcRunning ;


#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: Create buffers with length %d OK\n", blockFrames);
#endif

	m_BufferSwitchEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	return ASE_OK;

error:
	disposeBuffers();
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: Create buffers with length %d failed!\n", blockFrames);
#endif
	return ASE_InvalidParameter;
}

//---------------------------------------------------------------------------------------------
int AsioUAC2::disposeBuffers()
{
	long i;
	
#ifdef _ENABLE_TRACE
	DebugPrintf(L"ASIOUAC: AsioUAC2::disposeBuffers() begin\n");
#endif
	stop();
#ifndef NO_INPUTS
	if (inputBuffers != NULL)
	{
		for (i = 0; i < activeInputs; i++)
			delete inputBuffers[i];
	}

	activeInputs = 0;
#endif
	if (outputBuffers != NULL)
	{
		for (i = 0; i < activeOutputs; i++)
			delete outputBuffers[i];
	}

	activeOutputs = 0;

	if(m_BufferSwitchEvent)
		CloseHandle(m_BufferSwitchEvent);
	m_BufferSwitchEvent = NULL;
	
	return ASE_OK;
}

//--------------------------------------------------------------------------------------------------------
// private methods
//--------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------
// input
//--------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------
bool AsioUAC2::inputOpen ()
{
	if(!m_device)
		return false;

	m_inputSampleSize = m_device->GetADCSubslotSize();

	m_NumInputs = m_device->GetInputChannelNumber();
	if(inputBuffers)
		delete inputBuffers;
	inputBuffers = new char*[m_NumInputs * 2];
	if(inMap)
		delete inMap;
	inMap = new long[m_NumInputs];
	for (int i = 0; i < m_NumInputs; i++)
	{
		inputBuffers[i*2] = NULL;
		inputBuffers[i * 2 +1] = NULL;
		inMap[i] = 0;
	}
	m_InputsChPrt = 0;

	if(m_inputSampleSize == 4)
		m_device->SetADCCallback((FillDataCallback)AsioUAC2::sFillInputData4, (void*)this);
	else
		if(m_inputSampleSize == 3)
			m_device->SetADCCallback((FillDataCallback)AsioUAC2::sFillInputData3, (void*)this);

	m_AsioSyncEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	return true;
}

//---------------------------------------------------------------------------------------------
void AsioUAC2::inputClose ()
{
	if(inMap)
		delete inMap;
	if(inputBuffers)
		delete inputBuffers;
	if(m_AsioSyncEvent)
		CloseHandle(m_AsioSyncEvent);
	m_NumInputs = 0;
	inMap = NULL;
	inputBuffers = NULL;
	m_AsioSyncEvent = NULL;
}

//---------------------------------------------------------------------------------------------
void AsioUAC2::input()
{
}

//------------------------------------------------------------------------------------------------------------------
// output
//------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------
bool AsioUAC2::outputOpen()
{
	if(!m_device)
		return false;

	m_outputSampleSize = m_device->GetDACSubslotSize();

	m_NumOutputs = m_device->GetOutputChannelNumber();
	if(outputBuffers)
		delete outputBuffers;
	outputBuffers = new char*[m_NumOutputs * 2];
	if(outMap)
		delete outMap;
	outMap = new long[m_NumOutputs];
	for (int i = 0; i < m_NumOutputs; i++)
	{
		outputBuffers[i*2] = NULL;
		outputBuffers[i * 2+1] = NULL;
		outMap[i] = 0;
	}

	m_OutputsChPrt = 0;
	
#ifdef _ENABLE_TRACE
	DebugPrintf(L"output open mdevice :%d!\n",m_device);
#endif

	if(m_outputSampleSize == 4)
		m_device->SetDACCallback(AsioUAC2::sFillOutputData4, (void*)this);
	else
		if(m_outputSampleSize == 3)
			m_device->SetDACCallback(AsioUAC2::sFillOutputData3, (void*)this);
	return true;
}

//---------------------------------------------------------------------------------------------
void AsioUAC2::outputClose ()
{
	if(outMap)
		delete outMap;
	if(outputBuffers)
		delete outputBuffers;
	m_NumOutputs = 0;
	outMap = NULL;
	outputBuffers = NULL;
}

//---------------------------------------------------------------------------------------------
void AsioUAC2::output ()
{
}


struct ThreeByteSample
{
	UCHAR sample[3];
	ThreeByteSample(int val = 0)
	{
		UCHAR *ptrVal = (UCHAR *)&val;
		sample[0] = *(ptrVal);
		sample[1] = *(ptrVal+1);
		sample[2] = *(ptrVal+2);
	}
};

#define FourByteSample	int
#define AudioSample3 ThreeByteSample
#define  AudioSample4 FourByteSample


template <typename T_SRC, typename T_DST> void AsioUAC2::FillOutputData(UCHAR *buffer, int& len)
{
	if (activeOutputs == 0 || m_StopInProgress)
	{
#ifdef _ENABLE_TRACE
		DebugPrintf(L"ASIOUAC: Detected exit flag in output thread!\n");
#endif
		len = 0;
		return;
	}
	T_DST *sampleBuff = (T_DST *)buffer;
	int sampleLength = len / sizeof(T_DST);
#ifdef _ENABLE_TRACE
	//DebugPrintf(L"ASIOUAC: Fill output data with length %d, currentBufferPosition %d", sampleLength, currentOutBufferPosition);
#endif

	for (int i = 0; i < sampleLength; i += m_NumOutputs, sampleBuff += m_NumOutputs)
	{

		for (int j = 0; j < activeOutputs; j++)
		{
			int ch = outMap[j];
			T_SRC *hostBuffer0 = toggle ? ((T_SRC*)outputBuffers[j]) + blockFrames : (T_SRC*)outputBuffers[j];
			sampleBuff[ch] = hostBuffer0[currentOutBufferPosition];
		}
		

		currentOutBufferPosition ++;

		if(currentOutBufferPosition == blockFrames)
		{
			currentOutBufferPosition = 0;
			//need syncro with intput buffer
			if(activeInputs && m_AsioSyncEvent)
			{
				if(WaitForSingleObject(m_AsioSyncEvent, 100) == WAIT_TIMEOUT)
				{
#ifdef _ENABLE_TRACE
					DebugPrintf(L"ASIOUAC: Waiting input buffer error!\n");
#endif
					break;
				}
			}
			if(m_StopInProgress)
			{
#ifdef _ENABLE_TRACE
				DebugPrintf(L"ASIOUAC: Detected exit flag in output thread!\n");
#endif
				return;
			}
			if (m_AudioBufferReadyEvent)
			{
				toggle = !toggle;
				SetEvent(m_AudioBufferReadyEvent);
			}
		}
	}
}

template <typename T_SRC, typename T_DST> void AsioUAC2::FillInputData(UCHAR *buffer, int& len)
{
	if(activeInputs == 0 || m_StopInProgress)
	{
#ifdef _ENABLE_TRACE
		//DebugPrintf(L"ASIOUAC: Detected exit disposed flag in input thread!\n");
#endif
		len = 0;
		return;
	}
	T_SRC *sampleBuff = (T_SRC *)buffer;
	int sampleLength = len / sizeof(T_SRC);
#ifdef _ENABLE_TRACE
	//DebugPrintf(L"ASIOUAC: Fill input data with length %d, currentBufferPosition %d", sampleLength, currentInBufferPosition);
#endif

	for (int i = 0; i < sampleLength; i += m_NumInputs, sampleBuff += m_NumInputs)
	{
		for (int j = 0; j < activeInputs; j++)
		{
			int ch = inMap[j];
			T_DST *hostBuffer0 = toggle ? ((T_DST*)inputBuffers[j]) + blockFrames : (T_DST*)inputBuffers[j];
			hostBuffer0[currentInBufferPosition] = sampleBuff[ch];
		}
		currentInBufferPosition++;
		
		if(currentInBufferPosition == blockFrames)
		{
				currentInBufferPosition = 0;
				//need syncro with output buffer
				if(activeOutputs)
				{
					//inform output thread
					SetEvent(m_AsioSyncEvent);
					//waiting switch
					if(WaitForSingleObject(m_BufferSwitchEvent, 100) == WAIT_TIMEOUT)
					{
	#ifdef _ENABLE_TRACE
						DebugPrintf(L"ASIOUAC: Waiting buffer switch error!\n");
	#endif
						break;
					}
					if(m_StopInProgress)
					{
	#ifdef _ENABLE_TRACE
						DebugPrintf(L"ASIOUAC: Detected exit flag in input thread!\n");
	#endif
						return;
					}
				}
				else
				{
					if(m_StopInProgress)
					{
	#ifdef _ENABLE_TRACE
						DebugPrintf(L"ASIOUAC: Detected exit flag in input thread!\n");
	#endif
						return;
					}
					if (m_AudioBufferReadyEvent)
					{
						toggle = !toggle;
						SetEvent(m_AudioBufferReadyEvent);
					}
					//bufferSwitch ();
				}
		}
	}
}


void AsioUAC2::sFillOutputData3(void* context, UCHAR *buffer, int& len)
{
	if(context)
		((AsioUAC2*)context)->FillOutputData<ThreeByteSample,AudioSample3>(buffer, len);
}

void AsioUAC2::sFillInputData3(void* context, UCHAR *buffer, int& len)
{
	if(context)
		((AsioUAC2*)context)->FillInputData<AudioSample3, ThreeByteSample>(buffer, len);
}

void AsioUAC2::sFillOutputData4(void* context, UCHAR *buffer, int& len)
{
	if(context)
		((AsioUAC2*)context)->FillOutputData<FourByteSample,AudioSample4>(buffer, len);
}

void AsioUAC2::sFillInputData4(void* context, UCHAR *buffer, int& len)
{
	if(context)
		((AsioUAC2*)context)->FillInputData<AudioSample4, FourByteSample>(buffer, len);
}

void AsioUAC2::sDeviceNotify(void* context, int reason)
{
	//if(context)
	//	((AsioUAC2*)context)->DeviceNotify(reason);
}

int AsioUAC2::getOutputSampleSize()
{
	return m_outputSampleSize;
}
int AsioUAC2::getInputSampleSize()
{
	return m_inputSampleSize;
}

BOOL AsioUAC2::SetEventHandle(HANDLE _AudioBufferReadyEvent)
{
	m_AudioBufferReadyEvent = _AudioBufferReadyEvent;
	return TRUE;
}

int AsioUAC2::GetToggle()
{
	return toggle;
}