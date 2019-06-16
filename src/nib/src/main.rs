extern crate nib;

use nib::devices::Memballoon;
use nib::devices::MemballoonModel;

fn main() {
    let file_name = nib::util::build_file_name("guest", ".xml");
    println!("{}", file_name);

    let mut memballoon = Memballoon::new();

    println!("{}", memballoon.to_str());

    memballoon.set_model(MemballoonModel::VirtIO);
    println!("{}", memballoon.to_str());
}
