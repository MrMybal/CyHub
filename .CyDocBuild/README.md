# Build the CyHub manual

Use Node.js 22 or later and a writable, built checkout or local package installation of [CyDoc 0.1.0](https://github.com/MrMybal/CyDoc). Set `CYDOC_SOURCE_DIR` to that directory and run these commands from the CyHub root.

```powershell
node .CyDocBuild/build.mjs build
node .CyDocBuild/build.mjs check
```

Edit `manual/en` and `manual/fr` together. Preserve page IDs and explicit anchors. The reader writes to `../Documentation`; both commands write the export. The real app screenshot lives under `../assets/screenshots` and is also used by the README.
