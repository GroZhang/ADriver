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

#include "USBAudioDevice.h"
#include "audiotask.h"


#define NEXT_INDEX(x)		((x + 1) % (sizeof(m_isoBuffers) / sizeof(ISOBuffer)))

#define MAX_OVL_ERROR_COUNT	3
#define OVL_WAIT_TIMEOUT	100


bool AudioTask::BeforeStart()
{
	if(m_DataBufferSize == 0 || m_packetPerTransfer == 0 || m_packetSize == 0)
	{
#ifdef _ENABLE_TRACE
		debugPrintf(L"ASIOUAC: %s. Can't start AudioTask thread: unknown sample freq\n", TaskName());
#endif
		return FALSE;
	}
	if(!m_isoBuffers[0].DataBuffer)
	{
#ifdef _ENABLE_TRACE
		debugPrintf(L"ASIOUAC: %s. Can't start AudioTask thread: buffers not allocated\n", TaskName());
#endif
		return FALSE;
	}

	m_SubmittedCount = 0;
	m_CompletedCount = 0;
	m_FrameNumber = 0;
	m_LastStartFrame = 0;
	m_isoTransferErrorCount = 0;

	bool r = m_device->OvlInit(&m_OvlPool, MAX_OUTSTANDING_TRANSFERS);
	if(!r)
	{
#ifdef _ENABLE_TRACE
		debugPrintf(L"ASIOUAC: %s. Can't start AudioTask thread: OvlK_Init failed\n", TaskName());
#endif
		return FALSE;
	}
	for(int i = 0; i < sizeof(m_isoBuffers) / sizeof(ISOBuffer); i++)
	{
		ISOBuffer* bufferEL = m_isoBuffers + i;
		memset(bufferEL->DataBuffer, 0xAA, m_DataBufferSize);
		//memset(bufferEL->DataBuffer, 0, m_DataBufferSize);
        IsoK_SetPackets(bufferEL->IsoContext, m_packetSize);
        bufferEL->IsoPackets = bufferEL->IsoContext->IsoPackets;
        m_device->OvlAcquire(&bufferEL->OvlHandle, m_OvlPool);
	}
	m_outstandingIndex = 0;
	m_completedIndex = 0;

    // Reset the pipe.
    r = m_device->UsbResetPipe((UCHAR)m_pipeId);

	BeforeStartInternal();
	m_isStarted = TRUE;
#ifdef _ENABLE_TRACE
	debugPrintf(L"ASIOUAC: %s. Before start thread is OK\n", TaskName());
	m_sampleNumbers = 0;
	m_tickCount = 0;
#endif
	return TRUE;
}

bool AudioTask::AfterStop()
{
	if(!m_isStarted)
		return TRUE;

	m_device->UsbAbortPipe((UCHAR)m_pipeId);

	//index of last buffer in queue
	m_outstandingIndex;
	//index of last received buffer
	m_completedIndex;

    //  Cancel all transfers left outstanding.
#ifdef _ENABLE_TRACE
	debugPrintf(L"ASIOUAC: %s. Cancel outstanding transfers\n", TaskName());
#endif
    while(m_completedIndex != m_outstandingIndex)
    {
        ISOBuffer* nextBufferEL = m_isoBuffers + m_completedIndex;
        UINT transferred;
		m_device->OvlWaitOrCancel(nextBufferEL->OvlHandle, 0, &transferred);
		m_completedIndex = NEXT_INDEX(m_completedIndex);
    }
	m_device->ClearErrorCode();
	for(int i = 0; i < sizeof(m_isoBuffers) / sizeof(ISOBuffer); i++)
	{
		//  Free the iso buffer resources.
		ISOBuffer* bufferEL = m_isoBuffers + i;
		m_device->OvlRelease(bufferEL->OvlHandle);
	}
    // Free the overlapped pool.
    OvlK_Free(m_OvlPool);
	m_isStarted = FALSE;
	AfterStopInternal();
#ifdef _ENABLE_TRACE
	debugPrintf(L"ASIOUAC: %s. After stop thread is OK\n", TaskName());
#endif

	return TRUE;
}

