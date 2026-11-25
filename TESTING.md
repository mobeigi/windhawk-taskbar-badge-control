# Validation

## Automated checks

Run `powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test`.
The checks exercise the actual source helpers with inert Windhawk API stubs:

- Rule normalization, exact matching, and both filtering modes.
- Native host-layout decoding and rejection of unsupported patterns.
- Retaining executable identities for windowless groups.
- Process-path cache hits and invalidation on app/window/PID changes.
- Process-query deduplication for groups with several windows.
- Coalescing pending button/container updates and returning to an empty queue.
- Including both badge mechanisms while excluding base icons and progress.

These do not execute the Explorer hooks or XAML event subscriptions.

## Runtime regression checklist

Use this checklist for release testing. Standalone checks and CI builds do not
establish that every Explorer lifecycle path works correctly:

1. With an existing badge visible and the mod disabled, enable it without moving
   the cursor. The badge should hide immediately. Disable and confirm restoration.
2. Repeat with blocklist and allowlist modes, including empty lists. Saving new
   rules should update existing buttons without hovering.
3. Test a desktop overlay and a Windows-managed badge. Trigger new unread counts
   and clear them while the application is unfocused.
4. Open and close windows, switch profiles, minimize an app to its tray, and
   reopen it. Test executable, full-path, and AppUserModelID rules.
5. Check combined and uncombined buttons, reordering, taskbar overflow, and
   recycled containers. A different app must not inherit a hidden badge.
6. Change taskbar icon size and Windows theme. Check that rebuilt templates keep
   filtering and that allowing an app restores its current badge state.
7. Test each monitor, disconnect/reconnect a monitor, and change the primary
   monitor. No taskbar interaction should be needed to discover new roots.
8. Restart Explorer with the mod enabled. Then disable/re-enable several times.
   Check cleanup, absence of crashes, and restoration of badge appearance.

If something fails, use Windhawk's built-in logging. Relevant messages include
the startup root/button counts, application policy changes, and HRESULT errors.
Review log output for personal paths before sharing it.

## Continuous integration

`.github/workflows/build.yml` builds and tests on a clean Windows x64 runner with
a pinned Windhawk 1.7.3 portable toolchain. The offline installer is verified
against its published SHA-256 before extraction. The mod is only compiled; it
is not installed in Explorer. Standalone tests use inert Windhawk API stubs.

Keep the **Build and test (Windows x64)** job name stable if it is selected as a
required pull-request check. Fork pull requests run without repository secrets,
with read-only repository permissions and without persisted checkout credentials.

## Performance comparison

Use the same apps, windows, monitors, and logging configuration for both builds.
Compare Explorer CPU time over a quiet interval, then during unread-count bursts,
taskbar hover, window creation, and rule changes. WPR/WPA or a sampling profiler
can distinguish mod work from the rest of Explorer.

Verify that steady-state idle has no scan timer, that each process identity is
queried only when needed, and that native events do not create a self-sustaining
message/layout loop. No numerical CPU or latency claim has been established by
the standalone tests.
