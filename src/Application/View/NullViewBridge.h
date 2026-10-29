#pragma once

#include "IViewBridge.h"

namespace F4DH::Application
{
	// Degraded-mode bridge: every operation is a no-op, Create reports
	// unavailable. Used when PrismaUI F4 is absent.
	class NullViewBridge final : public IViewBridge
	{
	public:
		[[nodiscard]] bool Create() override { return false; }
		void Show() override {}
		void Hide() override {}
		void Focus() override {}
		void Unfocus() override {}
		void PushSnapshot(const std::string&) override {}
		void AppendLine(const std::string&) override {}
		void PushMessageSnapshot(const std::string&) override {}
		void AppendMessage(const std::string&) override {}
		void PushNoteSnapshot(const std::string&) override {}
		void PushNoteBody(const std::string&) override {}
		void SetCloseCallback(CloseCallback) override {}
		void SetClearCallbacks(ClearAllCallback, ClearQuestCallback) override {}
		void SetClearQuestsCallback(ClearQuestsCallback) override {}
		void SetClearMessagesCallback(ClearMessagesCallback) override {}
		void SetClearNotesCallback(ClearNotesCallback) override {}
		void SetNoteBodyRequestCallback(NoteBodyRequestCallback) override {}
		void SetNoteSearchRequestCallback(NoteSearchRequestCallback) override {}
		[[nodiscard]] bool IsHealthy() override { return false; }
	};
}
