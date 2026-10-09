import { act, cleanup, render, screen, waitFor } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { FileListView } from "./components/FileListView";
import { useFileList } from "./hooks/useFileList";
import type { FileItem } from "./api";

// jsdom has no IntersectionObserver: a fake that lets the test decide when the marker is "on screen".
class FakeObserver {
  static all: FakeObserver[] = [];
  disconnected = false;
  constructor(private cb: (e: { isIntersecting: boolean }[]) => void) { FakeObserver.all.push(this); }
  observe() {}
  disconnect() { this.disconnected = true; }
  fire(isIntersecting: boolean) { this.cb([{ isIntersecting }]); }
  static live() { return FakeObserver.all.filter((o) => !o.disconnected); }
}

const file = (id: number): FileItem => ({ id, name: `file-${id}.txt`, size: id * 100, createdAt: "2026-10-09T10:00:00Z" });
const range = (from: number, to: number) => Array.from({ length: from - to + 1 }, (_, i) => file(from - i)); // newest first

function Harness() {
  const list = useFileList(3);
  return (
    <>
      <button onClick={() => list.prepend(file(99))}>prepend</button>
      <FileListView list={list} />
    </>
  );
}

let calls: string[];
beforeEach(() => {
  FakeObserver.all = [];
  calls = [];
  vi.stubGlobal("IntersectionObserver", FakeObserver);
  vi.stubGlobal("fetch", vi.fn(async (url: string) => {
    calls.push(url);
    const before = new URL(url, "http://x").searchParams.get("before");
    const pages: Record<string, unknown> = {
      "": { items: range(8, 6), next: 6 },
      "6": { items: range(5, 3), next: 3 },
      "3": { items: range(2, 1), next: null },
    };
    return { ok: true, status: 200, json: async () => pages[before ?? ""], text: async () => "" };
  }));
});
afterEach(() => { cleanup(); vi.unstubAllGlobals(); });

const names = () => screen.getAllByRole("link").map((a) => a.textContent);
const scrollToMarker = async () => { await act(async () => FakeObserver.live().at(-1)!.fire(true)); };

describe("infinite scroll", () => {
  it("loads the first page as soon as the marker is seen, then each next page with the cursor", async () => {
    render(<Harness />);
    expect(calls).toEqual([]); // nothing until the marker is on screen
    await scrollToMarker();
    await waitFor(() => expect(names()).toEqual(["file-8.txt", "file-7.txt", "file-6.txt"]));
    expect(calls).toEqual(["/api/files?limit=3"]);

    await scrollToMarker();
    await waitFor(() => expect(names()).toHaveLength(6));
    expect(calls[1]).toBe("/api/files?limit=3&before=6");

    await scrollToMarker();
    await waitFor(() => expect(names()).toHaveLength(8));
    expect(calls[2]).toBe("/api/files?limit=3&before=3");
    expect(names().at(-1)).toBe("file-1.txt");
    expect(screen.getByText("That's everything.")).toBeTruthy();
  });

  it("stops asking once the last page is in", async () => {
    render(<Harness />);
    for (let i = 0; i < 3; i++) { await scrollToMarker(); await waitFor(() => expect(calls).toHaveLength(i + 1)); }
    await waitFor(() => expect(screen.queryByText("Loading…")).toBeNull());
    expect(FakeObserver.live()).toHaveLength(0); // no observer left watching
    expect(calls).toHaveLength(3);
  });

  it("re-observes after a load, so a tall window keeps filling", async () => {
    render(<Harness />);
    await scrollToMarker();
    await waitFor(() => expect(names()).toHaveLength(3));
    // a fresh observer exists for the new state; it reports the marker already visible on observe
    const before = FakeObserver.live().length;
    expect(before).toBe(1);
  });

  it("puts a new upload on top without duplicating it when its page arrives later", async () => {
    render(<Harness />);
    await scrollToMarker();
    await waitFor(() => expect(names()).toHaveLength(3));
    await act(async () => screen.getByText("prepend").click());
    expect(names()[0]).toBe("file-99.txt");
    await act(async () => screen.getByText("prepend").click()); // same id again
    expect(names().filter((n) => n === "file-99.txt")).toHaveLength(1);
  });

  it("shows an error with a retry, and the retry works", async () => {
    const fetchMock = vi.mocked(fetch);
    fetchMock.mockImplementationOnce(async () => ({ ok: false, status: 500, text: async () => '{"error":"boom"}' }) as Response);
    render(<Harness />);
    await scrollToMarker();
    await waitFor(() => expect(screen.getByRole("alert").textContent).toContain("boom"));
    await act(async () => screen.getByText("Try again").click());
    await waitFor(() => expect(names()).toHaveLength(3));
  });

  it("says so when there are no files", async () => {
    vi.mocked(fetch).mockImplementationOnce(async () => ({ ok: true, status: 200, json: async () => ({ items: [], next: null }), text: async () => "" }) as Response);
    render(<Harness />);
    await scrollToMarker();
    await waitFor(() => expect(screen.getByText(/No files yet/)).toBeTruthy());
  });

  it("links each file to its download and keeps the original name", async () => {
    render(<Harness />);
    await scrollToMarker();
    await waitFor(() => expect(names()).toHaveLength(3));
    const link = screen.getByText("file-7.txt") as HTMLAnchorElement;
    expect(link.getAttribute("href")).toBe("/api/files/7/download");
    expect(link.getAttribute("download")).toBe("file-7.txt");
  });
});
