#include "FSCommon.h"

void listDir(const char * dirname, uint8_t levels)
{
#ifdef FSCom
    char buffer[255];
    File root = FSCom.open(dirname, FILE_O_READ);
    if(!root){
        return;
    }
    if(!root.isDirectory()){
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory() && !String(file.name()).endsWith(".")) {
            if(levels){
                listDir(file.path(), levels -1);
                file.close();
            }
        } else {
            Serial.printf(" %s (%i Bytes)\n", file.path(), file.size());
            file.close();
        }
        file = root.openNextFile();
    }
    root.close();
#endif
}

void fsInit()
{
#ifdef FSCom
    if (!FSBegin())
    {
        Serial.print("ERROR filesystem mount Failed. Formatting...\n");
        assert(0); // FIXME - report failure to phone
    }
    Serial.printf("Filesystem files (%d/%d Bytes):\n", FSCom.usedBytes(), FSCom.totalBytes());
    listDir("/", 10);
#endif
}
