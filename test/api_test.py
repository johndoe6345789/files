#!/usr/bin/env python3
"""Integration test for the files stack. Usage: api_test.py <backend-url> <portal-url> [quota]
`quota` mode expects a backend started with FILES_MAX_TOTAL_BYTES=1000000 and only checks the storage limit."""
import hashlib, http.client, json, os, sys, urllib.parse
from urllib.parse import urlparse

BACK, PORTAL = urlparse(sys.argv[1]), urlparse(sys.argv[2])
QUOTA = len(sys.argv) > 3 and sys.argv[3] == "quota"
fails = 0

def call(base, method, path, body=None, headers=None):
    c = http.client.HTTPConnection(base.hostname, base.port, timeout=300)
    c.request(method, path, body=body, headers=headers or {})
    r = c.getresponse(); data = r.read(); h = {k.lower(): v for k, v in r.getheaders()}; c.close()
    return r.status, h, data

def jcall(base, method, path, body=None):
    s, h, d = call(base, method, path, body)
    return s, (json.loads(d) if d[:1] in (b"{", b"[") else d), h

def check(name, cond, detail=""):
    global fails
    print(("ok   " if cond else "FAIL ") + name + ("" if cond else f"   [{detail}]"))
    fails += 0 if cond else 1

def up(base, name, data):
    return jcall(base, "POST", "/api/files?name=" + urllib.parse.quote(name, safe=""), data)

if QUOTA:
    s, j, _ = up(BACK, "a.bin", os.urandom(600_000)); check("quota: first 600 kB fits", s == 201, s)
    s, j, _ = up(BACK, "b.bin", os.urandom(600_000)); check("quota: second 600 kB is refused with 507", s == 507 and "full" in j["error"], (s, j))
    s, j, _ = up(BACK, "c.bin", os.urandom(300_000)); check("quota: a smaller file still fits", s == 201, (s, j))
    s, info, _ = jcall(BACK, "GET", "/api/info"); check("quota: info shows usage and the limit", info["bytes"] == 900_000 and info["maxTotalBytes"] == 1_000_000, info)
    sys.exit(1 if fails else 0)

# ---- basics
s, j, h = jcall(BACK, "GET", "/api/health"); check("health", s == 200 and j == {"status": "ok"}, (s, j))
s, info, _ = jcall(BACK, "GET", "/api/info")
check("info", s == 200 and info["maxFileBytes"] == 10 * 1024**3 and info["files"] == 0 and info["bytes"] == 0, info)
s, j, _ = jcall(BACK, "GET", "/api/files"); check("empty list", s == 200 and j == {"items": [], "next": None}, j)

# ---- upload + download round trip, byte for byte
blob = os.urandom(3_000_017)
s, item, _ = up(BACK, "round trip.bin", blob)
check("upload returns 201 with the file", s == 201 and item["name"] == "round trip.bin" and item["size"] == len(blob) and item["id"] > 0 and item["createdAt"].endswith("Z"), (s, item))
s, h, d = call(BACK, "GET", f"/api/files/{item['id']}/download")
check("download is identical", s == 200 and hashlib.sha256(d).digest() == hashlib.sha256(blob).digest() and h["content-length"] == str(len(blob)), (s, len(d)))
check("download is an attachment of opaque bytes", h["content-type"] == "application/octet-stream" and h["content-disposition"].startswith("attachment;")
      and h["x-content-type-options"] == "nosniff" and "sandbox" in h["content-security-policy"], h)

# ---- a web page uploaded here must never be served as a page
s, it, _ = up(BACK, "evil.html", b"<script>alert(1)</script>")
s, h, d = call(BACK, "GET", f"/api/files/{it['id']}/download")
check("an uploaded .html is still downloaded as opaque bytes", h["content-type"] == "application/octet-stream" and "attachment" in h["content-disposition"] and d == b"<script>alert(1)</script>", h)

