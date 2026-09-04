package api

import (
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/ubibot/ubibot-open-server/internal/auth"
	"github.com/ubibot/ubibot-open-server/internal/model"
	"github.com/ubibot/ubibot-open-server/internal/store"
)

// --- request/response shapes -------------------------------------------
// These are this app's own admin REST API, not the device wire protocol —
// unlike internal/protocol, there's no external doc governing their
// shape, so they live next to the handlers that use them.

type loginRequest struct {
	Username string `json:"username"`
	Password string `json:"password"`
}

type loginResponse struct {
	Token     string `json:"token"`
	ExpiresIn int64  `json:"expires_in"`
	Username  string `json:"username"`
}

// deviceDTO is deliberately tiny per docs §7: a device only has an
// identity (pid/sn), a display name, an enable/disable status, and
// observed state (online/last-seen/created). There's no secret, source,
// activation flag, or per-device config to show anymore. PendingCommand
// (docs §9) is the one queued-but-not-yet-delivered command, if any, shown
// as-is (the exact object the device will receive as "cmd") so the admin
// console can render "queued: reboot" etc.; omitted once delivered.
// ProductName is resolved by matching PID against a Product row (docs §7's
// "产品/型号管理") — display-only, absent when no Product is registered for
// this pid.
type deviceDTO struct {
	ID             uint            `json:"id"`
	PID            string          `json:"pid"`
	SN             string          `json:"sn"`
	Name           string          `json:"name"`
	Status         int             `json:"status"`
	Online         bool            `json:"online"`
	LastSeenAt     *int64          `json:"last_seen_at"`
	CreatedAt      int64           `json:"created_at"`
	PendingCommand json.RawMessage `json:"pending_command,omitempty"`
	ProductName    string          `json:"product_name,omitempty"`
}

// toDeviceDTO's Online field uses the same rule (store.IsDeviceOnline) the
// offline-alert sweep does, so the device list/detail view and the alert
// center never disagree about which devices are up. now is the caller's
// s.Now() rather than time.Now() directly so this stays testable against
// a mocked clock. products resolves ProductName by d.PID -- pass nil (or a
// map that just doesn't contain this pid) when the caller has no use for
// it; a nil map read is a safe no-op in Go, not a panic.
func toDeviceDTO(d *model.Device, now time.Time, products map[string]model.Product) deviceDTO {
	dto := deviceDTO{
		ID:        d.ID,
		PID:       d.PID,
		SN:        d.SN,
		Name:      d.Name,
		Status:    d.Status,
		Online:    store.IsDeviceOnline(d, now),
		CreatedAt: d.CreatedAt.Unix(),
	}
	if p, ok := products[d.PID]; ok {
		dto.ProductName = p.Name
	}
	if d.LastSeenAt != nil {
		t := d.LastSeenAt.Unix()
		dto.LastSeenAt = &t
	}
	if d.PendingCmd != "" {
		dto.PendingCommand = json.RawMessage(d.PendingCmd)
	}
	return dto
}

type recordDTO struct {
	Ts int64          `json:"ts"`
	D  map[string]any `json:"d"`
}

func paginationParams(r *http.Request) (page, pageSize int) {
	page, _ = strconv.Atoi(r.URL.Query().Get("page"))
	pageSize, _ = strconv.Atoi(r.URL.Query().Get("page_size"))
	if page < 1 {
		page = 1
	}
	if pageSize < 1 {
		pageSize = 20
	}
	return page, pageSize
}

// --- handlers ------------------------------------------------------------

// AdminLogin handles POST /api/admin/login.
func (s *Server) AdminLogin(w http.ResponseWriter, r *http.Request) {
	var req loginRequest
	if err := decodeJSON(r, &req); err != nil || req.Username == "" || req.Password == "" {
		adminErr(w, 400, "username and password are required")
		return
	}

	admin, err := s.Store.AdminByUsername(req.Username)
	if err != nil {
		adminErr(w, 401, "invalid username or password")
		return
	}
	if !auth.VerifyPassword(admin.PasswordHash, req.Password) {
		adminErr(w, 401, "invalid username or password")
		return
	}

	token, ttl, err := s.Store.IssueAdminSession(admin.ID)
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}

	writeAPIJSON(w, 200, loginResponse{Token: token, ExpiresIn: int64(ttl.Seconds()), Username: admin.Username})
}

