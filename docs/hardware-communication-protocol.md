# UbiBot Open Platform Hardware Communication Protocol

### 0. Project Scope & Rationale for This Revision

This project (ubibot-open) is an open-source IoT platform implementation meant for internal use —
it is not a commercial platform aiming for completeness, reliability, or production-grade
security. The primary goal is: let a user connect a piece of hardware to the system and see data
with as few steps and as little configuration as possible. Any complex auth flow, state machine,
or command-dispatch channel that isn't strictly necessary for the core "get data flowing" path has
been stripped out wherever possible.

Compared to earlier versions, this revision:

- Targets a trusted internal-network environment, so device identity authentication has also been
  simplified away: there's no more DeviceSecret or signature computation — a device identifies
  itself with a plain-text pid+sn, with no need to pre-create the device on the platform by hand or
  go through a "self-activation approval / key binding" flow. Power it on, get it online, and it
  can report data.
- Removes the separate "activation" step (device activation / token session): a data-upload
  request identifies itself with pid+sn directly and completes the write in one request. A
  lightweight time-sync endpoint that performs no checks at all is kept, so a device with no local
  clock can fetch the current time.
- Removes the command-dispatch channel: the platform no longer pushes control commands such as
  reboot / calibrate / set_probe / OTA to devices. These features aren't necessary for the core
  "get data flowing" goal and are rarely needed in teaching/demo scenarios.
- Uploaded data no longer uses named fields like `temperature`/`humidity` — it's unified into
  `field1` – `field20` (20 max), which the platform just stores as-is without caring about their
  business meaning. `field1`/`field2`/`field3` carry a conventional default meaning
  (temperature/humidity/light); the rest are defined by the user.

The rest of this document describes the new, simplified protocol.

### 1. Bluetooth Provisioning (first-time network setup)

A device fresh from the factory or after a factory reset has no usable local WiFi configuration,
so it must first be provisioned over Bluetooth (BLE) to join WiFi before it can report data using
the HTTP protocol described in the later sections of this document. The provisioning flow happens
only between the App and the device; it does not go through the server.

**Flow**:

1. When the device powers on for the first time (or its locally saved WiFi connection fails / has
   been cleared), it automatically enters "provisioning mode": it starts BLE advertising, and the
   device name should include part of the sn (e.g. `UBIBOT-<last 6 characters of sn>`) so the App
   can tell which specific device it is in the scan list. Whether provisioning mode can also be
   triggered manually (e.g. by long-pressing a button) is up to the specific firmware
   implementation and is not mandated by this protocol.
2. The App scans for devices in provisioning mode (it may filter by the Service UUID defined in
   §1.1 or by device-name prefix) and lists them for the user to choose from.
3. Once the user selects a device, the App initiates a GATT connection.
4. After the connection is established, the App first reads the "Device Info" characteristic to
   confirm the pid/sn matches what the user expects.
5. The App writes the WiFi SSID and password to the "WiFi Config" characteristic.
6. The device tries to connect to that WiFi network, continuously pushing its current state
   (connecting / success / failure and the reason) to the App via Notify on the "Provisioning
   Status" characteristic.
7. On success: the device saves the WiFi configuration to local flash, exits provisioning mode
   (stops BLE advertising), disconnects BLE, and switches to normal networked operation —
   communicating with the server as described in §4 Time Sync and §5 Data Upload.
8. On failure: the device keeps the BLE connection and stays in provisioning mode, waiting for the
   App to write a new WiFi configuration and retry (see the `reason` field in §1.4 for common
   failure reasons).

#### 1.1 GATT Definition

The Service and Characteristics just use custom 128-bit UUIDs; they don't need to be registered
with the Bluetooth SIG. The UUIDs below are examples only — each project/product line should
generate its own and keep them consistent:

| Name | UUID (example) | Property | Description |
| --- | --- | --- | --- |
| Provisioning Service (Service) | 0000FF00-0000-1000-8000-00805F9B34FB | - | All provisioning-related characteristics live under this Service |
| Device Info (Characteristic) | 0000FF01-0000-1000-8000-00805F9B34FB | Read | Device identity information, see 1.2 |
| WiFi Config (Characteristic) | 0000FF02-0000-1000-8000-00805F9B34FB | Write | The App writes the WiFi connection info, see 1.3 |
| Provisioning Status (Characteristic) | 0000FF03-0000-1000-8000-00805F9B34FB | Notify | The device reports provisioning progress/result, see 1.4 |

Every characteristic uses UTF-8-encoded JSON as its data format. A single BLE ATT payload is
limited by the MTU (only 23 bytes by default; commonly 185–512 bytes after negotiation), so an
implementation should request an MTU negotiation after connecting, large enough to hold the JSON
below (200 bytes or less is generally enough). This protocol does not define a fragmentation
scheme for oversized content; anything that exceeds a single MTU is outside its scope.

#### 1.2 Device Info (Read)

```json
{ "pid": "ubibot_open_dev_v1", "sn": "sn_ws1_20001_1" }
```

