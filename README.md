# UbiBot Open Server

The device-facing backend and admin console for [UbiBot Open](https://github.com/ubibot-open) —
a Go API (device identity, data ingestion, SQLite storage) with a React/Ant Design admin console
embedded into it, built into a single self-contained binary. Part of a small multi-repo
ecosystem; see the [org profile](https://github.com/ubibot-open) for how it fits together with
the [WS1B firmware](https://github.com/ubibot-open/ubibot-ws1b), the
[serial debugging tool](https://github.com/ubibot-open/ubibot-serial-sync), the
[hardware-free device simulator](https://github.com/ubibot-open/ubibot-open-simulator), and the
[protocol/deployment docs](https://github.com/ubibot-open/ubibot-open-doc).

> This is an open-source, internal/educational-use IoT platform — not the commercial UbiBot
> platform, and not aiming for production-grade security or completeness. See the
> [protocol doc](https://github.com/ubibot-open/ubibot-open-doc/blob/main/protocol/hardware-communication-protocol.md#0-project-scope--rationale-for-this-revision)
> for what's deliberately out of scope (device auth signing, OTA, a general push channel, etc.) —
> the one exception is a narrow, ack-less command channel (reboot / change report interval,
> [protocol §9](https://github.com/ubibot-open/ubibot-open-doc/blob/main/protocol/hardware-communication-protocol.md#9-command-delivery-admin-triggered-optional)),
> queued from a device's detail page in the admin console.

## Repository layout

```
admin/        React + TypeScript + Ant Design admin console (Vite)
server/       Go backend — cmd/server is the entry point, internal/ holds the implementation
  cmd/        main.go: wiring, bootstrap, env vars
  internal/
    api/      HTTP handlers + router
    auth/     password hashing, admin sessions
    model/    data model (GORM structs)
    protocol/ device-facing protocol helpers
    store/    persistence (SQLite via GORM)
    webui/    embeds admin/'s build output into the Go binary
docs/         Internal design docs (protocol spec source of truth, feature backlog)
build.sh / build.ps1   Build the admin console, embed it, and compile the server binary
```

## Quick start

```bash
./build.sh        # Linux/macOS — requires Node.js/npm and Go 1.23+
# .\build.ps1      # Windows

./ubibot-server
# open http://localhost:8080
```

The API and admin console are served from the same address. On first run (no admin account in
the database yet), the log prints a one-time generated `admin` password — write it down, it's
only shown once. See the
[deployment guide](https://github.com/ubibot-open/ubibot-open-doc/blob/main/guides/deployment-flashing-guide.md#2-deploy-and-start-the-backend-ubibot-open-server)
for the full walkthrough, environment variables, and how to verify a device end to end.

## Testing without hardware

[ubibot-open-simulator](https://github.com/ubibot-open/ubibot-open-simulator) is a pure-C device
simulator, in its own repository, speaking the exact same HTTP protocol as the real WS1B firmware:

```bash
git clone https://github.com/ubibot-open/ubibot-open-simulator.git
cd ubibot-open-simulator
cmake -S . -B build && cmake --build build
./build/ub_device_sim --host 127.0.0.1 --port 8080
```

## Development

- **Backend**: `cd server && go build ./... && go test ./...`
- **Admin console** (dev server against a running backend): `cd admin && npm install && npm run dev`

## Documentation

- [Architecture Overview](https://github.com/ubibot-open/ubibot-open-doc/blob/main/architecture/overview.md)
  — how this repo fits together with the firmware and serial tool, plus this backend's own
  internal layout.
- [Hardware Communication Protocol](https://github.com/ubibot-open/ubibot-open-doc/blob/main/protocol/hardware-communication-protocol.md)
  — the device↔server HTTP protocol this backend implements.
- [Deployment, Flashing & Bring-up Guide](https://github.com/ubibot-open/ubibot-open-doc/blob/main/guides/deployment-flashing-guide.md)
  — deploying this backend alongside the firmware and serial tool.
- [Admin API Reference](https://github.com/ubibot-open/ubibot-open-doc/blob/main/api/admin-api.md)
  / [Open API Reference](https://github.com/ubibot-open/ubibot-open-doc/blob/main/api/open-api.md)
  — every `/api/admin/*` and `/api/open/v1/*` route this backend exposes.
- [docs/](docs/) in this repo — internal design docs (not user-facing): the protocol spec's
  source of truth and the feature backlog behind the admin console's roadmap.

## Contributing

See the [org-wide CONTRIBUTING.md](https://github.com/ubibot-open/.github/blob/main/CONTRIBUTING.md).

## License

[Apache License 2.0](LICENSE).
