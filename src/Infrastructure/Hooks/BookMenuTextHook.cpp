#include "BookMenuTextHook.h"

#include "Core/ILogger.h"
#include "Application/Capture/INoteSink.h"
#include "Infrastructure/Notes/NoteRecordResolver.h"

#include "F4SE/API.hpp"
#include "F4SE/Runtimes.hpp"

#include "REL/Id.hpp"
#include "REL/IHookFn.hpp"
#include "REL/REL.hpp"
#include "REL/Relocation.hpp"

#include "RE/N/NiPointer.hpp"
#include "RE/T/TESObjectBOOK.hpp"
#include "RE/T/TESObjectREFR.hpp"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <format>
#include <memory>
#include <string>

// OG-only prologue detour on BookMenu::PrepareBookText(void*, char*, u64)
// (Address Library ID 1263647, verified present in the user's
// version-1-10-163-0.bin at 0x1258e90). The entry begins:
//
//		4D 85 C0                test r8, r8
//		0F 84 7F 02 00 00       je entry+0x288
//
// — a 5-byte steal would split the conditional branch, so 9 bytes are stolen
// and the continuation re-implements both paths (NEXT_PROBE_CONTRACT):
//
//		offset 0:  4D 85 C0                         test r8, r8          (restored)
//		offset 3:  74 0E                            je +0x0E -> offset 19 (5 + 0x0E)
//		offset 5:  FF 25 00 00 00 00 <u64 entry+9>   absolute jump, 14 bytes
//		offset 19: FF 25 00 00 00 00 <u64 entry+0x288> absolute jump, 14 bytes
//
// No argument register is used as jump scratch. The patched entry is a Jump5
// to the thunk plus four 0x90 NOPs so no entry byte is left undefined.
// REX/REL hard-fail semantics (missing ID, WriteSafe error) are process-fatal
// by fork design and are NOT claimed recoverable; every recoverable failure
// path below leaves the detour uninstalled and the game untouched.

namespace
{
	using PrepareBookText_t = void(void* a_bookMenu, char* a_text, std::uint64_t a_length);

	// OG-slotted: resolves on OG only; the IsRuntimeOnlyOG gate below makes any
	// other runtime unreachable before construction.
	constexpr REL::Id<F4SE::RUNTIME_LATEST_OG> kPrepareBookTextId{ 1263647 };
	constexpr REL::Id<F4SE::RUNTIME_LATEST_OG> kTargetBookSlotId{ 823300 };
	constexpr REL::Id<F4SE::RUNTIME_LATEST_OG> kTargetRefSlotId{ 995765 };

	constexpr std::size_t kStolenBytes{ 9 };
	constexpr std::size_t kContinuationBytes{ 33 };
	constexpr std::size_t kMaxPreparedCopy{ 64 * 1024 };
	// The scan is bounded by length+1 per contract; this sanity cap keeps a
	// corrupt length from becoming an unbounded read (anything beyond it
	// could not contribute to evidence anyway — the copy cap is 64 KiB).
	constexpr std::size_t kMaxScanSpan{ 4 * 1024 * 1024 };

	// Live entry bytes the contract was built against. Validated after
	// IHookFn::Init captures them and BEFORE Enable (parent ruling P1): the
	// on-disk exe stores .text obfuscated, so only the live image can be
	// checked. Mismatch means wrong ID or a patched binary — never enable.
	constexpr std::uint8_t kExpectedEntry[kStolenBytes] = { 0x4D, 0x85, 0xC0, 0x0F, 0x84, 0x7F, 0x02, 0x00, 0x00 };

	class HookPreparePrologue final : public REL::IHookFn<PrepareBookText_t>
	{
	public:
		using value_type = std::decay_t<PrepareBookText_t>;

		HookPreparePrologue(std::string a_name, const REL::IId& a_id, const value_type& a_newFunc) :
			IHookFn(std::move(a_name), a_id, 0, a_newFunc)
		{
			InitData();
		}

		~HookPreparePrologue() noexcept override = default;

		HookPreparePrologue(const HookPreparePrologue&) = delete;
		HookPreparePrologue(HookPreparePrologue&&) noexcept = default;
		HookPreparePrologue& operator=(const HookPreparePrologue&) = delete;
		HookPreparePrologue& operator=(HookPreparePrologue&&) noexcept = default;

		[[nodiscard]] std::string_view GetTypeName() const noexcept override { return std::string_view{ "PreparePrologue9" }; }

		// Callable original: the 33-byte trampoline continuation built in
		// InitNewBytes. Armed into g_original before Enable.
		[[nodiscard]] value_type GetContinuation() const noexcept { return _continuation; }

		// Ruling P1: validate the captured live entry before Enable.
		[[nodiscard]] bool ValidateCapturedEntry() const noexcept
		{
			return _oldBytes.size() == kStolenBytes &&
				std::equal(_oldBytes.begin(), _oldBytes.end(), kExpectedEntry);
		}

