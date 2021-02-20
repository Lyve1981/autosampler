#pragma once

#include <exception>
#include <sstream>
#include <string>

enum ErrorType
{
	ErrUnknown = -1,
	ErrNone = 0,
	ErrInputDeviceNotFound,
	ErrInvalidInputBitCount,
	ErrAudioInput,
	ErrMidiOutputNotFound,
	ErrMidiOutput,
	ErrMax = 100,
};

class Error final : public std::exception
{
public:
	Error(ErrorType _type, const std::string& _message) : std::exception(_message.c_str()), m_errorType(_type) {}
	Error(ErrorType _type, const std::stringstream& _message) : Error(_type, _message.str()) {}

	ErrorType getErrorType() const { return m_errorType; }
private:
	const ErrorType m_errorType;
};