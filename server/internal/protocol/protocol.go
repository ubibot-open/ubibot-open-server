// Package protocol defines the wire format shared by the device-facing
// HTTP endpoints, mirroring docs/UbiBot开放平台硬件通信协议.md — deliberately
// tiny: no signing, no session tokens, no command channel. A device only
// ever needs pid+sn to identify itself.
package protocol

import (
	"encoding/json"
	"errors"
	"fmt"
)

// Business status codes (the "c" field). Zero means success; non-zero
// codes map to the HTTP status shown in the doc's error table (§7).
const (
	CodeOK                   = 0
	CodeTimestampOutOfWindow = 1002 // ts outside the ±5 minute window
	CodeMalformedBody        = 1003
	CodeDeviceDisabled       = 1103 // device exists but was disabled by an operator
	CodeRateLimited          = 1900
	CodeServerError          = 5000
)

// TimeSyncRequest is POST /api/v1/auth/time (docs §3) — no signature, no
// timestamp: a device with no clock reference yet can call this purely to
// learn the current time.
type TimeSyncRequest struct {
	PID string `json:"pid" binding:"required"`
	SN  string `json:"sn" binding:"required"`
}

// TimeSyncResponse returns the server's current time.
type TimeSyncResponse struct {
	C int   `json:"c"`
	T int64 `json:"t"`
}

// Payload is one sampled time point (docs §4); Fields maps field1..field20
// to numeric values, flattened directly into the payload object alongside
// ts (no nested "feed" wrapper). There's no fixed sensor vocabulary — the
// platform just stores whatever keys arrive (see docs §5 for the field1/2/3
// default-meaning convention, which is a display-only convention, not
// something this layer enforces).
type Payload struct {
	Ts     int64
	Fields map[string]float64
}

// MarshalJSON flattens Fields alongside ts into a single JSON object, e.g.
// {"ts":1788950400,"field1":25.6,"field2":60.2}.
func (p Payload) MarshalJSON() ([]byte, error) {
	m := make(map[string]interface{}, len(p.Fields)+1)
	for k, v := range p.Fields {
		m[k] = v
	}
	m["ts"] = p.Ts
	return json.Marshal(m)
}

// UnmarshalJSON reads ts out of the object and treats every other key as a
// field1..field20 entry.
func (p *Payload) UnmarshalJSON(data []byte) error {
	var raw map[string]json.RawMessage
	if err := json.Unmarshal(data, &raw); err != nil {
		return err
	}

	tsRaw, ok := raw["ts"]
	if !ok {
		return errors.New("payload: missing required field \"ts\"")
	}
	if err := json.Unmarshal(tsRaw, &p.Ts); err != nil {
		return fmt.Errorf("payload: ts: %w", err)
	}
	delete(raw, "ts")

	fields := make(map[string]float64, len(raw))
	for k, v := range raw {
		var f float64
		if err := json.Unmarshal(v, &f); err != nil {
			return fmt.Errorf("payload: field %q: %w", k, err)
		}
		fields[k] = f
	}
	p.Fields = fields
	return nil
}

// ReportRequest is POST /api/v1/data/report (docs §4) — the device's only
// other endpoint besides time-sync. Identity is just PID+SN, in the body,
// unauthenticated; Ts is the request's own timestamp (checked against a
// ±5 minute window), separate from each Payload's own Ts (which may be
// older, for batched offline-buffered samples).
type ReportRequest struct {
	PID      string    `json:"pid" binding:"required"`
	SN       string    `json:"sn" binding:"required"`
	Ts       int64     `json:"ts" binding:"required"`
	Payloads []Payload `json:"payloads" binding:"required"`
}

// ReportResponse is the reply to a data upload — deliberately minimal,
// just an ack and the server's clock for reference.
type ReportResponse struct {
	C int   `json:"c"`
	T int64 `json:"t"`
}

// ErrorResponse is the generic error envelope for every endpoint.
type ErrorResponse struct {
	C int    `json:"c"`
	M string `json:"m"`
}

// HTTPStatusFor maps a business code to the HTTP status the doc's error
// table specifies.
func HTTPStatusFor(code int) int {
	switch code {
	case CodeOK:
		return 200
	case CodeTimestampOutOfWindow, CodeMalformedBody:
		return 400
	case CodeDeviceDisabled:
		return 401
	case CodeRateLimited:
		return 429
	default:
		return 500
	}
}
