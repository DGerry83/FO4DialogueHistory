#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Core/Dialogue/DialogueBuffer.h"
#include "Core/ILogger.h"
#include "Core/Message/MessageBuffer.h"
#include "Core/Note/NoteStore.h"
#include "INoteBodySource.h"
#include "IViewBridge.h"

namespace F4DH::Application
{
	class IGameplayInputGate;

	enum class ViewState : std::uint8_t
	{
		Hidden,
		Open,
		Degraded,
	};

	class ViewController
	{
	public:
		ViewController(Core::DialogueBuffer& buffer, Core::MessageBuffer& messageBuffer, Core::NoteStore& noteStore, INoteBodySource& noteBodySource, IViewBridge& bridge, Core::ILogger& logger);

		void Toggle();
		void NotifyLine();
		void NotifyMessage();
		void NotifyNote();
		void OnCloseRequested();

		void OnClearAll();
		void OnClearQuest(std::uint32_t questId);
		void OnClearQuests(const std::vector<std::uint32_t>& questIds);
		void OnClearMessages();
		void OnClearNotes();
		void OnNoteBodyRequested(std::uint64_t entryId);
		void OnNoteSearchRequested(const std::string& term);

		void NotifyBulkChange();

		static void SetCloseTarget(ViewController* instance) noexcept;

		[[nodiscard]] ViewState State() const { return _state; }

		void SetTyping(bool typing) noexcept;
		void SetGameplayInputGate(IGameplayInputGate* gate) noexcept { _gameplayGate = gate; }
		[[nodiscard]] bool IsTypingActive() const { return _typing && _state == ViewState::Open; }

	private:
		void Open();
		void Close();
		void PushNoteSnapshot();

		Core::DialogueBuffer& _buffer;
		Core::MessageBuffer&  _messageBuffer;
		Core::NoteStore&      _noteStore;
		INoteBodySource&      _noteBodySource;
		IViewBridge&          _bridge;
		Core::ILogger&        _logger;
		ViewState             _state = ViewState::Hidden;
		bool                  _typing{ false };
		IGameplayInputGate*   _gameplayGate{ nullptr };
		std::string           _noteSearchTerm;
	};
}
