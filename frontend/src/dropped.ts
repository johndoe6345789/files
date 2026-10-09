/** The files in a drop, and the names of folders that were skipped (a folder can't be sent as one file). */
export function filesFromDrop(dt: DataTransfer): { files: File[]; folders: string[] } {
  const files: File[] = [];
  const folders: string[] = [];
  const items = Array.from(dt.items ?? []).filter((i) => i.kind === "file");
  if (items.length === 0) return { files: Array.from(dt.files), folders };
  for (const item of items) {
    const entry = typeof item.webkitGetAsEntry === "function" ? item.webkitGetAsEntry() : null;
    if (entry?.isDirectory) {
      folders.push(entry.name);
      continue;
    }
    const file = item.getAsFile();
    if (file) files.push(file);
  }
  return { files, folders };
}
