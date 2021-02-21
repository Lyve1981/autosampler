#pragma once

#include <algorithm>

#include "commandline.h"
#include "../lib/config.h"

#include <sstream>
#include <string>

namespace asCli
{
class Cli
{
	struct Arg
	{
		std::string name;
		std::string desc;
		std::string defaultValue;
		bool optional;
		std::vector<std::string> examples;
	};
public:
	Cli(int argc, char* argv[]);

	void printUsage();
	int run();

private:
	template<typename T> static std::string toString(const T& _value)
	{
		std::stringstream ss; ss << _value; return ss.str();
	}

	static std::string toString(const std::vector<uint8_t>& _value)
	{
		if(_value.empty())
			return std::string();

		std::stringstream ss;

		if(_value.size() == 1)
			ss << _value[0];
		return ss.str();
	}

	static std::string toString(const bool& _value)
	{
		return _value ? "1" : "0";
	}

	template<typename T> static T parse(const std::string& _input)
	{
		std::stringstream ss(_input);
		T target;
		ss >> target;
		return target;
	}

	template<> static std::string parse<std::string>(const std::string& _input)
	{
		return _input;
	}

	template<> static bool parse<bool>(const std::string& _input)
	{
		auto in(_input);
		std::transform(in.begin(), in.end(), in.begin(), ::tolower);
		if(in == "true" || in == "1" || atoi(in.c_str()) > 0)
			return true;
		return false;
	}

	template<> static std::vector<uint8_t> parse< std::vector<uint8_t> >(const std::string& _input);

	template<typename T> bool registerArgument(const char* _name, T& _target, const char* _description, bool _optional, const std::vector<std::string>& _examples, const std::string& _default = std::string())
	{
		Arg arg{};
		arg.name = _name;
		arg.desc = _description;
		arg.optional = _optional;
		arg.defaultValue = _default.empty() ? toString(_target) : _default;
		arg.examples = _examples;

		m_arguments.push_back(arg);

		if(m_commandLine.contains(_name))
		{
			const auto value = m_commandLine.get(_name);

			_target = parse<T>(value);
		}
		else if(!_optional)
		{
			std::stringstream ss;
			ss << "Argument '" << _name << "' is required but was not specified";
			throw std::exception(ss.str().c_str());
		}

		return true;
	}

	const CommandLine m_commandLine;
	asLib::Config m_config;
	std::vector<Arg> m_arguments;
};
}