class CUacFilterManager;

class CUacFilter : public CBaseFilter//, public CBaseReferenceClock
{
    CUacFilterManager * const m_pManager;
public:

    // Constructor
    CUacFilter(CUacFilterManager *pDump,
                LPUNKNOWN pUnk,
                CCritSec *pLock,
                HRESULT *phr);

    // Pin enumeration
    CBasePin * GetPin(int n);
    int GetPinCount();

    // Open and close the file as necessary
    STDMETHODIMP Run(REFERENCE_TIME tStart);
    STDMETHODIMP Pause();
    STDMETHODIMP Stop();
	FILTER_STATE GetState();
	REFERENCE_TIME GetStartTime();
};


//  Pin object
