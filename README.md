# AmiBroker UDP Data Plug-in

A fast, 64-bit, production-ready AmiBroker real-time data plugin based on the ADK DataPlugEX sample, using a high-performance UDP tick snapshot feed.  
Includes support for lossless multi-symbol delivery, on-the-fly config (dealer, bind IP), daily log rotation, and extension points for backfill and advanced buffering.

---

## 🚀 Quickstart

**Full developer docs:**  
See [Browse the HTML developer portal locally for step-by-step guides, patch instructions, and code snippets.](https://maxizodev.github.io/amibroker-udp-data-plugin/)&xrarr;

---

## Features

- ⚡ **UDP snapshot & tick feed** (background thread, zero data loss)
- 🔒 **Thread-safe buffer** (thousands/sec, hot-path optimized)
- 📝 **Windows config dialog** (dealer ID, interface, more)
- 🗒️ **Daily rolling log** (spdlog, only critical events)
- 🕒 **Backfill placeholder** (for intra-day support, ready to connect)
- 🔌 **Fully ADK-compliant exports** (`GetQuotesEx`, config, threading)
- 🛠️ **Auto-formatted HTML docs** (in `docs/`)

---

## File/Folder Structure

```
/
├── src/               # All plugin/adaptation sources
├── include/           # Common headers (ADK, plugin API)
├── docs/              # Developer site (open index.html!)
│   ├── index.html
│   ├── steps.html
│   └── ... (all guides, features, copy-paste code)
├── CMakeLists.txt     # Build config
├── README.md          # You are here
└── ...
```

---

## 🛠️ Build & Test

1. **Clone and open in Visual Studio (64-bit, Release)**
2. Install ADK headers/libs; add any required UDP/logging dependencies ([see docs](https://maxizodev.github.io/amibroker-udp-data-plugin//buildtest.html)).
3. Build DLL, copy to AmiBroker `Plugins` folder.
4. Configure plugin in AmiBroker GUI.
5. Start UDP feed source; confirm chart updates.
6. Check rolling `plugin_log.txt` on error/config.

Full details in [docs/buildtest.html](https://maxizodev.github.io/amibroker-udp-data-plugin/buildtest.html).

---

## 🧭 Developer Portal

- Documentation: all docs, code patches, and step-by-step [in HTML format](https://maxizodev.github.io/amibroker-udp-data-plugin/).
- Sidebar navigation, 1-click code copy for all required snippets.

---

## 🤝 Contribution

- Fork, create a feature branch, submit PR.
- Open issues or feature requests in GitHub/Azure (pick whichever is enabled).
- All new code must be covered in [the dev portal](https://maxizodev.github.io/amibroker-udp-data-plugin/) before merge.

---

## 📝 License

Your license terms here (e.g., MIT, Apache-2.0, or as appropriate for your org).

---

### Questions?

Open an issue or see [docs/index.html](https://maxizodev.github.io/amibroker-udp-data-plugin/) for troubleshooting and contacts.

---

**Happy building & fast trading!**
