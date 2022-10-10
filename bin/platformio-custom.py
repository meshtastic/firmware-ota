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

if (platform.name == "espressif32"):
    env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_patch_bin)