bool AudioTask::FreeBuffers()
{
    //  Free the iso buffer resources.
	if(m_isoBuffers[0].DataBuffer != NULL)
		for(int i = 0; i < sizeof(m_isoBuffers) / sizeof(ISOBuffer); i++)
		{
			ISOBuffer* nextBufferEL = m_isoBuffers + i;
			IsoK_Free(nextBufferEL->IsoContext);
			nextBufferEL->IsoContext = NULL;
			delete nextBufferEL->DataBuffer;
			nextBufferEL->DataBuffer = NULL;
		}
	m_outstandingIndex = 0;
	m_completedIndex = 0;
#ifdef _ENABLE_TRACE
	debugPrintf(L"ASIOUAC: %s. Free buffers is OK\n", TaskName());
#endif
	return TRUE;
}

bool AudioTask::AllocBuffers()
{
	if(m_packetPerTransfer == 0 || m_packetSize == 0)
	{
#ifdef _ENABLE_TRACE
		debugPrintf(L"ASIOUAC: %s. Can't start AllocBuffers: unknown sample freq\n", TaskName());
#endif
		return FALSE;
	}
	m_DataBufferSize = m_packetPerTransfer * m_packetSize;

    for (int i = 0; i < sizeof(m_isoBuffers) / sizeof(ISOBuffer); i++)
    {
        ISOBuffer* bufferEL = m_isoBuffers + i;
        bufferEL->DataBuffer = new UCHAR[m_DataBufferSize];
        memset(bufferEL->DataBuffer, 0xAA, m_DataBufferSize);
        //memset(bufferEL->DataBuffer, 0, m_DataBufferSize);
        IsoK_Init(&bufferEL->IsoContext, m_packetPerTransfer, 0);
        IsoK_SetPackets(bufferEL->IsoContext, m_packetSize);
    }

#ifdef _ENABLE_TRACE
		debugPrintf(L"ASIOUAC: %s. AllocBuffers OK\n", TaskName());
#endif
		return TRUE;
}

bool AudioTask::Work(volatile TaskState& taskState)
{
	ISOBuffer* nextXfer;
	UINT transferred;
	int dataLength = 0;

	m_buffersGuard.Enter();
	while(taskState == TaskStarted && NEXT_INDEX(m_outstandingIndex) != m_completedIndex && m_device->GetErrorCode() == ERROR_SUCCESS)
	{
		nextXfer = m_isoBuffers + m_outstandingIndex;
		dataLength = FillBuffer(nextXfer);
		m_outstandingIndex = NEXT_INDEX(m_outstandingIndex);
		m_device->OvlReUse(nextXfer->OvlHandle);
		SetNextFrameNumber(nextXfer);

		RWBuffer(nextXfer, dataLength);
	}

	//find next waiting buffer in queue
	nextXfer = m_isoBuffers + m_completedIndex;
	if (!nextXfer || taskState != TaskStarted) 
	{
#ifdef _ENABLE_TRACE
		debugPrintf(L"ASIOUAC: %s. No more packets!\n", TaskName());
#endif
		return TRUE;
	}

	if(!m_device->OvlWait(nextXfer->OvlHandle, OVL_WAIT_TIMEOUT, KOVL_WAIT_FLAG_NONE, &transferred))
//	if(!m_device->OvlWaitOrCancel(nextXfer->OvlHandle, OVL_WAIT_TIMEOUT, &transferred))
	{
		int deviceErrorCode = m_device->GetErrorCode();
#ifdef _ENABLE_TRACE
		debugPrintf(L"ASIOUAC: %s OvlK_Wait failed. ErrorCode: %08Xh\n", TaskName(), deviceErrorCode);
#endif
		m_isoTransferErrorCount++;
		if(//deviceErrorCode == ERROR_GEN_FAILURE ||
			m_isoTransferErrorCount >= MAX_OVL_ERROR_COUNT)
		{
#ifdef _ENABLE_TRACE
			debugPrintf(L"ASIOUAC: Notify to device about error\n"); //report to device error
#endif
			m_device->Notify(0);
			return FALSE;
		}
		else
			m_device->ClearErrorCode();
	}
	else
	{
		ProcessBuffer(nextXfer);
		m_isoTransferErrorCount = 0; //reset error count
	}

	IsoXferComplete(nextXfer, transferred);
#ifdef _ENABLE_TRACE
	CalcStatistics(transferred);
#endif
	m_completedIndex = NEXT_INDEX(m_completedIndex);
	m_buffersGuard.Leave();

	return TRUE;
}



