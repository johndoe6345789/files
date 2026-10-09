import { act, renderHook, waitFor } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import type { FileItem, UploadHandle } from "./api";
import { useUploads, type Uploader } from "./hooks/useUploads";

const fileOf = (name: string, size: number) => new File([new Uint8Array(size)], name);
const item = (id: number, name: string): FileItem => ({ id, name, size: 1, createdAt: "2026-10-09T10:00:00Z" });

// An uploader the test controls: each call is parked until the test settles it.
function controllable() {
  const pending: { name: string; resolve: (f: FileItem) => void; reject: (e: Error) => void; progress: (n: number) => void }[] = [];
  const uploader: Uploader = (file, progress) => {
    let resolve!: (f: FileItem) => void, reject!: (e: Error) => void;
    const promise = new Promise<FileItem>((res, rej) => { resolve = res; reject = rej; });
    pending.push({ name: file.name, resolve, reject, progress });
    return { promise, abort: () => {} } satisfies UploadHandle;
  };
  return { pending, uploader };
}

describe("useUploads", () => {
  it("rejects empty and oversized files without sending them", () => {
    const { pending, uploader } = controllable();
    const { result } = renderHook(() => useUploads(1000, () => {}, uploader));
    act(() => result.current.add([fileOf("empty", 0), fileOf("big", 1001), fileOf("ok", 1000)]));
    const by = Object.fromEntries(result.current.uploads.map((u) => [u.file.name, u]));
    expect(by.empty.status).toBe("error");
    expect(by.empty.error).toMatch(/empty/i);
    expect(by.big.status).toBe("error");
    expect(by.big.error).toMatch(/limit is 1000 B/);
    expect(pending.map((p) => p.name)).toEqual(["ok"]);
  });

  it("sends two at a time and starts the next as one finishes", async () => {
    const { pending, uploader } = controllable();
    const { result } = renderHook(() => useUploads(undefined, () => {}, uploader));
    act(() => result.current.add(["a", "b", "c", "d"].map((n) => fileOf(n, 5))));
    await waitFor(() => expect(pending.map((p) => p.name)).toEqual(["a", "b"]));
    expect(result.current.uploads.map((u) => u.status)).toEqual(["uploading", "uploading", "queued", "queued"]);
    await act(async () => pending[0].resolve(item(1, "a")));
    await waitFor(() => expect(pending.map((p) => p.name)).toEqual(["a", "b", "c"]));
  });

  it("reports progress and hands each finished file to onUploaded", async () => {
    const { pending, uploader } = controllable();
    const done = vi.fn();
    const { result } = renderHook(() => useUploads(undefined, done, uploader));
    act(() => result.current.add([fileOf("a", 5)]));
    await waitFor(() => expect(pending).toHaveLength(1));
    act(() => pending[0].progress(0.4));
    expect(result.current.uploads[0].progress).toBe(0.4);
    await act(async () => pending[0].resolve(item(7, "a")));
    expect(done).toHaveBeenCalledWith(item(7, "a"));
    expect(result.current.uploads[0].status).toBe("done");
  });

  it("keeps a failed upload with its message, and retry sends it again", async () => {
    const { pending, uploader } = controllable();
    const { result } = renderHook(() => useUploads(undefined, () => {}, uploader));
    act(() => result.current.add([fileOf("a", 5)]));
    await waitFor(() => expect(pending).toHaveLength(1));
    await act(async () => pending[0].reject(new Error("The storage is full.")));
    expect(result.current.uploads[0]).toMatchObject({ status: "error", error: "The storage is full." });
    act(() => result.current.retry(result.current.uploads[0].id));
    await waitFor(() => expect(pending).toHaveLength(2));
    expect(result.current.uploads[0].status).toBe("uploading");
  });

  it("dismiss removes an entry", () => {
    const { uploader } = controllable();
    const { result } = renderHook(() => useUploads(10, () => {}, uploader));
    act(() => result.current.add([fileOf("big", 11)]));
    act(() => result.current.dismiss(result.current.uploads[0].id));
    expect(result.current.uploads).toEqual([]);
  });
});
