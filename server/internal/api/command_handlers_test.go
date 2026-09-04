package api_test

import (
	"fmt"
	"testing"

	"github.com/ubibot/ubibot-open-server/internal/auth"
	"github.com/ubibot/ubibot-open-server/internal/model"
)

// TestSendDeviceCommand_RebootDeliveredOnceThenCleared covers the whole
// docs §9 lifecycle: an admin queues a command, it shows up as
// pending_command on the device until the device's next report, that
// report's response carries it as "cmd", and it's gone (not redelivered)
// on the report after that.
func TestSendDeviceCommand_RebootDeliveredOnceThenCleared(t *testing.T) {
	env := newTestEnv(t)
	adminAuth := env.createSuperAdmin(t, "admin", "s3cret-pw")

	rec, body := env.do(t, "POST", fmt.Sprintf("/api/admin/devices/%d/commands", env.dev.ID),
		map[string]any{"action": "reboot"}, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("queue reboot command failed: %d %v", rec.Code, body)
	}

	// Visible as pending before it's ever delivered.
	rec, body = env.do(t, "GET", fmt.Sprintf("/api/admin/devices/%d", env.dev.ID), nil, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("get device failed: %d %v", rec.Code, body)
	}
	dev := body["device"].(map[string]interface{})
	if dev["pending_command"] == nil {
		t.Fatalf("expected pending_command to be set before delivery, got %v", dev)
	}

	// First report after queuing: the response carries the command.
	rec, body = env.do(t, "POST", "/api/v1/data/report",
		report(testSN, env.now.Unix(), map[string]any{"field1": 20}), nil)
	if rec.Code != 200 {
		t.Fatalf("report failed: %d %v", rec.Code, body)
	}
	cmd, ok := body["cmd"].(map[string]interface{})
	if !ok || cmd["action"] != "reboot" {
		t.Fatalf("expected report response to carry {\"cmd\":{\"action\":\"reboot\"}}, got %v", body)
	}

	// No longer pending, and not redelivered on the next report.
	rec, body = env.do(t, "GET", fmt.Sprintf("/api/admin/devices/%d", env.dev.ID), nil, adminAuth)
	dev = body["device"].(map[string]interface{})
	if dev["pending_command"] != nil {
		t.Fatalf("expected pending_command to be cleared after delivery, got %v", dev)
	}

	rec, body = env.do(t, "POST", "/api/v1/data/report",
		report(testSN, env.now.Unix(), map[string]any{"field1": 21}), nil)
	if rec.Code != 200 {
		t.Fatalf("second report failed: %d %v", rec.Code, body)
	}
	if _, present := body["cmd"]; present {
		t.Fatalf("expected no \"cmd\" on the report after delivery, got %v", body)
	}
}

// TestSendDeviceCommand_SetIntervalValidatesSeconds guards the admin API's
// own sanity bounds on set_interval (docs §9 notes the firmware doesn't
// enforce a range itself, so the platform has to).
func TestSendDeviceCommand_SetIntervalValidatesSeconds(t *testing.T) {
	env := newTestEnv(t)
	adminAuth := env.createSuperAdmin(t, "admin", "s3cret-pw")

	for _, seconds := range []int{0, 59, 86401, 1000000} {
		rec, body := env.do(t, "POST", fmt.Sprintf("/api/admin/devices/%d/commands", env.dev.ID),
			map[string]any{"action": "set_interval", "seconds": seconds}, adminAuth)
		if rec.Code != 400 {
			t.Fatalf("seconds=%d: expected 400, got %d %v", seconds, rec.Code, body)
		}
	}

	rec, body := env.do(t, "POST", fmt.Sprintf("/api/admin/devices/%d/commands", env.dev.ID),
		map[string]any{"action": "set_interval", "seconds": 600}, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("expected a valid seconds value to be accepted, got %d %v", rec.Code, body)
	}

	rec, body = env.do(t, "POST", "/api/v1/data/report",
		report(testSN, env.now.Unix(), map[string]any{"field1": 20}), nil)
	if rec.Code != 200 {
		t.Fatalf("report failed: %d %v", rec.Code, body)
	}
	cmd, ok := body["cmd"].(map[string]interface{})
	if !ok || cmd["action"] != "set_interval" || cmd["seconds"].(float64) != 600 {
		t.Fatalf("expected {\"cmd\":{\"action\":\"set_interval\",\"seconds\":600}}, got %v", body)
	}
}

