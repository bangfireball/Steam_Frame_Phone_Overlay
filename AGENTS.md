# PhoneCast VR Agent Instructions

Before changing this repository:

1. Read `design.md` completely.
2. Read `docs/development.md` and the documentation for the subsystem being changed.
3. Review `git status`, the current branch, and recent commits.
4. Preserve the Core/VR/platform boundaries and the long-term Steam Frame ARM64 target.
5. Work one sprint or explicitly requested task at a time.
6. Build and run relevant tests before reporting completion.
7. Do not mark hardware, headset, latency, or visual behavior complete merely because it compiles; record physical validation separately.
8. Never start or restart SteamVR through Windows RDP. Follow the physical-console warning in `docs/development.md`.
9. Do not commit credentials, `.env` files, Pi authentication, Telegram configuration, Android `local.properties`, build output, or device-specific secrets.

For a new machine or agent environment, follow `docs/new-machine-setup.md`.
