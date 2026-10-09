import { useCallback, useEffect, useRef, useState } from "react";
import { fetchInfo, type Info } from "./api";
import { FileListView } from "./components/FileListView";
import { UploadList } from "./components/UploadList";
import { filesFromDrop } from "./dropped";
import { formatBytes } from "./format";
import { useFileList } from "./hooks/useFileList";
import { useUploads } from "./hooks/useUploads";

const hasFiles = (e: DragEvent) => Array.from(e.dataTransfer?.types ?? []).includes("Files");

export function App() {
  const list = useFileList();
  const [info, setInfo] = useState<Info>();
  const [notice, setNotice] = useState<string>();
  const [dragging, setDragging] = useState(false);
  const picker = useRef<HTMLInputElement>(null);

  const refreshInfo = useCallback(() => fetchInfo().then(setInfo).catch(() => undefined), []);
  useEffect(() => void refreshInfo(), [refreshInfo]);

  const { prepend } = list;
  const onUploaded = useCallback(
    (item: Parameters<typeof prepend>[0]) => {
      prepend(item);
      void refreshInfo();
    },
    [prepend, refreshInfo],
  );
  const { uploads, add, dismiss, retry } = useUploads(info?.maxFileBytes, onUploaded);

  // Dropping anywhere on the page works; a counter copes with dragenter/dragleave firing for child elements.
  useEffect(() => {
    let depth = 0;
    const enter = (e: DragEvent) => {
      if (!hasFiles(e)) return;
      e.preventDefault();
      depth++;
      setDragging(true);
    };
    const over = (e: DragEvent) => hasFiles(e) && e.preventDefault();
    const leave = (e: DragEvent) => {
      if (!hasFiles(e)) return;
      depth = Math.max(0, depth - 1);
      if (depth === 0) setDragging(false);
    };
    const drop = (e: DragEvent) => {
      if (!hasFiles(e) || !e.dataTransfer) return;
      e.preventDefault();
      depth = 0;
      setDragging(false);
      const { files, folders } = filesFromDrop(e.dataTransfer);
      setNotice(folders.length ? `Folders can't be uploaded, only files: ${folders.join(", ")}` : undefined);
      if (files.length) add(files);
    };
    window.addEventListener("dragenter", enter);
    window.addEventListener("dragover", over);
    window.addEventListener("dragleave", leave);
    window.addEventListener("drop", drop);
    return () => {
      window.removeEventListener("dragenter", enter);
      window.removeEventListener("dragover", over);
      window.removeEventListener("dragleave", leave);
      window.removeEventListener("drop", drop);
    };
  }, [add]);

  return (
    <main>
      <header>
        <div>
          <h1>Files</h1>
          {info && (
            <p className="sub">
              {info.files.toLocaleString()} {info.files === 1 ? "file" : "files"}, {formatBytes(info.bytes)} of {formatBytes(info.maxTotalBytes)}
            </p>
          )}
        </div>
        <button type="button" className="primary" onClick={() => picker.current?.click()}>
          Choose files
        </button>
        <input
          ref={picker}
          type="file"
          multiple
          hidden
          data-testid="picker"
          onChange={(e) => {
            const chosen = Array.from(e.target.files ?? []);
            e.target.value = "";
            setNotice(undefined);
            if (chosen.length) add(chosen);
          }}
        />
      </header>

      <p className="hint">
        Drop files anywhere on this page{info ? ` (up to ${formatBytes(info.maxFileBytes)} each)` : ""}. Anyone with this link can see and download them.
      </p>
      {notice && (
        <p className="status error" role="alert">
          {notice} <button type="button" className="link" onClick={() => setNotice(undefined)}>Dismiss</button>
        </p>
      )}
      <UploadList uploads={uploads} onDismiss={dismiss} onRetry={retry} />
      <FileListView list={list} />

      {dragging && (
        <div className="overlay" aria-hidden="true">
          <div>Drop to upload</div>
        </div>
      )}
    </main>
  );
}
