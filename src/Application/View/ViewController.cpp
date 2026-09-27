#include "ViewController.h"

#include "Core/Dialogue/PayloadBuilder.h"

namespace F4DH::Application
{
	ViewController::ViewController(Core::DialogueBuffer& buffer, IViewBridge& bridge) :
		_buffer(buffer),
		_bridge(bridge)
	{
		_bridge.SetCloseCallback([]() {
			// wired to the live instance by Composition at milestone 4
		});
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
