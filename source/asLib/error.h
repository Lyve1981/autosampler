#pragma once

#include <exception>
#include <sstream>
#include <string>

namespace asLib
{

enum ErrorType
{
	ErrUnknown = -1,
	ErrMax = 100,

	ErrNone = 0,
	ErrInputDeviceNotFound,
	ErrInvalidInputBitCount,
	ErrAudioInput,
	ErrMidiOutputNotFound,
	ErrMidiOutput,
	ErrFileIO,
};

class Error final : public std::runtime_error
{
public:
	Error(ErrorType _type, const std::string& _message) : std::runtime_error(_message.c_str()), m_errorType(_type) {}
	Error(ErrorType _type, const std::stringstream& _message) : Error(_type, _message.str()) {}

	ErrorType getErrorType() const { return m_errorType; }
private:
	const ErrorType m_errorType;
};
	
}
