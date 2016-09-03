#pragma once
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <AudioPolicy.h>

class UacDeviceInfo
{
public:
	~UacDeviceInfo()
	{
		if(DeviceId){
			CoTaskMemFree(DeviceId);
			DeviceId=NULL;
		}
		if(DeviceFriendlyName){
			CoTaskMemFree(DeviceFriendlyName);
			DeviceFriendlyName=NULL;
		}
		if(ControlPanelPageProviderId){
			CoTaskMemFree(ControlPanelPageProviderId);
			ControlPanelPageProviderId=NULL;
		}
	}

	LPWSTR DeviceId;
	LPWSTR DeviceFriendlyName;
	LPWSTR ControlPanelPageProviderId;

	UINT FormFactor;				//http://msdn.microsoft.com/en-us/library/windows/desktop/dd316569(v=vs.85).aspx

};

class CUacUtils
{
public:
	//CUacUtils();
	//~CUacUtils();
	static HRESULT GetDeviceInfos(bool includeDisconnected, UacDeviceInfo** ppDestInfos, int* pInfoCount, int* pIndexDefault);

private:
};

