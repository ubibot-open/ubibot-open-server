import { useEffect, useState } from 'react'
import { Button, Card, Form, Input, Modal, Popconfirm, Space, Table, Typography, message } from 'antd'
import type { ColumnsType } from 'antd/es/table'
import { PlusOutlined } from '@ant-design/icons'
import { useTranslation } from 'react-i18next'
import { createProduct, deleteProduct, listProducts, updateProduct, type Product } from '../../api/product'
import { apiErrorMessage } from '../../api/errors'

export default function ProductPage() {
  const { t } = useTranslation('product')
  const [rows, setRows] = useState<Product[]>([])
  const [loading, setLoading] = useState(false)
  const [open, setOpen] = useState(false)
  const [editing, setEditing] = useState<Product | null>(null)
  const [submitting, setSubmitting] = useState(false)
  const [form] = Form.useForm()

  const load = async () => {
    setLoading(true)
    try {
      const res = await listProducts()
      setRows(res.list)
    } catch (e) {
      message.error(apiErrorMessage(e, t('message.loadFailed')))
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => {
    load()
  }, [])

  const openCreate = () => {
    setEditing(null)
    form.resetFields()
    setOpen(true)
  }

  const openEdit = (p: Product) => {
    setEditing(p)
    form.setFieldsValue(p)
    setOpen(true)
  }

  const onSubmit = async (values: { pid: string; name: string; description?: string }) => {
    setSubmitting(true)
    try {
      if (editing) {
        await updateProduct(editing.id, { name: values.name, description: values.description })
      } else {
        await createProduct(values)
      }
      message.success(t('common:saveSuccess'))
      setOpen(false)
      load()
    } catch (e) {
      message.error(apiErrorMessage(e, t('common:saveFailed')))
    } finally {
      setSubmitting(false)
    }
  }

  const onDelete = async (id: number) => {
    try {
      await deleteProduct(id)
      message.success(t('common:deleteSuccess'))
      load()
    } catch (e) {
      message.error(apiErrorMessage(e, t('common:deleteFailed')))
    }
  }

  const columns: ColumnsType<Product> = [
    { title: 'PID', dataIndex: 'pid' },
    { title: t('table.name'), dataIndex: 'name' },
    { title: t('table.description'), dataIndex: 'description', ellipsis: true },
    {
      title: t('common:actions'),
      width: 140,
      render: (_, r) => (
        <Space>
          <a onClick={() => openEdit(r)}>{t('common:edit')}</a>
          <Popconfirm title={t('deleteConfirm')} onConfirm={() => onDelete(r.id)}>
            <a>{t('common:delete')}</a>
          </Popconfirm>
        </Space>
      ),
    },
  ]

  return (
    <div>
      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 16 }}>
        <Typography.Title level={4} style={{ margin: 0 }}>
          {t('pageTitle')}
        </Typography.Title>
        <Button type="primary" icon={<PlusOutlined />} onClick={openCreate}>
          {t('createButton')}
        </Button>
      </div>
      <Card>
        <Table rowKey="id" columns={columns} dataSource={rows} loading={loading} pagination={false} />
      </Card>

      <Modal
        title={editing ? t('modal.editTitle') : t('modal.createTitle')}
        open={open}
        onCancel={() => setOpen(false)}
        footer={null}
        destroyOnClose
      >
        <Form form={form} layout="vertical" onFinish={onSubmit}>
          <Form.Item
            name="pid"
            label="PID"
            rules={[{ required: true }]}
            extra={editing ? t('modal.pidExtraLocked') : t('modal.pidExtraHint')}
          >
            <Input disabled={!!editing} />
          </Form.Item>
          <Form.Item name="name" label={t('table.name')} rules={[{ required: true }]}>
            <Input />
          </Form.Item>
          <Form.Item name="description" label={t('table.description')}>
            <Input.TextArea rows={3} />
          </Form.Item>
          <Form.Item style={{ marginBottom: 0, textAlign: 'right' }}>
            <Space>
              <Button onClick={() => setOpen(false)}>{t('common:cancel')}</Button>
              <Button type="primary" htmlType="submit" loading={submitting}>
                {t('common:save')}
              </Button>
            </Space>
          </Form.Item>
        </Form>
      </Modal>
    </div>
  )
}
