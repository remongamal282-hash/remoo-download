# Extension Icons TODO

The extension currently needs icon files for proper display in:
- Extension toolbar button
- Browser notifications
- Extension management page (chrome://extensions)

## Required Icon Sizes

Create PNG icons with the following dimensions:

- `icon16.png` - 16x16 pixels (toolbar, context menu)
- `icon32.png` - 32x32 pixels (extension management)
- `icon48.png` - 48x48 pixels (extension management, notifications)
- `icon128.png` - 128x128 pixels (Chrome Web Store, installation)

## Design Guidelines

- Use a simple, recognizable design related to downloading
- Maintain good contrast for both light and dark themes
- Consider using a downward arrow or similar download symbol
- Use Remoo Download brand colors if available
- Ensure icons are clear and legible at small sizes

## Where to Place Icons

Place all icon files in the `extensions/chromium/` directory alongside `manifest.json`.

## Update manifest.json

Once icons are created, update the `manifest.json` to reference them:

```json
{
  "icons": {
    "16": "icon16.png",
    "32": "icon32.png",
    "48": "icon48.png",
    "128": "icon128.png"
  },
  "action": {
    "default_icon": {
      "16": "icon16.png",
      "32": "icon32.png"
    }
  }
}
```

## Temporary Workaround

For testing without icons:
- The extension will use default browser icons
- Notifications will show without custom icons
- Functionality is not affected, only visual appearance

## Design Tools

Consider using:
- Figma, Sketch, or Adobe Illustrator for vector design
- Export to PNG at required sizes
- Use online tools like realfavicongenerator.net for multi-size generation
