extern crate nib;

fn main() {
    let file_name = nib::util::build_file_name("guest", ".xml");
    println!("{}", file_name);
}