# ---- names
cases = {
    "../../etc/passwd": "passwd", "C:\\Users\\x\\a.txt": "a.txt", "a\nb\r\nc.txt": "abc.txt",
    "report\u202etxt.exe": "reporttxt.exe", "café €.txt": "café €.txt", "": None, "...": "file",
}
for raw, want in cases.items():
    if want is None:
        s, j, _ = jcall(BACK, "POST", "/api/files?name=", b"x"); check("missing name is a 400", s == 400, (s, j)); continue
    s, j, _ = up(BACK, raw, b"x"); check(f"name {raw!r} -> {want!r}", s == 201 and j["name"] == want, (s, j))
s, it, _ = up(BACK, 'we"ird;na%me.txt', b"x")
s, h, _ = call(BACK, "GET", f"/api/files/{it['id']}/download")
check("odd characters are safe in Content-Disposition", h["content-disposition"] == "attachment; filename=\"we_ird_na_me.txt\"; filename*=UTF-8''we%22ird%3Bna%25me.txt", h["content-disposition"])
s, it, _ = up(BACK, "naïve ☃.txt", b"x"); s, h, _ = call(BACK, "GET", f"/api/files/{it['id']}/download")
check("unicode name round-trips via filename*", "filename*=UTF-8''na%C3%AFve%20%E2%98%83.txt" in h["content-disposition"], h["content-disposition"])

# ---- bad requests
s, j, _ = jcall(BACK, "POST", "/api/files?name=x", b""); check("empty body is a 400", s == 400, (s, j))
for bad in ("abc", "99999999999999999999", "-1", "1.5"):
    s, j, _ = jcall(BACK, "GET", f"/api/files/{bad}/download"); check(f"download id {bad!r} is a 404", s == 404, s)
s, j, _ = jcall(BACK, "GET", "/api/files/99999/download"); check("unknown id is a 404", s == 404, s)
s, j, _ = jcall(BACK, "GET", "/api/files?limit=abc"); check("limit=abc is a 400", s == 400, s)
s, j, _ = jcall(BACK, "GET", "/api/files?before=abc"); check("before=abc is a 400", s == 400, s)
s, j, _ = jcall(BACK, "GET", "/api/files?limit=0"); check("limit=0 is raised to 1", s == 200 and len(j["items"]) == 1, j)
s, j, _ = jcall(BACK, "GET", "/api/files?limit=100000"); check("a huge limit is capped at 100", s == 200 and len(j["items"]) <= 100, len(j["items"]))

# ---- paging: newest first, a cursor, no gaps or repeats, even while new files arrive
ids = [up(BACK, f"page-{i:02}.txt", b"p")[1]["id"] for i in range(25)]
seen, cursor, pages = [], None, 0
while True:
    s, j, _ = jcall(BACK, "GET", "/api/files?limit=10" + (f"&before={cursor}" if cursor else ""))
    seen += [f["id"] for f in j["items"]]; pages += 1
    if pages == 1: up(BACK, "arrived-during-paging.txt", b"n")   # must not disturb the pages after it
    cursor = j["next"]
    if cursor is None: break
check("paging visits every file once, newest first", seen == sorted(set(seen), reverse=True) and set(ids) <= set(seen), seen[:12])
check("paging used several pages and the last has next=null", pages >= 4 and cursor is None, pages)

# ---- through the portal
s, j, h = jcall(PORTAL, "GET", "/api/health"); check("portal proxies the API", s == 200 and j["status"] == "ok", (s, j))
s, h, d = call(PORTAL, "GET", "/"); check("portal serves the page", s == 200 and b'<div id="root">' in d and "script-src 'self'" in h["content-security-policy"], (s, h))
s, h, d = call(PORTAL, "GET", "/some/client/route"); check("unknown paths fall back to the app", s == 200 and b'<div id="root">' in d, s)
s, j, _ = jcall(PORTAL, "DELETE", f"/api/files/{ids[0]}"); check("DELETE is not reachable through the portal", s == 403, s)
s, j, _ = jcall(PORTAL, "PUT", f"/api/files/{ids[0]}"); check("PUT is not reachable through the portal", s == 403, s)
big = os.urandom(150 * 1024 * 1024)
s, it, _ = up(PORTAL, "big.bin", big); check("a 150 MB upload through the portal works", s == 201 and it["size"] == len(big), (s, str(it)[:80]))
s, h, d = call(PORTAL, "GET", f"/api/files/{it['id']}/download")
check("and downloads back identical", s == 200 and hashlib.sha256(d).digest() == hashlib.sha256(big).digest(), (s, len(d)))
del d
# Over the 10 GiB limit: announced by Content-Length, so the answer comes without sending a single byte of body.
def announce(base, length):
    c = http.client.HTTPConnection(base.hostname, base.port, timeout=30)
    c.putrequest("POST", "/api/files?name=over.bin"); c.putheader("Content-Length", str(length)); c.endheaders()
    r = c.getresponse(); r.read(); c.close(); return r.status