// AdminMe handles GET /api/admin/me — lets the frontend show who's logged
// in without decoding anything client-side.
func (s *Server) AdminMe(w http.ResponseWriter, r *http.Request) {
	admin := currentAdmin(r)
	writeAPIJSON(w, 200, map[string]any{"username": admin.Username})
}

// productsForDevices batch-resolves the Product row (if any) for every
// distinct pid among devices, for annotating a page of them with
// product_name in one extra query instead of one per device.
func (s *Server) productsForDevices(devices []model.Device) (map[string]model.Product, error) {
	seen := make(map[string]struct{}, len(devices))
	pids := make([]string, 0, len(devices))
	for i := range devices {
		if _, ok := seen[devices[i].PID]; !ok {
			seen[devices[i].PID] = struct{}{}
			pids = append(pids, devices[i].PID)
		}
	}
	return s.Store.ProductsByPIDs(pids)
}

// ListDevices handles GET /api/admin/devices.
func (s *Server) ListDevices(w http.ResponseWriter, r *http.Request) {
	page, pageSize := paginationParams(r)

	devices, total, err := s.Store.ListDevices(page, pageSize)
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	products, err := s.productsForDevices(devices)
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}

	list := make([]deviceDTO, 0, len(devices))
	for i := range devices {
		list = append(list, toDeviceDTO(&devices[i], s.Now(), products))
	}
	writeAPIJSON(w, 200, map[string]any{"list": list, "total": total})
}

// fieldMetaDTO is a resolved field1..field20 display name/unit/icon for one
// entry of a dataWarehouseItemDTO.LastRecord -- the same fallback chain as
// deviceFieldSettingDTO (device override, else template, else empty), just
// trimmed to what the 数据仓库 list actually renders per row.
type fieldMetaDTO struct {
	Name string `json:"name"`
	Unit string `json:"unit"`
	SVG  string `json:"svg"`
}

// dataWarehouseItemDTO is a deviceDTO plus that device's single most recent
// telemetry record (nil if it has never reported), for the "数据仓库" list's
// sensor-data preview column. Embedding deviceDTO flattens its fields into
// this one's JSON object (id/pid/sn/... alongside last_record). FieldMeta
// covers only the keys present in LastRecord.D (not the full field1..
// field20 set) since that's all this row ever renders.
type dataWarehouseItemDTO struct {
	deviceDTO
	LastRecord *recordDTO              `json:"last_record"`
	FieldMeta  map[string]fieldMetaDTO `json:"field_meta,omitempty"`
}

// ListDataWarehouse handles GET /api/admin/devices/data-warehouse — like
// ListDevices, annotated with each device's latest report, so the frontend
// can render a live sensor-data preview per row without an extra request
// per device. Every device in the table has reported at least once by
// construction (see store.GetOrCreateDeviceBySN), so unlike the old
// "activated devices only" filter, this is now just ListDevices plus the
// latest-record join. FieldMeta is resolved here (rather than making the
// frontend fetch each device's field-settings separately) for the same
// reason the latest-record join is: N devices at once would otherwise mean
// N extra requests.
func (s *Server) ListDataWarehouse(w http.ResponseWriter, r *http.Request) {
	page, pageSize := paginationParams(r)

	devices, total, err := s.Store.ListDevices(page, pageSize)
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}

	ids := make([]uint, len(devices))
	for i := range devices {
		ids[i] = devices[i].ID
	}
	latest, err := s.Store.LatestRecordsByDevice(ids)
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	overrideRows, err := s.Store.ListDeviceFieldSettingsForDevices(ids)
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	templateRows, err := s.Store.ListIcons()
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	templates := templatesByKey(templateRows)
	overridesByDevice := make(map[uint]map[string]model.DeviceFieldSetting, len(devices))
	for _, o := range overrideRows {
		m, ok := overridesByDevice[o.DeviceID]
		if !ok {
			m = make(map[string]model.DeviceFieldSetting)
			overridesByDevice[o.DeviceID] = m
		}
		m[strings.ToLower(o.FieldKey)] = o
	}
	products, err := s.productsForDevices(devices)
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}

	now := s.Now()
	list := make([]dataWarehouseItemDTO, 0, len(devices))
	for i := range devices {
		item := dataWarehouseItemDTO{deviceDTO: toDeviceDTO(&devices[i], now, products)}
		if rec, ok := latest[devices[i].ID]; ok {
			var d map[string]any
			_ = json.Unmarshal([]byte(rec.Data), &d)
			item.LastRecord = &recordDTO{Ts: rec.Ts, D: d}

			overrides := overridesByDevice[devices[i].ID]
			meta := make(map[string]fieldMetaDTO, len(d))
			for k := range d {
				resolved := resolveFieldSetting(k, overrides, templates)
				meta[k] = fieldMetaDTO{Name: resolved.Name, Unit: resolved.Unit, SVG: resolved.SVG}
			}
			item.FieldMeta = meta
		}
		list = append(list, item)
	}
	writeAPIJSON(w, 200, map[string]any{"list": list, "total": total})
}

