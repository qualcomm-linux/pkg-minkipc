// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

//! Define MinkIPC bindings
//!
//! Since MinkIPC library is not written in Rust, we need bindings to make FFI
//! calls to it. The [`MinkIPC`] wraps around the raw pointer which is returned
//! by the library. The lifetime of the returned object determines the lifetime
//! of the connection.

use mink::object::Object;
use std::ffi::CString;
mod ffi;
pub mod close_notifier;

pub use close_notifier::{CloseNotifier, CloseHandler, CloseEvent};

pub struct MinkIPC {
    // This pointer stays hidden from user
    conn: *mut ffi::MinkIPC,
}

impl MinkIPC {
    pub fn connect(addr: &str) -> Result<(Self, Object), &'static str> {
        // Convert str into C-style string
        let addr_cstr = match CString::new(addr) {
            Ok(s) => s,
            Err(_e) => return Err("Invalid str. \0 bytes detected."),
        };
        let addr_ptr = addr_cstr.as_ptr();
        // Allocate proxy object that will be populated when connect() is successful
        let mut proxy_out = std::mem::MaybeUninit::uninit();
        // Call into C library
        let conn = unsafe { ffi::MinkIPC_connect(addr_ptr, proxy_out.as_mut_ptr()) };
        if conn.is_null() {
            return Err("MinkIPC_connect failed");
        }
        // When successful, the ffi::MinkIPC connection and Object are valid
        Ok((MinkIPC { conn }, unsafe { proxy_out.assume_init() }))
    }
}

// As soon as the MinkIPC struct is dropped, either by going out of scope or by
// being explicitly `drop()`ed, the underlying ffi::MinkIPC instance will be
// released. Since there can only ever be a refcount of 1 through this Rust
// interface, the instance will be cleaned up. Any Mink objects acquired
// through this interface will become invalid.
impl Drop for MinkIPC {
    fn drop(&mut self) {
        unsafe { ffi::MinkIPC_release(self.conn) }
    }
}
