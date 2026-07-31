package api

import (
	"encoding/json"
	"net/http"
	"time"
)

// writeJSON and decodeJSON are the two helpers every handler in this
// package uses instead of a web framework's binding/rendering — the
// device and admin APIs here are small enough that net/http's own
// ServeMux (Go 1.22+ method+pattern routing) covers routing needs
// without pulling in a framework and its transitive dependency tree.
func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)

	// 1. 先将 v 序列化为 JSON bytes
	data, err := json.Marshal(v)
	if err != nil {
		// 序列化失败，直接输出原始错误
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	// 2. 反序列化为 map，以便动态操作字段
	var m map[string]any
	if err := json.Unmarshal(data, &m); err != nil {
		// v 不是 JSON object（可能是数组、字符串等），直接原样输出
		_ = json.NewEncoder(w).Encode(v)
		return
	}

	// 3. 检查是否存在 timestamp，没有则插入
	if _, exists := m["timestamp"]; !exists {
		m["timestamp"] = time.Now().Unix()
	}

	// 4. 输出
	_ = json.NewEncoder(w).Encode(m)
}

// writeAPIJSON wraps writeJSON for the admin and open API surfaces,
// stamping every response body -- success and error alike -- with a
// "timestamp" field (server time, Unix seconds) so callers always have a
// clock reference to key off. v may be a map or a struct DTO; either way
// it's round-tripped through JSON so the field can be merged in generically.
// Device protocol responses (device_handlers.go, ratelimit.go) call
// writeJSON directly instead and keep their fixed wire format per
// docs/UbiBot开放平台硬件通信协议.md, which already carries its own clock
// reference in "t".
func writeAPIJSON(w http.ResponseWriter, status int, v any) {
	if body, err := json.Marshal(v); err == nil {
		m := map[string]any{}
		if err := json.Unmarshal(body, &m); err == nil {
			m["timestamp"] = time.Now().Unix()
			writeJSON(w, status, m)
			return
		}
	}
	writeJSON(w, status, v)
}

func decodeJSON(r *http.Request, v any) error {
	defer r.Body.Close()
	dec := json.NewDecoder(r.Body)
	return dec.Decode(v)
}
