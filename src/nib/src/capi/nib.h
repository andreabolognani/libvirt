int nib_util_build_file_name(char **fileName,
                             const char *base,
                             const char *ext);

typedef enum {
  NIB_DEVICES_MEMBALLOON_MODEL_NONE,
  NIB_DEVICES_MEMBALLOON_MODEL_VIRTIO,
  NIB_DEVICES_MEMBALLOON_MODEL_VIRTIO_NON_TRANSITIONAL,
  NIB_DEVICES_MEMBALLOON_MODEL_VIRTIO_TRANSITIONAL,
} NibDevicesMemballoonModel;

typedef struct {
  NibDevicesMemballoonModel model;
} NibDevicesMemballoon;

NibDevicesMemballoon *nib_devices_memballoon_new(void);
void nib_devices_memballoon_free(NibDevicesMemballoon *memballoon);

void nib_devices_memballoon_set_model(NibDevicesMemballoon *memballoon,
                                        NibDevicesMemballoonModel model);
NibDevicesMemballoonModel nib_devices_memballoon_get_model(const NibDevicesMemballoon *memballoon);

char *nib_devices_memballoon_to_str(const NibDevicesMemballoon *memballoon);
