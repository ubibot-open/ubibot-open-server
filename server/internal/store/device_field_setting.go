package store

import (
	"errors"
	"fmt"

	"gorm.io/gorm"

	"github.com/ubibot/ubibot-platform-open/internal/model"
)

// FieldKeys is the fixed field1..field20 key set every device's field
// settings page shows (see docs §6) -- independent of which ones a given
// device has actually reported so far.
var FieldKeys = buildFieldKeys()

func buildFieldKeys() []string {
	keys := make([]string, 20)
	for i := range keys {
		keys[i] = fmt.Sprintf("field%d", i+1)
	}
	return keys
}

// ListDeviceFieldSettings returns every field customized for one device --
// there is no row for a field that has never been customized (see
// UpsertDeviceFieldSetting), so this is often a small subset of FieldKeys.
func (s *Store) ListDeviceFieldSettings(deviceID uint) ([]model.DeviceFieldSetting, error) {
	var rows []model.DeviceFieldSetting
	err := s.db.Where("device_id = ?", deviceID).Find(&rows).Error
	return rows, err
}

// ListDeviceFieldSettingsForDevices returns every customized field across
// several devices in one query -- used by the 数据仓库 list so rendering N
// devices' sensor tags doesn't cost N extra requests.
func (s *Store) ListDeviceFieldSettingsForDevices(deviceIDs []uint) ([]model.DeviceFieldSetting, error) {
	if len(deviceIDs) == 0 {
		return nil, nil
	}
	var rows []model.DeviceFieldSetting
	err := s.db.Where("device_id IN ?", deviceIDs).Find(&rows).Error
	return rows, err
}

// UpsertDeviceFieldSetting creates or replaces one device's override for
// key. Name/Unit/SVG may be passed empty -- an empty attribute just means
// "not customized", which the caller (api.resolveFieldSetting) then falls
// back to the matching IconAsset template for, then to the raw key with no
// unit and the built-in icon.
func (s *Store) UpsertDeviceFieldSetting(deviceID uint, key, name, unit, svg string) (*model.DeviceFieldSetting, error) {
	var existing model.DeviceFieldSetting
	err := s.db.Where("device_id = ? AND field_key = ?", deviceID, key).First(&existing).Error
	if err == nil {
		existing.Name = name
		existing.Unit = unit
		existing.SVG = svg
		if err := s.db.Save(&existing).Error; err != nil {
			return nil, err
		}
		return &existing, nil
	}
	if !errors.Is(err, gorm.ErrRecordNotFound) {
		return nil, err
	}

	row := &model.DeviceFieldSetting{DeviceID: deviceID, FieldKey: key, Name: name, Unit: unit, SVG: svg}
	if err := s.db.Create(row).Error; err != nil {
		return nil, err
	}
	return row, nil
}

// DeleteDeviceFieldSetting removes a device's override for key, if any,
// reverting it back to the global template (or raw key/built-in icon if
// there's no template either). A no-op for a field that was never
// customized, same as store.DeleteIcon.
func (s *Store) DeleteDeviceFieldSetting(deviceID uint, key string) error {
	return s.db.Where("device_id = ? AND field_key = ?", deviceID, key).Delete(&model.DeviceFieldSetting{}).Error
}
