package api_test

import (
	"fmt"
	"testing"
)

// TestProductCRUDAndDeviceResolution covers the whole product lifecycle
// and the read side's only real behavior: a device's product_name resolves
// by matching its pid against a Product row, and stops resolving once that
// Product is deleted (docs §7 -- Product is display metadata only, never a
// foreign key on Device).
func TestProductCRUDAndDeviceResolution(t *testing.T) {
	env := newTestEnv(t)
	adminAuth := env.createSuperAdmin(t, "admin", "s3cret-pw")

	rec, body := env.do(t, "POST", "/api/admin/products",
		map[string]any{"pid": testPID, "name": "WS1B Sensor", "description": "Outdoor temp/humidity/light"}, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("create product failed: %d %v", rec.Code, body)
	}
	productID := int(body["id"].(float64))

	// A second product for the same pid must be rejected (unique pid).
	rec, body = env.do(t, "POST", "/api/admin/products",
		map[string]any{"pid": testPID, "name": "Duplicate"}, adminAuth)
	if rec.Code != 400 {
		t.Fatalf("expected a duplicate pid to be rejected, got %d %v", rec.Code, body)
	}

	// env.dev (seeded by newTestEnv) already has pid == testPID, so its
	// device_name should now resolve.
	rec, body = env.do(t, "GET", fmt.Sprintf("/api/admin/devices/%d", env.dev.ID), nil, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("get device failed: %d %v", rec.Code, body)
	}
	dev := body["device"].(map[string]interface{})
	if dev["product_name"] != "WS1B Sensor" {
		t.Fatalf("expected product_name to resolve from the matching pid, got %v", dev)
	}

	// Also shows up in the plain list.
	rec, body = env.do(t, "GET", "/api/admin/devices", nil, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("list devices failed: %d %v", rec.Code, body)
	}
	list := body["list"].([]interface{})
	found := false
	for _, item := range list {
		d := item.(map[string]interface{})
		if uint64(d["id"].(float64)) == uint64(env.dev.ID) {
			found = true
			if d["product_name"] != "WS1B Sensor" {
				t.Fatalf("expected product_name on the list row too, got %v", d)
			}
		}
	}
	if !found {
		t.Fatalf("expected env.dev in the device list, got %v", list)
	}

	// Update name/description.
	rec, body = env.do(t, "PATCH", fmt.Sprintf("/api/admin/products/%d", productID),
		map[string]any{"name": "WS1B Sensor v2", "description": "updated"}, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("update product failed: %d %v", rec.Code, body)
	}
	rec, body = env.do(t, "GET", "/api/admin/products", nil, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("list products failed: %d %v", rec.Code, body)
	}
	products := body["list"].([]interface{})
	if len(products) != 1 || products[0].(map[string]interface{})["name"] != "WS1B Sensor v2" {
		t.Fatalf("expected the updated name to stick, got %v", products)
	}

	// Delete -- the device falls back to no product_name, but keeps
	// reporting/existing exactly as before (Product is metadata only).
	rec, body = env.do(t, "DELETE", fmt.Sprintf("/api/admin/products/%d", productID), nil, adminAuth)
	if rec.Code != 200 {
		t.Fatalf("delete product failed: %d %v", rec.Code, body)
	}
	rec, body = env.do(t, "GET", fmt.Sprintf("/api/admin/devices/%d", env.dev.ID), nil, adminAuth)
	dev = body["device"].(map[string]interface{})
	if _, present := dev["product_name"]; present {
		t.Fatalf("expected product_name to be gone after deleting the product, got %v", dev)
	}
}