// GetDevice handles GET /api/admin/devices/{id} — detail view with recent
// telemetry, enough for "后台能看" without a full historical query UI
// (that's GetDeviceRecords).
func (s *Server) GetDevice(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil {
		adminErr(w, 400, "invalid id")
		return
	}

	dev, err := s.Store.DeviceByID(uint(id))
	if errors.Is(err, store.ErrNotFound) {
		adminErr(w, 404, "device not found")
		return
	}
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}

	records, err := s.Store.RecentRecords(dev.ID, 20)
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	recordDTOs := make([]recordDTO, 0, len(records))
	for _, rec := range records {
		var d map[string]any
		_ = json.Unmarshal([]byte(rec.Data), &d)
		recordDTOs = append(recordDTOs, recordDTO{Ts: rec.Ts, D: d})
	}

	products, err := s.Store.ProductsByPIDs([]string{dev.PID})
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}

	writeAPIJSON(w, 200, map[string]any{
		"device":  toDeviceDTO(dev, s.Now(), products),
		"records": recordDTOs,
	})
}

type renameDeviceRequest struct {
	Name string `json:"name"`
}

// RenameDevice handles PATCH /api/admin/devices/{id} — the only thing
// about a device an operator can configure after it auto-appears (see
// docs §7). An empty name is allowed (clears back to showing the SN in
// the frontend).
func (s *Server) RenameDevice(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil {
		adminErr(w, 400, "invalid id")
		return
	}

	var req renameDeviceRequest
	if err := decodeJSON(r, &req); err != nil {
		adminErr(w, 400, "malformed request body")
		return
	}

	if err := s.Store.RenameDevice(uint(id), req.Name); err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	s.audit(r, "device.rename", "device", uint(id), req.Name)

	dev, err := s.Store.DeviceByID(uint(id))
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	products, err := s.Store.ProductsByPIDs([]string{dev.PID})
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	writeAPIJSON(w, 200, toDeviceDTO(dev, s.Now(), products))
}

type setStatusRequest struct {
	Status int `json:"status"`
}

// SetDeviceStatus handles POST /api/admin/devices/{id}/status — the
// enable/disable toggle. A disabled device is rejected by every
// device-facing endpoint (docs §7/§8, code 1103).
func (s *Server) SetDeviceStatus(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil {
		adminErr(w, 400, "invalid id")
		return
	}

	var req setStatusRequest
	if err := decodeJSON(r, &req); err != nil {
		adminErr(w, 400, "status is required")
		return
	}
	if req.Status != model.DeviceStatusEnabled && req.Status != model.DeviceStatusDisabled {
		adminErr(w, 400, "invalid status")
		return
	}

	if err := s.Store.SetDeviceStatus(uint(id), req.Status); err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	s.audit(r, "device.set_status", "device", uint(id), strconv.Itoa(req.Status))
	writeAPIJSON(w, 200, map[string]any{"message": "ok"})
}

// sendCommandRequest is the body for POST .../commands. Seconds only
// applies to (and is required by) "set_interval"; it's ignored otherwise.
type sendCommandRequest struct {
	Action  string `json:"action"`
	Seconds int    `json:"seconds"`
}

// minReportIntervalSeconds/maxReportIntervalSeconds bound what an operator
// can push via set_interval — 1 minute floor so a fat-fingered value can't
// turn a device into a busy-loop that hammers the server and drains its
// battery, 1 day ceiling because anything looser isn't really "periodic
// reporting" anymore. The firmware doesn't enforce this range itself
// (docs §9) — it takes whatever seconds value it's told — so it's on the
// admin API to keep it sane before it's ever queued.
const (
	minReportIntervalSeconds = 60
	maxReportIntervalSeconds = 86400
)

