# UbiBot Smart IoT Open Platform Admin Console Feature List

Neither the old scaffold (`platform/ubibot-open-server`) nor the existing in-memory protocol
reference server (`server/internal`) is used as a foundation any more; the whole project
(including the device protocol server) is being built from scratch. This list no longer marks
"implemented or not" — instead it marks a **build priority**, to serve as the scheduling basis
for building from zero. [Hardware Communication Protocol](hardware-communication-protocol.md) is
kept as the design contract for device-side communication, and the new server implements it.

What the priorities mean:
- **P0 (Phase 1 / MVP loop)**: the prerequisites for getting the main path "device reports data →
  admin console can see it → admin console can dispatch a command and have it take effect"
  working end to end; missing any one of them means nothing runs.
- **P1 (Phase 2 / common capabilities)**: hard requirements for the vast majority of IoT
  scenarios, to be filled in right after the MVP is running.
- **P2 (Phase 3 / operations & experience)**: makes the platform pleasant to use and operate, but
  doesn't affect whether the core path works.
- **P3 💡 (Phase 4 / optional advanced)**: whether to build these depends on the product
  positioning; many can be left to a commercial edition or later iterations.

## 1. System Management (admin console foundation)

| Feature | Description | Priority |
| --- | --- | --- |
| Admin account management | Login, create/read/update/delete, password reset | P0 |
| Role/permission management (RBAC) | Menu tree, button/API-level permissions | P1 (a single super admin is enough to get Phase 1 running) |
| Customer (end-user) account management | Separate from the admin account system | P1 |
| Operation audit log | Operator / module / action / target object / IP | P1 |
| Login security | CAPTCHA, lockout after failed attempts, forced logout | P1 |
| Data dictionary, parameter configuration | Reusable for dropdown options and system parameters | P2 |
| File management | Upload, directories, public share links | P2 |
| In-app messages / notification center | System notifications, read status, real-time push | P2 |
| Scheduled job scheduling | Cron jobs, retry on failure, execution logs | P2 |
| Organization/department management | Multi-level organizational structure | P3 💡 |

## 2. Products & Device Identity

| Feature | Description | Priority |
| --- | --- | --- |
| Device triplet issuance mechanism | pid/sn/secret + HMAC-SHA256 signature, burned in at the factory, never sent back in plain text | P0 |
| Device registration & persistent storage | Device identity, keys, and status persisted to the database (previously an in-memory map, lost on restart) | P0 |
| Device list/detail/CRUD | The admin console can view and manage devices | P0 |
| Latest device data snapshot | The detail page shows the most recent report | P0 |
| Device status management (not activated / activated / disabled / deregistered) | | P1 |
| Online status determination | Infer online/offline from the last report time + report interval | P1 |
| Batch device generation/import/export | Issue triplets in bulk per production batch | P1 |
| Product/model (Product) management | Define a device class's default thing model, probe templates, and protocol parameters | P1 |
| Device groups/tags, geolocation, ownership transfer | | P2 |

## 3. Device Access & Communication

