import { useEffect, useRef } from "react";
import { downloadUrl } from "../api";
import { formatBytes, formatDate } from "../format";
import type { FileList } from "../hooks/useFileList";

/** The list, plus an invisible marker below it: when the marker scrolls near the screen, the next page loads. */
export function FileListView({ list }: { list: FileList }) {
  const { items, hasMore, loading, error, loadMore } = list;
  const marker = useRef<HTMLDivElement>(null);

  // Observe again after every load: if the marker is still on screen (a short list, a tall window),
  // observing it afresh reports that straight away and the next page follows.
  useEffect(() => {
    const el = marker.current;
    if (!el || !hasMore || loading || error) return;
    const observer = new IntersectionObserver((entries) => entries.some((e) => e.isIntersecting) && loadMore(), {
      rootMargin: "600px",
    });
    observer.observe(el);
    return () => observer.disconnect();
  }, [hasMore, loading, error, loadMore, items.length]);

  return (
    <section aria-label="Files">
      {items.length === 0 && !loading && !hasMore && !error && <p className="empty">No files yet. Drop one anywhere on this page.</p>}
      <ul className="files">
        {items.map((f) => (
          <li key={f.id}>
            <a className="name" href={downloadUrl(f.id)} download={f.name} title={f.name}>
              {f.name}
            </a>
            <span className="meta">
              {formatBytes(f.size)} <span aria-hidden="true">·</span> <time dateTime={f.createdAt}>{formatDate(f.createdAt)}</time>
            </span>
          </li>
        ))}
      </ul>
      <div ref={marker} className="marker" aria-hidden="true" />
      {loading && <p className="status">Loading…</p>}
      {error && (
        <p className="status error" role="alert">
          {error} <button type="button" className="link" onClick={loadMore}>Try again</button>
        </p>
      )}
      {!hasMore && items.length > 0 && <p className="status">That's everything.</p>}
    </section>
  );
}
