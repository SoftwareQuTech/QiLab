# QILab systemd service setup (Raspberry Pi 5)

This guide installs QILab as a systemd service using the release layout already used by the updater script:

- active binary symlink: `/opt/qilab/current`
- releases directory: `/opt/qilab/releases/<version>`
- external config: `/etc/qilab/nodes.yaml`
- external profiler output: `/var/log/qilab/profiler`

## 1) Create service user and directories

```bash
sudo mkdir -p /opt/qilab/releases
sudo mkdir -p /etc/qilab
sudo mkdir -p /var/log/qilab/profiler
sudo chown -R root:root /opt/qilab /etc/qilab /var/log/qilab
```

Service runs as `root` (required for priority elevation and CPU-core affinity operations).

## 2) Install service file

Service template is provided at [deploy/systemd/qilab.service](../deploy/systemd/qilab.service).

```bash
sudo cp deploy/systemd/qilab.service /etc/systemd/system/qilab.service
sudo systemctl daemon-reload
```

## 3) Install or update a release

Use the updater script:

```bash
sudo ./scripts/update_release.sh 0.0.3 /tmp/qilab-0.0.3-linux-arm64.tar.gz
```

This script extracts release files to `/opt/qilab/releases/<version>`, updates `/opt/qilab/current`, validates ARM64 binary architecture, and restarts the service.

## 4) Enable and start service

```bash
sudo systemctl enable qilab
sudo systemctl start qilab
```

## 5) Verify service

```bash
sudo systemctl status qilab --no-pager
journalctl -u qilab -n 100 --no-pager
```

Service output is also redirected to:

- `/var/log/qilab/qilab.log`

Quick check:

```bash
sudo tail -n 100 /var/log/qilab/qilab.log
```

Expected logs should include startup version line, and runtime should use:

- `QILAB_CONFIG_DIR=/etc/qilab`
- `QILAB_PROFILER_DIR=/var/log/qilab/profiler`

## 6) Stop / restart

```bash
sudo systemctl stop qilab
sudo systemctl restart qilab
```

Graceful stop behavior:

- Service uses `KillSignal=SIGINT` (not default SIGTERM)
- App handles both `SIGINT` and `SIGTERM`
- `TimeoutStopSec=20` allows cleanup (`on_abort`, thread joins, profiler save) before force kill

## 7) Optional: override service settings safely

```bash
sudo systemctl edit qilab
```

Example override:

```ini
[Service]
Environment=QILAB_CONFIG_DIR=/etc/qilab
Environment=QILAB_PROFILER_DIR=/var/log/qilab/profiler
```

Then reload + restart:

```bash
sudo systemctl daemon-reload
sudo systemctl restart qilab
```

## 8) Redirect output to file (if service already installed)

If you changed the service file and want file-based logs, ensure these lines exist in
`/etc/systemd/system/qilab.service` under `[Service]`:

```ini
StandardOutput=append:/var/log/qilab/qilab.log
StandardError=append:/var/log/qilab/qilab.log
```

Then apply:

```bash
sudo mkdir -p /var/log/qilab
sudo chown -R root:root /var/log/qilab
sudo systemctl daemon-reload
sudo systemctl restart qilab
```
