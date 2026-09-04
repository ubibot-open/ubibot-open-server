package api_test

import (
	"bytes"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/ubibot/ubibot-open-server/internal/api"
	"github.com/ubibot/ubibot-open-server/internal/auth"
	"github.com/ubibot/ubibot-open-server/internal/model"
	"github.com/ubibot/ubibot-open-server/internal/store"
)

const (
	testPID = "ubibot_open_dev_v1"
	testSN  = "sn_ws1_20001_1"

	// testEnvNow is the fixed value newTestEnv's mocked clock starts at --
	// pass it as reportAt's outerTs when a test wants an old/arbitrary
	// payload timestamp to survive the report handler's ±5min freshness
	// check (docs §8, code 1002).
	testEnvNow int64 = 1_700_000_000
)

// testEnv bundles a router with a device already provisioned (via the same
// GetOrCreateDeviceBySN path a real first report takes) and a clock the
// test controls, so the ±5min report window and offline-grace assertions
// can be exercised deterministically instead of racing the wall clock. Each
// test gets its own in-memory database (via a unique DSN) so tests can run
// in parallel without stepping on each other's rows.
type testEnv struct {
	router http.Handler
	srv    *api.Server
	now    time.Time
	dev    *model.Device
}

func newTestEnv(t *testing.T) *testEnv {
	t.Helper()

	dsn := fmt.Sprintf("file:%s?mode=memory&cache=shared", t.Name())
	st, err := store.Open(dsn)
	if err != nil {
		t.Fatalf("open store: %v", err)
	}

	dev, _, err := st.GetOrCreateDeviceBySN(testPID, testSN)
	if err != nil {
		t.Fatalf("provision device: %v", err)
	}

	srv := api.NewServer(st)
	env := &testEnv{srv: srv, now: time.Unix(testEnvNow, 0), dev: dev}
	srv.Now = func() time.Time { return env.now }
	env.router = api.NewRouter(srv, nil, false)
	return env
}

func (e *testEnv) do(t *testing.T, method, path string, body interface{}, headers map[string]string) (*httptest.ResponseRecorder, map[string]interface{}) {
	t.Helper()
	var buf bytes.Buffer
	if body != nil {
		if err := json.NewEncoder(&buf).Encode(body); err != nil {
			t.Fatalf("encode body: %v", err)
		}
	}
	req := httptest.NewRequest(method, path, &buf)
	req.Header.Set("Content-Type", "application/json")
	for k, v := range headers {
		req.Header.Set(k, v)
	}

	rec := httptest.NewRecorder()
	e.router.ServeHTTP(rec, req)

	var parsed map[string]interface{}
	if rec.Body.Len() > 0 {
		if err := json.Unmarshal(rec.Body.Bytes(), &parsed); err != nil {
			t.Fatalf("decode response %q: %v", rec.Body.String(), err)
		}
	}
	return rec, parsed
}

// report is a small helper building a docs-§5-shaped report body: one
// payload with the given ts and fields (field1..field20 -> value).
func report(sn string, ts int64, fields map[string]any) map[string]any {
	payload := map[string]any{"ts": ts}
	for k, v := range fields {
		payload[k] = v
	}
	return map[string]any{
		"pid": testPID, "sn": sn, "ts": ts,
		"payloads": []map[string]any{payload},
	}
}

// reportAt is report(), but with the outer (request-level) "ts" -- the one
// the report handler checks against its ±5min freshness window (docs §8,
// code 1002) -- set independently of the payload's own ts. Use this when a
// test wants to report an old/arbitrary payload timestamp (e.g. to probe a
// ts-range query) without also having to dodge the freshness check; docs §5
// explicitly allows payloads[].ts to be older than the outer ts, to
// backfill data buffered while offline.
func reportAt(sn string, outerTs, payloadTs int64, fields map[string]any) map[string]any {
	body := report(sn, payloadTs, fields)
	body["ts"] = outerTs
	return body
}

func TestTimeSync_ReturnsServerTime(t *testing.T) {
	env := newTestEnv(t)

	rec, body := env.do(t, "POST", "/api/v1/auth/time", map[string]any{
		"pid": testPID, "sn": testSN,
	}, nil)
	if rec.Code != 200 || body["c"].(float64) != 0 {
		t.Fatalf("time sync failed: %d %v", rec.Code, body)
	}
	if int64(body["t"].(float64)) != env.now.Unix() {
		t.Fatalf("expected server time %d, got %v", env.now.Unix(), body["t"])
	}
}

