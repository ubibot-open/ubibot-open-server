package api

import (
	"errors"
	"net/http"
	"strconv"
	"strings"

	"github.com/ubibot/ubibot-platform-open/internal/model"
	"github.com/ubibot/ubibot-platform-open/internal/store"
)

// deviceFieldSettingDTO is one field1..field20 entry, already resolved
// through the fallback chain: this device's own override, else the
// matching IconAsset template, else empty (the frontend then shows the raw
// key with no unit and its built-in icon). IsCustom reflects whether this
// device has its own override row at all, regardless of whether every
// attribute on it is filled in -- it's what the settings page uses to
// decide whether to offer a "reset to default" action.
type deviceFieldSettingDTO struct {
	Key      string `json:"key"`
	Name     string `json:"name"`
	Unit     string `json:"unit"`
	SVG      string `json:"svg"`
	IsCustom bool   `json:"is_custom"`
}

func firstNonEmpty(a, b string) string {
	if a != "" {
		return a
	}
	return b
}

func templatesByKey(templates []model.IconAsset) map[string]model.IconAsset {
	m := make(map[string]model.IconAsset, len(templates))
	for _, t := range templates {
		m[strings.ToLower(t.Key)] = t
	}
	return m
}

func overridesByKey(rows []model.DeviceFieldSetting) map[string]model.DeviceFieldSetting {
	m := make(map[string]model.DeviceFieldSetting, len(rows))
	for _, o := range rows {
		m[strings.ToLower(o.FieldKey)] = o
	}
	return m
}

// resolveFieldSetting applies the per-field fallback chain: device override
// attribute, else template attribute, else empty.
func resolveFieldSetting(key string, overrides map[string]model.DeviceFieldSetting, templates map[string]model.IconAsset) deviceFieldSettingDTO {
	lk := strings.ToLower(key)
	override, isCustom := overrides[lk]
	tmpl := templates[lk]

	return deviceFieldSettingDTO{
		Key:      key,
		Name:     firstNonEmpty(override.Name, tmpl.Name),
		Unit:     firstNonEmpty(override.Unit, tmpl.Unit),
		SVG:      firstNonEmpty(override.SVG, tmpl.SVG),
		IsCustom: isCustom,
	}
}

// ListDeviceFieldSettings handles GET /api/admin/devices/{id}/field-settings
// -- the "字段设置" tab's backing call, always returning all of field1..
// field20 (see store.FieldKeys) regardless of which ones this device has
// actually reported, so an operator can pre-name a field before it ever
// shows up in telemetry.
func (s *Server) ListDeviceFieldSettings(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil {
		adminErr(w, 400, "invalid id")
		return
	}
	if _, err := s.Store.DeviceByID(uint(id)); errors.Is(err, store.ErrNotFound) {
		adminErr(w, 404, "device not found")
		return
	} else if err != nil {
		adminErr(w, 500, "internal error")
		return
	}

	overrideRows, err := s.Store.ListDeviceFieldSettings(uint(id))
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	templateRows, err := s.Store.ListIcons()
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}

	overrides := overridesByKey(overrideRows)
	templates := templatesByKey(templateRows)
	list := make([]deviceFieldSettingDTO, 0, len(store.FieldKeys))
	for _, k := range store.FieldKeys {
		list = append(list, resolveFieldSetting(k, overrides, templates))
	}
	writeJSON(w, 200, map[string]any{"list": list})
}

type deviceFieldSettingRequest struct {
	Name string `json:"name"`
	Unit string `json:"unit"`
	SVG  string `json:"svg"`
}

// UpsertDeviceFieldSetting handles POST
// /api/admin/devices/{id}/field-settings/{key} -- creates or replaces this
// device's override for key. Any of name/unit/svg left empty just means
// "don't customize this attribute" (it keeps falling back to the template/
// raw key), matching store.UpsertIcon's re-upload-to-change workflow.
func (s *Server) UpsertDeviceFieldSetting(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil {
		adminErr(w, 400, "invalid id")
		return
	}
	key := r.PathValue("key")
	if key == "" {
		adminErr(w, 400, "invalid key")
		return
	}
	if _, err := s.Store.DeviceByID(uint(id)); errors.Is(err, store.ErrNotFound) {
		adminErr(w, 404, "device not found")
		return
	} else if err != nil {
		adminErr(w, 500, "internal error")
		return
	}

	var req deviceFieldSettingRequest
	if err := decodeJSON(r, &req); err != nil {
		adminErr(w, 400, "malformed request body")
		return
	}
	if len(req.SVG) > maxIconSVGBytes {
		adminErr(w, 400, "svg too large")
		return
	}
	if req.SVG != "" && !strings.Contains(req.SVG, "<svg") {
		adminErr(w, 400, "not a valid svg")
		return
	}

	if _, err := s.Store.UpsertDeviceFieldSetting(uint(id), key, req.Name, req.Unit, req.SVG); err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	s.audit(r, "device.field_setting.upsert", "device", uint(id), key)

	overrideRows, err := s.Store.ListDeviceFieldSettings(uint(id))
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	templateRows, err := s.Store.ListIcons()
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	writeJSON(w, 200, resolveFieldSetting(key, overridesByKey(overrideRows), templatesByKey(templateRows)))
}

// DeleteDeviceFieldSetting handles DELETE
// /api/admin/devices/{id}/field-settings/{key} -- reverts this device's key
// back to the template default (or raw key/built-in icon if there's no
// template either).
func (s *Server) DeleteDeviceFieldSetting(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil {
		adminErr(w, 400, "invalid id")
		return
	}
	key := r.PathValue("key")
	if key == "" {
		adminErr(w, 400, "invalid key")
		return
	}

	if err := s.Store.DeleteDeviceFieldSetting(uint(id), key); err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	s.audit(r, "device.field_setting.reset", "device", uint(id), key)
	writeJSON(w, 200, map[string]any{"message": "ok"})
}