		// Diagnostic-safe view for the mismatch log (never indexes empty).
		[[nodiscard]] std::string CapturedEntryHex() const
		{
			if (_oldBytes.size() != kStolenBytes) {
				return "<unavailable>";
			}
			std::string out;
			for (std::size_t i = 0; i < kStolenBytes; ++i) {
				out += std::format("{}{:02X}", i == 0 ? "" : " ", _oldBytes[i]);
			}
			return out;
		}

	protected:
		void InitNewBytes() override
		{
			// Entry: Jump5 -> thunk, then 4 NOPs so all 9 patched bytes are
			// defined (parent ruling P3).
			const auto assembly = REL::Asm::Jump5(
				this->_address,
				REL::GetTrampoline()->AllocateBranch5(this->_newFunc.GetAddress()));
			REL::WriteData(std::span(this->_newBytes).first(sizeof(assembly)), assembly);
			std::memset(this->_newBytes.data() + sizeof(assembly), 0x90, kStolenBytes - sizeof(assembly));

			// Continuation: contract layout, armed before Enable. test r8, r8
			// is restored; the short JE at offset 3 targets offset 19 (5+0x0E);
			// both absolute jumps are REL::Asm::Jump14 (FF 25 + inline u64).
			auto* continuation = std::bit_cast<std::uint8_t*>(_continuation);
			continuation[0] = 0x4D;  // test r8, r8
			continuation[1] = 0x85;
			continuation[2] = 0xC0;
			continuation[3] = 0x74;  // je +0x0E (rel8 from offset 5)
			continuation[4] = 0x0E;
			const REL::Asm::Jump14 jmpMain(this->_address + kStolenBytes);
			std::memcpy(continuation + 5, &jmpMain, sizeof(jmpMain));
			const REL::Asm::Jump14 jmpZero(this->_address + 0x288);
			std::memcpy(continuation + 19, &jmpZero, sizeof(jmpZero));
		}

	private:
		void InitData()
		{
			this->_size = kStolenBytes;
			this->_trampolineSize = kContinuationBytes;

			// The address must exist before IHookFn::Init validates _oldFunc;
			// the continuation bytes are filled in InitNewBytes once the live
			// entry has been captured.
			const auto continuation = REL::GetTrampoline()->Allocate(kContinuationBytes);
			const auto continuationAddr = std::bit_cast<std::uintptr_t>(continuation);
			_continuation = std::bit_cast<value_type>(continuationAddr);
			this->_oldFunc = continuationAddr;
		}

		value_type _continuation{ nullptr };
	};

	PrepareBookText_t*         g_original{ nullptr };
	F4DH::Core::ILogger*       g_log{ nullptr };
	const F4SE::TaskInterface* g_taskInterface{ nullptr };
	F4DH::Application::INoteSink* g_noteSink{ nullptr };
	bool                       g_verbose{ false };
	bool                       g_installed{ false };
	std::atomic<std::uint64_t> g_bookTextSeq{ 0 };

	// Allows nested original calls on the same thread without recursive
	// observation; reset on every exit (RAII, no exceptions cross the F4SE
	// ABI, and this stays correct even if that ever changes).
	struct ReentrancyGuard
	{
		explicit ReentrancyGuard(bool& a_flag) noexcept : flag(a_flag) { flag = true; }
		~ReentrancyGuard() noexcept { flag = false; }
		bool& flag;
	};

