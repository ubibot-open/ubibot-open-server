import { api } from './client'

// IconAsset is a field1..field20 default template -- name/unit/icon a
// device's own field settings (see api/fieldSettings.ts) fall back to
// until that device customizes the field itself. Backs the "图标库"
// (系统 > 图标库) template-library page; no longer read directly by the
// 数据仓库 page (see hooks/useDeviceFieldSettings.tsx), which resolves
// through a specific device's own settings instead.
export interface IconAsset {
  key: string
  name: string
  unit: string
  svg: string
  created_at: number
}

export function listIcons() {
  return api.get<{ list: IconAsset[] }>('/api/admin/icons')
}

// uploadIcon creates the template for key if it doesn't exist yet, or
// replaces it if it does -- there's no separate update endpoint,
// re-uploading is the update.
export function uploadIcon(input: { key: string; name: string; unit: string; svg: string }) {
  return api.post<IconAsset>('/api/admin/icons', input)
}

// deleteIcon reverts key back to whatever built-in default icon it has
// (or the generic fallback, if none).
export function deleteIcon(key: string) {
  return api.del<{ message: string }>(`/api/admin/icons/${encodeURIComponent(key)}`)
}
