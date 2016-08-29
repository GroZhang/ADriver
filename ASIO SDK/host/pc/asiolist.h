#ifndef __asiolist__
#define __asiolist__

#define DRVERR			-5000
#define DRVERR_INVALID_PARAM		DRVERR-1
#define DRVERR_DEVICE_ALREADY_OPEN	DRVERR-2
#define DRVERR_DEVICE_NOT_FOUND		DRVERR-3

#define MAXPATHLEN			512
#define MAXDRVNAMELEN		128

struct asiodrvstruct
{
	int						drvID;
	CLSID					clsid;
	WCHAR					dllpath[MAXPATHLEN];
	WCHAR					drvname[MAXDRVNAMELEN];
	LPVOID					asiodrv;
	struct asiodrvstruct	*next;
};

typedef struct asiodrvstruct ASIODRVSTRUCT;
typedef ASIODRVSTRUCT	*LPASIODRVSTRUCT;

class AsioDriverList {
public:
	AsioDriverList();
	~AsioDriverList();
	
	LONG asioOpenDriver (int,VOID **);
	LONG asioCloseDriver (int);

	// nice to have
	LONG asioGetNumDev (VOID);
	LONG asioGetDriverName(int, WCHAR *, int);
	LONG asioGetDriverPath(int, WCHAR *, int);
	LONG asioGetDriverCLSID (int,CLSID *);

	// or use directly access
	LPASIODRVSTRUCT	lpdrvlist;
	int				numdrv;
};

typedef class AsioDriverList *LPASIODRIVERLIST;

#endif
