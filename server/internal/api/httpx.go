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

	// 1. Marshal v into JSON bytes first
	data, err := json.Marshal(v)
	if err != nil {
		// Marshaling failed; write out the raw error directly
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return
	}

	// 2. Unmarshal into a map so fields can be manipulated dynamically
	var m map[string]any
	if err := json.Unmarshal(data, &m); err != nil {
		// v is not a JSON object (it may be an array, a string, etc.); write it out as-is
		_ = json.NewEncoder(w).Encode(v)
		return
	}

	// 3. Check whether timestamp exists; insert it if not
	if _, exists := m["timestamp"]; !exists {
		m["timestamp"] = time.Now().Unix()
	}

	// 4. Write the output
	_ = json.NewEncoder(w).Encode(m)
}

// writeAPIJSON wraps writeJSON for the admin and open API surfaces,
// stamping every response body -- success and error alike -- with a
// "timestamp" field (server time, Unix seconds) so callers always have a
// clock reference to key off. v may be a map or a struct DTO; either way
// it's round-tripped through JSON so the field can be merged in generically.
// Device protocol responses (device_handlers.go, ratelimit.go) call
// writeJSON directly instead and keep their fixed wire format per
// docs/hardware-communication-protocol.md, which already carries its own clock
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
