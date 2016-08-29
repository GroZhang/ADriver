#ifndef AsioUAC2_h
#define AsioUAC2_h

#include "StdAfx.h"
#include "USBAudioDevice.h"
#include "asio.h"

//---------------------------------------------------------------------------------------------
class AsioUAC2
{
public:
	AsioUAC2 ();
	~AsioUAC2 ();

	bool init (void* sysRef);
	void getDriverName (char *name);	// max 32 bytes incl. terminating zero
	long getDriverVersion ();

	int start ();
	int stop();

	int getChannels(long *numInputChannels, long *numOutputChannels);
	int getLatencies(long *inputLatency, long *outputLatency);
	int getBufferSize(long *minSize, long *maxSize,
	long *preferredSize, long *granularity);

	int canSampleRate(double sampleRate);
	int getSampleRate(double *sampleRate);
	int setSampleRate(double sampleRate);
	int getClockSources(ASIOClockSource *clocks, long *numSources);
	int setClockSource(long index);

	int getSamplePosition(ASIOSamples *sPos, ASIOTimeStamp *tStamp);
	int getChannelInfo(ASIOChannelInfo *info);

	int createBuffers(ASIOBufferInfo *bufferInfos, long numChannels,
		long bufferSize);
	int disposeBuffers();

	int outputReady();

	int getOutputSampleSize();
	int getInputSampleSize();
	int GetToggle();

	BOOL SetEventHandle(HANDLE _AudioBufferReadyEvent);

	long getMilliSeconds () {return milliSeconds;}

	template <typename T_SRC, typename T_DST> void FillInputData(UCHAR *buffer, int& len);
	template <typename T_SRC, typename T_DST> void FillOutputData(UCHAR *buffer, int& len);


	
	static void sFillOutputData3(void* context, UCHAR *buffer, int& len);
	static void sFillInputData3(void* context, UCHAR *buffer, int& len);
	static void sFillOutputData4(void* context, UCHAR *buffer, int& len);
	static void sFillInputData4(void* context, UCHAR *buffer, int& len);
	static void sDeviceNotify(void* context, int reason);
private:
friend void myTimer();

	bool inputOpen ();
	void inputClose ();
	void input ();

	bool outputOpen ();
	void outputClose ();
	void output ();


	double samplePosition;

	double sampleRate;

	ASIOTime asioTime;
	ASIOTimeStamp theSystemTime;

	int m_NumInputs;
	int m_NumOutputs;

	int m_OutputsChPrt;
	int m_InputsChPrt;

	int m_inputSampleSize;
	int m_outputSampleSize;
	char **inputBuffers;
	char **outputBuffers;
	long *inMap;
	long *outMap;

	long blockFrames;
	long inputLatency;
	long outputLatency;
	long activeInputs;
	long activeOutputs;
	long toggle;
	long milliSeconds;
	bool active, started;
	bool timeInfoMode, tcRead;
	char errorMessage[128];

private:
	int USBAudioClass;

	USBAudioDevice *m_device;

	int StartDevice();
	int StopDevice();
	int currentOutBufferPosition;
	int currentInBufferPosition;

	volatile bool	m_StopInProgress;
	HANDLE	m_AsioSyncEvent;
	HANDLE	m_BufferSwitchEvent;
	HANDLE  m_AudioBufferReadyEvent;
};

#endif