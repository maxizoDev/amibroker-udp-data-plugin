# Project Direction

This repository implements an **AmiBroker data plug-in** that delivers real-time
quotes from a UDP tick/snapshot feed and supplies historical bars (backfill) on
demand. It is a **Windows 64-bit DLL** loaded by `Broker.exe` via the AmiBroker
Development Kit (ADK) plug-in interface.

## Locked decisions

- **Implementation strategy: Option B — ADK-faithful.** Implement the real ADK
  data-plug-in contract (exports, structures, threading, backfill, streaming
  notification) exactly as documented by the ADK. Do not invent APIs.
- **Source of truth: the official ADK** at `https://gitlab.com/amibroker/adk.git`,
  cloned read-only into `.adk-reference/` (gitignored). The latest doc revision
  in that tree is **README.md (rev 2.11, 18 Aug 2024)**, which is a strict
  superset of `Docs/adk.html` (rev 2.10a, 4 Aug 2010); README is authoritative,
  the HTML is the legacy version of the same document.
- **There is no ADK sample called `DataPlugEX`.** Existing references in our
  `README.md` and `docs/*.html` are incorrect and must be rewritten against the
  real ADK sample names.
- **Reference samples for our use case** (real-time + backfill data plug-in):
  - `Samples/Data_Template/` — official scaffold (PluginInfo, Configure dialog
    via MFC, Notify, legacy `GetQuotes` wrapper, exports).
  - `Samples/QT/` (Quote Tracker) — canonical real-time + intraday backfill
    plug-in. Source of: `RecentInfo` table, `GetSymbolLimit`, `GetStatus`,
    `GetRecentInfo`, `GetIntradayBars`, `BlendQuoteArrays`, `Notify` state
    machine, status-bar context menu.
  - `Samples/ODBC/` — borrow its `PostMessage(WM_USER_STREAMING_UPDATE, …)`
    timer-driven idiom (asynchronous, matches the §3.4.1 non-blocking guidance
    better than QT's `SendMessage`).
  - `Samples/ASCII/` — minimal `GetQuotesEx` reference for the file-driven case
    (currently only relevant if we ever want a deterministic offline test mode).

## Required exports (ADK contract)

| Function | Required | Source |
|---|---|---|
| `GetPluginInfo` | yes | every plugin |
| `Init` / `Release` | yes | every plugin |
| `GetQuotes` (legacy) | yes — wrapper that calls `GetQuotesEx` | for AmiBroker < 5.27 |
| `GetQuotesEx` | yes | the data-fetch hot path |
| `Notify` | strongly recommended | wires `hMainWnd`, lifecycle |
| `Configure` | yes | shown when user clicks "Configure" in DB Settings |
| `SetTimeBase` | yes for intraday | accept/reject periodicity |
| `GetRecentInfo` | yes (real-time) | feeds AmiBroker's RT quote window |
| `GetSymbolLimit` | yes (real-time) | upper bound on streaming symbols |
| `GetPluginStatus` | recommended | status-bar feedback |
| `IsBackfillComplete` | recommended | per-symbol backfill flag (newer ADK) |
| `GetExtraData` | optional | non-quotation data |

`Plugin.h` and `Plugin_Legacy.h` from the ADK must be used **as-is**; don't
roll a custom subset. The required `WM_USER_STREAMING_UPDATE` value is
`WM_USER + 13000`; **post**, don't send, this message from worker threads
(per ADK README §3.4.1, plugin must not block on `GetQuotesEx` /
`GetRecentInfo`).

## Boundaries

- 64-bit only (`x64`, MSVC). The `AmiVar` struct uses `#pragma pack(2)`;
  packing must be preserved.
- The `Quotation` struct is **40 bytes, 8-byte aligned**, fields are all
  `float` (volume/open-int included). `AmiDate` is a 64-bit packed timestamp;
  in tick mode the plugin must guarantee unique timestamps (use the `MicroSec`
  / `MilliSec` fields, e.g. by counting consecutive ticks within one second).
- Configuration UI uses MFC (`CDialog::DoModal`) inside `Configure(...)`;
  this is the pattern in Data_Template, QT, and ODBC.
- For testing/debugging: launch `Broker.exe` from the IDE; optionally set
  `HKCU\Software\TJP\Broker\Settings\TrapExceptions = 0` in dev only.

## Use-case parameters (locked with user)

- **Live transport:** UDP feed (tick / snapshot). Wire format spec is provided
  by the user — to be added under `docs/spec/` and referenced from
  `ParseUdpPacket()`. Until the spec is in the tree, the parser body remains a
  TODO; wiring around it (socket, thread, buffer, conversion to `Quotation`)
  must already be ADK-correct.
- **Backfill transport:** separate HTTP/REST endpoint. Mirrors the QT idiom
  (HTTP request from worker, parse response into bars, post
  `WM_USER_STREAMING_UPDATE` so AmiBroker re-calls `GetQuotesEx`). Endpoint
  URL, auth, query params, and response format are user-supplied; treat them
  the same way as the UDP spec (config-driven, with a stubbed
  `RequestBackfill()` until specified).
- **Periodicities:** primary use is 1-minute and other intraday bars; design
  must accommodate 1-second and tick-by-tick (TBT) without architectural
  rework. EOD is not a focus. `SetTimeBase` accepts all intervals
  `< 86400`; tick mode requires the `MicroSec` collision-resolution rule.
- **Symbol limit:** 200–500. `GetSymbolLimit` returns 500; linear scan over
  the `RecentInfo` table is acceptable; pre-allocate ~512 slots.
- **Tick-uniqueness rule:** per-symbol counter packed into `PackDate.MicroSec`,
  reset whenever the integer second advances for that symbol. Matches
  AmiBroker's own RT plugins; works for 1-second and tick (TBT) bases without
  losing data.
- **Historical-bars residency:** the plugin keeps finished intraday bars in
  memory per symbol per periodicity (QT-style). `GetQuotesEx` does a tail
  `memcpy`/blend through `BlendQuoteArrays`; the worker thread is responsible
  for promoting ticks to bars. No on-demand recompression on the
  `GetQuotesEx` hot path.
- **Skeleton-first build order:** put the ADK-faithful contract in place
  *first* (sockets, worker thread, tick buffer, per-symbol bar table,
  `Quotation` conversion, all required exports, MFC Configure dialog,
  `WM_USER_STREAMING_UPDATE` plumbing, build). The two spec-dependent
  functions — `ParseUdpPacket()` and `RequestBackfill()` — stay clearly-marked
  TODOs until the user provides the wire and HTTP specs.

## Operating notes for future Claude sessions

- The ADK clone at `.adk-reference/` is gitignored; never commit it. If it's
  missing, re-clone with `git clone --depth=1 https://gitlab.com/amibroker/adk.git .adk-reference`.
- When in doubt about an interface or struct field, **read `.adk-reference/`
  first** — README.md, Include/Plugin.h, Include/Plugin_Legacy.h, then the
  Samples — before writing or changing code.
- Our `README.md` and `docs/*.html` predate the ADK-faithful direction and
  contain inaccuracies (DataPlugEX naming, fabricated function names like
  `GetBackfillQuotes`, fabricated line-number citations, Win32-style
  `ConfigDlgProc` instead of MFC, missing `WM_USER_STREAMING_UPDATE`,
  inverted backfill model). They will be rewritten **after** the code is
  ADK-faithful, not before. A discrepancy log is maintained in conversation
  until that rewrite happens.
