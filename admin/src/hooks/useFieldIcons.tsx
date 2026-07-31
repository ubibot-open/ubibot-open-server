import { useCallback, useEffect, useState } from 'react'
import type { ReactNode } from 'react'
import { listIcons, type IconAsset } from '../api/icon'
import { fieldIconMeta } from '../components/icons/SensorIcons'
import CustomSvgIcon from '../components/icons/CustomSvgIcon'

// Fetches the field1..field20 default templates (系统管理 > 字段配置) once
// and merges them with the built-in per-field icon set: a template icon
// for a key always wins over its built-in icon; everything else falls
// back to fieldIconMeta's default/generic icon. Used by the 字段配置 page
// itself (to preview what a field's default currently looks like) --
// actual device data rendering goes through useDeviceFieldSettings
// instead, since icons became a per-device setting.
export function useFieldIcons() {
  const [customIcons, setCustomIcons] = useState<Record<string, IconAsset>>({})
  const [loaded, setLoaded] = useState(false)

  const reload = useCallback(() => {
    listIcons()
      .then((res) => {
        const map: Record<string, IconAsset> = {}
        for (const it of res.list) map[it.key.toLowerCase()] = it
        setCustomIcons(map)
      })
      .finally(() => setLoaded(true))
  }, [])

  useEffect(() => {
    reload()
  }, [reload])

  const renderFieldIcon = useCallback(
    (key: string): ReactNode => {
      const custom = customIcons[key.toLowerCase()]
      if (custom) return <CustomSvgIcon svg={custom.svg} />
      return fieldIconMeta(key).icon
    },
    [customIcons],
  )

  // Only built-in icons get a forced currentColor tint via inline style --
  // a custom upload's colors are baked into its own SVG markup, so we leave
  // that alone (undefined means "don't override").
  const fieldColor = useCallback(
    (key: string): string | undefined => (customIcons[key.toLowerCase()] ? undefined : fieldIconMeta(key).color),
    [customIcons],
  )

  return { customIcons, loaded, reload, renderFieldIcon, fieldColor }
}
