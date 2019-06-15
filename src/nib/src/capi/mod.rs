use std::ffi::CStr;
use std::ffi::CString;
use std::mem;
use std::os::raw::c_char;

use super::util;

#[no_mangle]
pub extern "C"
fn nib_util_build_file_name(file_name: *mut *mut c_char,
                            base: *const c_char,
                            ext: *const c_char) -> i32
{
    if base.is_null() || ext.is_null() || file_name.is_null() {
        return -1;
    }
    let base = unsafe { CStr::from_ptr(base) }.to_str().unwrap();
    let ext = unsafe { CStr::from_ptr(ext) }.to_str().unwrap();

    let res = util::build_file_name(base, ext);

    unsafe { *file_name = CString::new(res).unwrap().into_raw() }
    return 0;
}
