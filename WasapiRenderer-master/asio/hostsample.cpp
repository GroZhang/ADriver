#include "../WasapiRendererFilter/stdafx.h"

// hostsample.cpp : a simple ASIO host example.
// - instantiates the driver
// - get the information from the driver
// - built up some audio channels
// - plays silence for 20 seconds
// - destruct the driver
// Note: This sample cannot work with the "ASIO DirectX Driver" as it does
//       not have a valid Application Window handle, which is used as sysRef
//       on the Windows platform.

#include <stdio.h>
#include <string.h>
#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"

#define TEST_RUN_TIME  20.0		// run for 20 seconds

enum {
	// number of input and outputs supported by the host application
	// you can change these to higher or lower values
	kMaxInputChannels = 32,
	kMaxOutputChannels = 32
};


// internal data storage
typedef struct DriverInfo
{
	// ASIOInit()
	ASIODriverInfo driverInfo;

	// ASIOGetChannels()
	long           inputChannels;
	long           outputChannels;

	// ASIOGetBufferSize()
	long           minSize;
	long           maxSize;
	long           preferredSize;
	long           granularity;

	// ASIOGetSampleRate()
	ASIOSampleRate sampleRate;

	// ASIOOutputReady()
	bool           postOutput;

	// ASIOGetLatencies ()
	long           inputLatency;
	long           outputLatency;

	// ASIOCreateBuffers ()
	long inputBuffers;	// becomes number of actual created input buffers
	long outputBuffers;	// becomes number of actual created output buffers
	ASIOBufferInfo bufferInfos[kMaxInputChannels + kMaxOutputChannels]; // buffer info's

	// ASIOGetChannelInfo()
	ASIOChannelInfo channelInfos[kMaxInputChannels + kMaxOutputChannels]; // channel info's
	// The above two arrays share the same indexing, as the data in them are linked together

	// Information from ASIOGetSamplePosition()
	// data is converted to double floats for easier use, however 64 bit integer can be used, too
	double         nanoSeconds;
	double         samples;
	double         tcSamples;	// time code samples

	// bufferSwitchTimeInfo()
	ASIOTime       tInfo;			// time info state
	unsigned long  sysRefTime;      // system reference time, when bufferSwitch() was called

	// Signal the end of processing in this example
	bool           stopped;
} DriverInfo;


DriverInfo asioDriverInfo = {0};
ASIOCallbacks asioCallbacks;

//----------------------------------------------------------------------------------
// some external references
extern AsioDrivers* asioDrivers;

// internal prototypes (required for the Metrowerks CodeWarrior compiler)
int main(int argc, char* argv[]);
long init_asio_static_data (DriverInfo *asioDriverInfo);
ASIOError create_asio_buffers (DriverInfo *asioDriverInfo);
unsigned long get_sys_reference_time();


// callback prototypes
void bufferSwitch(long index, ASIOBool processNow);
ASIOTime *bufferSwitchTimeInfo(ASIOTime *timeInfo, long index, ASIOBool processNow);
void sampleRateChanged(ASIOSampleRate sRate);
long asioMessages(long selector, long value, void* message, double* opt);



