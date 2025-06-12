// The MIT License (MIT)
// Windows Version Detection 1.0.0
// Copyright (C) 2022 by Shao Voon Wong (shaovoon@yahoo.com)
//
// http://opensource.org/licenses/MIT

#include "WinHttpWinVersion.h"
#include <Windows.h>

#pragma comment(lib, "ntdll")

extern "C" NTSTATUS __stdcall RtlGetVersion(OSVERSIONINFOEXW * lpVersionInformation);

bool WinHttpWinVersion::GetVersion(WinHttpWinVersionInfo& info)
{
	OSVERSIONINFOEXW osv;
	osv.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
	if (RtlGetVersion(&osv) == 0)
	{
		info.Major = osv.dwMajorVersion;
		info.Minor = osv.dwMinorVersion;
		info.BuildNum = osv.dwBuildNumber;
		if (osv.dwBuildNumber >= 22000)
			info.Major = 11;
		return true;
	}
	return false;
}

bool WinHttpWinVersion::IsBuildNumGreaterOrEqual(unsigned int buildNumber)
{
	WinHttpWinVersionInfo info;
	if (GetVersion(info))
	{
		return (buildNumber >= info.BuildNum);
	}
	return false;
}