bool AudioDACTask::BeforeStartInternal()
{
	UCHAR policyValue = 1;
	if(m_feedbackInfo)
	{
		m_feedbackInfo->ClearStatistics();
#ifdef _ENABLE_TRACE
		debugPrintf(L"ASIOUAC: %s. Clear feedback statistics\n", TaskName());
#endif
		m_feedbackInfo->SetIntervalValue((float)(8 / (1 << (m_interval - 1))));
//		m_feedbackInfo->SetValue(0);
		m_feedbackInfo->SetDefaultValue(m_defaultPacketSize);
	}
#ifdef _ENABLE_TRACE
		m_sampleNumbers = 0;
		m_tickCount = 0;
#endif
	return m_device->UsbSetPipePolicy((UCHAR)m_pipeId, ISO_ALWAYS_START_ASAP, 1, &policyValue) &&
		m_device->UsbSetPipePolicy((UCHAR)m_pipeId, RESET_PIPE_ON_RESUME, 1, &policyValue); //experimental
}

bool AudioDACTask::AfterStopInternal()
{
#ifdef _ENABLE_TRACE
	if(m_feedbackInfo)
		debugPrintf(L"ASIOUAC: %s. Maximum feedback value (%f), minimum feedback value (%f)\n", TaskName(),  m_feedbackInfo->GetMaxValue(), m_feedbackInfo->GetMinValue());
#endif
	return TRUE;
}

int AudioDACTask::FillBuffer(ISOBuffer* nextXfer)
{
	float raw_cur_feedback = m_feedbackInfo == NULL || m_feedbackInfo->GetValue() == 0.0f 
						?  m_defaultPacketSize
						:  m_feedbackInfo->GetValue(); //value in stereo samples in one second // BSB: or in one milisecond??

	int maxSamplesInPacket = m_packetSize / m_channelNumber / m_sampleSize; //max stereo samples in one packet
	if(raw_cur_feedback > (float)(maxSamplesInPacket))
	{
#ifdef _ENABLE_TRACE
		debugPrintf(L"ASIOUAC: %s. Feedback value (%f) larger than the maximum packet size\n", TaskName(), raw_cur_feedback);
#endif
		raw_cur_feedback = (float)maxSamplesInPacket;
	}
	int dataLength = 0;
	if(raw_cur_feedback > 0)
	{
		//raw_cur_feedback = m_defaultPacketSize;
		//in one second we have 8 / (1 << (m_interval - 1)) packets
		//one packet must contain samples number = [cur_feedback * (1 << (m_interval - 1)) / 8]
		float cur_feedback = raw_cur_feedback; //number stereo samples in one packet
		int icur_feedback = (int)(cur_feedback + 0.5f); // BSB: added +0.5f to get round() function. (int)(f) === floor(f), f>=0
		int nextOffSet = 0;
		static float addSample = 0; // BSB: added static.
		float frac = cur_feedback - icur_feedback;
		if(raw_cur_feedback == (float)maxSamplesInPacket) 
		{
			frac = 0.f;
			addSample = 0;
		}
		icur_feedback *= m_channelNumber * m_sampleSize;
		for (int packetIndex = 0; packetIndex < nextXfer->IsoContext->NumberOfPackets; packetIndex++)
		{
			nextXfer->IsoContext->IsoPackets[packetIndex].Offset = nextOffSet;
			nextOffSet += icur_feedback;
			addSample += frac;
			if(addSample > 0.5f) // 1.f)
			{
				nextOffSet += m_channelNumber * m_sampleSize; //append additional stereo sample
				addSample -= 1.f;
			}
			else if(addSample < -0.5f) // -1.f)	// BSB: Added negative case
			{
				nextOffSet -= m_channelNumber * m_sampleSize; //append additional stereo sample
				addSample += 1.f;
			}
			nextXfer->IsoContext->IsoPackets[packetIndex].Length = nextOffSet - nextXfer->IsoContext->IsoPackets[packetIndex].Offset;
		}
		dataLength = (int)nextOffSet;

		if(m_readDataCb)
			m_readDataCb(m_readDataCbContext, nextXfer->DataBuffer, dataLength);

#ifdef _ENABLE_TRACE
		//You can dump data for analise like this
		if(m_dumpFile) { 
			char packetstart[] = {'*', '*', 0x00, 0x80, 'P', 'a', 'c', 'k', 'e', 't', ' ', 's', 't', 'a', 'r', 't'}; // BSB added packet identifier 
			fwrite(packetstart, 1, 16, m_dumpFile); // "**", L: negative 16-bit fullscalle, "Packet start"
			fwrite(nextXfer->DataBuffer, 1, dataLength, m_dumpFile);
		}
#endif

#ifdef _ENABLE_TRACE
//		debugPrintf(L"ASIOUAC: %s. Transfer: feedback val = %.1f, send %.1f samples, transfer length=%d\n", TaskName(), raw_cur_feedback, (float)dataLength/8.f, dataLength);
#endif
	}
	else
	{
		//default transfer length
		dataLength = (int)(m_defaultPacketSize * m_packetPerTransfer);
		memset(nextXfer->DataBuffer, 0, dataLength);
	}
	return dataLength;
}

