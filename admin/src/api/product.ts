import { api } from './client'

// Product is display metadata for a device type/model (docs §7's "产品/
// 型号管理") — resolved onto a Device by matching pid, not a foreign key.
export interface Product {
  id: number
  pid: string
  name: string
  description: string
  created_at: number
}

export function listProducts() {
  return api.get<{ list: Product[] }>('/api/admin/products')
}

export function createProduct(input: { pid: string; name: string; description?: string }) {
  return api.post<Product>('/api/admin/products', input)
}

// updateProduct only takes name/description -- pid is immutable once
// created (it's the join key devices are matched against).
export function updateProduct(id: number, input: { name: string; description?: string }) {
  return api.patch<{ message: string }>(`/api/admin/products/${id}`, input)
}

// deleteProduct removes only the product's display metadata; devices
// already matched to its pid are unaffected -- they just stop resolving a
// product_name.
export function deleteProduct(id: number) {
  return api.del<{ message: string }>(`/api/admin/products/${id}`)
}
