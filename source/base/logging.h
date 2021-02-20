#pragma once

#include <sstream>
#include <string>

#ifdef _WIN32

	void g_logWin32( const std::string& _s );

	#define LOGWIN32(ss)	{ g_logWin32( (ss).str() ); }
#else
	void g_logUnix( const std::string& _s );
	#define LOGWIN32(ss)	{ g_logUnix( (ss).str() ); }
#endif

//#ifndef _MASTER_BUILD

#define LOG(S)																												\
{																															\
	std::stringstream ss;	ss << __FUNCTION__ << "@" << __LINE__ << ": " << S;												\
																															\
	LOGWIN32(ss)																											\
}

//#else
//	#define LOG(S)	{}
//#endif