	void thunk(void* a_bookMenu, char* a_text, std::uint64_t a_length)
	{
		thread_local bool inObservation{ false };
		if (inObservation) {
			// Nested original call (PrepareBookText re-entered on this
			// thread): vanilla path only, no recursive observation.
			g_original(a_bookMenu, a_text, a_length);
			return;
		}
		const ReentrancyGuard guard{ inObservation };

		// Vanilla first — a failure below must never break BookMenu.
		g_original(a_bookMenu, a_text, a_length);

		if ((!g_verbose && !g_noteSink) || !a_text || a_length == 0) {
			return;
		}

		// Ruling P4: immediate raw formID extraction only. The slots name the
		// currently-targeted book/reference; both are read once, null-checked,
		// and reduced to formIDs. The NiPointer is never copied (get() is a
		// plain member read); no engine pointer is retained.
		std::uint32_t bookFormID = 0;
		{
			const REL::Relocation<RE::TESObjectBOOK**> slot(kTargetBookSlotId);
			const RE::TESObjectBOOK*                   rawBook = *slot;
			bookFormID = rawBook ? rawBook->formID : 0;
		}
		std::uint32_t refFormID = 0;
		{
			const REL::Relocation<RE::NiPointer<RE::TESObjectREFR>*> slot(kTargetRefSlotId);
			const RE::NiPointer<RE::TESObjectREFR>&                   ni = *slot;
			const RE::TESObjectREFR*                                  refr = ni.get();
			refFormID = refr ? refr->formID : 0;
		}

		// Bounded NUL scan over at most length+1 bytes (sanity-capped); fail
		// closed when no NUL exists in the span.
		const std::uint64_t span = std::min<std::uint64_t>(a_length + 1, kMaxScanSpan);
		std::uint64_t       finalLength = 0;
		bool                nulFound = false;
		for (std::uint64_t i = 0; i < span; ++i) {
			if (a_text[i] == '\0') {
				nulFound = true;
				finalLength = i;
				break;
			}
		}
		if (!nulFound) {
			g_log->Warn(std::format("note probe [bookText {}]: prepared buffer had no NUL within the bounded span (length={}) — observation dropped",
				g_bookTextSeq.load(std::memory_order_relaxed) + 1, a_length));
			return;
		}

		const bool truncated = finalLength > kMaxPreparedCopy;
		F4DH::Infrastructure::Notes::PreparedTextObservation obs;
		obs.seq = g_bookTextSeq.fetch_add(1, std::memory_order_relaxed) + 1;
		obs.fromThreadId = ::GetCurrentThreadId();
		obs.bookFormID = bookFormID;
		obs.refFormID = refFormID;
		obs.finalLength = finalLength;
		obs.truncated = truncated;
		obs.body.assign(a_text, static_cast<std::size_t>(std::min<std::uint64_t>(finalLength, kMaxPreparedCopy)));

		// Value-only deferred logging on the game thread; the only hook-path
		// allocations are this bounded copy and the task node (user-action
		// rate — BookMenu prepares happen per book open, never per frame).
		// shared_ptr because std::function (the AddTask target) requires a
		// copyable callable; the observation stays sole-owned by the capture.
		auto sharedObs = std::make_shared<F4DH::Infrastructure::Notes::PreparedTextObservation>(std::move(obs));
		g_taskInterface->AddTask([sharedObs]() mutable {
			if (g_noteSink) {
				if (auto event = F4DH::Infrastructure::Notes::NoteRecordResolver::ResolvePreparedText(*sharedObs, *g_log)) {
					g_noteSink->OnNoteRead(*event);
				}
			}
			if (g_verbose) F4DH::Infrastructure::Notes::NoteRecordResolver::LogPreparedBookText(std::move(*sharedObs), *g_log);
		});
	}
}

namespace F4DH::Infrastructure
{
	bool BookMenuTextHook::Install(Core::ILogger& a_log, bool a_verbose, Application::INoteSink* a_sink)
	{
		if (g_installed) {
			return true;  // resolved (installed or explicitly disabled)
		}

		// OG-only gate BEFORE any Address Library ID is touched (parent ruling
		// P7): on NG/AE the slot/function IDs are never resolved, so the
		// fork-hard missing-ID path cannot trigger there.
		if (!F4SE::IsRuntimeOnlyOG()) {
			g_installed = true;
			a_log.Info("note probe: BookMenu::PrepareBookText detour disabled — diagnostic is OG-only pending instruction-level verification on this runtime");
			return true;
		}

		auto& trampoline = REL::GetTrampoline();
		if (trampoline->IsEmpty()) {
			trampoline->Create(1 << 8);  // self-allocated; F4SE branch pool untouched
		}

		static HookPreparePrologue hook("PrepareBookText", kPrepareBookTextId, &thunk);
		if (!hook.Init()) {
			a_log.Error("note probe: PrepareBookText detour init failed — prepared-text probe disabled (dialogue/message/note event probes unaffected)");
			return false;
		}
		if (!hook.ValidateCapturedEntry()) {
			a_log.Error(std::format("note probe: PrepareBookText live entry mismatch — expected 4D 85 C0 0F 84 7F 02 00 00, captured {}; detour NOT enabled (possible wrong ID or patched binary)",
				hook.CapturedEntryHex()));
			return false;
		}

		// Arm the original before enabling so no intercepted call can route
		// into an unwritten continuation.
		g_original = hook.GetContinuation();
		if (!hook.Enable()) {
			g_original = nullptr;
			a_log.Error("note probe: PrepareBookText detour enable failed — prepared-text probe disabled (everything else unaffected)");
			return false;
		}

		g_taskInterface = F4SE::GetTaskInterface().get();
		g_log = &a_log;
		g_noteSink = a_sink;
		g_verbose = a_verbose;
		g_installed = true;
		a_log.Info(std::format("note probe: BookMenu::PrepareBookText detour installed (OG, Address Library id {}, 9-byte prologue, 33-byte continuation), verboseCapture={}",
			kPrepareBookTextId.GetId(F4SE::RUNTIME_LATEST_OG), a_verbose ? 1 : 0));
		return true;
	}
}
