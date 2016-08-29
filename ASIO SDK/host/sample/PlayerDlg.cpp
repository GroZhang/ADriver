#include "stdafx.h"
#include "PlayerDlg.h"
#include "Strsafe.h"
#include "Windowsx.h"

#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"

#include <fstream>
#include <direct.h> 


#define DEFAULT_BLOCK_SIZE 32000
#define MAX_PATH_LEN					360
#define SZREGSTR_SOFTWARE				L"SOFTWARE"
#define SZREGSTR_ASIO					L"ASIO"

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' " \
	"version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define IDT_REFRESH 1234

//#define ASIO_DRIVER_NAME    L"iFi HD USB Audio"
//#define DLLNAME "asiouac2.dll"
//#define REGNAME _T("SOFTWARE\\ASIO\\iFi HD USB Audio")

#define ASIO_DRIVER_NAME    L"xCORE USB Audio"
#define DLLNAME "xCOREUSBAudio.dll"
#define REGNAME _T("SOFTWARE\\ASIO\\xCORE USB Audio")

HINSTANCE g_hInstance;
PlayerDlg g_playerDlg;
typedef HRESULT(_stdcall * RegisterProc)();
RegisterProc Register;
RegisterProc Unregister;

AsioDrivers* asioDrivers = 0;


int APIENTRY _tWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPTSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	HMODULE hRichEdit = NULL;

	g_hInstance = hInstance;

	hRichEdit = LoadLibrary(_T("RICHED32"));

	HINSTANCE hInst;

	hInst = LoadLibraryA(DLLNAME);//动态加载Dll

	Register = (RegisterProc)GetProcAddress(hInst, "DllRegisterServer");//获取Dll的导出函数

	Unregister = (RegisterProc)GetProcAddress(hInst, "DllUnregisterServer");//获取Dll的导出函数

	g_playerDlg.Show();


	if (hRichEdit)
		FreeLibrary(hRichEdit);

	return 0;
}

PlayerDlg::PlayerDlg()
{
	m_hWnd = NULL;
}

void PlayerDlg::Show()
{
	m_hSysFont = NULL;
	m_pMP3 = NULL;

	m_bAsioDriverLoaded = false;
	m_bAsioInitialized = false;
	m_bAsioBufferCreated = false;
	m_bAsioStarted = false;

	DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_PROPPAGE_MEDIUM), NULL, DlgProc, (LPARAM) this);

	OnDestroy();
}

void PlayerDlg::OnInitDialog()
{
	m_hLogBox = GetDlgItem(m_hWnd, IDC_MESSAGE);

	m_hSysFont = CreateFont(15, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET,
		OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, TEXT("Tahoma"));

	SendMessage(m_hLogBox, WM_SETFONT, (WPARAM)m_hSysFont, TRUE);
	EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_INSTALL), TRUE);
	EnableWindow(GetDlgItem(m_hWnd, IDC_BUTTON_UNINSTALL), TRUE);



	HWND m_hslider = GetDlgItem(m_hWnd, IDC_SLIDER_BUFFER);
	LPARAM range = (DWORD)DEFAULT_BLOCK_SIZE;
	range = (range << 16) | (DWORD)128;

	m_hBufferSel = GetDlgItem(m_hWnd, IDC_COMBO1);

	for (int i = 0; i < 8; i++)
	{
		WCHAR	text[32];
		wsprintf(text, _T("%d"), 128 << i);
		SendMessage(m_hBufferSel, CB_ADDSTRING, i, (LPARAM)text);
	}
	
	std::string path = DLLNAME;
	path = "C:/ProgramData/ASIO/" +path.substr(0, path.length() - 3) + "txt";
	std::ifstream ifile(path);
	if (!ifile.bad())
	{
		int i = 0;
		ifile >> i ;
		SendMessage(m_hBufferSel, CB_SETCURSEL, i, 0);
		ifile.close();
	}
	else
	{
		SendMessage(m_hBufferSel, CB_SETCURSEL, 0, 0);
	}

}

void PlayerDlg::OnDestroy()
{

	if (m_hSysFont)
	{
		DeleteObject(m_hSysFont);
	}

	m_hWnd = NULL;
}

void PlayerDlg::LogPrint(LPCTSTR szFormat, ...)
{
	TCHAR szBuffer[1024];
	
	va_list ap;
	va_start(ap, szFormat);
	StringCchVPrintf(szBuffer, sizeof(szBuffer) / sizeof(TCHAR), szFormat, ap);
	va_end(ap);

	int n = lstrlen(szBuffer);
	szBuffer[n] = '\r';
	szBuffer[n + 1] = '\n';
	szBuffer[n + 2] = 0;

	int len = GetWindowTextLength(m_hLogBox);
	SendMessage(m_hLogBox, EM_SETSEL, len, len);
	SendMessage(m_hLogBox, EM_REPLACESEL, FALSE, (LPARAM)szBuffer);
	SendMessage(m_hLogBox, EM_SCROLL, SB_PAGEDOWN, 0);
}

