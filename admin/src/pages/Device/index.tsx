import { useEffect, useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { Alert, Button, Form, Input, Modal, Popconfirm, Space, Table, Tag, Typography, Upload, message } from 'antd'
import type { ColumnsType } from 'antd/es/table'
import { UploadOutlined, DownloadOutlined, ImportOutlined } from '@ant-design/icons'
import { useTranslation } from 'react-i18next'
import {
  DeviceStatus,
  deleteDevice,
  exportDevicesCsv,
  importDevices,
  listDevices,
  renameDevice,
  setDeviceStatus,
  type Device,
  type ImportDevicesResult,
} from '../../api/device'
import { parseCsv } from '../../utils/csv'
import { apiErrorMessage } from '../../api/errors'

export default function DevicePage() {
  const { t } = useTranslation('device')
  const navigate = useNavigate()
  const [loading, setLoading] = useState(false)
  const [devices, setDevices] = useState<Device[]>([])
  const [total, setTotal] = useState(0)
  const [page, setPage] = useState(1)
  const [busyId, setBusyId] = useState<number | null>(null)
  const [renameTarget, setRenameTarget] = useState<Device | null>(null)
  const [renaming, setRenaming] = useState(false)
  const [renameForm] = Form.useForm()

  const [importOpen, setImportOpen] = useState(false)
  const [importText, setImportText] = useState('')
  const [importing, setImporting] = useState(false)
  const [importResult, setImportResult] = useState<ImportDevicesResult | null>(null)
  const [exporting, setExporting] = useState(false)

  function formatTime(ts: number | null) {
    if (!ts) return t('neverReported')
    return new Date(ts * 1000).toLocaleString()
  }

  const load = async (p = page) => {
    setLoading(true)
    try {
      const res = await listDevices(p, 20)
      setDevices(res.list)
      setTotal(res.total)
      setPage(p)
    } catch (e) {
      message.error(apiErrorMessage(e, t('loadFailed')))
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => {
    load(1)
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  const onDisable = async (id: number) => {
    setBusyId(id)
    try {
      await setDeviceStatus(id, DeviceStatus.Disabled)
      message.success(t('disableSuccess'))
      load()
    } catch (e) {
      message.error(apiErrorMessage(e, t('disableFailed')))
    } finally {
      setBusyId(null)
    }
  }

  const onEnable = async (id: number) => {
    setBusyId(id)
    try {
      await setDeviceStatus(id, DeviceStatus.Enabled)
      message.success(t('enableSuccess'))
      load()
    } catch (e) {
      message.error(apiErrorMessage(e, t('enableFailed')))
    } finally {
      setBusyId(null)
    }
  }

  const onDelete = async (id: number) => {
    setBusyId(id)
    try {
      await deleteDevice(id)
      message.success(t('deleteSuccess'))
      load()
    } catch (e) {
      message.error(apiErrorMessage(e, t('deleteFailed')))
    } finally {
      setBusyId(null)
    }
  }

  const openRename = (device: Device) => {
    setRenameTarget(device)
    renameForm.setFieldsValue({ name: device.name })
  }

  const onRename = async (values: { name: string }) => {
    if (!renameTarget) return
    setRenaming(true)
    try {
      await renameDevice(renameTarget.id, values.name)
      message.success(t('renameSuccess'))
      setRenameTarget(null)
      renameForm.resetFields()
      load()
    } catch (e) {
      message.error(apiErrorMessage(e, t('renameFailed')))
    } finally {
      setRenaming(false)
    }
  }

  // parseImportRows turns the pasted/loaded CSV text into {sn,pid,name}
  // rows for importDevices — an optional header row (first cell literally
  // "sn", case-insensitive) is skipped, and blank lines are ignored.
  const parseImportRows = () => {
    return parseCsv(importText)
      .filter((row) => row.length > 0 && row.some((cell) => cell.trim() !== ''))
      .filter((row, i) => !(i === 0 && row[0]?.trim().toLowerCase() === 'sn'))
      .map((row) => ({ sn: (row[0] ?? '').trim(), pid: (row[1] ?? '').trim(), name: (row[2] ?? '').trim() }))
  }

  const parsedImportRows = importOpen ? parseImportRows() : []

  const onImport = async () => {
    const rows = parseImportRows()
    if (rows.length === 0) {
      message.error(t('import.noRows'))
      return
    }
    setImporting(true)
    try {
      const res = await importDevices(rows)
      setImportResult(res)
      message.success(t('import.resultSummary', { created: res.created, skipped: res.skipped.length, failed: res.failed.length }))
      load()
    } catch (e) {
      message.error(apiErrorMessage(e, t('import.failed')))
    } finally {
      setImporting(false)
    }
  }

  const closeImport = () => {
    setImportOpen(false)
    setImportText('')
    setImportResult(null)
  }

  const onExport = async () => {
    setExporting(true)
    try {
      await exportDevicesCsv()
    } catch (e) {
      message.error(apiErrorMessage(e, t('export.failed')))
    } finally {
      setExporting(false)
    }
  }

  const columns: ColumnsType<Device> = [
    { title: 'ID', dataIndex: 'id', width: 60 },
    { title: t('columns.name'), dataIndex: 'name', render: (v: string, r) => v || r.sn },
    { title: 'SN', dataIndex: 'sn' },
    { title: 'PID', dataIndex: 'pid' },
    { title: t('columns.product'), dataIndex: 'product_name', render: (v: string) => v || <span style={{ color: 'rgba(0,0,0,0.35)' }}>—</span> },
    {
      title: t('columns.status'),
      dataIndex: 'status',
      width: 100,
      render: (status: number) =>
        status === DeviceStatus.Enabled ? (
          <Tag color="success">{t('common:enabled')}</Tag>
        ) : (
          <Tag color="default">{t('common:disabled')}</Tag>
        ),
    },
    {
      title: t('columns.online'),
      dataIndex: 'online',
      width: 80,
      render: (online: boolean) =>
        online ? <Tag color="green">{t('online.yes')}</Tag> : <Tag color="default">{t('online.no')}</Tag>,
    },
    { title: t('columns.lastSeen'), dataIndex: 'last_seen_at', render: formatTime },
    {
      title: t('columns.actions'),
      width: 220,
      render: (_, r) => (
        <Space size="small" wrap>
          <a onClick={() => navigate(`/device/${r.id}`)}>{t('detail')}</a>
          <a onClick={() => openRename(r)}>{t('renameButton')}</a>
          {r.status === DeviceStatus.Enabled && (
            <Popconfirm title={t('disableConfirmTitle')} onConfirm={() => onDisable(r.id)}>
              <a>{t('disableButton')}</a>
            </Popconfirm>
          )}
          {r.status === DeviceStatus.Disabled && <a onClick={() => onEnable(r.id)}>{t('enableButton')}</a>}
          <Popconfirm
            title={t('deleteConfirmTitle')}
            description={t('deleteConfirmContent')}
            onConfirm={() => onDelete(r.id)}
          >
            <a style={{ color: '#ff4d4f' }}>{t('common:delete')}</a>
          </Popconfirm>
        </Space>
      ),
    },
  ]

  return (
    <div>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 16 }}>
        <Typography.Title level={4} style={{ margin: 0 }}>
          {t('title')}
        </Typography.Title>
        <Space>
          <Button icon={<ImportOutlined />} onClick={() => setImportOpen(true)}>
            {t('import.button')}
          </Button>
          <Button icon={<DownloadOutlined />} loading={exporting} onClick={onExport}>
            {t('export.button')}
          </Button>
        </Space>
      </div>

      <Table
        rowKey="id"
        columns={columns}
        dataSource={devices}
        loading={loading || busyId !== null}
        pagination={{ current: page, total, pageSize: 20, onChange: load }}
      />

      <Modal
        title={t('renameButton')}
        open={renameTarget !== null}
        onCancel={() => setRenameTarget(null)}
        footer={null}
        destroyOnClose
      >
        <Form form={renameForm} layout="vertical" onFinish={onRename}>
          <Form.Item name="name" label={t('columns.name')} rules={[{ required: true }]}>
            <Input placeholder={renameTarget?.sn} />
          </Form.Item>
          <Form.Item style={{ marginBottom: 0, textAlign: 'right' }}>
            <Space>
              <Button onClick={() => setRenameTarget(null)}>{t('common:cancel')}</Button>
              <Button type="primary" htmlType="submit" loading={renaming}>
                {t('common:save')}
              </Button>
            </Space>
          </Form.Item>
        </Form>
      </Modal>

      <Modal
        title={t('import.title')}
        open={importOpen}
        onCancel={closeImport}
        footer={null}
        width={640}
        destroyOnClose
      >
        <p style={{ color: 'rgba(0,0,0,0.45)' }}>{t('import.instructions')}</p>
        <Space style={{ marginBottom: 12 }}>
          <Upload
            accept=".csv,text/csv"
            showUploadList={false}
            beforeUpload={(file) => {
              file.text().then(setImportText)
              return false
            }}
          >
            <Button icon={<UploadOutlined />}>{t('import.chooseFile')}</Button>
          </Upload>
        </Space>
        <Input.TextArea
          rows={8}
          value={importText}
          onChange={(e) => setImportText(e.target.value)}
          placeholder={t('import.placeholder')}
        />
        <p style={{ marginTop: 8, marginBottom: 16 }}>{t('import.parsedCount', { count: parsedImportRows.length })}</p>

        {importResult && (
          <Alert
            style={{ marginBottom: 16 }}
            type={importResult.failed.length > 0 ? 'warning' : 'success'}
            message={t('import.resultSummary', {
              created: importResult.created,
              skipped: importResult.skipped.length,
              failed: importResult.failed.length,
            })}
            description={
              (importResult.skipped.length > 0 || importResult.failed.length > 0) && (
                <>
                  {importResult.skipped.length > 0 && <div>{t('import.skippedList', { list: importResult.skipped.join(', ') })}</div>}
                  {importResult.failed.length > 0 && <div>{t('import.failedList', { list: importResult.failed.join('; ') })}</div>}
                </>
              )
            }
          />
        )}

        <div style={{ textAlign: 'right' }}>
          <Space>
            <Button onClick={closeImport}>{t('common:cancel')}</Button>
            <Button type="primary" loading={importing} disabled={parsedImportRows.length === 0} onClick={onImport}>
              {t('import.submitButton')}
            </Button>
          </Space>
        </div>
      </Modal>
    </div>
  )
}
