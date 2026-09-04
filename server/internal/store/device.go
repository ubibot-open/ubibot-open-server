package store

import (
	"errors"
	"time"

	"gorm.io/gorm"

	"github.com/ubibot/ubibot-open-server/internal/model"
)

var ErrNotFound = errors.New("not found")
var ErrAlreadyExists = errors.New("already exists")

// MinOfflineGrace is how long a device can go quiet before it's considered
// offline. Per docs §5/§7, devices no longer tell the platform their
// upload interval (there's no cfg push anymore), so this is now a single
// fixed floor for every device rather than a per-device multiplier.
const MinOfflineGrace = 2 * time.Minute

// minOfflineGraceOverride lets the "offline_grace_minutes" system
// parameter (see param.go, wired from main.go) raise the floor above the
// MinOfflineGrace default without needing a code change. Zero means "no
// override, use the constant" — tests never touch this, so existing
// online/offline assertions keep their fixed 2-minute floor.
var minOfflineGraceOverride time.Duration

// SetMinOfflineGrace overrides the offline-grace floor at runtime. Pass 0
// to fall back to the MinOfflineGrace constant.
func SetMinOfflineGrace(d time.Duration) {
	minOfflineGraceOverride = d
}

// IsDeviceOnline reports whether dev has reported within the grace period
// — used so the admin device list/detail view and the alert system never
// disagree about what "online" means.
func IsDeviceOnline(dev *model.Device, now time.Time) bool {
	if dev.LastSeenAt == nil {
		return false
	}
	grace := MinOfflineGrace
	if minOfflineGraceOverride > 0 {
		grace = minOfflineGraceOverride
	}
	return now.Sub(*dev.LastSeenAt) <= grace
}

// GetOrCreateDeviceBySN is the entire "provisioning" story per docs §5: a
// device identifies itself with pid+sn and nothing else, so the first
// successful report from an SN the platform hasn't seen creates the row
// on the spot — no admin action, no secret, no pre-registration. created
// is true only when this call is what created the row (useful for
// callers that want to log/audit first contact).
func (s *Store) GetOrCreateDeviceBySN(pid, sn string) (dev *model.Device, created bool, err error) {
	dev, err = s.DeviceBySN(sn)
	if err == nil {
		return dev, false, nil
	}
	if !errors.Is(err, ErrNotFound) {
		return nil, false, err
	}

	d := &model.Device{
		PID:    pid,
		SN:     sn,
		Status: model.DeviceStatusEnabled,
	}
	if err := s.db.Create(d).Error; err != nil {
		// Likely a race with a concurrent first-contact request for the
		// same SN (unique index) — fetch whichever row won instead of
		// failing the report outright.
		if dev, ferr := s.DeviceBySN(sn); ferr == nil {
			return dev, false, nil
		}
		return nil, false, err
	}
	return d, true, nil
}

// CreateDeviceForImport pre-registers a device an admin already knows the
// pid/sn/name of ahead of time (e.g. from a manufacturing batch's serial
// list) — the batch-import counterpart to GetOrCreateDeviceBySN's
// auto-creation on first report. Returns ErrAlreadyExists (not a generic
// error) if sn is already taken, so a bulk import can report per-row
// created/skipped counts instead of aborting the whole batch on the first
// duplicate. The pre-created row behaves exactly like an auto-created one
// once the real device reports: GetOrCreateDeviceBySN matches it by sn.
func (s *Store) CreateDeviceForImport(pid, sn, name string) (*model.Device, error) {
	if _, err := s.DeviceBySN(sn); err == nil {
		return nil, ErrAlreadyExists
	} else if !errors.Is(err, ErrNotFound) {
		return nil, err
	}

	d := &model.Device{PID: pid, SN: sn, Name: name, Status: model.DeviceStatusEnabled}
	if err := s.db.Create(d).Error; err != nil {
		return nil, err
	}
	return d, nil
}

// ListAllDevices returns every device, newest first, with no pagination —
// backs the "export all devices to CSV" admin action. ListDevices' 200-row
// page cap makes it unsuitable for that (a real fleet can exceed it).
func (s *Store) ListAllDevices() ([]model.Device, error) {
	var devices []model.Device
	err := s.db.Order("id desc").Find(&devices).Error
	return devices, err
}

