#pragma once

#include <sstream>
#include <string>

namespace asBase
{

#ifdef _WIN32
	void g_logWin32( const std::string& _s );

	#define DO_LOG(ss)	{ asBase::g_logWin32( (ss).str() ); }
#else
	void g_logUnix( const std::string& _s );

	#define DO_LOG(ss)		{ asBase::g_logUnix( (ss).str() ); }
#endif

#define LOG(S)																												\
{																															\
	std::stringstream ss;	ss << __FUNCTION__ << "@" << __LINE__ << ": " << S;												\
																															\
	DO_LOG(ss)																											\
}

}
