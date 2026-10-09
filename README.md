# files

A deliberately simple public file drop: an infinite-scroll list of files, drop new ones anywhere on the page or
pick them with a file dialog. **There is no login.** Anyone who can reach it can list, upload and download.

- **Frontend:** React + TypeScript (Vite). Infinite scroll, drag and drop anywhere on the page, a file picker,
  per-file upload progress, two uploads at a time, retry on failure.
- **Backend:** [Drogon](https://github.com/drogonframework/drogon) (C++17) with SQLite for the file list and the
  files themselves on disk. Uploads are streamed straight into the file's final place (no temporary copy, memory use
  independent of the size) and downloads support `Range`, so a 10 GB file can be resumed.
- **Front door:** nginx serves the built page and proxies `/api`.

```sh
docker compose up --build        # http://localhost:8080
```

The first build takes about ten minutes because Conan compiles Drogon and its dependencies; that layer is cached
until `backend/conanfile.txt` changes.

## Limits

| | |
| --- | --- |
| One file | 10 GB (`kMaxFileBytes` in `backend/src/Config.h`, `client_max_body_size` in `backend/config.json` and `frontend/nginx.conf`: keep the three equal). Anything in front of the portal (another proxy) needs the same limit and must not buffer request bodies |
| All files together | 100 GiB, `FILES_MAX_TOTAL_BYTES` (bytes) |
| Uploads per visitor | 20 per minute, burst 10 (`frontend/nginx.conf`), answered with 429 |
| Names | cleaned on upload: no directory part, no control or bidi-override characters, valid UTF-8, 200 bytes |

There is no delete in the public API, so nobody can remove other people's files.

## Big uploads

- The upload handler is a Drogon *streaming* handler (`backend/src/Upload.cc`): the body arrives in pieces and each piece is
  written to `<id>.part`, which is renamed when the body is complete. A failed or interrupted upload leaves nothing behind.
- It flushes to disk and drops the kernel's cached pages every 16 MiB. Without that, the page cache of a big write is charged
  to the container's memory limit and, when the disk is slower than the network, the kernel kills the backend (found the hard
  way: `Memory cgroup out of memory`, exit 137). With it, a 4 GiB upload ran under a 96 MB limit and a busy disk without a hitch.
- Every proxy in front must allow 10 GB and stream instead of buffering, or a big upload is first copied to the proxy's disk
  (and, with default settings, refused above its own limit). The bundled nginx does (`proxy_request_buffering off`).

## Keeping a public upload page safe

Files are always served as `application/octet-stream` with `Content-Disposition: attachment`,
`X-Content-Type-Options: nosniff` and `Content-Security-Policy: sandbox`, so an uploaded `.html` can never run as a
page on your domain. That does not stop people from hosting something unpleasant, so only put this somewhere you
are comfortable being the host of.

To remove a file, ask the backend directly (it is not reachable through the nginx front door):

```sh
docker compose exec backend curl -X DELETE http://127.0.0.1:8080/api/files/<id>
```

## API

| | |
| --- | --- |
| `GET /api/files?limit=50&before=<id>` | `{ "items": [{ "id", "name", "size", "createdAt" }], "next": <id or null> }`, newest first. `limit` 1..100. Pass `next` as `before` for the following page. |
| `POST /api/files?name=<file name>` | The request body **is** the file. `201` with the new item; `400` empty or no name; `413` too big; `507` storage full. `curl --data-binary @photo.jpg -H 'Content-Type: application/octet-stream' "http://localhost:8080/api/files?name=photo.jpg"` |
| `GET /api/files/<id>/download` | The file as an attachment. Supports a single `Range: bytes=…` (206/416). |
| `GET /api/info` | `{ maxFileBytes, maxTotalBytes, files, bytes }` |
| `GET /api/health` | `{ "status": "ok" }` |
| `DELETE /api/files/<id>` | Backend only (see above). |

## Develop

```sh
cd frontend && npm install && npm test     # vitest: formatting, paging, upload queue
cd frontend && npm run dev                 # needs the backend on :8080 (the dev server proxies /api to it)
```

`backend/tests/util_test.cc` (name cleaning, `Content-Disposition`, number parsing) is built and run during
`docker build`, so a broken change fails the image build. `test/api_test.py` is an end-to-end test of the running stack:

```sh
docker compose up -d --build --wait
docker run --rm --network files_default -v "$PWD/test:/t:ro" python:3.12-slim \
  python /t/api_test.py http://backend:8080 http://portal:8080
```

It needs an empty `data` volume (it checks the starting state), uploads a 150 MB file (the 10 GB limit itself is checked with a
header-only request; a real 10 GiB upload is a manual test), and finishes by tripping the
rate limit, so run it against a throwaway stack, not one that matters.

## Layout

```
backend/    Drogon app: src/ (controller, storage, util), tests/, Dockerfile, config.json
frontend/   React app, Dockerfile (builds the app, runs its tests, copies it into nginx), nginx.conf
test/       api_test.py
compose.yml
```

## License

MIT