| Field | Description |
| --- | --- |
| pid | Product model |
| sn | Device serial number |

#### 1.3 WiFi Config (Write)

```json
{ "ssid": "MyHomeWiFi", "password": "12345678" }
```

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| ssid | string | Yes | WiFi network name |
| password | string | No | WiFi password; may be omitted or sent as an empty string for an open (passwordless) network |

Note: this protocol applies no application-layer encryption to this data — the WiFi password is
sent as plain-text JSON over a BLE Write, relying on the physical proximity required by the BLE
connection itself as basic protection, consistent with the overall scope in §0 (a trusted
internal-network scenario, not aiming for production-grade security). If the device's BLE stack
supports pairing encryption (Bonding / LE Secure Connections), enabling it is recommended, but it
is not required by this protocol.

#### 1.4 Provisioning Status (Notify)

```json
{ "status": "connecting" }
```

```json
{ "status": "failed", "reason": "wrong_password" }
```

| Field | Description |
| --- | --- |
| status | See the table below |
| reason | Only meaningful when status=failed, see the table below; omitted in all other cases |

| status | Description |
| --- | --- |
| connecting | WiFi config received; trying to connect |
| success | WiFi connected; the device is about to disconnect BLE and exit provisioning mode |
| failed | WiFi connection failed; see `reason` for the specific cause |

| reason | Description |
| --- | --- |
| wrong_password | Wrong password |
| ap_not_found | No WiFi access point with the specified SSID was found |
| timeout | Connection timed out (weak signal, unresponsive router, etc.) |
| unknown | Any other uncategorized error |

### 2. Transport

| Item | Requirement |
| --- | --- |
| Protocol | HTTP |
| Method | POST |
| Data format | JSON, UTF-8 |
| Content-Type | application/json |

Note: this protocol performs no authentication whatsoever — it fully trusts the pid+sn in the
request. It's only suitable for a trusted internal-network environment; do not apply this scheme
directly to a deployment that needs real security or is exposed on the public internet.

### 3. Device Identity

A device identifies itself with just two fields, both sent in plain text, with no key/signature
required:

| Field | Description |
| --- | --- |
| pid (ProductID) | Product model, used to distinguish device types |
| sn (SerialNumber) | The device's unique serial number |

### 4. Time Sync

When a device has no local clock (first power-on, or an RTC reset from power loss), it can call
this endpoint to fetch the server's current time. This endpoint performs no identity check at all
— no signature required or verified.

**POST /api/v1/auth/time**

Request:

```json
{ "pid": "ubibot_open_dev_v1", "sn": "sn_ws1_20001_1"}
```

Response:

```json
{ "c": 0, "t": 1788950400 }
```

| Field | Description |
| --- | --- |
| t | Server's current Unix timestamp, in seconds |

### 5. Data Upload (the only device-facing endpoint)

**POST /api/v1/data/report**

Request body: a single report can carry multiple timestamped readings (e.g. several rounds of
data buffered while the device was offline):

```json
{
  "pid": "ubibot_open_dev_v1",
  "sn": "sn_ws1_20001_1",
  "ts": 1788950400,
  "payloads": [
    { "ts": 1788950400, "field1": 25.6, "field2": 60.2 },
    { "ts": 1788951000, "field1": 25.8, "field2": 59.9, "field3": 812 }
  ]
}
```

| Field | Type | Required | Description |
| --- | --- | --- | --- |
| pid | string | Yes | Product model |
| sn | string | Yes | Device serial number — the sole basis for identifying the device (no signature check) |
| ts | Int64 | Yes | Unix timestamp (seconds) when this request was made, used for a basic time-window check (optional check, see §8) |
| payloads | array | Yes | One or more records. Typically one record corresponds to one sample point and carries every field for that moment; it's also fine to split a single sampling round across multiple records, each carrying only one or a few fields (see "Merging by time window" below) — both styles are handled equivalently by the server |
| payloads[].ts | Int64 | Yes | Unix timestamp (seconds) for this sample point (may be earlier than the outer `ts`, for backfilling data buffered while offline) |
| payloads[].field1~field20 | number | Yes | `field1`~`field20` -> numeric value, see §6. Omit unused field numbers entirely — don't send an empty value |

**Merging by time window**: it is not required that "one record = one complete sample point" —
fields from the same sampling round may be split across multiple records (e.g. a sensor read
sequentially, with each field's `ts` off by a few seconds). The server groups `payloads` by `ts`:
after sorting, it starts a new group at the first ungrouped record, and any subsequent record
whose `ts` is within 60 seconds of that group's anchor (the first record's `ts` in the group)
joins the same group; anything more than 60 seconds past the anchor is treated as the next,
independent round of sampling and starts a new group. Records within one group are merged and
saved as a single data point (`ts` is the group's anchor; if the same field number appears more
than once within a group, the last occurrence wins). **Anything more than 60 seconds apart is
never merged**, even if it's only a few seconds past the previous group — each round is its own
independent data point, and being in the same request doesn't mean records can be blindly merged
into one.

