# Privacy Policy for Remoo Download Browser Extension

**Last Updated:** January 2025  
**Version:** 0.5.0

## Overview

Remoo Download Integration is a browser extension that works with the Remoo Download desktop application to provide faster, more reliable downloads. This privacy policy explains what data the extension accesses, how it's used, and how your privacy is protected.

## What Data the Extension Accesses

The Remoo Download extension requires access to the following data to function:

### 1. Download Information
- **What:** URLs of files you choose to download, file names, file sizes, and referrer URLs
- **Why:** To send download requests to your local Remoo Download application
- **Where:** This data is sent only to the Remoo Download native host running on your computer

### 2. Browser Storage
- **What:** Your extension settings (enabled/disabled status, file size threshold, file extension filters)
- **Why:** To remember your preferences between browser sessions
- **Where:** Stored locally in your browser using Chrome's sync storage API

### 3. Context Menu Interactions
- **What:** Links, videos, or audio files you explicitly choose to download via the context menu
- **Why:** To provide the "Download with Remoo Download" option when you right-click
- **Where:** Only processed when you actively select the menu option

## What Data We Do NOT Collect

- **No browsing history:** We do not track or record which websites you visit
- **No personal information:** We do not collect names, email addresses, or any identifying information
- **No analytics or telemetry:** We do not send any data to external servers for tracking or analysis
- **No third-party sharing:** We do not share, sell, or transmit any data to third parties

## How Data is Used

All data accessed by the extension is used exclusively for these purposes:

1. **Local Communication Only:** Download information is sent only to the Remoo Download native host application running on your own computer via the Chrome Native Messaging API

2. **User Preferences:** Settings are stored locally in your browser to maintain your chosen configuration

3. **Download Interception:** The extension monitors download events only to determine if a download should be handled by Remoo Download based on your configured filters (file size, file extensions)

## Data Storage and Transmission

- **Local Only:** All communication happens between your browser and your local Remoo Download application on the same computer
- **No Cloud Storage:** No data is uploaded to any cloud service or remote server
- **No External Networks:** The extension does not make any network requests to external servers

## Permissions Explained

The extension requests the following permissions:

- **downloads:** To detect when downloads start and to cancel browser downloads that are being handled by Remoo Download
- **contextMenus:** To add the "Download with Remoo Download" option to right-click menus
- **nativeMessaging:** To communicate with the Remoo Download application on your computer
- **storage:** To save your extension preferences locally in the browser
- **notifications:** To show status messages (e.g., "Download started", "Service not running")
- **host_permissions (`<all_urls>`):** Required to intercept downloads from any website you visit (but only processes downloads, not browsing)

## Children's Privacy

Remoo Download is not directed at children under the age of 13, and we do not knowingly collect any data from children.

## Open Source

Remoo Download is open-source software. You can review the complete source code to verify our privacy practices at:
https://github.com/remoo-download/remoo-download

## Changes to This Policy

If we make material changes to this privacy policy, we will update the "Last Updated" date and version number at the top of this document. Continued use of the extension after changes constitutes acceptance of the updated policy.

## Contact

If you have questions or concerns about this privacy policy or our data practices, please open an issue on our GitHub repository:
https://github.com/remoo-download/remoo-download/issues

## Your Rights

You have the right to:

- **Disable the extension** at any time via `chrome://extensions`
- **Review stored data** via Chrome DevTools (F12 → Application → Storage)
- **Clear stored data** by removing and reinstalling the extension
- **Review the source code** to understand exactly how your data is handled

## Compliance

This extension complies with:
- Chrome Web Store Developer Program Policies
- General Data Protection Regulation (GDPR) principles
- California Consumer Privacy Act (CCPA) principles

## Summary

**In plain language:** Remoo Download only sees download links when you start a download or use the context menu. It sends these links to your local Remoo Download application on your own computer. It does not track your browsing, collect personal information, or send any data to external servers. Everything stays on your computer.
