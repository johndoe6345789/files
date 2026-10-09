import { formatBytes } from "../format";
import type { Upload } from "../hooks/useUploads";

interface Props {
  uploads: Upload[];
  onDismiss: (id: number) => void;
  onRetry: (id: number) => void;
}

export function UploadList({ uploads, onDismiss, onRetry }: Props) {
  if (uploads.length === 0) return null;
  return (
    <ul className="uploads" aria-label="Uploads" aria-live="polite">
      {uploads.map((u) => (
        <li key={u.id} className={u.status}>
          <span className="name" title={u.file.name}>{u.file.name}</span>
          <span className="meta">{formatBytes(u.file.size)}</span>
          {(u.status === "queued" || u.status === "uploading") && (
            <progress max={1} value={u.status === "queued" ? 0 : u.progress} aria-label={`Uploading ${u.file.name}`} />
          )}
          {u.status === "queued" && <span className="meta">Waiting…</span>}
          {u.status === "uploading" && <span className="meta">{Math.round(u.progress * 100)}%</span>}
          {u.status === "done" && <span className="meta ok">Uploaded</span>}
          {u.status === "error" && (
            <>
              <span className="meta error">{u.error}</span>
              {u.file.size > 0 && !u.error?.startsWith("Too large") && (
                <button type="button" className="link" onClick={() => onRetry(u.id)}>Retry</button>
              )}
              <button type="button" className="link" onClick={() => onDismiss(u.id)}>Dismiss</button>
            </>
          )}
        </li>
      ))}
    </ul>
  );
}
