package api

import (
	"net/http"
	"strconv"
)

// productDTO mirrors model.Product exactly -- there's nothing else to a
// product yet (no default field/probe templates, see docs §7's "产品/型号
// 管理" for what's intentionally deferred).
type productDTO struct {
	ID          uint   `json:"id"`
	PID         string `json:"pid"`
	Name        string `json:"name"`
	Description string `json:"description"`
	CreatedAt   int64  `json:"created_at"`
}

// ListProducts handles GET /api/admin/products.
func (s *Server) ListProducts(w http.ResponseWriter, r *http.Request) {
	rows, err := s.Store.ListProducts()
	if err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	list := make([]productDTO, 0, len(rows))
	for _, p := range rows {
		list = append(list, productDTO{ID: p.ID, PID: p.PID, Name: p.Name, Description: p.Description, CreatedAt: p.CreatedAt.Unix()})
	}
	writeAPIJSON(w, 200, map[string]any{"list": list})
}

type createProductRequest struct {
	PID         string `json:"pid"`
	Name        string `json:"name"`
	Description string `json:"description"`
}

// CreateProduct handles POST /api/admin/products. pid must be unique --
// registering a second Product for a pid that already has one fails
// (update the existing one instead).
func (s *Server) CreateProduct(w http.ResponseWriter, r *http.Request) {
	var req createProductRequest
	if err := decodeJSON(r, &req); err != nil || req.PID == "" || req.Name == "" {
		adminErr(w, 400, "pid and name are required")
		return
	}
	p, err := s.Store.CreateProduct(req.PID, req.Name, req.Description)
	if err != nil {
		adminErr(w, 400, "a product for this pid already exists, or the request was otherwise invalid")
		return
	}
	s.audit(r, "product.create", "product", p.ID, req.PID)
	writeAPIJSON(w, 200, productDTO{ID: p.ID, PID: p.PID, Name: p.Name, Description: p.Description, CreatedAt: p.CreatedAt.Unix()})
}

type updateProductRequest struct {
	Name        string `json:"name"`
	Description string `json:"description"`
}

// UpdateProduct handles PATCH /api/admin/products/{id} -- name/description
// only; pid is immutable once created (see store.UpdateProduct).
func (s *Server) UpdateProduct(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil {
		adminErr(w, 400, "invalid id")
		return
	}
	var req updateProductRequest
	if err := decodeJSON(r, &req); err != nil || req.Name == "" {
		adminErr(w, 400, "name is required")
		return
	}
	if err := s.Store.UpdateProduct(uint(id), req.Name, req.Description); err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	s.audit(r, "product.update", "product", uint(id), req.Name)
	writeAPIJSON(w, 200, map[string]any{"message": "ok"})
}

// DeleteProduct handles DELETE /api/admin/products/{id} -- removes only
// the product's display metadata; devices already matched to its pid are
// unaffected (see store.DeleteProduct).
func (s *Server) DeleteProduct(w http.ResponseWriter, r *http.Request) {
	id, err := strconv.Atoi(r.PathValue("id"))
	if err != nil {
		adminErr(w, 400, "invalid id")
		return
	}
	if err := s.Store.DeleteProduct(uint(id)); err != nil {
		adminErr(w, 500, "internal error")
		return
	}
	s.audit(r, "product.delete", "product", uint(id), "")
	writeAPIJSON(w, 200, map[string]any{"message": "ok"})
}