// DeleteDevice permanently removes a device and its telemetry/alert
// history. Irreversible; the admin frontend confirms with the operator
// before calling this.
func (s *Store) DeleteDevice(id uint) error {
	return s.db.Transaction(func(tx *gorm.DB) error {
		deletes := []func() error{
			func() error { return tx.Where("device_id = ?", id).Delete(&model.DeviceRecord{}).Error },
			func() error { return tx.Where("device_id = ?", id).Delete(&model.AlertRule{}).Error },
			func() error { return tx.Where("device_id = ?", id).Delete(&model.AlertEvent{}).Error },
		}
		for _, del := range deletes {
			if err := del(); err != nil {
				return err
			}
		}
		return tx.Delete(&model.Device{}, id).Error
	})
}

// RenameDevice sets a device's display name — the only thing about a
// device an operator can configure after it appears (see docs §7).
func (s *Store) RenameDevice(id uint, name string) error {
	return s.db.Model(&model.Device{}).Where("id = ?", id).Update("name", name).Error
}

// DeviceBySN looks up a device by serial number.
func (s *Store) DeviceBySN(sn string) (*model.Device, error) {
	var d model.Device
	if err := s.db.Where("sn = ?", sn).First(&d).Error; err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			return nil, ErrNotFound
		}
		return nil, err
	}
	return &d, nil
}

func (s *Store) DeviceByID(id uint) (*model.Device, error) {
	var d model.Device
	if err := s.db.First(&d, id).Error; err != nil {
		if errors.Is(err, gorm.ErrRecordNotFound) {
			return nil, ErrNotFound
		}
		return nil, err
	}
	return &d, nil
}

// ListDevices returns a page of devices ordered newest-first, plus the
// total row count for pagination. Every device in this table has, by
// construction (see GetOrCreateDeviceBySN), reported at least once — there
// is no more "provisioned but never activated" state to filter out, so
// this is also what backs 数据仓库 (see api.ListDataWarehouse).
func (s *Store) ListDevices(page, pageSize int) ([]model.Device, int64, error) {
	if page < 1 {
		page = 1
	}
	if pageSize < 1 || pageSize > 200 {
		pageSize = 20
	}

	var total int64
	if err := s.db.Model(&model.Device{}).Count(&total).Error; err != nil {
		return nil, 0, err
	}

	var devices []model.Device
	err := s.db.Order("id desc").
		Offset((page - 1) * pageSize).
		Limit(pageSize).
		Find(&devices).Error
	if err != nil {
		return nil, 0, err
	}
	return devices, total, nil
}

// SetDeviceStatus enables or disables a device (model.DeviceStatusEnabled /
// model.DeviceStatusDisabled). A disabled device is rejected by every
// device-facing endpoint (see docs §7/§8, code 1103).
func (s *Store) SetDeviceStatus(id uint, status int) error {
	return s.db.Model(&model.Device{}).Where("id = ?", id).Update("status", status).Error
}

// SetPendingCommand queues cmdJSON (the exact JSON object to embed as the
// report response's "cmd" field, e.g. `{"action":"reboot"}`) for delivery
// on this device's next successful report (docs §9). Overwrites any
// not-yet-delivered command — only one is ever queued at a time. Pass ""
// to cancel a pending command without sending a new one.
func (s *Store) SetPendingCommand(id uint, cmdJSON string) error {
	return s.db.Model(&model.Device{}).Where("id = ?", id).Update("pending_cmd", cmdJSON).Error
}

// PopPendingCommand returns this device's queued command (docs §9), if
// any, and clears it in the same call. Delivery is at-most-once and
// fire-and-forget: once handed back here to be embedded in a report
// response, the platform considers it delivered whether or not the device
// actually applies it — there's no ack channel to confirm that (see the
// protocol doc's rationale). Returns "" (no error) if nothing is queued or
// the device doesn't exist.
func (s *Store) PopPendingCommand(id uint) (string, error) {
	var dev model.Device
	err := s.db.Select("pending_cmd").First(&dev, id).Error
	if errors.Is(err, gorm.ErrRecordNotFound) {
		return "", nil
	}
	if err != nil {
		return "", err
	}
	if dev.PendingCmd == "" {
		return "", nil
	}
	if err := s.db.Model(&model.Device{}).Where("id = ?", id).Update("pending_cmd", "").Error; err != nil {
		return "", err
	}
	return dev.PendingCmd, nil
}

// TouchLastSeen records that a device just successfully reported in. now
// is caller-supplied (see api.Server.Now) rather than time.Now() directly
// so the online/offline window this feeds (IsDeviceOnline, OfflineSweep)
// can be driven by a test's mocked clock instead of the wall clock.
func (s *Store) TouchLastSeen(id uint, now time.Time) error {
	return s.db.Model(&model.Device{}).Where("id = ?", id).Update("last_seen_at", now).Error
}
