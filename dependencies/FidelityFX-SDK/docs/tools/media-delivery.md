<!-- @page page_tools_media_delivery FidelityFX Media Delivery -->

<h1>FidelityFX Media Delivery System</h1>

The FidelityFX Media Delivery System is a tool distributed as part of the FidelityFX SDK package and is used to download, verify and extract the media bundles used to run the SDK samples.

<h2>Using the Media Delivery System</h2>

The current version of the Media Delivery System is kept in the /sdk/tools/media_delivery folder.

To use it, drive it from the `UpdateMedia.bat` or `ClearMediaCache.bat` files in the SDK root folder. Those batch files will get a known media bundle or clear the download cache respectively.

The following arguments and options will allow you to use the system outside of those two ways:

<h3>Command line syntax:</h3>

"MediaDelivery.exe \[Options\]"

**Options**

| Option             | Description                                                                                                  |
|--------------------|--------------------------------------------------------------------------------------------------------------|
| **-help**          | Show the complete usage of the tool.                                                                         |
| **-clear-cache**   | Delete all cached media bundles.                                                                             |
| **-debug**         | Enabled debug mode (only available in internal builds).                                                      |
| **-download-only** | Download a media bundle without extracting it.                                                               |
| **-force**         | Skip confirmation when the media directory will be overwritten and always extract the bundle.                |
| **-host**          | The hostname to connect to (only available in internal builds, defaults to `ffx-sdk-assets.amdgameeng.com`). |
| **-root-dir**      | Set the root directory of the FidelityFX SDK (defaults to the current working directory).                    |
| **-target-sha256** | The SHA256 hash of the media bundle you want to download.                                                    |
| **-latest**        | Downloads the most recent stable media bundle.                                                               |
| **-unstable**      | Downloads the most recent unstable media bundle.                                                             |