//----------------------------------------------------------------------------------
long init_asio_static_data (DriverInfo *asioDriverInfo)
{	// collect the informational data of the driver
	// get the number of available channels
	if(ASIOGetChannels(&asioDriverInfo->inputChannels, &asioDriverInfo->outputChannels) == ASE_OK)
	{

		DebugPrintf(L"ASIOGetChannels: inputs=%d, outputs=%d", asioDriverInfo->inputChannels, asioDriverInfo->outputChannels);

		// get the usable buffer sizes
		if(ASIOGetBufferSize(&asioDriverInfo->minSize, &asioDriverInfo->maxSize, &asioDriverInfo->preferredSize, &asioDriverInfo->granularity) == ASE_OK)
		{
			DebugPrintf(L"ASIOGetBufferSize: min=%d, max=%d, preferred=%d, granularity=%d",
					 asioDriverInfo->minSize, asioDriverInfo->maxSize,
					 asioDriverInfo->preferredSize, asioDriverInfo->granularity);

			// get the currently selected sample rate
			if(ASIOCanSampleRate(asioDriverInfo->sampleRate) == ASE_OK)
			{
				DebugPrintf(L"ASIOGetSampleRate: sampleRate=%f", asioDriverInfo->sampleRate);
				//if (asioDriverInfo->sampleRate <= 0.0 || asioDriverInfo->sampleRate > 96000.0)
				//{
					// Driver does not store it's internal sample rate, so set it to a know one.
					// Usually you should check beforehand, that the selected sample rate is valid
					// with ASIOCanSampleRate().
				if (ASIOSetSampleRate(asioDriverInfo->sampleRate) == ASE_OK)
					{
						if(ASIOGetSampleRate(&asioDriverInfo->sampleRate) == ASE_OK)
							DebugPrintf(L"ASIOGetSampleRate: sampleRate: %f", asioDriverInfo->sampleRate);
						else
							return -6;
					}
					else
						return -5;
				//}

				// check wether the driver requires the ASIOOutputReady() optimization
				// (can be used by the driver to reduce output latency by one block)
				if(ASIOOutputReady() == ASE_OK)
					asioDriverInfo->postOutput = true;
				else
					asioDriverInfo->postOutput = false;


				DebugPrintf(L"ASIOOutputReady() - %s", asioDriverInfo->postOutput ? L"Supported":L"Not supported");

				return 0;
			}
			return -3;
		}
		return -2;
	}
	return -1;
}


//----------------------------------------------------------------------------------
// conversion from 64 bit ASIOSample/ASIOTimeStamp to double float
#if NATIVE_INT64
	#define ASIO64toDouble(a)  (a)
#else
	const double twoRaisedTo32 = 4294967296.;
	#define ASIO64toDouble(a)  ((a).lo + (a).hi * twoRaisedTo32)
#endif

double s_LastSineAngle;

void FillSineWave_Int32LSB(INT32* pLeft, INT32* pRight, int len)
{
	double angle = s_LastSineAngle;
	
	while (len-- > 0)
	{
		//int v = (int)(0x7FFFFFFF * sin(angle));
		short sv = (short)(0x7FFF * sin(angle));
		int v = sv << 16;
		
		*pLeft++ = v;
		*pRight++ = v;
		angle += 0.05f;
	}

	if (angle > 3.141592*2)
		angle -= 3.141592*2;

	s_LastSineAngle = angle;
}

void FillAudioData(INT32* pLeft, INT32* pRight, int len)
{
	if (m_pMP3 == NULL)
	{
		FillSineWave_Int32LSB(pLeft, pRight, len);
	}
	else
	{
		m_pMP3->FillBuffer(pLeft, pRight, len);
	}
}

