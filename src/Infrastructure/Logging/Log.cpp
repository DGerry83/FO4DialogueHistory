#include "Log.h"

// The sink is opened by F4SE::Init(..., { .logName = "FO4DialogueHistory.log" })
// in Plugin.cpp (CommonLibF4 writes Documents/My Games/<save>/F4SE/<logName>);
// Log::Init remains the documented seam for any future sink configuration.

namespace F4DH::Infrastructure
{
	void Log::Init() {}

	void Log::Debug(const std::string& message) { REX::DEBUG("{}", message); }
	void Log::Info(const std::string& message) { REX::INFO("{}", message); }
	void Log::Warn(const std::string& message) { REX::WARN("{}", message); }
	void Log::Error(const std::string& message) { REX::ERROR("{}", message); }
}
