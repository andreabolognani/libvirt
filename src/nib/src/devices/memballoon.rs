#[repr(C)]
#[derive(Copy, Clone)]
pub enum MemballoonModel {
    None,
    VirtIO,
    VirtIONonTransitional,
    VirtIOTransitional,
}

#[repr(C)]
pub struct Memballoon {
    model: MemballoonModel,
}

impl Memballoon {

    pub fn new() -> Self {
        Self {
            model: MemballoonModel::VirtIO,
        }
    }

    pub fn set_model(&mut self, model: MemballoonModel) {
        self.model = model;
    }

    pub fn get_model(&self) -> MemballoonModel {
        self.model
    }

    pub fn to_str(&self) -> String {
        let mut ret = String::from("");

        match self.model {
            MemballoonModel::VirtIO => {
                ret.push_str("virtio-memballoon");
            },
            MemballoonModel::VirtIONonTransitional => {
                ret.push_str("virtio-memballoon-non-transitional");
            },
            MemballoonModel::VirtIOTransitional => {
                ret.push_str("virtio-memballoon-transitional");
            },
            MemballoonModel::None => {},
        }

        ret
    }
}