bool AudioDACTask::RWBuffer(ISOBuffer* nextXfer, int len)
{
	if(!m_device->UsbIsoWritePipe(m_pipeId, nextXfer->DataBuffer, len, (LPOVERLAPPED)nextXfer->OvlHandle, nextXfer->IsoContext))
	{
#ifdef _ENABLE_TRACE
		debugPrintf(L"ASIOUAC: %s. IsoWritePipe failed. ErrorCode: %08Xh\n", TaskName(),  m_device->GetErrorCode());
#endif
		return FALSE;
	}
	return TRUE;
}

void AudioDACTask::ProcessBuffer(ISOBuffer* buffer)
{
}

#ifdef _ENABLE_TRACE
void AudioDACTask::CalcStatistics(int sampleNumbers)
{
	if(m_tickCount == 0)
	{
		m_tickCount = GetTickCount();
		m_sampleNumbers = 0;
	}
	else
	{
		DWORD curTick = GetTickCount();
		m_sampleNumbers += sampleNumbers / m_channelNumber / m_sampleSize;
		if(curTick - m_tickCount > 1000)
		{
			if(m_feedbackInfo == NULL)
				debugPrintf(L"ASIOUAC: %s. Current sample freq: %f\n", TaskName(), (float)m_sampleNumbers / (float)(curTick - m_tickCount) * 1000.f);
			else
				debugPrintf(L"ASIOUAC: %s. Current sample freq: %f (interval %d), by fb=%f\n", TaskName(), (float)m_sampleNumbers / (float)(curTick - m_tickCount) * 1000.f, curTick - m_tickCount, m_feedbackInfo->GetFreqValue());
			m_sampleNumbers = 0;
			m_tickCount = curTick;
		}
	}
}
#endif

bool AudioADCTask::BeforeStartInternal()
{
	if(m_feedbackInfo != NULL)
		//m_feedbackInfo->SetValue(0);
		m_feedbackInfo->SetDefaultValue(m_defaultPacketSize);
	//return TRUE;
#ifdef _ENABLE_TRACE
		m_sampleNumbers = 0;
		m_tickCount = 0;
#endif

	UCHAR policyValue = 1;
	return m_device->UsbSetPipePolicy((UCHAR)m_pipeId, ISO_ALWAYS_START_ASAP, 1, &policyValue);
}

bool AudioADCTask::AfterStopInternal()
{
	return TRUE;
}

int AudioADCTask::FillBuffer(ISOBuffer* nextXfer)
{
	return m_packetPerTransfer * m_packetSize;
}

