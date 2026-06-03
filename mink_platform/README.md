# mink_platform

## Overview

`mink_platform` contains the **HLOS Mink Daemon** (`hlosminkdaemon`) — an Android system daemon
that serves as the **Android-side bridge** in Qualcomm's Mink IPC framework. It runs on the Android
(HLOS) partition and connects Android userspace clients to Qualcomm's secure and trusted execution
environments.

In Qualcomm's multi-domain architecture, several isolated execution environments coexist on the
same SoC: the Android HLOS, QTEE (Qualcomm Trusted Execution Environment / TrustZone), QTVM
(Qualcomm Trusted VM), OEM VM, and the Modem. The daemon's job is to act as the central routing
hub that lets all of these domains discover and invoke Mink services in each other.

## Architecture

```
Android Userspace Clients
        |
        | Unix socket (/dev/socket/hlos_mink_opener)
        v
  ┌─────────────────────────────────────────────────────┐
  │               HLOSMinkDaemon                        │
  │                                                     │
  │  TEnv (IModule)  ──►  MinkHub  ──►  QTEE (TrustZone)│
  │                               ──►  QTVM  (QRTR:5008)│
  │                               ──►  OEM VM(QRTR:5010)│
  │                                                     │
  │  ssgtzd socket   ──►  CRegister (ITzdRegister)      │
  │                        (QTEE client registration)   │
  │                                                     │
  │  QRTR:5013       ──►  MinkModemOpener               │
  │                        (Modem ↔ QTEE routing)       │
  │                                                     │
  │  QRTR:5033       ──►  TEnv (REMOTE)                 │
  │                        (QTVM → HLOS services)       │
  │                                                     │
  │  taAutoLoad      ──►  TA image serving to QTEE      │
  │                                                     │
  │  Vendor clients  ──►  QWES, SecureChannel, etc.     │
  └─────────────────────────────────────────────────────┘
```

## Key Components and Functionality

### 1. Central IPC Hub (`MinkHub`)

At startup the daemon creates a `MinkHub` instance that acts as a routing table connecting all
remote domains. It maintains persistent QRTR connections to:

| Domain   | Transport | Port / Socket |
|----------|-----------|---------------|
| QTEE     | QTEE IPC  | (kernel driver) |
| QTVM     | QRTR      | 5008 |
| OEM VM   | QRTR      | 5010 |
| Modem    | QRTR      | 5013 |

When a client requests a Mink service by UID, the hub decides whether to satisfy the request
locally (from an HLOS-side service) or forward it to one of the remote domains.

### 2. TEnv — Trusted Environment / Service Opener

`TEnv` is the `IModule` object that Android clients receive when they connect to the daemon's Unix
socket (`/dev/socket/hlos_mink_opener`). It is the main entry point for all service discovery:

- Authenticates the caller using its Linux PID/UID.
- Creates a per-client `MinkHubSession` and credential object.
- Routes `open(uid)` calls to either a local HLOS service or a remote domain via the hub.
- Enforces access-control: non-vendor clients are restricted to a configured UID allowlist.

A second `TEnv` instance (in `REMOTE` mode) is published on QRTR port 5033 so that QTVM can
connect back and open HLOS-side services.

### 3. QTEE Client Registration (`CRegister` / `ssgtzd` socket)

The daemon also listens on `/dev/socket/ssgtzd` and exposes the `ITzdRegister` interface. This
allows QTEE-side Trusted Applications to register themselves as HLOS clients:

- Validates raw credential buffers supplied by QTEE.
- Creates a `ClientEnv` (a `Custom` TEnv variant) with per-client resource quotas (max live
  objects, max concurrent clients).
- Caches environments in `CredentialsManager` so reconnecting clients reuse their session.
- Enforces a configurable system-wide limit on simultaneous QTEE clients.

### 4. TA Auto-Loading (`taAutoLoad`)

The optional `libtaautoload.so` module enables QTEE to pull Trusted Application (TA) images from
the Android filesystem on demand:

- `CRequestTABuffer` — a callback object that reads a TA ELF binary from the filesystem and
  returns it as a DMA-mapped buffer.
- `CRegisterTABufCBO` — registers the above callback with QTEE so QTEE can invoke it whenever it
  needs to load a TA that is not already resident.

This removes the need to embed TA images in the firmware; they can live in `/vendor` and be loaded
dynamically.

### 5. Modem Mink Service (`MinkModemOpener`)

Published on QRTR port 5013, this service allows the Modem's own Mink hub to establish a
connection to HLOS:

- The modem calls `register()` to hand its hub object to HLOS.
- HLOS stores it in an `ObjectTableMT` and exposes a `CTunnelInvokeMgr` back to the modem.
- This enables end-to-end Mink IPC between the Modem and QTEE, routed through HLOS.
- On Modem SSR (Sub-System Restart) the old registration is forcibly cleared before accepting a
  new one.

### 6. Vendor Client Initialization

`loadHLOSMinkdVendorClients()` initializes a set of vendor-specific HLOS services that depend on
the Mink IPC channel being up:

| Client Name     | Purpose |
|-----------------|---------|
| `QWES_COMMON`   | Qualcomm Wireless Edge Services — common initialization |
| `SECURE_CHANNEL`| Establishes a secure channel between HLOS and QTEE |
| `QWES`          | Qualcomm Wireless Edge Services — main service |
| `QWES_COMMON` (setflag) | QWES feature-flag service |

It also loads any additional TAs listed in `/vendor/etc/ssg/ta_config.json` (both file-based and
embedded TA images).

## Build Restrictions

This codebase does **not support** compilation on general open-source platforms (standard Linux
distributions, macOS, or Windows). It relies on Android-specific dependencies, headers, and build
systems (AOSP `Android.mk`). These services must be built and run as part of the Android platform.