func TestTimeSync_RejectsMalformedBody(t *testing.T) {
	env := newTestEnv(t)

	rec, body := env.do(t, "POST", "/api/v1/auth/time", map[string]any{"pid": testPID}, nil)
	if rec.Code != 400 || body["c"].(float64) != 1003 {
		t.Fatalf("expected malformed-body rejection, got %d %v", rec.Code, body)
	}
}

func TestReport_AutoCreatesUnseenDeviceAndDedupesByTs(t *testing.T) {
	env := newTestEnv(t)
	const newSN = "sn_new_device_1"

	rec, body := env.do(t, "POST", "/api/v1/data/report",
		report(newSN, env.now.Unix(), map[string]any{"field1": 25.6}), nil)
	if rec.Code != 200 || body["c"].(float64) != 0 {
		t.Fatalf("report failed: %d %v", rec.Code, body)
	}

	dev, err := env.srv.Store.DeviceBySN(newSN)
	if err != nil {
		t.Fatalf("expected the unseen SN to have been auto-created: %v", err)
	}

	// Same (device, ts) again with a different value must not overwrite —
	// the unique index + ON CONFLICT DO NOTHING makes this a no-op.
	env.do(t, "POST", "/api/v1/data/report",
		report(newSN, env.now.Unix(), map[string]any{"field1": 999}), nil)

	records, err := env.srv.Store.RecentRecords(dev.ID, 10)
	if err != nil {
		t.Fatalf("recent records: %v", err)
	}
	if len(records) != 1 {
		t.Fatalf("expected exactly 1 deduped record, got %d", len(records))
	}
	var d map[string]any
	_ = json.Unmarshal([]byte(records[0].Data), &d)
	if d["field1"].(float64) != 25.6 {
		t.Fatalf("expected the first value to win, got %v", d["field1"])
	}
}

// recordFieldsByTs is a small test helper: fetches deviceID's recent
// records and decodes each one's Data into a plain map, keyed by Ts.
func recordFieldsByTs(t *testing.T, env *testEnv, deviceID uint) map[int64]map[string]any {
	t.Helper()
	records, err := env.srv.Store.RecentRecords(deviceID, 50)
	if err != nil {
		t.Fatalf("recent records: %v", err)
	}
	byTs := make(map[int64]map[string]any, len(records))
	for _, r := range records {
		var d map[string]any
		if err := json.Unmarshal([]byte(r.Data), &d); err != nil {
			t.Fatalf("decode record data: %v", err)
		}
		byTs[r.Ts] = d
	}
	return byTs
}

// TestReport_MergesPayloadEntriesWithinTimeWindow guards against a
// real-hardware shape: some devices split one sampling round's fields
// across several payloads[] entries -- one field per entry, with a few
// seconds of drift between them (sequential sensor reads) -- rather than
// batching them into a single object (docs §5). Entries within
// store.FieldMergeWindow of the round's anchor ts must end up merged into
// one stored record; before the fix, only the first entry for a given
// exact ts survived and the rest were silently dropped as if they were
// duplicate re-reports.
func TestReport_MergesPayloadEntriesWithinTimeWindow(t *testing.T) {
	env := newTestEnv(t)
	const newSN = "sn_split_fields_device"

	const anchorTs = 1514767395
	const laterTs = 1514767409 // 14s later, well within the 60s window
	body := map[string]any{
		"pid": testPID, "sn": newSN, "ts": env.now.Unix(),
		"payloads": []map[string]any{
			{"ts": anchorTs, "field1": 29.291221618652344},
			{"ts": anchorTs, "field2": 40.831615447998047},
			{"ts": anchorTs, "field3": 439.95001220703125},
			{"ts": anchorTs, "field4": 0.014166667126119137},
			{"ts": anchorTs, "field6": 26.9375},
			{"ts": anchorTs, "field7": 27.625},
			{"ts": laterTs, "field5": -56},
		},
	}

	rec, respBody := env.do(t, "POST", "/api/v1/data/report", body, nil)
	if rec.Code != 200 || respBody["c"].(float64) != 0 {
		t.Fatalf("report failed: %d %v", rec.Code, respBody)
	}

	dev, err := env.srv.Store.DeviceBySN(newSN)
	if err != nil {
		t.Fatalf("expected the unseen SN to have been auto-created: %v", err)
	}

	byTs := recordFieldsByTs(t, env, dev.ID)
	if len(byTs) != 1 {
		t.Fatalf("expected all 7 entries to merge into 1 record (all within the merge window), got %d: %+v", len(byTs), byTs)
	}

	merged, ok := byTs[anchorTs]
	if !ok {
		t.Fatalf("expected the merged record to be keyed by the group's anchor ts %d, got %+v", anchorTs, byTs)
	}
	want := map[string]float64{
		"field1": 29.291221618652344, "field2": 40.831615447998047,
		"field3": 439.95001220703125, "field4": 0.014166667126119137,
		"field5": -56, "field6": 26.9375, "field7": 27.625,
	}
	for field, wantVal := range want {
		got, ok := merged[field].(float64)
		if !ok {
			t.Fatalf("expected merged record to have %s, got %v", field, merged)
		}
		if got != wantVal {
			t.Fatalf("field %s: expected %v, got %v", field, wantVal, got)
		}
	}
	if len(merged) != len(want) {
		t.Fatalf("expected exactly %d fields merged, got %v", len(want), merged)
	}
}