void PlayerDlg::OnTimer()
{

}


void PlayerDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
	switch (LOWORD(wParam))
	{
	case IDC_BUTTON_INSTALL:
		OnInstall();
		break;
	case IDC_BUTTON_UNINSTALL:
		OnUninstall();
		break;
	case IDC_COMBO1:
		OnBufferSel();
		break;
	case IDC_BUTTON_LOADDRIVER:
		OnLoad();
		break;
	case IDCANCEL:
		EndDialog(m_hWnd, IDCANCEL);
		break;
	}
}

INT_PTR CALLBACK PlayerDlg::DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static PlayerDlg* pWnd;

	switch (message)
	{
	case WM_TIMER:
		if (wParam == IDT_REFRESH)
			pWnd->OnTimer();
		break;

	case WM_INITDIALOG:
		pWnd = (PlayerDlg*)lParam;
		pWnd->m_hWnd = hWnd;
		pWnd->OnInitDialog();
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		pWnd->OnCommand(wParam, lParam);
		break;
	case WM_HSCROLL:
		pWnd->OnHscroll(); 
		break;
	case WM_USER_LOGMSG:
		pWnd->LogPrint(_T("%s"), (LPCTSTR)lParam);
		break;

	}

	return (INT_PTR)FALSE;
}

void PlayerDlg::OnInstall()
{
	HRESULT hr = Register();
	if (FAILED(hr))
	{
		wprintf(L"注册文件错误，错误代码为：x%08x\r\n", hr);
	}
}

void PlayerDlg::OnUninstall()
{
	HRESULT hr = Unregister();
	if (FAILED(hr))
	{
		wprintf(L"注册文件错误，错误代码为：x%08x\r\n", hr);
	}
}

void PlayerDlg::OnBufferSel()
{
	std::string path =  DLLNAME;
	path = "C:/ProgramData/ASIO";
	if (!PathFileExists(_T("C:/ProgramData/ASIO")))
	{
		mkdir(path.c_str());
	}
	
	path = DLLNAME;
	path = "C:/ProgramData/ASIO/" + path.substr(0, path.length() - 3) + "txt";
	
	std::ofstream ofile(path);
	if (!ofile.bad())
	{
		int i = 0;
		ofile.clear();
		i = SendMessage(m_hBufferSel, CB_GETCURSEL, 0, 0);
		ofile << i << " ";
		ofile << (128 << i);
		ofile.close();
	}
}

void PlayerDlg::OnLoad()
{
	LONG inputChannels, outputChannels;
	long           preferredSize;
	long           granularity;
	long           minSize;
	long           maxSize;


	if (!asioDrivers)
		asioDrivers = new AsioDrivers();

	if (asioDrivers->asioGetNumDev() <= 0)
	{
		LogPrint(_T("No ASIO Driver found."));
		return;
	}

	if (asioDrivers)
		asioDrivers->loadDriver(ASIO_DRIVER_NAME);

	ASIODriverInfo driverInfo;
	memset(&driverInfo, 0, sizeof(ASIODriverInfo));
	ASIOInit(&driverInfo);

	LogPrint(_T("asioVersion:   %d"), driverInfo.asioVersion);
	LogPrint(_T("driverVersion:   %d"), driverInfo.driverVersion);
	LogPrint(_T("Name:   %S"), driverInfo.name);
	LogPrint(_T("ErrorMessage:   %S"), driverInfo.errorMessage);
	LogPrint(_T("----------------------------------------"));
	// internal data storage
	if (ASIOGetChannels(&inputChannels, &outputChannels) == ASE_OK)
	{
		LogPrint(_T("ASIOGetChannels (inputs: %d, outputs: %d);\n"), inputChannels, outputChannels);

		// get the usable buffer sizes
		if (ASIOGetBufferSize(&minSize, &maxSize, &preferredSize, &granularity) == ASE_OK)
		{
			LogPrint(_T("ASIOBufferSize (minSize: %d, maxSize: %d);\n"), minSize, maxSize);

		}
	}
}

void PlayerDlg::OnHscroll()
{
	WCHAR	text[32];
	WCHAR	szregpath[MAX_PATH_LEN];
	LONG	rc;
	HWND m_hslider = GetDlgItem(m_hWnd, IDC_SLIDER_BUFFER);
	int pos = SendMessage(m_hslider, TBM_GETPOS, 0, 0);
	if (pos>0)
	{
		wsprintf(text, _T("%d"), pos);
		SetWindowText(m_hslider, text);


		wsprintf(szregpath, REGNAME, SZREGSTR_SOFTWARE, SZREGSTR_ASIO);
		HKEY hkey;
		if ((rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, szregpath, 0, KEY_ALL_ACCESS, &hkey)) == ERROR_SUCCESS) {
			rc = RegSetValueEx(hkey, _T("BUFFERSIZE"), 0, REG_DWORD, (BYTE *)&pos, sizeof(DWORD));
			RegCloseKey(hkey);
			if (rc == ERROR_SUCCESS) rc = 0;
		}
	}
}