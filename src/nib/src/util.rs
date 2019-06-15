pub fn build_file_name(base: &str, ext: &str) -> String {
    let mut ret = String::from(base);
    ret.push_str(ext);
    ret
}
