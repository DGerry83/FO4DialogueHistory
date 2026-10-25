#include "ViewController.h"

#include <format>

#include "Core/Dialogue/PayloadBuilder.h"
#include "Core/Message/MessagePayloadBuilder.h"
#include "Application/Input/IGameplayInputGate.h"

namespace F4DH::Application
{
	namespace
	{
		// Single routing target for every global thunk (close + clear), set
		// by SetCloseTarget at composition time.
		ViewController* g_target{ nullptr };

		void CloseThunk()
		{
			if (g_target) {
				g_target->OnCloseRequested();
			}
		}

		void ClearAllThunk()
		{
			if (g_target) {
				g_target->OnClearAll();
			}
		}

		void ClearQuestThunk(std::uint32_t questId)
		{
			if (g_target) {
				g_target->OnClearQuest(questId);
			}
		}

		void ClearQuestsThunk(const std::vector<std::uint32_t>& questIds)
		{
			if (g_target) {
				g_target->OnClearQuests(questIds);
			}
		}

		void ClearMessagesThunk()
		{
			if (g_target) {
				g_target->OnClearMessages();
			}
		}
	}

	void ViewController::SetCloseTarget(ViewController* instance) noexcept
	{
		g_target = instance;
	}

	ViewController::ViewController(Core::DialogueBuffer& buffer, Core::MessageBuffer& messageBuffer, IViewBridge& bridge, Core::ILogger& logger) :
		_buffer(buffer),
		_messageBuffer(messageBuffer),
		_bridge(bridge),
		_logger(logger)
	{
		_bridge.SetCloseCallback(&CloseThunk);
		_bridge.SetClearCallbacks(&ClearAllThunk, &ClearQuestThunk);
		_bridge.SetClearQuestsCallback(&ClearQuestsThunk);
		_bridge.SetClearMessagesCallback(&ClearMessagesThunk);
	}

	void ViewController::Toggle()
	{
		if (_state == ViewState::Open) {
			Close();
		} else if (_state == ViewState::Hidden) {
			Open();
		}
	}

	void ViewController::NotifyLine()
	{
		if (_state == ViewState::Open) {
			const auto lines = _buffer.Snapshot();
			if (!lines.empty()) {
				_bridge.AppendLine(Core::PayloadBuilder::BuildAppend(lines.back()));
			}
		}
	}

	void ViewController::NotifyMessage()
	{
		// Mirror of NotifyLine on the independent message channel: the panel
		// appends live only while open; hidden/closed panels skip the push and
		// receive the buffer's next snapshot on open. Safe from any engine
		// thread — the bridge marshals to the game thread exactly like AppendLine.
		if (_state == ViewState::Open) {
			const auto messages = _messageBuffer.Snapshot();
			if (!messages.empty()) {
				_bridge.AppendMessage(Core::MessagePayloadBuilder::BuildAppend(messages.back()));
			}
		}
	}

	void ViewController::OnCloseRequested()
	{
		if (_state == ViewState::Open) {
			Close();
		}
	}

	void ViewController::OnClearAll()
	{
		_buffer.Clear();
		_logger.Info("clear history: removed all lines");
		_bridge.PushSnapshot(Core::PayloadBuilder::BuildSnapshot(_buffer.Snapshot()));
	}

	void ViewController::OnClearQuest(std::uint32_t questId)
	{
		const auto removed = _buffer.RemoveQuest(questId);
		_logger.Info(std::format("clear history: removed {} line(s) for questId {}", removed, questId));
		_bridge.PushSnapshot(Core::PayloadBuilder::BuildSnapshot(_buffer.Snapshot()));
	}

	void ViewController::OnClearQuests(const std::vector<std::uint32_t>& questIds)
	{
		for (const auto questId : questIds) {
			const auto removed = _buffer.RemoveQuest(questId);
			_logger.Info(std::format("clear history: removed {} line(s) for questId {}", removed, questId));
		}
		_bridge.PushSnapshot(Core::PayloadBuilder::BuildSnapshot(_buffer.Snapshot()));
	}

	void ViewController::NotifyBulkChange()
	{
		if (_state == ViewState::Open) {
			_bridge.PushSnapshot(Core::PayloadBuilder::BuildSnapshot(_buffer.Snapshot()));
		}
	}

	void ViewController::OnClearMessages()
	{
		// Message-only clear: dialogue storage, snapshots, search, and
		// selection are untouched (AC10). The empty message snapshot is pushed
		// like the dialogue clear's refresh so an open panel updates and a
		// later DOM-ready replay restores the cleared state.
		_messageBuffer.Clear();
		_logger.Info("clear history: removed all messages");
		_bridge.PushMessageSnapshot(Core::MessagePayloadBuilder::BuildSnapshot(_messageBuffer.Snapshot()));
	}

	void ViewController::SetTyping(bool typing) noexcept
	{
		// #005 C2: toggle the gate on typing-ACTIVE transitions (typing && Open),
		// not raw typing transitions — inactive->inactive repeats are no-ops and
		// the gate itself is idempotent besides.
		const bool wasActive = IsTypingActive();
		_typing = typing;
		if (!_gameplayGate || wasActive == IsTypingActive()) {
			return;
		}
		if (IsTypingActive()) {
			_gameplayGate->BlockGameplayInput();
		} else {
			_gameplayGate->UnblockGameplayInput();
		}
	}

	void ViewController::Open()
	{
		_typing = false;  // #005: authoritative reset — no stale typing state survives a reopen
		if (_gameplayGate) {
			_gameplayGate->UnblockGameplayInput();  // #005 C2: stuck-state net, same role as the _typing reset
		}
		if (_state == ViewState::Degraded) {
			return;
		}
		if (!_bridge.IsHealthy() && !_bridge.Create()) {
			_state = ViewState::Degraded;
			return;
		}
		// Fresh open always lands on Dialogue History (the view selects its
		// dialogue tab on setHistory), but the message channel replays
		// independently — both caches/snapshots go down their own transports.
		_bridge.PushSnapshot(Core::PayloadBuilder::BuildSnapshot(_buffer.Snapshot()));
		_bridge.PushMessageSnapshot(Core::MessagePayloadBuilder::BuildSnapshot(_messageBuffer.Snapshot()));
		_bridge.Show();
		_bridge.Focus();
		_state = ViewState::Open;
	}

	void ViewController::Close()
	{
		_typing = false;  // #005: authoritative reset — close while typing must not stick the flag
		if (_gameplayGate) {
			_gameplayGate->UnblockGameplayInput();  // #005 C2: stuck-state net — close while typing must not stick the block either
		}
		_bridge.Unfocus();
		_bridge.Hide();
		_state = ViewState::Hidden;
	}
}
