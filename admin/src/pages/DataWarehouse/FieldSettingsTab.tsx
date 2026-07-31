import { useState } from 'react'
import { Button, Card, Form, Input, Modal, Popconfirm, Space, Table, Tag, Typography, Upload, message } from 'antd'
import type { ColumnsType } from 'antd/es/table'
import { UploadOutlined } from '@ant-design/icons'
import { useTranslation } from 'react-i18next'
import { upsertDeviceFieldSetting, resetDeviceFieldSetting, type DeviceFieldSetting } from '../../api/fieldSettings'
import { apiErrorMessage } from '../../api/errors'
import { fieldDisplayColor, fieldDisplayIcon } from '../../utils/fieldMeta'
import type { useDeviceFieldSettings } from '../../hooks/useDeviceFieldSettings'

// Lets an operator name/unit/icon this one device's field1..field20 (see
// docs/UbiBot开放平台硬件通信协议.md §5) -- every field is editable
// regardless of whether it has reported data yet, since a field can be
// named ahead of time. Sharing the parent's useDeviceFieldSettings instance
// (rather than fetching its own copy) means saving here immediately
// updates the dashboard/history tabs' labels too, without a page reload.
export default function FieldSettingsTab({
  deviceId,
  fieldSettings,
}: {
  deviceId: number
  fieldSettings: ReturnType<typeof useDeviceFieldSettings>
}) {
  const { t } = useTranslation('dataWarehouseDetail')
  const { list, loaded, reload } = fieldSettings
  const [target, setTarget] = useState<DeviceFieldSetting | null>(null)
  const [file, setFile] = useState<File | null>(null)
  const [submitting, setSubmitting] = useState(false)
  const [form] = Form.useForm()

  const openEdit = (row: DeviceFieldSetting) => {
    setFile(null)
    form.setFieldsValue({ name: row.name, unit: row.unit })
    setTarget(row)
  }

  const onReset = async (key: string) => {
    try {
      await resetDeviceFieldSetting(deviceId, key)
      message.success(t('fieldSettings.message.resetSuccess'))
      reload()
    } catch (e) {
      message.error(apiErrorMessage(e, t('fieldSettings.message.resetFailed')))
    }
  }

  const onSubmit = async (values: { name: string; unit: string }) => {
    if (!target) return
    setSubmitting(true)
    try {
      let svg = target.svg
      if (file) {
        const text = await file.text()
        if (!text.includes('<svg')) {
          message.error(t('fieldSettings.message.invalidFile'))
          setSubmitting(false)
          return
        }
        svg = text
      }
      await upsertDeviceFieldSetting(deviceId, target.key, {
        name: values.name?.trim() ?? '',
        unit: values.unit?.trim() ?? '',
        svg,
      })
      message.success(t('fieldSettings.message.saveSuccess'))
      setTarget(null)
      reload()
    } catch (e) {
      message.error(apiErrorMessage(e, t('fieldSettings.message.saveFailed')))
    } finally {
      setSubmitting(false)
    }
  }

  const columns: ColumnsType<DeviceFieldSetting> = [
    {
      title: t('fieldSettings.table.icon'),
      width: 60,
      render: (_, r) => (
        <span style={{ fontSize: 20, color: fieldDisplayColor(r.key, r) }}>{fieldDisplayIcon(r.key, r)}</span>
      ),
    },
    { title: t('fieldSettings.table.key'), dataIndex: 'key', width: 100 },
    { title: t('fieldSettings.table.name'), render: (_, r) => r.name || <Typography.Text type="secondary">{r.key}</Typography.Text> },
    { title: t('fieldSettings.table.unit'), dataIndex: 'unit', width: 100 },
    {
      title: t('fieldSettings.table.source'),
      width: 100,
      render: (_, r) =>
        r.is_custom ? <Tag color="blue">{t('fieldSettings.source.custom')}</Tag> : <Tag color="default">{t('fieldSettings.source.default')}</Tag>,
    },
    {
      title: t('common:actions'),
      width: 140,
      render: (_, r) => (
        <Space>
          <a onClick={() => openEdit(r)}>{t('fieldSettings.editButton')}</a>
          {r.is_custom && (
            <Popconfirm title={t('fieldSettings.resetConfirm')} onConfirm={() => onReset(r.key)}>
              <a>{t('common:reset')}</a>
            </Popconfirm>
          )}
        </Space>
      ),
    },
  ]

  return (
    <div>
      <Typography.Paragraph type="secondary">{t('fieldSettings.subtitle')}</Typography.Paragraph>
      <Card>
        <Table rowKey="key" columns={columns} dataSource={list} loading={!loaded} pagination={false} size="small" />
      </Card>

      <Modal
        title={t('fieldSettings.modal.title', { key: target?.key })}
        open={target !== null}
        onCancel={() => setTarget(null)}
        footer={null}
        destroyOnClose
      >
        <Form form={form} layout="vertical" onFinish={onSubmit}>
          <Form.Item name="name" label={t('fieldSettings.modal.nameLabel')}>
            <Input placeholder={t('fieldSettings.modal.namePlaceholder')} />
          </Form.Item>
          <Form.Item name="unit" label={t('fieldSettings.modal.unitLabel')}>
            <Input placeholder={t('fieldSettings.modal.unitPlaceholder')} />
          </Form.Item>
          <Form.Item label={t('fieldSettings.modal.fileLabel')}>
            <Upload
              accept=".svg,image/svg+xml"
              beforeUpload={(f) => {
                setFile(f)
                return false
              }}
              maxCount={1}
              onRemove={() => setFile(null)}
            >
              <Button icon={<UploadOutlined />}>{t('fieldSettings.modal.selectFileButton')}</Button>
            </Upload>
          </Form.Item>
          <Form.Item style={{ marginBottom: 0, textAlign: 'right' }}>
            <Space>
              <Button onClick={() => setTarget(null)}>{t('common:cancel')}</Button>
              <Button type="primary" htmlType="submit" loading={submitting}>
                {t('fieldSettings.modal.submitButton')}
              </Button>
            </Space>
          </Form.Item>
        </Form>
      </Modal>
    </div>
  )
}
