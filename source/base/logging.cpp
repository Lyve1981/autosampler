#include "logging.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

void g_logWin32(const std::string& _s )
{
	::OutputDebugStringA( (_s + "\n").c_str() );
	puts(_s.c_str());
}

#else

void g_logUnix(const std::string& _s )
{
	puts(_s.c_str());
//	fputs(_s.c_str(), stderr);auto
}

#endif
