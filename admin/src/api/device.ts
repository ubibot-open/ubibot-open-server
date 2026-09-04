import { api } from './client'
import type { FieldMeta } from '../utils/fieldMeta'

// PendingCommand mirrors whatever object the device will receive as "cmd"
// on its next report response (docs §9) — currently either
// { action: 'reboot' } or { action: 'set_interval', seconds: number }.
export interface PendingCommand {
  action: string
  seconds?: number
}

export interface Device {
  id: number
  pid: string
  sn: string
  name: string
  status: number
  online: boolean
  last_seen_at: number | null
  created_at: number
  pending_command?: PendingCommand
}

export interface DeviceRecord {
  ts: number
  d: Record<string, unknown>
}

export const DeviceStatus = {
  Enabled: 1,
  Disabled: 2,
} as const

export function listDevices(page = 1, pageSize = 20) {
  return api.get<{ list: Device[]; total: number }>(`/api/admin/devices?page=${page}&page_size=${pageSize}`)
}

// A Device plus its single most recent telemetry record (null if it has
// never reported) -- backs the "数据仓库" (data warehouse) page. field_meta
// resolves each key present in last_record.d through that device's own
// field settings (falling back to the template library, see
// api/fieldSettings.ts) -- computed server-side so listing N devices
// doesn't cost N extra requests.
export interface DataWarehouseItem extends Device {
  last_record: DeviceRecord | null
  field_meta?: Record<string, FieldMeta>
}

export function listDataWarehouse(page = 1, pageSize = 20) {
  return api.get<{ list: DataWarehouseItem[]; total: number }>(
    `/api/admin/devices/data-warehouse?page=${page}&page_size=${pageSize}`,
  )
}

export function getDevice(id: number) {
  return api.get<{ device: Device; records: DeviceRecord[] }>(`/api/admin/devices/${id}`)
}

// renameDevice is the only per-device config left -- devices otherwise
// appear/disappear purely based on whether they've reported data (see
// docs/UbiBot开放平台硬件通信协议.md §6).
export function renameDevice(id: number, name: string) {
  return api.patch<Device>(`/api/admin/devices/${id}`, { name })
}

export function setDeviceStatus(id: number, status: number) {
  return api.post<{ message: string }>(`/api/admin/devices/${id}/status`, { status })
}

// sendDeviceCommand queues a command for delivery on the device's next
// report (docs §9) — fire-and-forget, no ack: the platform can't confirm
// the device actually received or applied it. Only one command is ever
// queued per device; sending a new one overwrites whatever hadn't been
// delivered yet.
export function sendDeviceCommand(id: number, cmd: PendingCommand) {
  return api.post<{ message: string; cmd: PendingCommand }>(`/api/admin/devices/${id}/commands`, cmd)
}

// cancelDeviceCommand withdraws a not-yet-delivered command. A no-op if
// nothing was queued or it already went out on the device's last report.
export function cancelDeviceCommand(id: number) {
  return api.del<{ message: string }>(`/api/admin/devices/${id}/commands`)
}

// deleteDevice permanently removes the device and all of its associated
// data (telemetry, alert rules/events). Irreversible -- callers must
// confirm with the user first.
export function deleteDevice(id: number) {
  return api.del<{ message: string }>(`/api/admin/devices/${id}`)
}

// getDeviceRecords is the "历史数据查询" backing call — start/end are Unix
// seconds, omit either to leave that bound open.
export function getDeviceRecords(id: number, opts: { start?: number; end?: number; page?: number; pageSize?: number }) {
  const params = new URLSearchParams()
  if (opts.start) params.set('start', String(opts.start))
  if (opts.end) params.set('end', String(opts.end))
  params.set('page', String(opts.page ?? 1))
  params.set('page_size', String(opts.pageSize ?? 50))
  return api.get<{ list: DeviceRecord[]; total: number }>(`/api/admin/devices/${id}/records?${params}`)
}
