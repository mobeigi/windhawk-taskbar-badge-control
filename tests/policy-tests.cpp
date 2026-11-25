// Compile with WH_EDITING: Windhawk API calls become inert editor stubs.
// Including the mod tests its actual policy helpers, not a second implementation.
#include "../taskbar-badge-control.wh.cpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

int g_checks = 0;

void Check(bool condition, const char* description) {
    ++g_checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(1);
    }
}

int RunTests() {
    BYTE prologue[] = {0x48, 0x83, 0xec, 0x28, 0x48, 0x83, 0xc1, 0x48};
    Check(DecodeHostElementOffsetX64(prologue, 8) == 0x48, "decode known x64 hosted-element offset");
    Check(!DecodeHostElementOffsetX64(prologue, 7), "reject truncated host prologue");
    prologue[3] = 0x38;
    Check(DecodeHostElementOffsetX64(prologue, 8) == 0x48, "x64 stack-frame size is independent of the field offset");
    prologue[7] = 0x10;
    Check(DecodeHostElementOffsetX64(prologue, 8) == 0x10, "decode alternative aligned host offset");
    prologue[7] = 0x11;
    Check(!DecodeHostElementOffsetX64(prologue, 8), "reject misaligned host offset");
    prologue[7] = 0;
    Check(!DecodeHostElementOffsetX64(prologue, 8), "reject zero host offset instead of guessing");
    prologue[7] = 0x48;
    prologue[0] = 0xe9;
    Check(!DecodeHostElementOffsetX64(prologue, 8), "reject unrecognized or detoured prologue");
    prologue[0] = 0x48;

    DWORD armPrologue[] = {0xD503237F, 0xA9BF7BFD, 0x910003FD, 0xF8410C08};
    auto armBytes = reinterpret_cast<BYTE const*>(armPrologue);
    Check(DecodeHostElementOffsetArm64(armBytes, 16) == 0x10, "decode known ARM64 hosted-element offset on any test host");
    Check(!DecodeHostElementOffsetArm64(armBytes, 15), "reject truncated ARM64 prologue");
    armPrologue[3] = 0xF8448C08;
    Check(DecodeHostElementOffsetArm64(armBytes, 16) == 0x48, "decode alternative ARM64 field offset");
    BYTE unaligned[17];
    std::memcpy(unaligned + 1, armPrologue, sizeof(armPrologue));
    Check(DecodeHostElementOffsetArm64(unaligned + 1, 16) == 0x48, "ARM64 byte decoding does not depend on input alignment");
    armPrologue[3] = 0xF8411C08;
    Check(!DecodeHostElementOffsetArm64(armBytes, 16), "reject misaligned ARM64 field offset");
    armPrologue[3] = 0xF8400C08;
    Check(!DecodeHostElementOffsetArm64(armBytes, 16), "reject zero ARM64 field offset");
    armPrologue[3] = 0xF8410C08;
    armPrologue[0] = 0xD503201F;
    Check(!DecodeHostElementOffsetArm64(armBytes, 16), "reject unsupported ARM64 instruction sequence");
    armPrologue[0] = 0xD503237F;
#if defined(_M_X64)
    Check(DecodeHostElementOffset(prologue, 8) == 0x48, "native x64 decoder dispatch");
#elif defined(_M_ARM64)
    Check(DecodeHostElementOffset(armBytes, 16) == 0x10, "native ARM64 decoder dispatch");
#endif

    Settings settings;
    settings.applications.insert(Normalize(L"chrome.exe"));
    Identity chrome{L"Chrome.Profile1", {LR"(C:\Program Files\Google\Chrome\Application\chrome.exe)"}};
    Identity other{L"Example.App!App", {LR"(C:\Apps\Example.exe)"}};
    Identity unknown;

    winrt::hstring stringId{L"Chrome.Profile1"};
    Identity converted;
    converted.appId = stringId;
    Check(converted.appId == L"Chrome.Profile1", "WinRT AppUserModelID conversion preserves text");
    Check(converted.known(), "WinRT identity conversion is known");
    Check(!unknown.known(), "empty identity is not known");

    Check(Normalize(L" \tChrome.EXE\r\n") == L"CHROME.EXE", "normalize case and outer whitespace");
    Check(Normalize(L" \t\r\n").empty(), "empty normalized rule");
    Check(Normalize(L" \u00e9\u00f6\u03c9\u044f.exe ") == L"\u00c9\u00d6\u03a9\u042f.EXE", "invariant Unicode matching across Latin Greek and Cyrillic");
    Settings unicodeRules;
    unicodeRules.applications = {Normalize(L"C:\\caf\u00e9\\process.exe")};
    Identity unicodePath{L"Example", {L"c:\\CAF\u00c9\\PROCESS.EXE"}};
    Check(ShouldHide(unicodePath, unicodeRules), "non-ASCII path matches case-insensitively");
    unicodeRules.applications = {Normalize(L"Editeur.\u00e9")};
    Check(ShouldHide(Identity{L"EDITEUR.\u00c9", {}}, unicodeRules), "non-ASCII application ID matches case-insensitively");
    Check(ShouldHide(chrome, settings), "blocklist executable name");
    Check(!ShouldHide(other, settings), "blocklist leaves other apps alone");
    Check(!ShouldHide(unknown, settings), "blocklist unknown identity fails open");

    settings.allowlist = true;
    Check(!ShouldHide(chrome, settings), "allowlist permits listed executable");
    Check(ShouldHide(other, settings), "allowlist blocks other identified apps");
    Check(!ShouldHide(unknown, settings), "allowlist unknown identity fails open");

    settings.allowlist = false;
    settings.applications = {Normalize(L"chrome")};
    Check(!ShouldHide(chrome, settings), "no partial executable matches");
    settings.applications = {Normalize(L"*.exe")};
    Check(!ShouldHide(chrome, settings), "wildcards are not interpreted");
    settings.applications = {Normalize(chrome.paths.front())};
    Check(ShouldHide(chrome, settings), "full path match");
    settings.applications = {Normalize(L" chrome.PROFILE1 ")};
    Check(ShouldHide(chrome, settings), "AppUserModelID match");

    Identity packaged{L"Example.App!App", {}};
    settings.applications = {Normalize(L"example.app!app")};
    Check(ShouldHide(packaged, settings), "packaged app without executable path");
    Identity multi{L"Group", {LR"(C:\Apps\Other.exe)", chrome.paths.front()}};
    settings.applications = {Normalize(L"chrome.exe")};
    Check(ShouldHide(multi, settings), "any matching group member matches the group");

    settings.applications.clear();
    Check(!ShouldHide(chrome, settings), "empty blocklist allows identified apps");
    settings.allowlist = true;
    Check(ShouldHide(chrome, settings), "empty allowlist blocks identified apps");
    Check(!ShouldHide(unknown, settings), "empty allowlist still fails open for unknown identity");

    ThreadState state;
    RememberIdentityPaths(state, chrome);
    Identity windowless{L"CHROME.PROFILE1", {}};
    RememberIdentityPaths(state, windowless);
    Check(windowless.paths == chrome.paths, "retain executable identity when group loses its windows");
    settings.allowlist = false;
    settings.applications = {Normalize(L"chrome.exe")};
    Check(ShouldHide(windowless, settings), "windowless known group remains blocked by executable rule");
    Identity unrelated{L"Other.Group", {}};
    RememberIdentityPaths(state, unrelated);
    Check(unrelated.paths.empty(), "never borrow paths from another app identity");
    Identity updated{L"Chrome.Profile1", {LR"(C:\Different\chrome.exe)"}};
    RememberIdentityPaths(state, updated);
    windowless.paths.clear();
    RememberIdentityPaths(state, windowless);
    Check(windowless.paths == updated.paths, "new observed executable path replaces cached path");
    Identity noAppId{L"", chrome.paths};
    auto cacheSize = state.identityPaths.size();
    RememberIdentityPaths(state, noAppId);
    Check(state.identityPaths.size() == cacheSize, "do not cache paths without an app ID");

    Check(NeedsProcessPath(Normalize(L"process.exe")), "executable rules need process paths");
    Check(NeedsProcessPath(Normalize(LR"(C:\Apps\process.exe)")), "full path rules need process paths");
    Check(!NeedsProcessPath(Normalize(L"Example.App!App")), "application ID rules avoid process queries");
    Check(!NeedsProcessPath(L""), "empty rule needs no process query");
    Settings appIdRules;
    appIdRules.applications = {Normalize(L"Example.App!App")};
    Check(!NeedsIdentityPaths(packaged, appIdRules), "known application IDs need no path lookup");
    Identity legacy;
    legacy.windows.push_back({reinterpret_cast<HWND>(static_cast<UINT_PTR>(1)), 1});
    Check(NeedsIdentityPaths(legacy, appIdRules), "legacy app without an AppUserModelID still resolves its executable");
    appIdRules.needsProcessPaths = true;
    Check(!NeedsIdentityPaths(packaged, appIdRules), "direct application ID match bypasses unrelated executable rules");
    Check(NeedsIdentityPaths(chrome, appIdRules), "non-matching application ID falls back to executable rules");
    Check(IsBadgeElementName(L"OverlayIcon"), "classic overlays are included");
    Check(IsBadgeElementName(L"BadgeControl"), "Windows-managed badges are included");
    Check(!IsBadgeElementName(L"Icon"), "base app icons are excluded");
    Check(!IsBadgeElementName(L"ProgressIndicator"), "progress indicators are excluded");

    auto window = reinterpret_cast<HWND>(static_cast<UINT_PTR>(0x101));
    auto secondWindow = reinterpret_cast<HWND>(static_cast<UINT_PTR>(0x102));
    int queries = 0;
    auto resolver = [&](WindowIdentity const& window) {
        ++queries;
        return L"C:\\Apps\\process" + std::to_wstring(window.process) + L".exe";
    };
    Identity observed{L"App", {}, {{window, 42}}};
    Check(ResolveIdentityPaths(observed, {}, false, resolver), "first process lookup succeeds");
    Check(queries == 1 && observed.paths.size() == 1, "first identity opens one process");
    Identity unchanged{L"App", {}, {{window, 42}}};
    Check(ResolveIdentityPaths(unchanged, observed, true, resolver), "unchanged snapshot reuses cache");
    Check(queries == 1 && unchanged.paths == observed.paths, "unchanged badge updates make no process query");

    Identity twoWindows{L"App", {}, {{window, 42}, {secondWindow, 42}}};
    ResolveIdentityPaths(twoWindows, observed, true, resolver);
    Check(queries == 2 && twoWindows.paths.size() == 1, "two windows in one process need one lookup on membership change");
    Identity reordered{L"App", {}, {{secondWindow, 42}, {window, 42}}};
    ResolveIdentityPaths(reordered, twoWindows, true, resolver);
    Check(queries == 2 && reordered.paths == twoWindows.paths, "window order changes do not invalidate membership cache");
    Identity changedProcess{L"App", {}, {{window, 43}}};
    ResolveIdentityPaths(changedProcess, observed, true, resolver);
    Check(queries == 3 && changedProcess.paths != observed.paths, "HWND reuse with a different PID invalidates cache");
    Identity rebound{L"Different.App", {}, {{window, 42}}};
    ResolveIdentityPaths(rebound, observed, true, resolver);
    Check(queries == 4, "rebinding application identity invalidates cache");
    Identity forced{L"App", {}, {{window, 42}}};
    ResolveIdentityPaths(forced, observed, false, resolver);
    Check(queries == 5, "realized-container reset forces a fresh process lookup");
    Identity failed{L"App", {}, {{window, 42}}};
    Check(!ResolveIdentityPaths(failed, {}, false, [](auto const&) { return std::wstring{}; }),
          "failed process lookup is not marked resolved");
    Check(ResolveIdentityPaths(failed, {}, false, resolver) && queries == 6,
          "failed lookup can be retried on a later event");
    Identity mixed{L"App", {}, {{window, 42}, {secondWindow, 43}}};
    ResolveIdentityPaths(mixed, {}, false, resolver);
    Check(queries == 8 && mixed.paths.size() == 2, "mixed-process group resolves each process once");
    Identity noWindows{L"App", {}};
    Check(ResolveIdentityPaths(noWindows, observed, true, resolver) && queries == 8,
          "windowless identity makes no process query");

    PendingWork work;
    Check(!work.HasWork(), "idle work queue is empty");
    work.posted = true;
    Check(!work.HasWork(), "a previously posted message does not manufacture work");
    work.discovery = true;
    Check(work.HasWork(), "lifecycle event schedules discovery");
    work = {};
    work.all = true;
    Check(work.HasWork(), "settings event schedules all tracked buttons");
    work = {};
    auto key = reinterpret_cast<void*>(static_cast<UINT_PTR>(0x201));
    work.buttons.insert(key);
    work.buttons.insert(key);
    Check(work.HasWork() && work.buttons.size() == 1, "repeated updates coalesce per button");
    work = {};
    work.subtrees[key] = {};
    work.subtrees[key] = {};
    Check(work.HasWork() && work.subtrees.size() == 1, "repeated container updates coalesce per subtree");
    work = {};
    Check(!work.HasWork() && !work.posted, "drained queue returns to idle");

    Mask snapshot;
    Data::Binding noBinding{nullptr};
    auto zero = winrt::box_value(0.0);
    auto one = winrt::box_value(1.0);
    Check(UpdateMaskSnapshot(snapshot, one, noBinding, true), "capture the initial native opacity");
    Check(!UpdateMaskSnapshot(snapshot, zero, noBinding), "do not capture the mod-owned local zero during an animation");
    Check(winrt::unbox_value<double>(snapshot.originalLocal) == 1.0, "effective-value notification preserves original opacity");
    Check(!UpdateMaskSnapshot(snapshot, zero, noBinding), "animation ending at local zero also preserves the original");
    Check(UpdateMaskSnapshot(snapshot, winrt::box_value(0.25), noBinding), "a genuine new local opacity replaces the snapshot");
    Check(winrt::unbox_value<double>(snapshot.originalLocal) == 0.25, "save the latest native base opacity");
    Check(UpdateMaskSnapshot(snapshot, nullptr, noBinding), "cleared local value replaces the snapshot");
    Check(!snapshot.originalLocal, "cleared snapshot remains unset");
    Check(UpdateMaskSnapshot(snapshot, zero, noBinding, true), "an initially zero native opacity is a legitimate original");
    Check(winrt::unbox_value<double>(snapshot.originalLocal) == 0.0, "preserve an initially hidden native badge");
    Check(!IsMaskOpacity(winrt::box_value(L"binding-like value")), "non-numeric local values are not mistaken for the masking zero");

    bool previousFailure = g_cleanupFailed.load();
    int cleanups = 0;
    Check(!CleanupAction(L"injected test failure", [] { throw std::runtime_error("test"); }), "cleanup exceptions do not escape");
    Check(g_cleanupFailed, "cleanup failure is recorded instead of silently discarded");
    Check(CleanupAction(L"following cleanup", [&] { ++cleanups; }), "later cleanup still runs after a failure");
    Check(cleanups == 1, "remaining resources are not skipped after a cleanup exception");
    g_cleanupFailed = previousFailure;

    RestoreAll(state);
    Check(state.cleanupComplete && state.identityPaths.empty(), "controlled cleanup releases state and records completion");
    auto emptyState = std::make_shared<ThreadState>();
    (*g_states)[123] = emptyState;
    Check(StatesSnapshot().size() == 1, "state snapshot works with explicit-lifetime container");
    RestoreAll(*emptyState);
    g_states.reset();
    Check(StatesSnapshot().empty(), "state lookup is safe after explicit container reset");
    g_states.emplace();

    std::printf("All %d checks passed.\n", g_checks);
    return 0;
}

int main() {
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        int result = RunTests();
        winrt::uninit_apartment();
        return result;
    } catch (winrt::hresult_error const& error) {
        std::fprintf(stderr, "WinRT test failure: %08X\n", static_cast<unsigned>(error.code().value));
        return 1;
    }
}
