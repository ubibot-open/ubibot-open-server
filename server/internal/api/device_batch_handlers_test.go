package api_test

import (
	"encoding/csv"
	"net/http/httptest"
	"strings"
	"testing"
)

// TestImportDevices_CreatesSkipsAndReportsFailures covers the three
// per-row outcomes docs §7's batch import promises: a good new row is
// created, a row reusing an existing sn is skipped (not an error for the
// whole batch), and a row missing a required field is reported as failed
// -- all in the same request.
func TestImportDevices_CreatesSkipsAndReportsFailures(t *testing.T) {
	env := newTestEnv(t)
	adminAuth := env.createSuperAdmin(t, "admin", "s3cret-pw")

	rec, body := env.do(t, "POST", "/api/admin/devices/import", map[string]any{
		"rows": []map[string]any{
			{"sn": "sn_batch_001", "pid": testPID, "name": "Batch Device 1"},
			{"sn": testSN, "pid": testPID, "name": "Should be skipped"}, // already exists (env.dev)
			{"sn": "", "pid": testPID, "name": "Missing SN"},            // invalid row
		},
	}, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("import failed: %d %v", rec.Code, body)
	}
	if body["created"].(float64) != 1 {
		t.Fatalf("expected exactly 1 created row, got %v", body)
	}
	skipped := body["skipped"].([]interface{})
	if len(skipped) != 1 || skipped[0] != testSN {
		t.Fatalf("expected the existing sn to be reported as skipped, got %v", body)
	}
	failed := body["failed"].([]interface{})
	if len(failed) != 1 {
		t.Fatalf("expected exactly 1 failed row, got %v", body)
	}

	// The imported device shows up immediately, pre-named, before it has
	// ever reported.
	rec, body = env.do(t, "GET", "/api/admin/devices", nil, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("list devices failed: %d %v", rec.Code, body)
	}
	list := body["list"].([]interface{})
	var imported map[string]interface{}
	for _, item := range list {
		d := item.(map[string]interface{})
		if d["sn"] == "sn_batch_001" {
			imported = d
		}
	}
	if imported == nil || imported["name"] != "Batch Device 1" {
		t.Fatalf("expected the imported device to appear pre-named, got list=%v", list)
	}

	// It behaves exactly like an auto-created device once it actually
	// reports -- matched by sn, not re-created as a duplicate.
	rec, body = env.do(t, "POST", "/api/v1/data/report",
		report("sn_batch_001", env.now.Unix(), map[string]any{"field1": 1}), nil)
	if rec.Code != 200 || body["c"].(float64) != 0 {
		t.Fatalf("report from a pre-imported device failed: %d %v", rec.Code, body)
	}
}

func TestImportDevices_RejectsEmptyBatch(t *testing.T) {
	env := newTestEnv(t)
	adminAuth := env.createSuperAdmin(t, "admin", "s3cret-pw")

	rec, body := env.do(t, "POST", "/api/admin/devices/import", map[string]any{"rows": []map[string]any{}}, adminAuth)
	if rec.Code != 400 {
		t.Fatalf("expected an empty batch to be rejected, got %d %v", rec.Code, body)
	}
}

// TestExportDevicesCSV hits the export endpoint directly (env.do can't be
// reused here -- it always decodes the response as JSON, and this endpoint
// deliberately returns text/csv) and parses the body back with the
// standard csv package to check the header row and env.dev's own row.
func TestExportDevicesCSV(t *testing.T) {
	env := newTestEnv(t)
	adminAuth := env.createSuperAdmin(t, "admin", "s3cret-pw")

	req := httptest.NewRequest("GET", "/api/admin/devices/export.csv", nil)
	req.Header.Set("Authorization", adminAuth["Authorization"])
	rec := httptest.NewRecorder()
	env.router.ServeHTTP(rec, req)

	if rec.Code != 200 {
		t.Fatalf("export failed: %d %s", rec.Code, rec.Body.String())
	}
	if ct := rec.Header().Get("Content-Type"); !strings.HasPrefix(ct, "text/csv") {
		t.Fatalf("expected a text/csv content type, got %q", ct)
	}

	rows, err := csv.NewReader(rec.Body).ReadAll()
	if err != nil {
		t.Fatalf("parse csv: %v (body: %q)", err, rec.Body.String())
	}
	if len(rows) < 2 {
		t.Fatalf("expected a header row plus at least one device row, got %v", rows)
	}
	header := rows[0]
	if header[0] != "id" || header[3] != "sn" {
		t.Fatalf("unexpected header row: %v", header)
	}

	var devRow []string
	for _, row := range rows[1:] {
		if row[3] == testSN {
			devRow = row
		}
	}
	if devRow == nil {
		t.Fatalf("expected env.dev's sn %q in the export, got rows=%v", testSN, rows)
	}
	if devRow[1] != testPID {
		t.Fatalf("expected pid column to be %q, got %v", testPID, devRow)
	}
}
