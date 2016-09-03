#include <streams.h>

#include "UacRendererFilteruids.h"
#include "UacRendererFilter.h"
#include "CUacFilter.h"
#include "CUacInputPin.h"
#include "UacRenderer.h"
#include "UacHelper.h"
#include "stdafx.h"
#include "CUacFilterManager.h"
#include "PropertyPages.h"

// Setup data

const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
    &MEDIATYPE_NULL,            // Major type
    &MEDIASUBTYPE_NULL          // Minor type
};

const AMOVIESETUP_PIN sudPins =
{
    L"Input",                   // Pin string name
    FALSE,                      // Is it rendered
    FALSE,                      // Is it an output
    FALSE,                      // Allowed none
    FALSE,                      // Likewise many
    &CLSID_NULL,                // Connects to filter
    L"Output",                  // Connects to pin
    1,                          // Number of types
    &sudPinTypes                // Pin information
};

const AMOVIESETUP_FILTER sudDump = 
{
	&CLSID_UacRendererFilter,                // Filter CLSID
    L"UacRendererFilter",                    // String name
    MERIT_DO_NOT_USE,           // Filter merit
    1,                          // Number pins
    &sudPins                    // Pin details
};


//
//  Object creation stuff
//
CFactoryTemplate g_Templates[]= 
{
	{ L"UacRendererFilter", &CLSID_UacRendererFilter, CUacFilterManager::CreateInstance, NULL, &sudDump },
	{ L"Saturation Props", &CLSID_UacProp, CUacFilterProperties::CreateInstance, NULL, NULL }
};
int g_cTemplates =sizeof(g_Templates)/sizeof(g_Templates[0]);


// Constructor

CUacFilter::CUacFilter(CUacFilterManager *pDump,
                         LPUNKNOWN pUnk,
                         CCritSec *pLock,
                         HRESULT *phr) :
						 CBaseFilter(NAME("CUacFilter"), pUnk, pLock, CLSID_UacRendererFilter),
    m_pManager(pDump)
{
	HRESULT hr=S_OK;
}







//
// GetPin
//
CBasePin * CUacFilter::GetPin(int n)
{
    if (n == 0) {
        return m_pManager->m_pPin;
    } else {
        return NULL;
    }
}


//
// GetPinCount
//
int CUacFilter::GetPinCount()
{
    return 1;
}


//
// Stop
//
// Overriden to close the dump file
//
	bool bStarted=false;
STDMETHODIMP CUacFilter::Stop()
{
	bStarted=false;
 	DebugPrintf(L"CUacFilter::Stop \n");
    CAutoLock cObjectLock(m_pLock);
	m_pManager->StopRendering(true,false);  
	HRESULT hr=CBaseFilter::Stop();
    return hr;
}

FILTER_STATE CUacFilter::GetState()
{
	return m_State;
}

REFERENCE_TIME CUacFilter::GetStartTime()
{
	LONGLONG units=m_tStart.GetUnits();
	return m_tStart.m_time;
}

STDMETHODIMP CUacFilter::Pause()
{
 	DebugPrintf(L"CUacFilter::Pause \n");
    CAutoLock cObjectLock(m_pLock);
	HRESULT hr=CBaseFilter::Pause();
	m_pManager->PauseRendering();
 	DebugPrintf(L"CUacFilter::Pause returning\n");
	bStarted=false;
    return hr;
}


// Run
//
    // the start parameter is the difference to be added to the
    // sample's stream time to get the reference time for
    // its presentation
STDMETHODIMP CUacFilter::Run(REFERENCE_TIME tStart)
{
	bStarted=true;
 	DebugPrintf(L"CUacFilter::Run \n");
    CAutoLock cObjectLock(m_pLock);
	HRESULT hr=CBaseFilter::Run(tStart);
	m_pManager->StartRendering();
    return hr;
}