| Feature | Description | Priority |
| --- | --- | --- |
| Time sync + HMAC-signature activation | POST /auth/time, /auth/activate; two paths: single-use nonce and ±5-minute window | P0 |
| Token authentication & renewal | 24h validity, X-Token-Expires-In | P0 |
| Strict did-token binding check | Prevents one device from impersonating another when reporting | P0 (a security baseline — get it right in Phase 1, don't leave it for Phase 2) |
| Data report endpoint (batched recs + ack) | | P0 |
| (did, ts) deduplication | Prevents dirty data from duplicate reports | P0 |
| Command dispatch channel + admin-side trigger | cfg/cmd carried in the response; **there must also be an entry point in the admin console that can actually issue a command** — a channel with no entry point is as good as not having done it | P0 |
| Request rate limiting (429) | | P1 |
| Monotonically increasing ts for replay protection | Hardens the ±5-minute-window path against captured-and-replayed requests | P1 |
| Config polling endpoint (GET, no data report needed) | | P2 |
| MQTT access | High-frequency / low-latency scenarios | P2 |
| Connection count / throughput monitoring | | P2 |
| CoAP/LwM2M access, gateway/sub-device proxying | For cellular/NB-IoT or Zigbee gateway scenarios | P3 💡 |

## 4. Thing Model & Data Collection

| Feature | Description | Priority |
| --- | --- | --- |
| Sensor reading reports (including composite sensors such as NPK) | | P0 |
| Collection/report interval configuration (cfg.ci/ui) | | P0 |
| Historical data storage | Time-series data persisted to the database — the prerequisite for "the admin console has data to look at" | P0 |
| Enabled-field trimming (cfg.fe) | | P1 |
| Custom probe data read configuration (set_probe) | RS485/Modbus registers, data types, byte order, linear conversion, see protocol §7.2; includes ack/nak feedback on the execution result | P1 (get the core path working first; probe configuration is the first capability to land in Phase 2) |
| Historical data query/charts | Query by device + field + time range | P1 |
| Thing model (properties/events/services) definition | Standardized product capabilities, reusable by the UI and rules engine | P2 |
| Data downsampling/archiving, data export | | P2 |

## 5. Command Dispatch & Remote Control

| Feature | Description | Priority |
| --- | --- | --- |
| Command queue dispatch + ack confirmation | | P0 |
| nak failure feedback | Per-command failure reasons reported back | P1 |
| Command management admin page | Visual dispatch, view history and execution status | P1 |
| Actual business logic for remote reboot/calibration | Currently just a `tp` string; needs to be bound to real actions | P1 |
| Batch command dispatch | Dispatch in bulk by group/filter criteria | P2 |
| OTA firmware updates | Version management, staged rollout, progress reporting | P2 |

## 6. Rules Engine & Alerts

| Feature | Description | Priority |
| --- | --- | --- |
| Threshold alerts | Sensor value out of range | P1 |
| Offline alerts | No report within the expected report interval | P1 |
| Probe fault alerts | Device-side faults such as a failed set_probe execution (nak) | P1 |
| Alert records & handling workflow | List, acknowledge / handle / close | P1 |
| Alert notification channels | In-app messages first; SMS/email/Webhook can come later | P1 (in-app messages) / P2 (SMS, email, Webhook) |
| Data flow rules (forward to MQ/Webhook, trigger other devices) | | P2 |
| Scene linkage / automation | Device-to-device linkage | P3 💡 |

## 7. Visualization & Dashboards

| Feature | Description | Priority |
| --- | --- | --- |
| Device real-time data cards, historical charts | | P1 |
| Admin console home Dashboard | | P1 |
| Device map, statistical reports | | P2 |
| Custom dashboards | Assemble chart widgets by drag and drop | P3 💡 |

## 8. Open Capabilities (Open Platform / API)

| Feature | Description | Priority |
| --- | --- | --- |
| Device-side C SDK | HMAC/JSON/protocol encoding and decoding, rewritten together with the protocol server | P0 |
| Admin management API | | P0 |
| OpenAPI documentation, API Key management | For third-party developers | P2 |
| Webhook subscriptions | | P2 |
| Third-party platform integrations (Home Assistant, DingTalk, WeCom, etc.) | | P3 💡 |

## 9. Multi-tenancy & Commercialization

| Feature | Description | Priority |
| --- | --- | --- |
| Per-customer data isolation | Devices belong to a customer | P1 (can be raised to P0 if the product is positioned for multiple customers from the start — confirm against the actual positioning) |
| Per-project/space isolation | | P3 💡 |
| Plan/quota management, usage statistics & billing | | P3 💡 |

## 10. Security & Operations

| Feature | Description | Priority |
| --- | --- | --- |
| Device identity authentication (HMAC + temporary token) | | P0 |
| Transport-layer recommendation (HTTPS preferred) | | P0 (already defined in the docs; follow it when implementing) |
| Data backup & recovery | Plan for this early once the database goes live | P1 |
| Operation audit log, centralized logging | | P1 |
| System monitoring dashboard (online devices / QPS / error rate) | | P2 |
| Disaster recovery / active-active deployment | | P3 💡 |

---

## Recommended Build Roadmap

**Phase 1 (P0, MVP loop)**: rewrite the device protocol server (authentication, replay
protection, did-token binding, data reporting, deduplication) + supporting persistent storage
(device identity / tokens / historical data) + the device-side C SDK + a minimal admin console
(login, device list/detail, manually dispatch one command and see its ack). Only once this phase
is working does the main line "devices can connect, the admin console can see them, and commands
can be dispatched" truly exist.

**Phase 2 (P1, fill in common capabilities)**: custom probe data reads (set_probe landed + nak
feedback), historical data query, online status determination, threshold/offline alerts, RBAC and
operation auditing, the command management page, request rate limiting.

**Phase 3 (P2, operations & experience)**: dashboards/reports, OTA, message center and scheduled
jobs, the open API, supporting system-management features such as files/dictionary/parameters,
system monitoring.

**Phase 4 (P3, optional advanced)**: multi-tenant billing, scene linkage, CoAP/gateway
sub-devices, third-party integrations, disaster recovery / active-active — decide whether to
invest in these based on the product positioning.
