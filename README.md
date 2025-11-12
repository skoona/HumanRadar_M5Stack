# BSP: UniFi Protect Alarm Viewer

## Overview

<table>
<tr>
  <td width="50" valign="top">
    <img src="./spiffs/skoona-devel-icon.png">
  </td>
  <td valign="top">
Displays motion, people, and vehicle events triggered by UniFi Protect camera systems.  A Snapshot is display for the most recent events.  The home button retrieves the sna;shot for the "Front Door" camera.
  </td>
</tr>
</table>

## Build and Flash

To build and flash the example for a specific `{board}` and `{port}`, use the following command:

```
idf.py -D SDKCONFIG_DEFAULTS=sdkconfig.bsp.{board} -p {port} flash monitor
```
Make sure the correct board name is set in the `main/idf_component.yml` file under the `dependencies` section.

## Launch Example

You can also try this example using ESP Launchpad:

<a href="https://espressif.github.io/esp-launchpad/?flashConfigURL=https://espressif.github.io/esp-bsp/config.toml&app=display-">
    <img alt="Try it with ESP Launchpad" src="https://espressif.github.io/esp-launchpad/assets/try_with_launchpad.png" width="250" height="70">
</a>
