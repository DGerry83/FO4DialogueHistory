#pragma once

#include "Core/ILogger.h"

namespace F4DH::Infrastructure
{
	// F4SE log file implementation of ILogger.
	// Target: Documents/My Games/Fallout4/F4SE/FO4DialogueHistory.log
	class Log final : public Core::ILogger
	{
	public:
		// Initializes the F4SE logger sink. Call once from Plugin.cpp.
		static void Init();

		void Debug(const std::string& message) override;
		void Info(const std::string& message) override;
		void Warn(const std::string& message) override;
		void Error(const std::string& message) override;
	};
}
