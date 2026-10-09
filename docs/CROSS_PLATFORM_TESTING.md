# Windows and Apple Silicon test checklist

The pull-request workflow at `.github/workflows/pr-builds.yml` builds:

- **Windows x64**: CPU-only Whisper installer (`windows-x64-cpu-installer`).
- **macOS arm64**: native Apple Silicon, Metal-enabled DMG (`macos-arm64-metal-dmg`).

After a green run, go to **Actions → PR builds → selected run → Artifacts**
to download the installer or DMG. CI artifacts are kept for seven days.

## Before testing

These are **unsigned/unnotarized development artifacts**, without a bundled
Whisper model. They are not production releases. Only install builds from your
own repository after inspecting the corresponding commit and CI results.
On the Mac, Gatekeeper may require explicit approval under **System Settings →
Privacy & Security** after opening the app. Never disable Gatekeeper globally.

**Security note:** The workflow branch was created from `master`. If
[PR #1](https://github.com/rordev/voiceTyperD/pull/1) has not been merged,
this test build does not contain those privacy changes. Do not dictate
confidential text with it.

## Windows 11 + NVIDIA PC

1. Scan the installer with Microsoft Defender before installing.
2. Allow microphone access in Windows Settings as needed.
3. Pick a local Whisper model in voiceTyper Settings (the model is not bundled).
4. Leave optional LLM processing off during the initial test.
5. Dictate into Notepad, then a browser text field.
6. Check stop/start hotkey, voice-stop command, and whether existing clipboard
   contents are preserved.
7. Repeat using English, Ukrainian, and Russian; compare punctuation and
   language-switching behavior.
8. Review diagnostic log settings and any unexpected network connections.

This PR's Windows build is CPU-only. The existing release pipeline builds the
CUDA/Vulkan variants; runtime CUDA acceleration must be tested on a machine
with an NVIDIA GPU.

## M1 MacBook Pro

1. Copy the app from the DMG to Applications.
2. Approve opening the locally built, ad-hoc-signed app when macOS requests it.
3. Grant microphone, Accessibility, and Input Monitoring permissions if requested.
4. Configure a local Whisper model and choose the Metal backend when available.
5. Dictate into TextEdit, then a browser text field.
6. Verify start/stop hotkey, insertion into the originally focused app, and
   clipboard restoration.
7. Repeat with English, Ukrainian, and Russian.
8. Restart the app and verify the permissions and hotkeys still work.

## Acceptance criteria

- Both CI jobs succeed and upload their artifacts.
- Dictation works on each real device with LLM **off**.
- Text is pasted into the target application without unintended focus changes.
- No unexpected outbound calls during offline dictation.
- Sensitive recognized text is not saved in diagnostic logs once the privacy
  changes from PR #1 are included.
- macOS Metal and Windows CUDA work on the respective real hardware.

CI compilation does **not** test audio input, microphone permission prompts,
text insertion, GPU execution, or network privacy. Those require manual tests.

## Later: LM Studio over Tailscale

After local dictation works on both devices, configure optional LLM text
post-processing using the Windows RTX 5090 host over Tailscale. First validate
that only transcription text, not microphone audio, is sent and that loss of the
Tailscale connection falls back to raw local transcription. Restrict the LM
Studio server to trusted clients, enable authentication, and avoid public
exposure.
