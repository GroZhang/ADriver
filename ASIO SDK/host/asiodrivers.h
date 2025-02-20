#ifndef __AsioDrivers__
#define __AsioDrivers__

#include "ginclude.h"

#if MAC
#include "CodeFragments.hpp"

class AsioDrivers : public CodeFragments

#elif WINDOWS
#include <windows.h>
#include "asiolist.h"

class AsioDrivers : public AsioDriverList

#elif SGI || BEOS
#include "asiolist.h"

class AsioDrivers : public AsioDriverList

#else
#error implement me
#endif

{
public:
	AsioDrivers();
	~AsioDrivers();
	
	bool getCurrentDriverName(WCHAR *name);
	long getDriverNames(WCHAR **names, long maxDrivers);
	bool loadDriver(WCHAR *name);
	void removeCurrentDriver();
	long getCurrentDriverIndex() {return curIndex;}
protected:
	unsigned long connID;
	long curIndex;
};

#endif
