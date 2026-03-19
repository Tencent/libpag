# Privacy Policy - PAGX Viewer

**Last updated: March 19, 2026**

## Data Collection

PAGX Viewer does **not** collect, transmit, or store any personal data. No analytics, telemetry, or tracking of any kind is used.

## File Handling

PAGX files you open or drag into the viewer are processed **entirely locally** within your browser. File contents are never sent to any external server.

## Local Storage

The extension uses browser IndexedDB to cache its own runtime resources (WASM module and font files) for faster loading. No user files or personal information are stored. You can clear this cache at any time through Chrome's "Clear browsing data" settings.

## Permissions

- **File URL access**: Used to open local `.pagx` files. Requires manual opt-in by the user.
- **Active Tab / Scripting**: Used to detect and redirect `.pagx` file navigation to the built-in viewer.
- **Web Navigation**: Used to intercept `.pagx` file downloads and open them in the viewer.

## Network Access

The extension makes **no network requests**. All resources (WASM engine, fonts, viewer UI) are bundled within the extension package.

## Contact

For questions or concerns, please open an issue at: https://github.com/libpag/libpag/issues
