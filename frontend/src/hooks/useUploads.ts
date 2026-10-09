import { useCallback, useEffect, useRef, useState } from "react";
import { uploadFile, type FileItem, type UploadHandle } from "../api";
import { formatBytes } from "../format";

export type UploadStatus = "queued" | "uploading" | "done" | "error";
export interface Upload {
  id: number;
  file: File;
  progress: number;
  status: UploadStatus;
  error?: string;
}

export type Uploader = (file: File, onProgress: (fraction: number) => void) => UploadHandle;

const CONCURRENT = 2;
const DONE_VISIBLE_MS = 4000;

/** Upload queue: validates files, sends up to two at a time, reports each finished one through onUploaded. */
export function useUploads(
  maxFileBytes: number | undefined,
  onUploaded: (item: FileItem) => void,
  uploader: Uploader = uploadFile,
) {
  const [uploads, setUploads] = useState<Upload[]>([]);
  const nextId = useRef(1);
  const started = useRef(new Set<number>());
  const onUploadedRef = useRef(onUploaded);
  onUploadedRef.current = onUploaded;

  const patch = useCallback((id: number, change: Partial<Upload>) => {
    setUploads((list) => list.map((u) => (u.id === id ? { ...u, ...change } : u)));
  }, []);

  const add = useCallback(
    (files: File[]) => {
      const entries: Upload[] = files.map((file) => {
        let error: string | undefined;
        if (file.size === 0) error = "The file is empty.";
        else if (maxFileBytes !== undefined && file.size > maxFileBytes)
          error = `Too large: the limit is ${formatBytes(maxFileBytes)} per file.`;
        return { id: nextId.current++, file, progress: 0, status: error ? "error" : "queued", error };
      });
      setUploads((list) => [...list, ...entries]);
    },
    [maxFileBytes],
  );

  const dismiss = useCallback((id: number) => setUploads((l) => l.filter((u) => u.id !== id)), []);

  const retry = useCallback((id: number) => {
    started.current.delete(id);
    setUploads((l) => l.map((u) => (u.id === id ? { ...u, status: "queued", progress: 0, error: undefined } : u)));
  }, []);

  // Start queued uploads whenever a slot is free.
  useEffect(() => {
    let running = uploads.filter((u) => u.status === "uploading").length;
    for (const u of uploads) {
      if (running >= CONCURRENT) break;
      if (u.status !== "queued" || started.current.has(u.id)) continue;
      started.current.add(u.id);
      running++;
      patch(u.id, { status: "uploading", progress: 0 });
      uploader(u.file, (fraction) => patch(u.id, { progress: fraction }))
        .promise.then((item) => {
          patch(u.id, { status: "done", progress: 1 });
          onUploadedRef.current(item);
          setTimeout(() => dismiss(u.id), DONE_VISIBLE_MS);
        })
        .catch((e: unknown) => patch(u.id, { status: "error", error: e instanceof Error ? e.message : String(e) }));
    }
  }, [uploads, uploader, patch, dismiss]);

  return { uploads, add, dismiss, retry };
}
