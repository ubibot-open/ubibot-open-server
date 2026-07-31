import { useCallback, useEffect, useState } from 'react'
import { listDeviceFieldSettings, type DeviceFieldSetting } from '../api/fieldSettings'
import { fieldDisplayColor, fieldDisplayIcon, fieldDisplayLabel } from '../utils/fieldMeta'

// Fetches one device's field1..field20 settings (系统 > 设备详情 > 字段设置)
// and exposes the same render helpers the 数据仓库 list page derives from
// its embedded field_meta (see utils/fieldMeta.tsx and api/device.ts) --
// used by pages that only ever look at a single device at a time (设备详情,
// 监控), where fetching that one device's settings once is cheap, unlike a
// device list where it'd mean N requests for N rows.
export function useDeviceFieldSettings(deviceId: number | null) {
  const [settings, setSettings] = useState<Record<string, DeviceFieldSetting>>({})
  const [list, setList] = useState<DeviceFieldSetting[]>([])
  const [loaded, setLoaded] = useState(false)

  const reload = useCallback(() => {
    if (!deviceId) {
      setSettings({})
      setList([])
      setLoaded(true)
      return
    }
    setLoaded(false)
    listDeviceFieldSettings(deviceId)
      .then((res) => {
        const map: Record<string, DeviceFieldSetting> = {}
        for (const it of res.list) map[it.key.toLowerCase()] = it
        setSettings(map)
        setList(res.list)
      })
      .finally(() => setLoaded(true))
  }, [deviceId])

  useEffect(() => {
    reload()
  }, [reload])

  const metaOf = useCallback((key: string) => settings[key.toLowerCase()], [settings])
  const fieldLabel = useCallback((key: string) => fieldDisplayLabel(key, metaOf(key)), [metaOf])
  const renderFieldIcon = useCallback((key: string) => fieldDisplayIcon(key, metaOf(key)), [metaOf])
  const fieldColor = useCallback((key: string) => fieldDisplayColor(key, metaOf(key)), [metaOf])

  return { settings, list, loaded, reload, metaOf, fieldLabel, renderFieldIcon, fieldColor }
}