ASIOTime *bufferSwitchTimeInfo(ASIOTime *timeInfo, long index, ASIOBool processNow)
{	// the actual processing callback.
	// Beware that this is normally in a seperate thread, hence be sure that you take care
	// about thread synchronization. This is omitted here for simplicity.
	static long processedSamples = 0;

	// store the timeInfo for later use
	asioDriverInfo.tInfo = *timeInfo;

	// get the time stamp of the buffer, not necessary if no
	// synchronization to other media is required
	if (timeInfo->timeInfo.flags & kSystemTimeValid)
		asioDriverInfo.nanoSeconds = ASIO64toDouble(timeInfo->timeInfo.systemTime);
	else
		asioDriverInfo.nanoSeconds = 0;

	if (timeInfo->timeInfo.flags & kSamplePositionValid)
		asioDriverInfo.samples = ASIO64toDouble(timeInfo->timeInfo.samplePosition);
	else
		asioDriverInfo.samples = 0;

	if (timeInfo->timeCode.flags & kTcValid)
		asioDriverInfo.tcSamples = ASIO64toDouble(timeInfo->timeCode.timeCodeSamples);
	else
		asioDriverInfo.tcSamples = 0;

	// get the system reference time
	asioDriverInfo.sysRefTime = get_sys_reference_time();

#if 0 //WINDOWS && _DEBUG
	// a few debug messages for the Windows device driver developer
	// tells you the time when driver got its interrupt and the delay until the app receives
	// the event notification.
	static double last_samples = 0;
	char tmp[128];
	sprintf (tmp, "diff: %d / %d ms / %d ms / %d samples                 \n", asioDriverInfo.sysRefTime - (long)(asioDriverInfo.nanoSeconds / 1000000.0), asioDriverInfo.sysRefTime, (long)(asioDriverInfo.nanoSeconds / 1000000.0), (long)(asioDriverInfo.samples - last_samples));
	OutputDebugStringA (tmp);
	last_samples = asioDriverInfo.samples;
#endif

	// buffer size in samples
	long buffSize = asioDriverInfo.preferredSize;

	// perform the processing
	if (asioDriverInfo.channelInfos[asioDriverInfo.inputBuffers].type == ASIOSTInt32LSB)
	{
/*
		FillSineWave_Int32LSB(
			(int32_t*)(asioDriverInfo.bufferInfos[asioDriverInfo.inputBuffers].buffers[index]), 
			(int32_t*)(asioDriverInfo.bufferInfos[asioDriverInfo.inputBuffers+1].buffers[index]), buffSize);
*/

		g_playerDlg.FillAudioData(
			(int32_t*)(asioDriverInfo.bufferInfos[asioDriverInfo.inputBuffers].buffers[index]),
			(int32_t*)(asioDriverInfo.bufferInfos[asioDriverInfo.inputBuffers + 1].buffers[index]), buffSize);
	}
	else
	for (int i = 0; i < asioDriverInfo.inputBuffers + asioDriverInfo.outputBuffers; i++)
	{
		if (asioDriverInfo.bufferInfos[i].isInput == false)
		{
			// OK do processing for the outputs only
			switch (asioDriverInfo.channelInfos[i].type)
			{
			case ASIOSTInt16LSB:
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 2);
				break;
			case ASIOSTInt24LSB:		// used for 20 bits as well
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 3);
				break;
			case ASIOSTInt32LSB:
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;

			case ASIOSTFloat32LSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;

			case ASIOSTFloat64LSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 8);
				break;

				// these are used for 32 bit data buffer, with different alignment of the data inside
				// 32 bit PCI bus systems can more easily used with these
			case ASIOSTInt32LSB16:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32LSB18:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32LSB20:		// 32 bit data with 20 bit alignment
			case ASIOSTInt32LSB24:		// 32 bit data with 24 bit alignment
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;

			case ASIOSTInt16MSB:
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 2);
				break;
			case ASIOSTInt24MSB:		// used for 20 bits as well
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 3);
				break;
			case ASIOSTInt32MSB:
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;
			case ASIOSTFloat32MSB:		// IEEE 754 32 bit float, as found on Intel x86 architecture
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;
			case ASIOSTFloat64MSB: 		// IEEE 754 64 bit double float, as found on Intel x86 architecture
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 8);
				break;

				// these are used for 32 bit data buffer, with different alignment of the data inside
				// 32 bit PCI bus systems can more easily used with these
			case ASIOSTInt32MSB16:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32MSB18:		// 32 bit data with 18 bit alignment
			case ASIOSTInt32MSB20:		// 32 bit data with 20 bit alignment
			case ASIOSTInt32MSB24:		// 32 bit data with 24 bit alignment
				memset (asioDriverInfo.bufferInfos[i].buffers[index], 0, buffSize * 4);
				break;
			}
		}
	}

	// finally if the driver supports the ASIOOutputReady() optimization, do it here, all data are in place
	if (asioDriverInfo.postOutput)
		ASIOOutputReady();

	if (processedSamples >= asioDriverInfo.sampleRate * TEST_RUN_TIME)	// roughly measured
		asioDriverInfo.stopped = true;
	else
		processedSamples += buffSize;

	return 0L;
}

