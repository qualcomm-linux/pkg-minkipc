// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

use mink::object::Object;
use std::os::raw::c_void;

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct MinkIPC {
    _unused: [u8; 0],
}

// Option<Object> and Object are the same layout due to guaranteed NPO (null-pointer optimization)
#[allow(improper_ctypes)]
extern "C" {
    pub fn MinkIPC_connect(
        address: *const ::std::os::raw::c_char,
        proxyOut: *mut Object,
    ) -> *mut MinkIPC;
}

extern "C" {
    #[doc = "Decrement reference count.\nWhen the count goes to 0, *me* is deleted."]
    pub fn MinkIPC_release(me: *mut MinkIPC);
}

// CloseNotifier related constants
pub const EVENT_CLOSE: u32 = 0x00000020;
pub const EVENT_CRASH: u32 = EVENT_CLOSE - 1;
pub const EVENT_DELETE: u32 = EVENT_CLOSE - 2;
pub const EVENT_DETACH: u32 = EVENT_CLOSE - 3;
#[allow(dead_code)]
pub const EVENT_UNKNOWN: u32 = 0x0000F000;

// CloseNotifier callback function type
pub type CloseHandlerFunc = extern "C" fn(data: *mut c_void, event: i32);

// CloseNotifier opaque structure
#[allow(dead_code)]
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct CloseNotifier {
    _unused: [u8; 0],
}

#[allow(improper_ctypes)]
extern "C" {
    pub fn CloseNotifier_new(
        func: CloseHandlerFunc,
        data: *mut c_void,
        target: Object,
        obj_out: *mut Object,
    ) -> i32;
}
