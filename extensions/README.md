# Browser Extensions

This directory contains the SDS-02/SDS-03/SDS-08 browser integration artifacts.

- `chromium/` targets Chrome and Edge with Manifest V3.
- `firefox/` targets Firefox with Native Messaging support.

Both extensions send `addDownload` messages to the native host named
`com.remoodownload.native_host`.

## Native Messaging Notes

Native Messaging manifests are shipped under `resources/native-messaging/`.

For Chromium-family browsers, the `allowed_origins` value must match the final
extension ID assigned by Chrome or Edge. The repository uses
`aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa` as a development placeholder ID so the manifest
stays syntactically valid before packaging. Release packaging should replace it
with the store extension ID.

Firefox uses the stable extension ID declared in `extensions/firefox/manifest.json`:
`extension@remoodownload.com`.
