# Mink Adaptor Library

The Mink Adaptor library implements a Mink-IPC interface, which in-turn allows clients to utilize the [Mink-IDL](https://github.com/quic/mink-idl-compiler) programming interface to communicate across security domain boundaries with QTEE via [QCOMTEE](https://github.com/quic/quic-teec).

## Mink Adaptor, QCOMTEE and QTEE interactions

1.  The Mink Adaptor library executes in the context of a client process and serves as a "User Invoke Gateway" running in user mode atop Linux.
    For outbound invocations, this gateway provides a function that proxies `Object_invoke` calls. Client code invokes QTEE objects by calling `Object_invoke` either directly or through an [IDL-generated stub](https://github.com/quic/mink-idl-compiler). When an Object structure identifies a QTEE object, its invoke member points to the outbound invoke function provided by the gateway. This function then makes a request of the kernel driver.
    For inbound invocations, the gateway provides code that waits for incoming invocations and dispatches the calls to local objects. This code also makes use of the kernel driver.
    The interaction between this gateway and the driver constitute the “Driver API”.
2.  The QCOMTEE Driver accepts requests from the client process and constructs an SMC command record that will be sent to QTEE.  The driver receives virtual addresses provided by the client process. When provided buffers are contiguous, it will make use of those buffers in-place. Otherwise, it will allocate from a contiguous memory pool for the duration of the invocation. Objects (both the target object and input object parameters) are identified using integer handles. These are values previously returned from SMC calls, or `0`, which identifies the **“primordial”** object exposed to both Linux and QTEE.
3.  The QTEE Invoke Router performs Object and Memory validation, translation of handles, and "locking" of resources to ensure that the translated values remain valid for the duration of the invocation. Particularly, the object handles are translated to `Object` structures that are to be called on a service thread within QTEE.

<p align="center">
<img src="../docs/images/objectinvoke.png">
</p>

### Mink-objects supported by Mink Adaptor

The Mink Adaptor library supports the following Mink objects for its user-space clients:

#### Root Environment Object

A Root Environment Object is a type of Remote Object that allows invocation to the **"primordial"** Object within QTEE. This is the first object which the user-space clients acquire from the Mink Adaptor Gateway to initiate communication with entities within QTEE.

#### Callback Objects

These objects are forwarded from the Linux environment to QTEE to allow domains within QTEE to hold remote references to them. Entities in the QTEE domain (e.g. Trusted Applications) can utilize these remote references to invoke object functionality implemented by entities in the Linux domain (e.g. Linux user-space Applications).

#### Memory Objects

A Memory Object represents contiguous page-aligned memory shared with QTEE. Clients can write into this memory and share the Memory Object with QTEE via `Object_invoke`.

## Tests

You can run the `smcinvoke_client` binary with the following commands:

- _QTEE Diagnostics_ `smcinvoke_client -d <iterations to run>`
- _Callback Objects_ `smcinvoke_client -c /path/to/tzecotestapp.mbn <iterations to run>`
- _Memory Objects_   `smcinvoke_client -m /path/to/tzecotestapp.mbn <iterations to run>`

