#pragma once

#include <string>

namespace F4DH::Core
{
	// Logging abstraction so Core and Application never see F4SE/spdlog types.
	class ILogger
	{
	public:
		virtual ~ILogger() = default;

		virtual void Debug(const std::string& message) = 0;
		virtual void Info(const std::string& message) = 0;
		virtual void Warn(const std::string& message) = 0;
		virtual void Error(const std::string& message) = 0;
	};
}
