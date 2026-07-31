import { api } from './client'

// DeviceFieldSetting is one field1..field20 entry for a specific device,
// already resolved server-side through the fallback chain: this device's
// own override, else the global template (see api/icon.ts), else empty --
// an empty name/unit means "show the raw field key with no unit" and an
// empty svg means "use the built-in icon set" (see
// hooks/useDeviceFieldSettings.tsx). is_custom reflects whether this
// device has its own override row at all, used to decide whether to offer
// a "reset to default" action.
export interface DeviceFieldSetting {
  key: string
  name: string
  unit: string
  svg: string
  is_custom: boolean
}

// listDeviceFieldSettings always returns all of field1..field20, in that
// order, regardless of which ones this device has actually reported yet.
export function listDeviceFieldSettings(deviceId: number) {
  return api.get<{ list: DeviceFieldSetting[] }>(`/api/admin/devices/${deviceId}/field-settings`)
}

// upsertDeviceFieldSetting creates or replaces this device's override for
// key -- there's no separate update endpoint, re-saving is the update.
// Leaving svg out (or empty) keeps whatever icon this field currently
// falls back to.
export function upsertDeviceFieldSetting(
  deviceId: number,
  key: string,
  input: { name: string; unit: string; svg?: string },
) {
  return api.post<DeviceFieldSetting>(
    `/api/admin/devices/${deviceId}/field-settings/${encodeURIComponent(key)}`,
    input,
  )
}

// resetDeviceFieldSetting reverts key back to the template default (or raw
// key/built-in icon if there's no template either).
export function resetDeviceFieldSetting(deviceId: number, key: string) {
  return api.del<{ message: string }>(
    `/api/admin/devices/${deviceId}/field-settings/${encodeURIComponent(key)}`,
  )
}