For example, take the following request (a common real-hardware pattern — one field per record —
where two sampling rounds are far apart in time):

```json
{
  "pid": "ubibot-ws1b",
  "sn": "RV41554WS1B",
  "ts": 1514767409,
  "payloads": [
    { "ts": 1514767395, "field1": 29.291221618652344 },
    { "ts": 1514767395, "field2": 40.831615447998047 },
    { "ts": 1514767395, "field3": 439.95001220703125 },
    { "ts": 1514767395, "field4": 0.014166667126119137 },
    { "ts": 1514767395, "field6": 26.9375 },
    { "ts": 1514767395, "field7": 27.625 },
    { "ts": 1514767409, "field5": -56 }
  ]
}
```

The largest `ts` gap across these 7 records is 1514767409 - 1514767395 = 14 seconds, within the
60-second window, so they're all merged and saved as **1 data point** (`ts=1514767395`, containing
7 fields: field1–field4, field6, field7, field5).

Now compare that to two sampling rounds spaced further apart (the second one 2 minutes after the
first):

```json
{
  "pid": "ubibot-ws1b",
  "sn": "RV41554WS1B",
  "ts": 1514767515,
  "payloads": [
    { "ts": 1514767395, "field1": 29.29, "field2": 40.83 },
    { "ts": 1514767515, "field1": 29.31, "field2": 40.79 }
  ]
}
```

The two `ts` values are 120 seconds apart, past the 60-second merge window, so they're saved as
**2 separate data points** (`ts=1514767395` and `ts=1514767515` each on their own) rather than
merged into one.

Response:

```json
{ "c": 0, "t": 1788950400 }
```

| Field | Description |
| --- | --- |
| c | Business status code — 0 for success, see §8 for other values |
| t | Server's current Unix timestamp (seconds); the device may use it to calibrate its local clock (optional, not required) |

Server behavior:
- If `sn` has never been seen before, a device record is created automatically (pid/sn/first-report
  time, etc.); afterward it's treated as an existing device.
- If a device has been manually disabled by an admin in the admin console, this endpoint rejects
  all of that device's data (see §8 — disabling is an admin action on an existing device, not a
  precondition for a new device to be onboarded).

### 6. Data Fields (field1 ~ field20)

Named sensor fields like `temperature`/`humidity` are no longer used — everything is unified into
numbered fields, up to 20. The platform stores the numeric value under each number as-is and
doesn't care what physical quantity each number represents, except for these 3 fields which carry
a conventional default meaning:

| Field | Default meaning |
| --- | --- |
| field1 | Temperature |
| field2 | Humidity |
| field3 | Light level |
| field4 ~ field20 | Defined entirely by the user (e.g. CO2, soil pH, battery voltage, etc.) — the platform makes no assumptions |

This default meaning is only a convention used for admin-console display (e.g. icon, unit) — it
is not enforced at the protocol level. A device is free to report only `field1`, or start from
`field4` onward; the platform saves whatever it's given either way.

### 7. Device Management (admin-console side, not part of the device-facing protocol)

The following is not something a device needs to implement — it's a description of admin-console
behavior, to help make sense of the "auto-create/disable" behavior mentioned in §5:

- A device automatically appears in the admin console's device list the first time it
  successfully reports data — no need to create it beforehand.
- An admin can rename a device, view its historical data, disable/re-enable it, or delete it along
  with all of its data from the admin console.
- Once a device is disabled, all of its subsequent report requests are rejected (data is no
  longer processed); re-enabling it restores normal behavior.
- Deleting a device permanently deletes it along with all of its historical data.

### 8. Error Handling

| HTTP status | c | Scenario | Device behavior |
| --- | --- | --- | --- |
| 200 | 0 | Success | Process normally |
| 400 | 1002 | Timestamp outside the ±5-minute window | Check the local clock, or call the §4 time-sync endpoint to calibrate and retry |
| 400 | 1003 | Malformed request body | Check the firmware's serialization logic; retry next cycle |
| 401 | 1103 | Device has been disabled by an admin | Stop retrying and raise an alert (LED/log); needs manual review |
| 429 | 1900 | Rate limited | Drop this report; wait for the next cycle |
| 5xx | 5000 | Server-side failure | Retry on the next upload cycle |

### Appendix: Capabilities Removed in This Revision

To match the scope adjustments above, the following capabilities that existed in earlier versions
of this protocol have been removed entirely and are no longer part of this project's scope:
- Device identity authentication (HMAC signature, the DeviceSecret derivation formula) —
  targeting a trusted internal-network environment, this has been simplified further down to
  plain-text pid+sn device identification, with no signature/key check at all.
- Session tokens and their renewal.
- The entire channel for the server to push control commands to a device (formerly the `cmd`
  field), including config polling (formerly `/device/poll`), custom probe read configuration
  (formerly `set_probe`), and firmware OTA updates.
- The self-service device activation approval / key-binding flow (including encrypted submission
  and RSA key pairs).
