# sysfs_fw_loader

Small tool to load firmwares using the Linux sysfs interface.

It's useful when you don't want to use initramfs and kernel modules, but your drivers need to load firmwares.

You need Linux 5.19+ and `CONFIG_FW_LOADER_USER_HELPER` enabled.

## Usage

Create directory at `/etc/sysfs_fw_loader/` and put json files inside:

```json
[
        {
                "name": "brcmfmac43430-sdio.sudomaker,cle.bin",
                "file": "/lib/firmware/brcm/brcmfmac43430-sdio.bin"
        },
        {
                "name": "brcmfmac43430-sdio.sudomaker,cle.txt",
                "file": "/lib/firmware/brcm/brcmfmac43430-sdio.txt"
        },
        {
                "name": "BCM43430A1.sudomaker,cle.hcd",
                "file": "/lib/firmware/brcm/BCM43430A1.hcd"
        },
        {
                "name": "brcmfmac43430-sdio.sudomaker,cle.clm_blob",
                "file": ""
        },
        {
                "name": "brcmfmac43430-sdio.clm_blob",
                "file": ""
        }
]
```

The `name` field is the directory names in `/sys/class/firmware/`. It's partially matched.

Use an empty `file` field to indicate you want to instruct the kernel to skip loading this firmware.

You can use the environment variable `SYSFS_FW_LOADER_CONFIG_DIR` to specify an alternative config directory.

This program will exit when all specified firmware files are loaded.

## Build
### Requirements
- CMake
- C++17 compatible compiler with std::filesystem

```shell
mkdir build
cd build
cmake ..
make
```

Or if you're in a hurry:
```shell
g++ -std=c++17 main.cpp -o sysfs_fw_loader
```

## Credits
This program uses the following open source components:
- nlohmann/json

## License
AGPLv3