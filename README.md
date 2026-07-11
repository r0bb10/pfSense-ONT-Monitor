# pfSense ONT Monitor

pfSense CE 2.8.x package that adds a dashboard widget and status page for
ZTE F6005 ONTs. It reads device, GPON optics, and Ethernet link information
from the ONT WebUI.

## Features

- Authenticated ZTE WebUI fetch using its native SHA-256 form login.
- Dashboard widget refreshed at a configurable interval.
- Status page with device, GPON, and Ethernet values.
- Settings stored in pfSense package configuration, not in a web-served file.
- Defaults for a stock F6005: `192.168.1.1` and `admin/admin`.

## Install

```sh
/usr/local/sbin/pkg-static add -f /tmp/pfSense-pkg-ont-monitor-*.pkg
```

## Configuration

Configuration option are in **Services > ONT Monitor**, then add **ONT Monitor** from
the Dashboard widget selector.