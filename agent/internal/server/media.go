package server

import (
	"github.com/motionsense/agent/internal/recording"
	"net/http"
	"path/filepath"
)

func mediaHandler(dcimRoot string) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		path, err := recording.MediaFilePath(dcimRoot,
			r.PathValue("date"), r.PathValue("segment"), r.PathValue("file"))
		if err != nil {
			http.NotFound(w, r)
			return
		}

		switch filepath.Ext(path) {
		case ".ts":
			// a .ts listed in a playlist is complete and never rewritten
			w.Header().Set("Content-Type", "video/mp2t")
			w.Header().Set("Cache-Control", "public, max-age=86400")
		case ".m3u8":
			// live segment's playlist grows until ENDLIST — always revalidate
			w.Header().Set("Content-Type", "application/vnd.apple.mpegurl")
			w.Header().Set("Cache-Control", "no-cache")
		}

		// ServeFile handles Range requests, needed for video seeking
		http.ServeFile(w, r, path)
	}
}
