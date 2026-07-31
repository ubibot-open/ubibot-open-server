import type { ReactNode } from 'react'
import { fieldIconMeta } from '../components/icons/SensorIcons'
import CustomSvgIcon from '../components/icons/CustomSvgIcon'

// A resolved field1..field20 display name/unit/icon for one device -- the
// shape both api/device.ts's embedded per-row field_meta (数据仓库 list)
// and api/fieldSettings.ts's per-device settings (hooks/
// useDeviceFieldSettings.tsx) share, so both can go through the same
// render helpers below.
export interface FieldMeta {
  name: string
  unit: string
  svg: string
}

// fieldDisplayLabel joins name+unit, e.g. "室内温度 (℃)". An unset name
// falls back to the raw field key; an unset unit is simply omitted rather
// than shown as "field1 ()" (see docs/UbiBot开放平台硬件通信协议.md §5 --
// naming/units are entirely optional per field, per device).
export function fieldDisplayLabel(key: string, meta?: FieldMeta): string {
  const name = meta?.name || key
  return meta?.unit ? `${name} (${meta.unit})` : name
}

export function fieldDisplayIcon(key: string, meta?: FieldMeta): ReactNode {
  if (meta?.svg) return <CustomSvgIcon svg={meta.svg} />
  return fieldIconMeta(key).icon
}

// Only the built-in icon set gets a forced currentColor tint -- a custom
// SVG's colors are baked into its own markup (see hooks/useFieldIcons.tsx
// for the same rule under the old global-icon system).
export function fieldDisplayColor(key: string, meta?: FieldMeta): string | undefined {
  return meta?.svg ? undefined : fieldIconMeta(key).color
}
