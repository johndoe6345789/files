import { useCallback, useRef, useState } from "react";
import { fetchPage, type FileItem } from "../api";

export interface FileList {
  items: FileItem[];
  hasMore: boolean;
  loading: boolean;
  error: string | null;
  /** Loads the next page; does nothing while one is already loading or when there are no more. */
  loadMore: () => void;
  /** Puts a just-uploaded file at the top. */
  prepend: (item: FileItem) => void;
}

/** Newest-first list, paged with a cursor: `before=<id of the last file we have>`. */
export function useFileList(pageSize = 50): FileList {
  const [items, setItems] = useState<FileItem[]>([]);
  const [hasMore, setHasMore] = useState(true);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const cursor = useRef<number | null>(null);
  const busy = useRef(false);
  const done = useRef(false);

  const loadMore = useCallback(() => {
    if (busy.current || done.current) return;
    busy.current = true;
    setLoading(true);
    setError(null);
    fetchPage(cursor.current, pageSize)
      .then((page) => {
        cursor.current = page.next;
        if (page.next === null) {
          done.current = true;
          setHasMore(false);
        }
        // An upload can land while a page is in flight, so never show the same file twice.
        setItems((old) => {
          const seen = new Set(old.map((f) => f.id));
          return [...old, ...page.items.filter((f) => !seen.has(f.id))];
        });
      })
      .catch((e: unknown) => setError(e instanceof Error ? e.message : String(e)))
      .finally(() => {
        busy.current = false;
        setLoading(false);
      });
  }, [pageSize]);

  const prepend = useCallback((item: FileItem) => {
    setItems((old) => (old.some((f) => f.id === item.id) ? old : [item, ...old]));
  }, []);

  return { items, hasMore, loading, error, loadMore, prepend };
}
