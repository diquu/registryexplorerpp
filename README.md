# Registry Explorer++

made by diquuuu

A fast, offline registry hive explorer for Windows, built in C++20 with Direct2D-style
rendering over Dear ImGui and DirectX 11. No runtime, no installer, no dependencies:
one `registryexplorerpp.exe`.

Reads `.dat` hives the way forensic tools do — without ever touching the live registry —
and shows the key tree and values in the same dense, searchable, sortable table format as
[jumplens](https://github.com/diquu/jumplens).

## What it does today

- Opens any hive file (`.dat`, `.hive`): SYSTEM, SOFTWARE, SAM, SECURITY, NTUSER fill, BCD, DRIVERS, UsrClass and the
  rest. Works on 45 MB hives in under a second.
- Full key tree with expand/collapse, truncated names with full path in the details drawer.
- Values table:
  - `#`, `NAME`, `TYPE`, `DATA` columns with sorting on every column (click to toggle).
  - Live search across value name and data (Ctrl+F).
  - Right-click a value for **Copy value name**, **Copy value data**, **Copy key path**.
- Details drawer: key path, last write time, flags, subkey/value counts.
- Export the whole hive to CSV (UTF-8 with BOM) for downstream analysis.
- Status bar with key and value counts; `made by diquuuu` branding.

## What you will not find

There is no registry **live** mode. The tool never writes, never locks, never modifies the system registry. It is a
forensic reader, not a registry editor.

## Building

Requirements: Visual Studio Build Tools with the C++ desktop workload, Windows SDK 10.0.26100.0 or newer.

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

The result is `build\registryexplorerpp.exe`. `build\` is disposable; everything else is source.

## Credits

- **Eric Zimmerman** — the cell format, record layouts and validation logic were cross-checked against his
  MIT-licensed [Registry](https://github.com/EricZimmerman/Registry) parser (C#). No code was taken from it; the format
  is the same because the format is the format.
- **Dear ImGui** (docking branch) by Omar Cornut, MIT.
- **Icons**: [Phosphor Icons](https://phosphoricons.com) (MIT), rasterized from SVG sources shipped in `data/icons/`.
- **Fonts**: loaded from the Windows font directory at runtime (Segoe UI, Consola). Not redistributed.

## License

MIT. See `LICENSE`.