check("10 GiB + 1 byte is refused by the portal with 413", announce(PORTAL, 10 * 1024**3 + 1) == 413)
check("10 GiB + 1 byte is refused by the backend with 413", announce(BACK, 10 * 1024**3 + 1) == 413)
del big

# ---- resumable downloads (Range)
data = bytes(range(256)) * 1000
s, it, _ = up(BACK, "ranged.bin", data)
def rng(base, header):
    return call(base, "GET", f"/api/files/{it['id']}/download", None, {"Range": header} if header else None)
s, h, d = rng(BACK, None); check("downloads advertise Accept-Ranges", s == 200 and h.get("accept-ranges") == "bytes" and d == data, h)
for base, label in ((BACK, "backend"), (PORTAL, "portal")):
    s, h, d = rng(base, "bytes=0-9"); check(f"{label}: first 10 bytes -> 206", s == 206 and d == data[:10] and h["content-range"] == f"bytes 0-9/{len(data)}", (s, h.get("content-range"), d[:12]))
    s, h, d = rng(base, "bytes=250000-"); check(f"{label}: open-ended range resumes at an offset", s == 206 and d == data[250000:] and h["content-range"] == f"bytes 250000-{len(data)-1}/{len(data)}", (s, h.get("content-range"), len(d)))
    s, h, d = rng(base, "bytes=-5"); check(f"{label}: last 5 bytes", s == 206 and d == data[-5:], (s, d))
s, h, d = rng(BACK, f"bytes={len(data)}-"); check("a range past the end is 416 with the size", s == 416 and h["content-range"] == f"bytes */{len(data)}", (s, h))
s, h, d = rng(BACK, "bytes=0-1,5-6"); check("several ranges are ignored (whole file)", s == 200 and d == data, s)
s, h, d = rng(BACK, "bytes=garbage"); check("a junk Range is ignored (whole file)", s == 200 and d == data, s)

# ---- operator delete, straight to the backend
s, j, _ = jcall(BACK, "DELETE", f"/api/files/{it['id']}"); check("operator DELETE works", s == 200 and j["deleted"] == it["id"], (s, j))
s, j, _ = jcall(BACK, "GET", f"/api/files/{it['id']}/download"); check("a deleted file is a 404", s == 404, s)
s, j, _ = jcall(BACK, "DELETE", f"/api/files/{it['id']}"); check("deleting twice is a 404", s == 404, s)

# ---- rate limit (last: it makes later uploads through the portal fail)
codes = [up(PORTAL, f"rl-{i}.txt", b"r")[0] for i in range(45)]
check("uploads through the portal are rate limited (429)", 429 in codes and codes.count(201) >= 10, {c: codes.count(c) for c in set(codes)})
s, j, _ = jcall(PORTAL, "GET", "/api/files"); check("reading is not rate limited", s == 200, s)

# Behind CapRover the visitor is the second-to-last X-Forwarded-For entry; a visitor-supplied prefix must not help.
def post_as(chain, name="x.txt"):
    return call(PORTAL, "POST", "/api/files?name=" + name, b"r", {"X-Forwarded-For": chain})[0]
codes = [post_as(f"10.9.{i}.1, 5.5.5.5, 6.6.6.6") for i in range(40)]   # a different fake prefix every time, same visitor
check("faking the start of X-Forwarded-For does not escape the limit", 429 in codes and codes.count(201) <= 12, {c: codes.count(c) for c in set(codes)})
check("another visitor is not held back by it", post_as("1.1.1.1, 7.7.7.7, 6.6.6.6") == 201)

print("ALL PASSED" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
