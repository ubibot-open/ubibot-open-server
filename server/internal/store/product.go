package store

import "github.com/ubibot/ubibot-open-server/internal/model"

// CreateProduct registers display metadata for a device type/model,
// keyed by pid (unique — creating a second Product for the same pid
// fails on the unique index). See model.Product's doc comment: this
// never touches any existing Device row.
func (s *Store) CreateProduct(pid, name, description string) (*model.Product, error) {
	p := &model.Product{PID: pid, Name: name, Description: description}
	if err := s.db.Create(p).Error; err != nil {
		return nil, err
	}
	return p, nil
}

// ListProducts returns every registered product, newest first.
func (s *Store) ListProducts() ([]model.Product, error) {
	var rows []model.Product
	err := s.db.Order("id desc").Find(&rows).Error
	return rows, err
}

// UpdateProduct changes a product's name/description. pid is intentionally
// not editable here — it's the join key devices are matched against, so
// changing it would silently "move" every device of that pid to look
// unmatched; delete and recreate instead if a pid was really a mistake.
func (s *Store) UpdateProduct(id uint, name, description string) error {
	return s.db.Model(&model.Product{}).Where("id = ?", id).Updates(map[string]any{
		"name": name, "description": description,
	}).Error
}

// DeleteProduct removes the product's display metadata only. Devices
// already matched to its pid are unaffected — they just stop resolving a
// product_name until/unless another Product row is created for that pid.
func (s *Store) DeleteProduct(id uint) error {
	return s.db.Delete(&model.Product{}, id).Error
}

// ProductsByPIDs batch-resolves a set of pids to their Product row, for
// annotating a page of devices with product_name without one query per
// device. pids not backed by any Product row are simply absent from the
// result map.
func (s *Store) ProductsByPIDs(pids []string) (map[string]model.Product, error) {
	if len(pids) == 0 {
		return map[string]model.Product{}, nil
	}
	var rows []model.Product
	if err := s.db.Where("pid IN ?", pids).Find(&rows).Error; err != nil {
		return nil, err
	}
	byPID := make(map[string]model.Product, len(rows))
	for _, p := range rows {
		byPID[p.PID] = p
	}
	return byPID, nil
}