func TestSendDeviceCommand_UnsupportedAction(t *testing.T) {
	env := newTestEnv(t)
	adminAuth := env.createSuperAdmin(t, "admin", "s3cret-pw")

	rec, body := env.do(t, "POST", fmt.Sprintf("/api/admin/devices/%d/commands", env.dev.ID),
		map[string]any{"action": "calibrate"}, adminAuth)
	if rec.Code != 400 {
		t.Fatalf("expected an unsupported action to be rejected, got %d %v", rec.Code, body)
	}
}

func TestSendDeviceCommand_DeviceNotFound(t *testing.T) {
	env := newTestEnv(t)
	adminAuth := env.createSuperAdmin(t, "admin", "s3cret-pw")

	rec, body := env.do(t, "POST", "/api/admin/devices/999999/commands",
		map[string]any{"action": "reboot"}, adminAuth)
	if rec.Code != 404 {
		t.Fatalf("expected 404 for a nonexistent device, got %d %v", rec.Code, body)
	}
}

// TestCancelDeviceCommand covers withdrawing a command before it's ever
// delivered -- the device's next report should not see it.
func TestCancelDeviceCommand(t *testing.T) {
	env := newTestEnv(t)
	adminAuth := env.createSuperAdmin(t, "admin", "s3cret-pw")

	rec, body := env.do(t, "POST", fmt.Sprintf("/api/admin/devices/%d/commands", env.dev.ID),
		map[string]any{"action": "reboot"}, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("queue command failed: %d %v", rec.Code, body)
	}

	rec, body = env.do(t, "DELETE", fmt.Sprintf("/api/admin/devices/%d/commands", env.dev.ID), nil, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("cancel command failed: %d %v", rec.Code, body)
	}

	rec, body = env.do(t, "POST", "/api/v1/data/report",
		report(testSN, env.now.Unix(), map[string]any{"field1": 20}), nil)
	if rec.Code != 200 {
		t.Fatalf("report failed: %d %v", rec.Code, body)
	}
	if _, present := body["cmd"]; present {
		t.Fatalf("expected no \"cmd\" after cancelling, got %v", body)
	}
}

// TestSendDeviceCommand_RequiresDeviceWritePermission mirrors
// TestRBACDeniesUnpermittedAction (p1_handlers_test.go): a device:read-only
// role must not be able to queue a command.
func TestSendDeviceCommand_RequiresDeviceWritePermission(t *testing.T) {
	env := newTestEnv(t)

	role, err := env.srv.Store.CreateRole("只读操作员", "readonly_op", []string{model.PermDeviceRead})
	if err != nil {
		t.Fatalf("create role: %v", err)
	}
	hash, err := auth.HashPassword("pw12345")
	if err != nil {
		t.Fatalf("hash password: %v", err)
	}
	if _, err := env.srv.Store.CreateAdmin("readonly", hash, role.ID); err != nil {
		t.Fatalf("create admin: %v", err)
	}
	_, loginBody := env.do(t, "POST", "/api/admin/login", map[string]any{"username": "readonly", "password": "pw12345"}, nil)
	readonlyAuth := map[string]string{"Authorization": "Bearer " + loginBody["token"].(string)}

	rec, body := env.do(t, "POST", fmt.Sprintf("/api/admin/devices/%d/commands", env.dev.ID),
		map[string]any{"action": "reboot"}, readonlyAuth)
	if rec.Code != 403 {
		t.Fatalf("expected device:write to be required, got %d %v", rec.Code, body)
	}
}
