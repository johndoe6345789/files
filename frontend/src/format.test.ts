import { describe, expect, it } from "vitest";
import { formatBytes, formatDate } from "./format";

describe("formatBytes", () => {
  it.each([
    [0, "0 B"], [1, "1 B"], [1023, "1023 B"], [1024, "1 KB"], [1536, "1.5 KB"], [10 * 1024, "10 KB"],
    [150 * 1024, "150 KB"], [1048576, "1 MB"], [200 * 1048576, "200 MB"], [5.5 * 1073741824, "5.5 GB"],
    [20 * 1073741824, "20 GB"], [3 * 1024 ** 4, "3 TB"],
  ])("%d -> %s", (n, text) => expect(formatBytes(n)).toBe(text));
  it("does not invent numbers for bad input", () => {
    expect(formatBytes(-1)).toBe("?");
    expect(formatBytes(NaN)).toBe("?");
  });
});

describe("formatDate", () => {
  it("formats a UTC ISO time", () => expect(formatDate("2026-10-09T12:30:00Z")).toMatch(/2026/));
  it("shows unparseable input as it is", () => expect(formatDate("yesterday")).toBe("yesterday"));
});
