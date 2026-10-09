export interface FileItem {
  id: number;
  name: string;
  size: number;
  createdAt: string;
}
export interface Page {
  items: FileItem[];
  /** Cursor for the next page (pass as `before`), or null when this was the last one. */
  next: number | null;
}
export interface Info {
  maxFileBytes: number;
  maxTotalBytes: number;
  files: number;
  bytes: number;
}

async function getJson<T>(url: string, signal?: AbortSignal): Promise<T> {
  const res = await fetch(url, { signal });
  if (!res.ok) throw new Error(await errorMessage(res.status, () => res.text()));
  return (await res.json()) as T;
}

export function fetchPage(before: number | null, limit = 50, signal?: AbortSignal): Promise<Page> {
  const q = new URLSearchParams({ limit: String(limit) });
  if (before !== null) q.set("before", String(before));
  return getJson<Page>(`/api/files?${q}`, signal);
}

export function fetchInfo(signal?: AbortSignal): Promise<Info> {
  return getJson<Info>("/api/info", signal);
}

export const downloadUrl = (id: number) => `/api/files/${id}/download`;

async function errorMessage(status: number, body: () => Promise<string>): Promise<string> {
  if (status === 413) return "That file is too large.";
  if (status === 429) return "Too many uploads at once, wait a moment and try again.";
  if (status === 507) return "The storage is full.";
  try {
    const parsed = JSON.parse(await body());
    if (parsed && typeof parsed.error === "string") return parsed.error;
  } catch {
    /* not JSON */
  }
  return `The server answered ${status}.`;
}

export interface UploadHandle {
  promise: Promise<FileItem>;
  abort: () => void;
}

/** Sends the file as the raw request body (so the browser reports real upload progress). */
export function uploadFile(file: File, onProgress: (fraction: number) => void): UploadHandle {
  const xhr = new XMLHttpRequest();
  const promise = new Promise<FileItem>((resolve, reject) => {
    xhr.open("POST", `/api/files?name=${encodeURIComponent(file.name)}`);
    xhr.setRequestHeader("Content-Type", "application/octet-stream");
    xhr.responseType = "text";
    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable && e.total > 0) onProgress(e.loaded / e.total);
    };
    xhr.onload = async () => {
      if (xhr.status === 201) {
        try {
          resolve(JSON.parse(xhr.responseText) as FileItem);
        } catch {
          reject(new Error("The server sent an unreadable answer."));
        }
      } else {
        reject(new Error(await errorMessage(xhr.status, async () => xhr.responseText)));
      }
    };
    xhr.onerror = () => reject(new Error("The upload failed, check your connection."));
    xhr.onabort = () => reject(new Error("Cancelled."));
    xhr.send(file);
  });
  return { promise, abort: () => xhr.abort() };
}
