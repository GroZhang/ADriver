class CUacFilterProperties : public CBasePropertyPage
{
private:
    IRendererFilterUac *m_pUacFilter;    // Pointer to the filter's custom interface.
	UacDeviceInfo *m_pDeviceInfos;
	void UpdateFormatText();
	void PopulateDeviceList();

public:
    CUacFilterProperties(IUnknown *pUnk);
	virtual HRESULT 		OnConnect(IUnknown *pUnk);
	virtual HRESULT 		OnDisconnect();
	virtual HRESULT 		OnActivate();
	//INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	static CUnknown * WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT *pHr) 
	{
		CUacFilterProperties *pNewObject = new CUacFilterProperties(pUnk);
		if (pNewObject == NULL) 
		{
			*pHr = E_OUTOFMEMORY;
		}
		return pNewObject;
	} 

};