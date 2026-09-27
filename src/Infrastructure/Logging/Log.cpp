#include "Log.h"

// Milestone 1: wire to F4SE::log / logger::info etc. per CommonLibF4
// conventions (F4SE.log sink opened in Init).

namespace F4DH::Infrastructure
{
	void Log::Init() {}

	void Log::Debug(const std::string&) {}
	void Log::Info(const std::string&) {}
	void Log::Warn(const std::string&) {}
	void Log::Error(const std::string&) {}
}
