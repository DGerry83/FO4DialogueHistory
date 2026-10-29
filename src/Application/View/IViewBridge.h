#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace F4DH::Application
{
	// Abstracts show/hide/focus/push operations on the UI view.
	class IViewBridge
	{
	public:
		using CloseCallback = void (*)();
		using ClearAllCallback = void (*)();
		using ClearQuestCallback = void (*)(std::uint32_t);
		using ClearQuestsCallback = void (*)(const std::vector<std::uint32_t>&);
		using ClearMessagesCallback = void (*)();  // message-history clear request (no payload — always all)
		using ClearNotesCallback = void (*)();
		using NoteBodyRequestCallback = void (*)(std::uint64_t);
		using NoteSearchRequestCallback = void (*)(const std::string&);
		using NoteFolderCreateCallback = void (*)(std::uint64_t, const std::string&);
		using NoteMoveCallback = void (*)(std::uint64_t, std::uint64_t);
		using NoteDeleteCallback = void (*)(std::uint64_t);

		virtual ~IViewBridge() = default;

		[[nodiscard]] virtual bool Create() = 0;  // false = unavailable (degraded)
		virtual void Show() = 0;
		virtual void Hide() = 0;
		virtual void Focus() = 0;
		virtual void Unfocus() = 0;
		virtual void PushSnapshot(const std::string& json) = 0;
		virtual void AppendLine(const std::string& json) = 0;
		// Independent message-history channel (setMessageHistory / appendMessage).
		// Separate transport from the dialogue payloads by contract.
		virtual void PushMessageSnapshot(const std::string& json) = 0;
		virtual void AppendMessage(const std::string& json) = 0;
		virtual void PushNoteSnapshot(const std::string& json) = 0;
		virtual void PushNoteBody(const std::string& json) = 0;
		virtual void PushNoteOperationResult(const std::string& json) = 0;
		virtual void SetCloseCallback(CloseCallback fn) = 0;
		virtual void SetClearCallbacks(ClearAllCallback allFn, ClearQuestCallback questFn) = 0;
		virtual void SetClearQuestsCallback(ClearQuestsCallback questsFn) = 0;
		virtual void SetClearMessagesCallback(ClearMessagesCallback fn) = 0;
		virtual void SetClearNotesCallback(ClearNotesCallback fn) = 0;
		virtual void SetNoteBodyRequestCallback(NoteBodyRequestCallback fn) = 0;
		virtual void SetNoteSearchRequestCallback(NoteSearchRequestCallback fn) = 0;
		virtual void SetNoteFolderCreateCallback(NoteFolderCreateCallback fn) = 0;
		virtual void SetNoteMoveCallback(NoteMoveCallback fn) = 0;
		virtual void SetNoteDeleteCallback(NoteDeleteCallback fn) = 0;
		[[nodiscard]] virtual bool IsHealthy() = 0;
	};
}
