// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear

//! CloseNotifier Rust bindings
//!
//! This module provides a safe Rust wrapper around the CloseNotifier C library.
//! CloseNotifier allows registering callbacks that are invoked when a connection
//! or object is closed, crashed, deleted, or detached.

use std::ffi::c_void;
use mink::object::Object;
use crate::ffi;

/// Events that can be reported by CloseNotifier
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CloseEvent {
    /// Normal close event
    Close,
    /// Connection crashed
    Crash,
    /// Object was deleted
    Delete,
    /// Connection was detached
    Detach,
    /// Unknown event with raw event code
    Unknown(u32),
}

impl From<i32> for CloseEvent {
    fn from(event: i32) -> Self {
        match event as u32 {
            ffi::EVENT_CLOSE => CloseEvent::Close,
            ffi::EVENT_CRASH => CloseEvent::Crash,
            ffi::EVENT_DELETE => CloseEvent::Delete,
            ffi::EVENT_DETACH => CloseEvent::Detach,
            other => CloseEvent::Unknown(other),
        }
    }
}

/// Trait for handling close events
pub trait CloseHandler: Send + Sync {
    /// Called when a close event occurs
    fn on_close(&self, event: CloseEvent);
}

/// Internal wrapper for the callback data
struct CallbackWrapper<T: CloseHandler> {
    handler: T,
}

/// C callback function that bridges to Rust
extern "C" fn callback_wrapper<T: CloseHandler>(data: *mut c_void, event: i32) {
    if !data.is_null() {
        let wrapper = unsafe { &*(data as *const CallbackWrapper<T>) };
        wrapper.handler.on_close(CloseEvent::from(event));
    }
}

/// Safe Rust wrapper for CloseNotifier
pub struct CloseNotifier<T: CloseHandler> {
    /// The underlying Object returned by CloseNotifier_new
    notifier: Object,
    /// Boxed callback data to ensure it stays alive
    _callback_data: Box<CallbackWrapper<T>>,
}

impl<T: CloseHandler + 'static> CloseNotifier<T> {
    /// Create a new CloseNotifier
    ///
    /// # Arguments
    /// * `handler` - The handler that will receive close events
    /// * `target` - The target object to monitor for close events
    ///
    /// # Returns
    /// * `Ok(CloseNotifier)` - Successfully created notifier
    /// * `Err(&'static str)` - Error message if creation failed
    pub fn new(handler: T, target: Object) -> Result<Self, &'static str> {
        let callback_data = Box::new(CallbackWrapper { handler });
        let callback_ptr = Box::into_raw(callback_data) as *mut c_void;

        let mut notifier_out = std::mem::MaybeUninit::uninit();

        let result = unsafe {
            ffi::CloseNotifier_new(
                callback_wrapper::<T>,
                callback_ptr,
                target,
                notifier_out.as_mut_ptr(),
            )
        };

        if result != 0 {
            // Clean up memory on failure
            unsafe { let _ = Box::from_raw(callback_ptr as *mut CallbackWrapper<T>); };
            return Err("CloseNotifier_new failed");
        }

        let callback_data = unsafe { Box::from_raw(callback_ptr as *mut CallbackWrapper<T>) };
        let notifier = unsafe { notifier_out.assume_init() };

        Ok(CloseNotifier {
            notifier,
            _callback_data: callback_data,
        })
    }

    /// Get a reference to the underlying Object
    pub fn object(&self) -> &Object {
        &self.notifier
    }
}

impl<T: CloseHandler> Drop for CloseNotifier<T> {
    fn drop(&mut self) {
        // The Object will automatically handle reference counting
        // The callback_data will be automatically cleaned up when the Box is dropped
    }
}

// Implement Send and Sync for CloseNotifier if T is Send + Sync
unsafe impl<T: CloseHandler + Send> Send for CloseNotifier<T> {}
unsafe impl<T: CloseHandler + Sync> Sync for CloseNotifier<T> {}

#[cfg(test)]
mod tests {
    use super::*;

    struct TestHandler {
        name: String,
    }

    impl CloseHandler for TestHandler {
        fn on_close(&self, event: CloseEvent) {
            println!("Handler '{}' received event: {:?}", self.name, event);
        }
    }

    #[test]
    fn test_close_event_conversion() {
        assert_eq!(CloseEvent::from(ffi::EVENT_CLOSE as i32), CloseEvent::Close);
        assert_eq!(CloseEvent::from(ffi::EVENT_CRASH as i32), CloseEvent::Crash);
        assert_eq!(CloseEvent::from(ffi::EVENT_DELETE as i32), CloseEvent::Delete);
        assert_eq!(CloseEvent::from(ffi::EVENT_DETACH as i32), CloseEvent::Detach);
        assert_eq!(CloseEvent::from(0x1234), CloseEvent::Unknown(0x1234));
    }
}
