package api

import (
	"encoding/csv"
	"errors"
	"fmt"
	"net/http"
	"strconv"
	"strings"

	"github.com/ubibot/ubibot-open-server/internal/model"
	"github.com/ubibot/ubibot-open-server/internal/store"
)

// maxImportRows caps one batch-import request so a pasted spreadsheet
// mistake (or abuse) can't turn into an unbounded number of inserts in a
// single request.
const maxImportRows = 5000

type importDeviceRow struct {
	SN   string `json:"sn"`
	PID  string `json:"pid"`
	Name string `json:"name"`
}

type importDevicesRequest struct {
	Rows []importDeviceRow `json:"rows"`
}

// ImportDevices handles POST /api/admin/devices/import — bulk pre-registers
// devices ahead of time (docs §7's "批量设备管理"), e.g. from a production
// batch's serial-number list, before any of them have ever reported. This
// is purely a naming/tagging convenience: a device that was never imported
// still auto-registers on its own first report exactly as before (docs
// §5/§7 unchanged) — importing it first just means it shows up with the
// right name/pid immediately instead of after the fact. Per-row results
// let a batch with a few bad/duplicate rows still succeed for the rest
// instead of failing atomically.
func (s *Server) ImportDevices(w http.ResponseWriter, r *http.Request) {
	var req importDevicesRequest
	if err := decodeJSON(r, &req); err != nil || len(req.Rows) == 0 {
		adminErr(w, 400, "rows is required and must not be empty")
		return
	}
	if len(req.Rows) > maxImportRows {
		adminErr(w, 400, fmt.Sprintf("too many rows in one batch (max %d)", maxImportRows))
		return
	}

	created := 0
	skipped := make([]string, 0)
	failed := make([]string, 0)
	for i, row := range req.Rows {
		sn := strings.TrimSpace(row.SN)
		pid := strings.TrimSpace(row.PID)
		if sn == "" || pid == "" {
			failed = append(failed, fmt.Sprintf("row %d: sn and pid are required", i+1))
			continue
		}
		if _, err := s.Store.CreateDeviceForImport(pid, sn, strings.TrimSpace(row.Name)); err != nil {
			if errors.Is(err, store.ErrAlreadyExists) {
				skipped = append(skipped, sn)
			} else {
				failed = append(failed, fmt.Sprintf("row %d (%s): %v", i+1, sn, err))
			}
			continue
		}
		created++
	}

	s.audit(r, "device.import", "device", 0, fmt.Sprintf("created=%d skipped=%d failed=%d", created, len(skipped), len(failed)))
	writeAPIJSON(w, 200, map[string]any{"created": created, "skipped": skipped, "failed": failed})
}

// ExportDevicesCSV handles GET /api/admin/devices/export.csv — every
// device, no pagination (unlike ListDevices' 200-row page cap, unsuitable
// for exporting a real fleet), as a CSV download (docs §7's "批量设备管理").
func (s *Server) ExportDevicesCSV(w http.ResponseWriter, r *http.Request) {
	devices, err := s.Store.ListAllDevices()
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	products, err := s.productsForDevices(devices)
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}

	w.Header().Set("Content-Type", "text/csv; charset=utf-8")
	w.Header().Set("Content-Disposition", `attachment; filename="devices.csv"`)

	cw := csv.NewWriter(w)
	_ = cw.Write([]string{"id", "pid", "product_name", "sn", "name", "status", "online", "last_seen_at", "created_at"})

	now := s.Now()
	for i := range devices {
		dto := toDeviceDTO(&devices[i], now, products)
		lastSeen := ""
		if dto.LastSeenAt != nil {
			lastSeen = strconv.FormatInt(*dto.LastSeenAt, 10)
		}
		status := "disabled"
		if dto.Status == model.DeviceStatusEnabled {
			status = "enabled"
		}
		_ = cw.Write([]string{
			strconv.FormatUint(uint64(dto.ID), 10),
			dto.PID,
			dto.ProductName,
			dto.SN,
			dto.Name,
			status,
			strconv.FormatBool(dto.Online),
			lastSeen,
			strconv.FormatInt(dto.CreatedAt, 10),
		})
	}
	cw.Flush()
}
