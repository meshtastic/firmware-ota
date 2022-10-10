import os
import hashlib
import struct
import sys
from os.path import join
from readprops import readProps
from datetime import datetime

Import("env")
platform = env.PioPlatform()

def esp32_patch_bin(source, target, env):
    prefsLoc = env.subst("$PROJECT_DIR/version.properties")
    verObj = readProps(prefsLoc)
    now = datetime.now()
    firmware_name = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    with open(firmware_name, 'r+b') as fh:
        # write version string to 0x30
        fh.seek(48)
        fh.write(str.encode(verObj['long'].ljust(32, '\0')))
        # write name string to 0x50
        fh.seek(80)
        fh.write(str.encode(verObj['name'].ljust(32, '\0')))
        # write time string to 0x70
        fh.seek(112)
        fh.write(str.encode(now.strftime("%H:%M:%S").ljust(16, '\0')))
        # write date string to 0x80
        fh.seek(128)
        fh.write(str.encode(now.strftime("%b %d %Y").ljust(16, '\0')))
    fh.close()
    image = LoadFirmwareImage("esp32", firmware_name)
    calc_checksum = image.calculate_checksum()
    with open(firmware_name, 'r+b') as fh:
        fh.seek(-48, os.SEEK_END)
        align = 15 - (fh.tell() % 16)
        fh.seek(align,1)
        fh.write(struct.pack(b"B", calc_checksum))
        image_length = fh.tell()
        fh.seek(0)
        digest = hashlib.sha256()
        digest.update(fh.read(image_length))
        fh.write(digest.digest())
    fh.close()

if (platform.name == "espressif32"):
    sys.path.append(join(platform.get_package_dir("tool-esptoolpy")))
    from esptool.bin_image import LoadFirmwareImage
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_patch_bin)