bool AudioADCTask::RWBuffer(ISOBuffer* nextXfer, int len)
{
	if(!m_device->UsbIsoReadPipe(m_pipeId, nextXfer->DataBuffer, len, (LPOVERLAPPED)nextXfer->OvlHandle, nextXfer->IsoContext))
	{
#ifdef _ENABLE_TRACE
		debugPrintf(L"ASIOUAC: %s. IsoReadPipe (ADC) failed. ErrorCode: %08Xh\n", TaskName(), m_device->GetErrorCode());
#endif
		return FALSE;
	}
	return TRUE;
}

#define PACK_ADC_BUFFER

void AudioADCTask::ProcessBuffer(ISOBuffer* buffer)
{
	int packetLength = 0;
	int recLength = 0;
	for(int i = 0; i < buffer->IsoContext->NumberOfPackets; i++)
	{
		packetLength = buffer->IsoContext->IsoPackets[i].Length;
#ifdef PACK_ADC_BUFFER
		memcpy(buffer->DataBuffer + recLength, buffer->DataBuffer + buffer->IsoPackets[i].Offset, packetLength);
		recLength += packetLength;
#else
		recLength += packetLength;
		if(m_writeDataCb && packetLength > 0)
			m_writeDataCb(m_writeDataCbContext, buffer->DataBuffer + buffer->IsoPackets[i].Offset, packetLength);
#endif
	}
#ifdef PACK_ADC_BUFFER
	if(m_writeDataCb)
		m_writeDataCb(m_writeDataCbContext, buffer->DataBuffer, recLength);
#endif
	if(m_feedbackInfo)
	{
/*
		int div = buffer->IsoContext->NumberOfPackets * m_channelNumber * m_sampleSize * (1 << (m_interval - 1)) / 8;
		int d1 = recLength / div;
		int d2 = recLength % div;
		m_feedbackInfo->SetValue((d1 << 14) + d2);

		int div = buffer->IsoContext->NumberOfPackets * m_channelNumber * m_sampleSize;
*/		
		int div = buffer->IsoContext->NumberOfPackets * m_channelNumber * m_sampleSize;
		float d1 = (float)recLength / div;
		
		int d2 = (int)(d1*(1<<16));

		//int d2 = recLength % div;
		//m_feedbackInfo->SetValue((d1 << 16) + d2);
		m_feedbackInfo->SetValue( d2);
	}
}


#ifdef _ENABLE_TRACE
void AudioADCTask::CalcStatistics(int sampleNumbers)
{
}
#endif


int AudioFeedbackTask::FillBuffer(ISOBuffer* nextXfer)
{
	return m_packetPerTransfer * m_packetSize;
}

bool AudioFeedbackTask::RWBuffer(ISOBuffer* nextXfer, int len)
{
	if(!m_device->UsbIsoReadPipe(m_pipeId, nextXfer->DataBuffer, len, (LPOVERLAPPED)nextXfer->OvlHandle, nextXfer->IsoContext))
	{
#ifdef _ENABLE_TRACE
		debugPrintf(L"ASIOUAC: %s. IsoReadPipe (feedback) failed. ErrorCode: %08Xh\n", TaskName(), m_device->GetErrorCode());
#endif
		return FALSE;
	}
	return TRUE;
}

void AudioFeedbackTask::ProcessBuffer(ISOBuffer* nextXfer)
{
	if(m_feedbackInfo == NULL)
		return;
	KISO_PACKET isoPacket = nextXfer->IsoPackets[nextXfer->IsoContext->NumberOfPackets - 1];
	//TODO: isoPacket.Length may be 3 or 4
	if (isoPacket.Length > 1)
	{
		int feedback = *((int*)(nextXfer->DataBuffer + isoPacket.Offset));
		m_feedbackInfo->SetValue(feedback);
	}
}


bool AudioFeedbackTask::BeforeStartInternal()
{
	//if(m_feedbackInfo != NULL)
	//	m_feedbackInfo->SetValue(0);
	return TRUE;
}

bool AudioFeedbackTask::AfterStopInternal()
{
	return TRUE;
}
