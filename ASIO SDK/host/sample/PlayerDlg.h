#pragma once

#define WM_USER_LOGMSG	(WM_USER +1)
#define WM_STOP_ASIO	(WM_USER +2)

class DecodeMP3;

class PlayerDlg
{
public:
	PlayerDlg();

	void Show();

	void LogPrint(LPCTSTR pszFormat, ...);

	inline HWND GetHwnd() const { return m_hWnd; }

private:
	static INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

	void OnInitDialog();
	void OnDestroy();
	void OnCommand(WPARAM wParam, LPARAM lParam);
	void OnTimer();

	void StartAsio();
	void StopAsio();
	void UpdateAsioState();
	
	HWND m_hWnd;
	HWND m_hLogBox;
	HWND m_hOutputDeviceList;
	HFONT m_hSysFont;
	HWND m_hBufferSel;

	DecodeMP3* m_pMP3;

	bool m_bAsioDriverLoaded;
	bool m_bAsioInitialized;
	bool m_bAsioBufferCreated;
	bool m_bAsioStarted;

	void OnInstall();
	void OnUninstall();
	void OnLoad();
	void OnHscroll();
	void OnBufferSel();
public:
	UINT32 m_sampleRate;
	UINT32 m_channels; 

};

int MsgBoxF(UINT uType, LPCTSTR pszFormat, ...);
int MsgBox(LPCTSTR pszMessage, UINT uType = MB_OK);

extern PlayerDlg g_playerDlg;