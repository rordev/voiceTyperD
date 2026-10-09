# LM Studio over Tailscale (Windows host + M1 Mac client)

This setup sends **recognized text only**, not raw microphone audio, to
LM Studio on the Windows RTX 5090. Before enabling remote processing, verify
both Windows and Apple Silicon checks on this pull request are green; a passing
endpoint-policy unit test alone does not validate the desktop builds. Speech recognition remains local through
whisper.cpp (Metal on an Apple Silicon Mac, CUDA on Windows). LLM processing
is optional and defaults to **off**. If LM Studio is unavailable, the original
dictated text is pasted unchanged.

## Windows (LM Studio host)

1. Install and sign into Tailscale on the Windows machine and the Mac, in the
   same trusted tailnet.
2. On Windows, run `tailscale ip -4` (or look in the Tailscale app). Copy
   **that machine's actual** 100.64.0.0/10 address. Do not guess it.
3. In LM Studio (version 0.4.0 or later), load a text model for formatting.
   Open the Developer tab, start the API server on port 1234 (or your chosen
   port), and enable **Serve on Local Network**.
4. Enable **Require Authentication** in Server Settings. Create a token with
   the least privileges necessary to perform inference. Keep the token private;
   do not put it in GitHub, logs, screenshots, or URLs.
5. Restrict the Windows firewall and the Tailscale tailnet access controls so
   **only the Mac and other explicitly trusted devices** can reach the server
   port. Verify that other local-network devices cannot reach it. Serving on
   Local Network otherwise exposes the port outside Tailscale too.
6. Keep MCP/tool access disabled for the LLM server unless you specifically
   need and trust those capabilities. Do not enable public port forwarding.

## Mac (voiceTyper client)

1. Run Tailscale and confirm you can reach the Windows Tailscale IP.
2. Keep voiceTyper's Whisper backend local (Metal) and test dictation first.
3. In voiceTyper's LLM settings, use:
   `http://<ACTUAL-WINDOWS-TAILSCALE-IP>:1234/v1/chat/completions`
   (replace the placeholder, including angle brackets).
4. Enter the LM Studio model identifier and API token, then use the
   built-in **Test** button with **non-sensitive** text.
5. Enable optional LLM processing only after the test succeeds. When turned
   off, transcription stays on-device.

**Important:** The current app stores an API key entered into its settings
without OS keychain encryption. Anyone with access to the local account's
settings may be able to recover it. Prefer a limited, revocable token; never
reuse a personal or corporate credential. OS keychain integration is future work.

## Why the endpoint restrictions?

The security policy accepts only numeric loopback addresses and numeric
Tailscale IPv4 addresses in `100.64.0.0/10`, with an explicit port and the
`/v1/chat/completions` path. It rejects DNS names (including MagicDNS),
ordinary LAN IPs, embedded credentials, query strings, and URL fragments.
This intentionally breaks general cloud-LLM endpoints and other provider
paths for now. Requests bypass HTTP proxies and never follow HTTP redirects.

This is **defense in depth, not proof of a Tailscale tunnel**. Other networks
can use the CGNAT range. Confirm that the destination matches the IP assigned
to your Windows device by Tailscale; enforce tailnet ACLs and Windows firewall
rules, and test that the connection fails when Tailscale is disconnected.
HTTP inside a correctly configured Tailscale tunnel is encrypted by
Tailscale, but the destination LM Studio process still sees the plaintext.
If LM Studio binds to 0.0.0.0, its service may be visible on non-Tailscale
interfaces unless blocked separately.

## Expected fallback

When LLM processing is off, the client doesn't send transcription text to
LM Studio. When it is on and the endpoint is invalid, unreachable, returns an
error, or times out, voiceTyper pastes the unmodified local transcription.
Do not expect an LLM rewrite while disconnected.
