class CUacRenderer;
class CUacInputPin;
class CUacFilter;
class CResampler;

    enum ReceivedSampleActions
    {
        ReceivedSampleActions_Accept,
        ReceivedSampleActions_RejectSilent,
		ReceivedSampleActions_RejectLoud
    };


class CUacFilterManager : public IRendererFilterUac, 
							 public ISpecifyPropertyPages,
							 public CBaseReferenceClock
{
    friend class CUacFilter;
    friend class CUacInputPin;
	friend class CBasePin;

    CUacFilter			*m_pFilter;       // Methods for filter interfaces
    CUacInputPin			*m_pPin;          // A simple rendered input pin
	CUacRenderer			*m_pRenderer;
	CResampler				*m_pResampler;


	CPosPassThru			*m_pPosition;      // Renderer position controls

    CCritSec				m_Lock;                // Main renderer critical section
    CCritSec				m_ReceiveLock;         // Sublock for received samples
    CCritSec				m_MediaTypeLock;         // Sublock for received samples

	RefCountingWaveFormatEx	*m_pCurrentMediaTypeReceive;
	RefCountingWaveFormatEx	*m_pCurrentMediaTypeResample;
	//RefCountingWaveFormatEx	*m_pCurrentMediaTypeResample;
	ReceivedSampleActions	m_currentMediaTypeSampleReceivedAction;
	bool					m_IsExclusive;

public:
	STDMETHODIMP GetPages(CAUUID *pPages)
    {
        if (pPages == NULL) return E_POINTER;
        pPages->cElems = 1;
        pPages->pElems = (GUID*)CoTaskMemAlloc(sizeof(GUID));
        if (pPages->pElems == NULL) 
        {
            return E_OUTOFMEMORY;
        }
        pPages->pElems[0] = CLSID_UacProp;
        return S_OK;
    }

    STDMETHODIMP QueryInterface(REFIID riid, __deref_out void **ppv) { 
        return GetOwner()->QueryInterface(riid,ppv);            
    };                                                          
    STDMETHODIMP_(ULONG) AddRef() {                             
        return GetOwner()->AddRef();                            
    };                                                          
    STDMETHODIMP_(ULONG) Release() {                            
        return GetOwner()->Release();                           
    };
    CUacFilterManager(LPUNKNOWN pUnk, HRESULT *phr);
    ~CUacFilterManager();

    static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    HRESULT SampleReceived(IMediaSample *pSample);
	bool StartRendering();
	bool PauseRendering();
	bool StopRendering(bool clearQueue, bool clearFormats);
	bool ClearQueue();
	bool CheckFormat(WAVEFORMATEX* requestedFormat);
	STDMETHOD(GetUacMixFormat)(WAVEFORMATEX** ppFormat);
	STDMETHOD(GetExclusiveMode)(bool* pIsExclusive);
	STDMETHOD(SetExclusiveMode)(bool pIsExclusive);
	STDMETHOD(GetActiveMode)(int* pMode);
	STDMETHOD(SetDevice)(LPCWSTR pDevID);
	STDMETHOD(GetDevice)(LPWSTR* ppDevID);

	STDMETHOD(GetCurrentInputFormat)(RefCountingWaveFormatEx** ppFormat);
	STDMETHOD(GetCurrentResampledFormat)(RefCountingWaveFormatEx** ppFormat);
	STDMETHOD(GetDeviceInfos)(bool includeDisconnected, UacDeviceInfo** ppDestInfos, int* pInfoCount, int* pIndexDefault);

private:
	HRESULT SetFormatReceived(CMediaType* pmt);
	HRESULT SetFormatProcessed();
	REFERENCE_TIME GetPrivateTime();
    // Overriden to say what interfaces we support where
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
};