//----------------------------------------------------------------------------------
void bufferSwitch(long index, ASIOBool processNow)
{	// the actual processing callback.
	// Beware that this is normally in a seperate thread, hence be sure that you take care
	// about thread synchronization. This is omitted here for simplicity.

	// as this is a "back door" into the bufferSwitchTimeInfo a timeInfo needs to be created
	// though it will only set the timeInfo.samplePosition and timeInfo.systemTime fields and the according flags
	ASIOTime  timeInfo;
	memset (&timeInfo, 0, sizeof (timeInfo));

	// get the time stamp of the buffer, not necessary if no
	// synchronization to other media is required
	if(ASIOGetSamplePosition(&timeInfo.timeInfo.samplePosition, &timeInfo.timeInfo.systemTime) == ASE_OK)
		timeInfo.timeInfo.flags = kSystemTimeValid | kSamplePositionValid;

	bufferSwitchTimeInfo (&timeInfo, index, processNow);
}


//----------------------------------------------------------------------------------
void sampleRateChanged(ASIOSampleRate sRate)
{
	// do whatever you need to do if the sample rate changed
	// usually this only happens during external sync.
	// Audio processing is not stopped by the driver, actual sample rate
	// might not have even changed, maybe only the sample rate status of an
	// AES/EBU or S/PDIF digital input at the audio device.
	// You might have to update time/sample related conversion routines, etc.
}

//----------------------------------------------------------------------------------
long asioMessages(long selector, long value, void* message, double* opt)
{
	// currently the parameters "value", "message" and "opt" are not used.
	long ret = 0;
	switch(selector)
	{
		case kAsioSelectorSupported:
			if(value == kAsioResetRequest
			|| value == kAsioEngineVersion
			|| value == kAsioResyncRequest
			|| value == kAsioLatenciesChanged
			// the following three were added for ASIO 2.0, you don't necessarily have to support them
			|| value == kAsioSupportsTimeInfo
			|| value == kAsioSupportsTimeCode
			|| value == kAsioSupportsInputMonitor)
				ret = 1L;
			break;
		case kAsioResetRequest:
			// defer the task and perform the reset of the driver during the next "safe" situation
			// You cannot reset the driver right now, as this code is called from the driver.
			// Reset the driver is done by completely destruct is. I.e. ASIOStop(), ASIODisposeBuffers(), Destruction
			// Afterwards you initialize the driver again.
			asioDriverInfo.stopped;  // In this sample the processing will just stop
			ret = 1L;
			break;
		case kAsioResyncRequest:
			// This informs the application, that the driver encountered some non fatal data loss.
			// It is used for synchronization purposes of different media.
			// Added mainly to work around the Win16Mutex problems in Windows 95/98 with the
			// Windows Multimedia system, which could loose data because the Mutex was hold too long
			// by another thread.
			// However a driver can issue it in other situations, too.
			ret = 1L;
			break;
		case kAsioLatenciesChanged:
			// This will inform the host application that the drivers were latencies changed.
			// Beware, it this does not mean that the buffer sizes have changed!
			// You might need to update internal delay data.
			ret = 1L;
			break;
		case kAsioEngineVersion:
			// return the supported ASIO version of the host application
			// If a host applications does not implement this selector, ASIO 1.0 is assumed
			// by the driver
			ret = 2L;
			break;
		case kAsioSupportsTimeInfo:
			// informs the driver wether the asioCallbacks.bufferSwitchTimeInfo() callback
			// is supported.
			// For compatibility with ASIO 1.0 drivers the host application should always support
			// the "old" bufferSwitch method, too.
			ret = 1;
			break;
		case kAsioSupportsTimeCode:
			// informs the driver wether application is interested in time code info.
			// If an application does not need to know about time code, the driver has less work
			// to do.
			ret = 0;
			break;
	}
	return ret;
}


