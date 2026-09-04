import { api, getToken, ApiError, BASE_URL } from './client'
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
  // product_name resolves by matching pid against a registered Product
  // (see api/product.ts) — absent if no Product is registered for this
  // device's pid.
  product_name?: string
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

// importDevices bulk pre-registers devices ahead of time (docs §7's
// "批量设备管理") -- e.g. from a production batch's serial-number list,
// before any of them have ever reported. Per-row results, not
// all-or-nothing: a
// duplicate sn is reported in `skipped` (not an error), a row missing a
// required field lands in `failed` with a reason, and the rest of the
// batch still goes through either way.
export interface ImportDevicesResult {
  created: number
  skipped: string[]
  failed: string[]
}

export function importDevices(rows: { sn: string; pid: string; name?: string }[]) {
  return api.post<ImportDevicesResult>('/api/admin/devices/import', { rows })
}

// exportDevicesCsv downloads every device (not just the current page) as a
// CSV file. Bypasses the shared `api` client because that always parses
// the response as JSON -- this endpoint deliberately returns text/csv --
// so the bearer token is attached by hand here instead.
export async function exportDevicesCsv(filename = 'devices.csv') {
  const token = getToken()
  const headers: Record<string, string> = {}
  if (token) headers.Authorization = `Bearer ${token}`
  const res = await fetch(`${BASE_URL}/api/admin/devices/export.csv`, { headers })
  if (!res.ok) {
    const text = await res.text()
    let message = `export failed (${res.status})`
    try {
      message = JSON.parse(text).message ?? message
    } catch {
      // response wasn't JSON (e.g. a plain-text error page) -- keep the default message
    }
    throw new ApiError(res.status, message)
  }
  const blob = await res.blob()
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = filename
  a.click()
  URL.revokeObjectURL(url)
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
