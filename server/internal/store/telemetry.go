package store

import (
	"encoding/json"
	"sort"

	"gorm.io/gorm"
	"gorm.io/gorm/clause"

	"github.com/ubibot/ubibot-platform-open/internal/model"
	"github.com/ubibot/ubibot-platform-open/internal/protocol"
)

// FieldMergeWindow bounds how far apart, in time, two payloads[] entries'
// ts may be and still be folded into the same stored record (docs §5). A
// device that's been offline can buffer several sampling rounds and upload
// them all in one request, and even a single round's own fields may be
// split across a few entries with slightly different ts (sequential sensor
// reads finishing a few seconds apart) -- those should merge. But entries
// far apart in time are genuinely separate rounds and must stay separate
// records, or a slow drift across a long buffered batch would collapse
// unrelated samples into one row. 1 minute is a deliberately generous
// example of "same round"; tune here if real hardware needs otherwise.
const FieldMergeWindow = 60

// SaveRecords persists payloads for deviceID, first grouping them into
// rounds: payloads are sorted by ts, then swept in order -- each group
// starts at the first not-yet-grouped entry (its ts becomes that group's
// anchor) and keeps absorbing subsequent entries whose ts is within
// FieldMergeWindow seconds of the anchor; the first entry beyond that
// starts a new group instead of being folded in (see FieldMergeWindow).
// The anchor never moves once set, so a chain of many close-together
// entries can't drift the group's effective span past the window. Every
// group becomes one model.DeviceRecord keyed by its anchor ts. Duplicate
// (device_id, ts) pairs *across* requests are silently ignored via ON
// CONFLICT DO NOTHING against the unique index on model.DeviceRecord —
// this is the "同一时间点去重" requirement, enforced by the database
// instead of an application-level check-then-insert (which would race
// under concurrent uploads).
func (s *Store) SaveRecords(deviceID uint, payloads []protocol.Payload) error {
	if len(payloads) == 0 {
		return nil
	}

	sorted := make([]protocol.Payload, len(payloads))
	copy(sorted, payloads)
	sort.SliceStable(sorted, func(i, j int) bool { return sorted[i].Ts < sorted[j].Ts })

	type group struct {
		anchor int64
		fields map[string]float64
	}
	groups := make([]*group, 0, len(sorted))
	for _, p := range sorted {
		var g *group
		if n := len(groups); n > 0 {
			last := groups[n-1]
			if p.Ts-last.anchor <= FieldMergeWindow {
				g = last
			}
		}
		if g == nil {
			g = &group{anchor: p.Ts, fields: make(map[string]float64, len(p.Fields))}
			groups = append(groups, g)
		}
		for k, v := range p.Fields {
			g.fields[k] = v
		}
	}

	rows := make([]model.DeviceRecord, 0, len(groups))
	for _, g := range groups {
		data, err := json.Marshal(g.fields)
		if err != nil {
			return err
		}
		rows = append(rows, model.DeviceRecord{DeviceID: deviceID, Ts: g.anchor, Data: string(data)})
	}

	return s.db.Clauses(clause.OnConflict{DoNothing: true}).Create(&rows).Error
}

// LatestRecordsByDevice returns, for each ID in deviceIDs that has at least
// one stored record, only that device's single most recent DeviceRecord —
// one query instead of len(deviceIDs) separate ones, via a window function
// over the existing (device_id, ts) index. Backs the "数据仓库" list's
// per-row sensor-data preview.
func (s *Store) LatestRecordsByDevice(deviceIDs []uint) (map[uint]model.DeviceRecord, error) {
	result := make(map[uint]model.DeviceRecord, len(deviceIDs))
	if len(deviceIDs) == 0 {
		return result, nil
	}

	var rows []model.DeviceRecord
	err := s.db.Raw(`
		SELECT id, device_id, ts, data, created_at FROM (
			SELECT *, ROW_NUMBER() OVER (PARTITION BY device_id ORDER BY ts DESC) AS rn
			FROM device_records
			WHERE device_id IN ?
		) WHERE rn = 1
	`, deviceIDs).Scan(&rows).Error
	if err != nil {
		return nil, err
	}
	for _, r := range rows {
		result[r.DeviceID] = r
	}
	return result, nil
}

// RecentRecords returns a device's most recent telemetry, newest first —
// used by the admin device-detail page's "最近上报数据" panel.
func (s *Store) RecentRecords(deviceID uint, limit int) ([]model.DeviceRecord, error) {
	if limit < 1 || limit > 200 {
		limit = 20
	}
	var rows []model.DeviceRecord
	err := s.db.Where("device_id = ?", deviceID).
		Order("ts desc").
		Limit(limit).
		Find(&rows).Error
	return rows, err
}

// QueryRecords returns a device's telemetry within [start, end] (Unix
// seconds; either may be 0 to leave that bound open), oldest first — this
// is the "历史数据查询" page's backing query, as opposed to RecentRecords'
// fixed newest-first snapshot for the detail view.
func (s *Store) QueryRecords(deviceID uint, start, end int64, page, pageSize int) ([]model.DeviceRecord, int64, error) {
	if page < 1 {
		page = 1
	}
	if pageSize < 1 || pageSize > 500 {
		pageSize = 100
	}

	scope := func(db *gorm.DB) *gorm.DB {
		db = db.Where("device_id = ?", deviceID)
		if start > 0 {
			db = db.Where("ts >= ?", start)
		}
		if end > 0 {
			db = db.Where("ts <= ?", end)
		}
		return db
	}

	var total int64
	if err := scope(s.db.Model(&model.DeviceRecord{})).Count(&total).Error; err != nil {
		return nil, 0, err
	}

	var rows []model.DeviceRecord
	err := scope(s.db).Order("ts asc").Offset((page - 1) * pageSize).Limit(pageSize).Find(&rows).Error
	if err != nil {
		return nil, 0, err
	}
	return rows, total, nil
}

// CountRecordsSince returns how many telemetry rows have ts >= since —
// used by the dashboard summary for "今日上报条数".
func (s *Store) CountRecordsSince(since int64) (int64, error) {
	var n int64
	err := s.db.Model(&model.DeviceRecord{}).Where("ts >= ?", since).Count(&n).Error
	return n, err
}

// DailyRecordCount is one bucket of RecordCountsByDay's result.
type DailyRecordCount struct {
	Day   string `json:"day"`
	Count int64  `json:"count"`
}

// RecordCountsByDay buckets telemetry rows by calendar day (UTC) for
// ts >= since — the dashboard trend chart's backing query.
func (s *Store) RecordCountsByDay(since int64) ([]DailyRecordCount, error) {
	var rows []DailyRecordCount
	err := s.db.Model(&model.DeviceRecord{}).
		Select("date(ts, 'unixepoch') as day, count(*) as count").
		Where("ts >= ?", since).
		Group("day").
		Order("day asc").
		Scan(&rows).Error
	return rows, err
}