//----------------------------------------------------------------------------------
ASIOError create_asio_buffers (DriverInfo *asioDriverInfo)
{	// create buffers for all inputs and outputs of the card with the 
	// preferredSize from ASIOGetBufferSize() as buffer size
	long i;
	ASIOError result;

	// fill the bufferInfos from the start without a gap
	ASIOBufferInfo *info = asioDriverInfo->bufferInfos;

	// prepare inputs (Though this is not necessaily required, no opened inputs will work, too
	if (asioDriverInfo->inputChannels > kMaxInputChannels)
		asioDriverInfo->inputBuffers = kMaxInputChannels;
	else
		asioDriverInfo->inputBuffers = 2;
	for(i = 0; i < asioDriverInfo->inputBuffers; i++, info++)
	{
		info->isInput = ASIOTrue;
		info->channelNum = i+2;
		info->buffers[0] = info->buffers[1] = 0;
	}

	// prepare outputs
	if (asioDriverInfo->outputChannels > kMaxOutputChannels)
		asioDriverInfo->outputBuffers = kMaxOutputChannels;
	else
		asioDriverInfo->outputBuffers = 2;
	for(i = 0; i < asioDriverInfo->outputBuffers; i++, info++)
	{
		info->isInput = ASIOFalse;
		info->channelNum = i+2;
		info->buffers[0] = info->buffers[1] = 0;
	}

	// create and activate buffers
	result = ASIOCreateBuffers(asioDriverInfo->bufferInfos,
		asioDriverInfo->inputBuffers + asioDriverInfo->outputBuffers,
		asioDriverInfo->preferredSize, &asioCallbacks);
	if (result == ASE_OK)
	{
		// now get all the buffer details, sample word length, name, word clock group and activation
		for (i = 0; i < asioDriverInfo->inputBuffers + asioDriverInfo->outputBuffers; i++)
		{
			asioDriverInfo->channelInfos[i].channel = asioDriverInfo->bufferInfos[i].channelNum;
			asioDriverInfo->channelInfos[i].isInput = asioDriverInfo->bufferInfos[i].isInput;
			result = ASIOGetChannelInfo(&asioDriverInfo->channelInfos[i]);
			if (result != ASE_OK)
				break;
		}

		if (result == ASE_OK)
		{
			// get the input and output latencies
			// Latencies often are only valid after ASIOCreateBuffers()
			// (input latency is the age of the first sample in the currently returned audio block)
			// (output latency is the time the first sample in the currently returned audio block requires to get to the output)
			result = ASIOGetLatencies(&asioDriverInfo->inputLatency, &asioDriverInfo->outputLatency);
			if (result == ASE_OK)
				DebugPrintf(L"ASIOGetLatencies: input=%d, output=%d", asioDriverInfo->inputLatency, asioDriverInfo->outputLatency);
		}
	}
	return result;
}

void PlayerDlg::CreateAsio()
{
	TCHAR drvname[64];

	assert(asioDrivers == NULL);
	asioDrivers = new AsioDrivers();

	if (asioDrivers->asioGetNumDev() <= 0)
	{
		DebugPrintf(L"No ASIO Driver found.");
		return;
	}

	for (int i = 0; i < asioDrivers->asioGetNumDev(); i++)
	{
		asioDrivers->asioGetDriverName(1, drvname, 48);
		if (!asioDrivers->loadDriver(drvname))
		{
			continue;
		}
		SendMessage(m_hOutputDeviceList, CB_ADDSTRING, 0, (LPARAM) drvname);
		SendMessage(m_hOutputDeviceList, CB_SETCURSEL, asioDrivers->asioGetNumDev() -1, 0);

	
		m_bAsioDriverLoaded = true;

		// initialize the driver
		if (ASIOInit(&asioDriverInfo.driverInfo) == ASE_OK)
		{
			break;
		}
	}

	DebugPrintf(L("asioVersion:   %d"), asioDriverInfo.driverInfo.asioVersion);
	DebugPrintf(L("driverVersion:   %d"), asioDriverInfo.driverInfo.driverVersion);
	DebugPrintf(L("Name:   %S"), asioDriverInfo.driverInfo.name);
	DebugPrintf(L("ErrorMessage:   %S"), asioDriverInfo.driverInfo.errorMessage);
	DebugPrintf(L("----------------------------------------"));


}