// TestReport_DoesNotMergeRoundsBeyondTimeWindow is the other half of
// store.FieldMergeWindow's contract: two sampling rounds far enough apart
// in time are genuinely separate data points and must NOT be collapsed
// into one record just because they arrived in the same offline-buffered
// upload -- merging must be bounded by time, not "same request".
func TestReport_DoesNotMergeRoundsBeyondTimeWindow(t *testing.T) {
	env := newTestEnv(t)
	const newSN = "sn_two_far_apart_rounds"

	const firstTs = 1514767395
	const secondTs = firstTs + 120 // 2 minutes later, beyond the 60s window
	body := map[string]any{
		"pid": testPID, "sn": newSN, "ts": env.now.Unix(),
		"payloads": []map[string]any{
			{"ts": firstTs, "field1": 29.29, "field2": 40.83},
			{"ts": secondTs, "field1": 29.31, "field2": 40.79},
		},
	}

	rec, respBody := env.do(t, "POST", "/api/v1/data/report", body, nil)
	if rec.Code != 200 || respBody["c"].(float64) != 0 {
		t.Fatalf("report failed: %d %v", rec.Code, respBody)
	}

	dev, err := env.srv.Store.DeviceBySN(newSN)
	if err != nil {
		t.Fatalf("expected the unseen SN to have been auto-created: %v", err)
	}

	byTs := recordFieldsByTs(t, env, dev.ID)
	if len(byTs) != 2 {
		t.Fatalf("expected 2 independent records (120s apart, beyond the merge window), got %d: %+v", len(byTs), byTs)
	}
	if byTs[firstTs]["field1"].(float64) != 29.29 {
		t.Fatalf("expected round 1's own field1, got %+v", byTs[firstTs])
	}
	if byTs[secondTs]["field1"].(float64) != 29.31 {
		t.Fatalf("expected round 2's own field1, got %+v", byTs[secondTs])
	}
}

func TestReport_RejectsMalformedBody(t *testing.T) {
	env := newTestEnv(t)

	rec, body := env.do(t, "POST", "/api/v1/data/report", map[string]any{
		"pid": testPID, "sn": testSN,
	}, nil)
	if rec.Code != 400 || body["c"].(float64) != 1003 {
		t.Fatalf("expected malformed-body rejection for a missing ts/payloads, got %d %v", rec.Code, body)
	}
}

func TestReport_RejectsTimestampOutsideWindow(t *testing.T) {
	env := newTestEnv(t)

	future := env.now.Add(10 * time.Minute).Unix()
	rec, body := env.do(t, "POST", "/api/v1/data/report",
		report(testSN, future, map[string]any{"field1": 20}), nil)
	if rec.Code != 400 || body["c"].(float64) != 1002 {
		t.Fatalf("expected timestamp-out-of-window rejection, got %d %v", rec.Code, body)
	}
}