// SendDeviceCommand handles POST /api/admin/devices/{id}/commands — queues
// a command for delivery on the device's next report (docs §9: "reboot" or
// "set_interval"). Only one command is ever queued per device; sending a
// new one overwrites whatever hadn't been delivered yet. This is
// fire-and-forget — there's no ack channel, so the platform has no way to
// confirm the device actually received or applied it; if in doubt, send it
// again once you'd expect the device to have reported by.
func (s *Server) SendDeviceCommand(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil {
		adminErr(w, 400, "invalid id")
		return
	}

	var req sendCommandRequest
	if err := decodeJSON(r, &req); err != nil {
		adminErr(w, 400, "malformed request body")
		return
	}

	var cmdJSON []byte
	switch req.Action {
	case "reboot":
		cmdJSON, _ = json.Marshal(map[string]any{"action": "reboot"})
	case "set_interval":
		if req.Seconds < minReportIntervalSeconds || req.Seconds > maxReportIntervalSeconds {
			adminErr(w, 400, fmt.Sprintf("seconds must be between %d and %d", minReportIntervalSeconds, maxReportIntervalSeconds))
			return
		}
		cmdJSON, _ = json.Marshal(map[string]any{"action": "set_interval", "seconds": req.Seconds})
	default:
		adminErr(w, 400, "unsupported action")
		return
	}

	if _, err := s.Store.DeviceByID(uint(id)); errors.Is(err, store.ErrNotFound) {
		adminErr(w, 404, "device not found")
		return
	} else if err != nil {
		adminErr(w, 500, "internal error")
		return
	}

	if err := s.Store.SetPendingCommand(uint(id), string(cmdJSON)); err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	s.audit(r, "device.send_command", "device", uint(id), string(cmdJSON))
	writeAPIJSON(w, 200, map[string]any{"message": "queued", "cmd": json.RawMessage(cmdJSON)})
}

// CancelDeviceCommand handles DELETE /api/admin/devices/{id}/commands —
// cancels a command that hasn't been delivered yet. A no-op (not an error)
// if nothing was queued, or if it already went out on the device's last
// report — there's no way to un-deliver that.
func (s *Server) CancelDeviceCommand(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil {
		adminErr(w, 400, "invalid id")
		return
	}

	if err := s.Store.SetPendingCommand(uint(id), ""); err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	s.audit(r, "device.cancel_command", "device", uint(id), "")
	writeAPIJSON(w, 200, map[string]any{"message": "ok"})
}

// DeleteDevice handles DELETE /api/admin/devices/{id} — permanently
// removes the device and every record that references it (see
// store.DeleteDevice). Irreversible; the frontend is expected to confirm
// with the operator before ever calling this.
func (s *Server) DeleteDevice(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil {
		adminErr(w, 400, "invalid id")
		return
	}

	dev, err := s.Store.DeviceByID(uint(id))
	if errors.Is(err, store.ErrNotFound) {
		adminErr(w, 404, "device not found")
		return
	}
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}

	if err := s.Store.DeleteDevice(uint(id)); err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	s.audit(r, "device.delete", "device", uint(id), dev.SN)
	writeAPIJSON(w, 200, map[string]any{"message": "ok"})
}

// GetDeviceRecords handles GET /api/admin/devices/{id}/records?start=&end=
// — the "历史数据查询" page's backing endpoint. start/end are Unix
// seconds; omit either to leave that bound open.
func (s *Server) GetDeviceRecords(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil {
		adminErr(w, 400, "invalid id")
		return
	}
	start, _ := strconv.ParseInt(r.URL.Query().Get("start"), 10, 64)
	end, _ := strconv.ParseInt(r.URL.Query().Get("end"), 10, 64)
	page, pageSize := paginationParams(r)

	records, total, err := s.Store.QueryRecords(uint(id), start, end, page, pageSize)
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	list := make([]recordDTO, 0, len(records))
	for _, rec := range records {
		var d map[string]any
		_ = json.Unmarshal([]byte(rec.Data), &d)
		list = append(list, recordDTO{Ts: rec.Ts, D: d})
	}
	writeAPIJSON(w, 200, map[string]any{"list": list, "total": total})
}