void PlayerDlg::DestroyAsio()
{
	if (m_bAsioDriverLoaded)
	{
		if (m_bAsioInitialized)
		{
			if (m_bAsioBufferCreated)
			{
				if (m_bAsioStarted)
				{
					ASIOStop();
				}

				ASIODisposeBuffers();
			}

			ASIOExit();
		}

		asioDrivers->removeCurrentDriver();
	}
}

void PlayerDlg::StartAsio()
{
	asioDriverInfo.sampleRate = m_sampleRate;
	//asioDriverInfo.outputChannels = m_channels;
	asioDriverInfo.inputChannels = 0;
	if (init_asio_static_data(&asioDriverInfo) == 0)
	{
		// ASIOControlPanel(); you might want to check wether the ASIOControlPanel() can open

		// set up the asioCallback structure and create the ASIO data buffer
		asioCallbacks.bufferSwitch = &bufferSwitch;
		asioCallbacks.sampleRateDidChange = &sampleRateChanged;
		asioCallbacks.asioMessage = &asioMessages;
		asioCallbacks.bufferSwitchTimeInfo = &bufferSwitchTimeInfo;

		if (create_asio_buffers(&asioDriverInfo) == ASE_OK)
		{
			

			DebugPrintf(L("Output BitStream sample type: %d"), asioDriverInfo.channelInfos[asioDriverInfo.inputBuffers].type);
		}
		else
		{
			DebugPrintf(L("ASIO Buffer creation failed."));
		}
	}
	else
	{
		DebugPrintf(L("ASIO Static Data initialization failed."));
	}
	if (ASIOStart() != ASE_OK)
	{
		m_bAsioStarted = false;
		DebugPrintf(L("ASIO Start failed."));
		return;
	}

	m_bAsioStarted = true;
	DebugPrintf(L("\r\nASIO Driver started succefully."));
}

void PlayerDlg::StopAsio()
{
	ASIOStop();
	m_bAsioStarted = false;
	DebugPrintf(L("ASIO stopped."));
}

void PlayerDlg::UpdateAsioState()
{
	DebugPrintf(L"%d ms / %d ms / %d samples", asioDriverInfo.sysRefTime, (long)(asioDriverInfo.nanoSeconds / 1000000.0), (long)asioDriverInfo.samples);

	// create a more readable time code format (the quick and dirty way)
	double remainder = asioDriverInfo.tcSamples;
	long hours = (long)(remainder / (asioDriverInfo.sampleRate * 3600));
	remainder -= hours * asioDriverInfo.sampleRate * 3600;
	long minutes = (long)(remainder / (asioDriverInfo.sampleRate * 60));
	remainder -= minutes * asioDriverInfo.sampleRate * 60;
	long seconds = (long)(remainder / asioDriverInfo.sampleRate);
	remainder -= seconds * asioDriverInfo.sampleRate;
	DebugPrintf(L" / TC: %2.2d:%2.2d:%2.2d:%5.5d", (long)hours, (long)minutes, (long)seconds, (long)remainder);
}

unsigned long get_sys_reference_time()
{	// get the system reference time
#if WINDOWS
	return timeGetTime();
#elif MAC
static const double twoRaisedTo32 = 4294967296.;
	UnsignedWide ys;
	Microseconds(&ys);
	double r = ((double)ys.hi * twoRaisedTo32 + (double)ys.lo);
	return (unsigned long)(r / 1000.);
#endif
}
