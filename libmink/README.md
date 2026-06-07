# libmink

## Overview
The libraries contained within the `libmink` directory are specifically designed and optimized for the **Android platform**. They provide essential Inter-Process Communication (IPC) mechanisms and utilities that are tightly coupled with Android's system architecture and security model.

## IPC Model
`libmink` implements **Mink IPC** — a capability-based, object-oriented remote invocation framework. Rather than raw message passing, the model exposes services as typed `Object` references. A client that connects to a service receives a **proxy object** whose method calls are transparently marshalled over the underlying transport and dispatched to the real service implementation in another process or execution environment. Reference counting (`retain`/`release`) governs object lifetime across process boundaries.

Two service-interface contracts are defined:

| Interface | Description |
|-----------|-------------|
| `IOpener` | Simple opener: the client supplies a service UID and receives a service object. No credential forwarding. |
| `IModule` | Module opener: extends `IOpener` with caller-credential forwarding. Required for any entity acting as a hub or cross-VM gateway. |

## Supported Transport Protocols
The transport layer (`MinkSocket` / `SockAgnostic`) abstracts over five socket back-ends, selected at connection time:

| Protocol | Constant | Typical use |
|----------|----------|-------------|
| UNIX domain socket | `UNIX` | Intra-VM, between Android user-space processes on the same virtual machine |
| Qualcomm IPC Router | `QIPCRTR` | Inter-VM between PVM (HLOS) and QTVM over the QRTR kernel bus |
| Virtual Socket | `VSOCK` | Inter-VM between QTVM and OEMVM (or any two VMs sharing a hypervisor VSOCK channel) |
| Qualcomm Message Queue | `QMSGQ` | Inter-VM via the QMSGQ kernel facility |
| Simulated | `SIMULATED` | Off-target / unit-test mode: QIPCRTR and VSOCK semantics emulated over UNIX sockets |

In addition, the `TZCom` component provides a separate, non-socket path into **QTEE** (Qualcomm Trusted Execution Environment) via the kernel `smcinvoke` driver (Secure Monitor Call). This path is used by `MinkHub` when a requested service UID resolves to a Trusted Application running inside the secure world.

## Communicating Entities
The library is designed to interconnect the following entities within a Qualcomm multi-VM / multi-EE Android system:

```
┌─────────────────────────────────────────────────────────────────┐
│  Android device                                                 │
│                                                                 │
│  ┌──────────────────┐   QIPCRTR   ┌──────────────────────────┐  │
│  │  PVM / HLOS      │◄───────────►│  QTVM (Trusted UI VM)    │  │
│  │  (Primary VM,    │             │                          │  │
│  │   Android OS)    │             │  ┌────────────────────┐  │  │
│  │                  │             │  │  MinkHub (QTVM)    │  │  │
│  │  ┌────────────┐  │             │  └────────────────────┘  │  │
│  │  │ MinkHub    │  │             └──────────┬───────────────┘  │
│  │  │ (PVM)      │  │                        │ VSOCK            │
│  │  └────────────┘  │             ┌──────────▼───────────────┐  │
│  │       │ UNIX     │             │  OEMVM (OEM VM)          │  │
│  │  ┌────▼───────┐  │             │                          │  │
│  │  │ App / Svc  │  │             │  ┌────────────────────┐  │  │
│  │  │ processes  │  │             │  │  MinkHub (OEMVM)   │  │  │
│  │  └────────────┘  │             │  └────────────────────┘  │  │
│  └──────────────────┘             └──────────────────────────┘  │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  QTEE — Qualcomm Trusted Execution Environment           │   │
│  │  (secure world; accessed via smcinvoke / TZCom, not      │   │
│  │   sockets; Trusted Applications identified by UID)       │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

| Entity | Role | Transport to reach it |
|--------|------|-----------------------|
| **PVM / HLOS** | Primary Virtual Machine running the main Android OS | — (local hub) |
| **QTVM** | Qualcomm Trusted VM, hosts Trusted UI and related secure services | QIPCRTR (from PVM), VSOCK (from OEMVM) |
| **OEMVM** | OEM-defined Virtual Machine for vendor-specific isolated workloads | VSOCK (from QTVM) |
| **QTEE** | Qualcomm Trusted Execution Environment (ARM TrustZone secure world) | `smcinvoke` kernel driver via `TZCom` |
| **Intra-VM processes** | Android user-space processes (apps, daemons, HALs) within the same VM | UNIX domain socket |

Each VM runs its own **MinkHub** daemon instance. The hub acts as a service broker: it routes `IModule_open()` requests to locally registered services (via UNIX socket) or forwards them to a remote hub on another VM (via QIPCRTR or VSOCK) or to QTEE (via `TZCom`). Caller identity is carried as a **credential object** (`ICredentials`) that is verified and wrapped at each hub boundary, enforcing the platform security policy.

## Key Components

| Directory | Library / Component | Purpose |
|-----------|---------------------|---------|
| `Transport/MinkSocket` | `libminksocket` | Core transport: socket abstraction (`SockAgnostic`), marshalling, object table, thread pool, per-protocol back-ends (UNIX, QRTR, VSOCK, QMSGQ) |
| `Transport/TZCom` | `libTZCom` | QTEE transport: wraps `smcinvoke` into the Mink `Object` model via `IClientEnv` |
| `Transport/FdWrapper` | `libfdwrapper` | Wraps a Linux file descriptor as a transferable Mink `Object` |
| `Hub/MinkHub` | `libminkhub` | Service broker library: local service registry, remote connection management, credential wrapping |
| `Hub/ExampleHub` | `exampleHub` binary | Reference hub daemon showing PVM / QTVM / OEMVM topology wiring |
| `Credentials` | `libosindcredentials` | OS-independent credential construction and wrapping |
| `SDK/QTVM` | `minkidl` + headers | IDL compiler and SDK headers for writing Trusted Applications and their HLOS client stubs |

## Build Restrictions
It is important to note that this codebase is **not intended** for compilation or usage on general open-source platforms (such as standard Linux distributions, macOS, or Windows). The project relies on Android-specific dependencies, headers, and build systems. Consequently, attempting to compile `libmink` in a non-Android environment is unsupported and will likely result in significant build failures. These components should only be utilized as an integral part of the Android build process.
