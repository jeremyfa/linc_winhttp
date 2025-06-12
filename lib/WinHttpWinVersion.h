// The MIT License (MIT)
// Windows Version Detection 1.0.0
// Copyright (C) 2022 by Shao Voon Wong (shaovoon@yahoo.com)
//
// http://opensource.org/licenses/MIT

#pragma once

struct WinHttpWinVersionInfo
{
	WinHttpWinVersionInfo() : Major(0), Minor(0), BuildNum(0) {}
	unsigned int Major;
	unsigned int Minor;
	unsigned int BuildNum;
};

class WinHttpWinVersion
{
public:
	static bool GetVersion(WinHttpWinVersionInfo& info);
	static bool IsBuildNumGreaterOrEqual(unsigned int buildNumber);
};
