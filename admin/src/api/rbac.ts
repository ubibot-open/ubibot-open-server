import { api } from './client'

export interface Role {
  id: number
  name: string
  code: string
  permissions: string[]
}

export const PermissionCodes = [
  { value: 'device:read', label: 'Device View' },
  { value: 'device:write', label: 'Device Management' },
  { value: 'alert:manage', label: 'Alert Management' },
  { value: 'system:manage', label: 'System Management (Roles/Admins/Logs)' },
  { value: '*', label: 'All Permissions' },
] as const

export function listRoles() {
  return api.get<{ list: Role[] }>('/api/admin/roles')
}

export function createRole(input: { name: string; code: string; permissions: string[] }) {
  return api.post<Role>('/api/admin/roles', input)
}

export function updateRole(id: number, input: { name: string; permissions: string[] }) {
  return api.patch<{ message: string }>(`/api/admin/roles/${id}`, input)
}

export function deleteRole(id: number) {
  return api.del<{ message: string }>(`/api/admin/roles/${id}`)
}

export interface AdminUser {
  id: number
  username: string
  role_id: number
  role_name?: string
  created_at: number
}

export function listAdminUsers() {
  return api.get<{ list: AdminUser[] }>('/api/admin/users')
}

export function createAdminUser(input: { username: string; password: string; role_id: number }) {
  return api.post<AdminUser>('/api/admin/users', input)
}

export function updateAdminUser(id: number, input: { role_id?: number; password?: string }) {
  return api.patch<{ message: string }>(`/api/admin/users/${id}`, input)
}

export function deleteAdminUser(id: number) {
  return api.del<{ message: string }>(`/api/admin/users/${id}`)
}

export interface AuditLog {
  id: number
  username: string
  action: string
  target_type: string
  target_id: number
  detail: string
  ip: string
  created_at: number
}

export function listAuditLogs(page = 1, pageSize = 20) {
  return api.get<{ list: AuditLog[]; total: number }>(`/api/admin/audit-logs?page=${page}&page_size=${pageSize}`)
}
