extern crate libtool;

fn main() {
    libtool::generate_convenience_lib("libnib").unwrap();
}
