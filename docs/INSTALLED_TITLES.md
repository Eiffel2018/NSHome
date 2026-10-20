# Installed application catalogue (6802 schema 1)

Available in 0.1.11 development source. Native build and offline tests are the
validation boundary until a console check is recorded. This is a read-only
interface; it never starts/stops a game or changes a user, cheat or SD file.
The current installed 0.1.10 build and public 0.1.9 ZIP do not have this API.

## Installation evidence

The background worker lists NS application records, then requires both:

- `nsIsAnyApplicationEntityInstalled` reports an installed entity.
- NS reports base `Application` metadata; its NCM entry has `Full` install type
  and every content ID listed in that base entry exists in that storage.

Archived/history-only records, update/DLC-only installations and bases missing
listed content are excluded. Applications include NSP homebrew forwarders.
Storage IDs: 2 = inserted game card, 4 = internal user storage, 5 = SD.
This checks presence, not cryptographic integrity or whether launch will succeed.
No ROM-directory scan or PC history is used as installation evidence.

Names come from NS control/NACP data: preferred system language, then first
available language. If name metadata cannot be read, the verified application
remains listed with `name:null`, `name_source:"unavailable"`, and its `name_rc`.
Malformed UTF-8 is replaced; names are bounded to 511 UTF-8 bytes.
Titles are sorted by the complete, uppercase, 16-hex-digit base Title ID.

## Commands

Use the existing `SWITCHU 1` greeting and authentication; one command per socket.
All successful replies start `OK titles ` followed by one UTF-8 JSON object.
The maximum response line is 65536 bytes, including its newline.

```text
titles refresh
titles status
titles page BOOT GENERATION OFFSET LIMIT
```

`refresh` queues a background rebuild and returns `building`. Repeating it while
building returns the same generation. A new refresh after completion invalidates
the previous snapshot. It does not modify the launch queue/request ID.
`status` reports `idle`, `building`, `ready`, `failed`, or `invalidated`.
Poll the same boot/generation; only `ready` can be paged. Failed builds publish
no partial results. A 60-second scan budget is checked between native service
calls; the worker cannot interrupt a kernel IPC already in progress.

`BOOT` and `GENERATION` are exact unsigned decimal integers, sent in JSON as
**strings**, never floating-point values. Offset is 0..4096; limit is 1..16.
Preserve the returned pair on every page. `next:null` marks the last page.
An empty successful catalogue has `total:0`, `items:[]`, `next:null`.

Example page (names and IDs below are illustrative):

```json
{
  "schema": 1,
  "source": "ns+ncm.full_application_contents",
  "boot": "142418635",
  "generation": "1",
  "state": "ready",
  "rc": "0x00000000",
  "total": 1,
  "scanned": 3,
  "page_max": 16,
  "profile_selection_supported": false,
  "offset": 0,
  "next": null,
  "items": [{
    "title_id": "0100000000000001",
    "name": "Example",
    "name_source": "nacp.system_language",
    "name_rc": "0x00000000",
    "storage_id": 5
  }]
}
```

Refresh/status have the same header without `offset`, `next`, or `items`.
`scanned` counts unique NS records; `total` counts verified applications.
`rc` is an opaque native result. Name errors do not change catalogue `rc`.

| Reply | Meaning |
|---|---|
| `ERR titles-unavailable` | Worker could not start |
| `ERR titles-stale` | Boot/generation changed; discard partial pages |
| `ERR titles-not-ready` | No ready snapshot for this pair |
| `ERR titles-range` | Offset is beyond this snapshot's end |
| `ERR invalid-command` | Bad syntax, noncanonical decimal, or out-of-range limit |

Observed NS record/SD mount events invalidate a ready snapshot. This is a bounded
point-in-time catalogue, not an atomic inventory/launch transaction. Refresh to
observe later changes; do not automatically retry a stale traversal or silently
merge pages from different generations. Max 4096 NS records; exceeding the bound
or an evidence-service failure fails the rebuild rather than truncating it.

## Python

```sh
python samples/18_installed_titles.py --host SWITCH_IP --page-size 16 --wait 60
```

Common `--port`/`--timeout` options still apply. `--wait` bounds client polling
(1..120 seconds); each socket operation has its own `--timeout`.
`Client.titles_refresh()`, `titles_status()`, and
`titles_page(boot,generation,offset=0,limit=16)` expose the individual steps.
`Client.installed_titles(wait=60,page_size=16)` refreshes once, polls, validates all
pages, and checks the generation again before returning. Errors discard partial
results; it never automatically submits another refresh. Sample 18 prints JSON.

Offline checks: `bash tests/run.sh`, then
`python -m unittest discover -s tests -p 'installed_titles_client_test.py'`.
Tests cover archive/missing-base exclusion, failed/inconsistent rebuilds,
pagination/reboot identity, large UTF-8 replies, unavailable names, and no retry.

There is no account list or explicit-UID launch command. Existing
`gamestart TITLE_ID` keeps its first-account behavior.