func TestAdminLoginAndDeviceListFlow(t *testing.T) {
	env := newTestEnv(t)

	role, err := env.srv.Store.CreateRole("超级管理员", model.RoleSuper, []string{"*"})
	if err != nil {
		t.Fatalf("create role: %v", err)
	}
	hash, err := auth.HashPassword("s3cret-pw")
	if err != nil {
		t.Fatalf("hash password: %v", err)
	}
	if _, err := env.srv.Store.CreateAdmin("admin", hash, role.ID); err != nil {
		t.Fatalf("create admin: %v", err)
	}

	// Wrong password rejected.
	rec, _ := env.do(t, "POST", "/api/admin/login", map[string]any{"username": "admin", "password": "wrong"}, nil)
	if rec.Code != 401 {
		t.Fatalf("expected wrong password to be rejected, got %d", rec.Code)
	}

	// Correct login issues a bearer token.
	rec, body := env.do(t, "POST", "/api/admin/login", map[string]any{"username": "admin", "password": "s3cret-pw"}, nil)
	if rec.Code != 200 {
		t.Fatalf("admin login failed: %d %v", rec.Code, body)
	}
	adminToken := body["token"].(string)
	adminAuth := map[string]string{"Authorization": "Bearer " + adminToken}

	// Protected endpoints reject requests with no/invalid session.
	rec, _ = env.do(t, "GET", "/api/admin/devices", nil, nil)
	if rec.Code != 401 {
		t.Fatalf("expected 401 without session, got %d", rec.Code)
	}

	// List devices shows the seeded device.
	rec, body = env.do(t, "GET", "/api/admin/devices", nil, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("list devices failed: %d %v", rec.Code, body)
	}
	list := body["list"].([]interface{})
	if len(list) != 1 {
		t.Fatalf("expected 1 device, got %d", len(list))
	}

	// Rename it — the only per-device config left (docs §7).
	rec, body = env.do(t, "PATCH", fmt.Sprintf("/api/admin/devices/%d", env.dev.ID),
		map[string]any{"name": "客厅传感器"}, adminAuth)
	if rec.Code != 200 || body["name"] != "客厅传感器" {
		t.Fatalf("rename device failed: %d %v", rec.Code, body)
	}

	// Delete it.
	rec, _ = env.do(t, "DELETE", fmt.Sprintf("/api/admin/devices/%d", env.dev.ID), nil, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("delete device failed: %d", rec.Code)
	}
	rec, _ = env.do(t, "GET", fmt.Sprintf("/api/admin/devices/%d", env.dev.ID), nil, adminAuth)
	if rec.Code != 404 {
		t.Fatalf("expected deleted device to 404, got %d", rec.Code)
	}
}

// TestAdminAPI_ResponsesCarryTimestamp checks that every admin API response
// -- success and error alike -- carries a "timestamp" field (see
// writeAPIJSON in httpx.go), independent of whatever business data the
// endpoint returns.
func TestAdminAPI_ResponsesCarryTimestamp(t *testing.T) {
	env := newTestEnv(t)
	before := time.Now().Unix()

	// Error response (missing bearer token).
	rec, body := env.do(t, "GET", "/api/admin/devices", nil, nil)
	if rec.Code != 401 {
		t.Fatalf("expected 401, got %d", rec.Code)
	}
	assertRecentTimestamp(t, body, before)

	// Success response (login).
	role, err := env.srv.Store.CreateRole("超级管理员", model.RoleSuper, []string{"*"})
	if err != nil {
		t.Fatalf("create role: %v", err)
	}
	hash, err := auth.HashPassword("s3cret-pw")
	if err != nil {
		t.Fatalf("hash password: %v", err)
	}
	if _, err := env.srv.Store.CreateAdmin("admin", hash, role.ID); err != nil {
		t.Fatalf("create admin: %v", err)
	}
	rec, body = env.do(t, "POST", "/api/admin/login", map[string]any{"username": "admin", "password": "s3cret-pw"}, nil)
	if rec.Code != 200 {
		t.Fatalf("admin login failed: %d %v", rec.Code, body)
	}
	assertRecentTimestamp(t, body, before)
}

func assertRecentTimestamp(t *testing.T, body map[string]interface{}, notBefore int64) {
	t.Helper()
	raw, ok := body["timestamp"]
	if !ok {
		t.Fatalf("response missing \"timestamp\" field: %v", body)
	}
	ts := int64(raw.(float64))
	now := time.Now().Unix()
	if ts < notBefore || ts > now {
		t.Fatalf("timestamp %d not within expected range [%d, %d]", ts, notBefore, now)
	}
}
