#include "ViewController.h"

#include <format>

#include "Core/Dialogue/PayloadBuilder.h"

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
	}

	void ViewController::SetCloseTarget(ViewController* instance) noexcept
	{
		g_target = instance;
	}

	ViewController::ViewController(Core::DialogueBuffer& buffer, IViewBridge& bridge, Core::ILogger& logger) :
		_buffer(buffer),
		_bridge(bridge),
		_logger(logger)
	{
		_bridge.SetCloseCallback(&CloseThunk);
		_bridge.SetClearCallbacks(&ClearAllThunk, &ClearQuestThunk);
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

	void ViewController::NotifyBulkChange()
	{
		if (_state == ViewState::Open) {
			_bridge.PushSnapshot(Core::PayloadBuilder::BuildSnapshot(_buffer.Snapshot()));
		}
	}

	void ViewController::Open()
	{
		if (_state == ViewState::Degraded) {
			return;
		}
		if (!_bridge.IsHealthy() && !_bridge.Create()) {
			_state = ViewState::Degraded;
			return;
		}
		_bridge.PushSnapshot(Core::PayloadBuilder::BuildSnapshot(_buffer.Snapshot()));
		_bridge.Show();
		_bridge.Focus();
		_state = ViewState::Open;
	}

	void ViewController::Close()
	{
		_bridge.Unfocus();
		_bridge.Hide();
		_state = ViewState::Hidden;
	}
}
