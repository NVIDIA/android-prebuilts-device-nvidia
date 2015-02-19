/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <linux/input.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include <fs_mgr.h>
#include "roots.h"
#include "common.h"
#include "device.h"
#include "screen_ui.h"
#include "make_ext4fs.h"
extern "C" {
#include "wipe.h"
}

static const char* HEADERS[] = { "Volume up/down to move highlight;",
                                 "enter button to select.",
                                 "",
                                 NULL };

static const char* ITEMS[] =  {"reboot system now",
                               "apply update from ADB",
                               "apply update from external SD card",
                               "wipe data/factory reset",
                               "wipe cache partition",
                               "reboot to bootloader",
                               "power down",
                               NULL };

static const char* USERCALIB_PATH = "/mnt/usercalib";

int erase_usercalibration_partition() {
    Volume *v = volume_for_path(USERCALIB_PATH);
    if (v == NULL) {
        // most devices won't have /mnt/usercalib, so this is not an error.
        return 0;
    }

    ensure_path_unmounted(USERCALIB_PATH);
    format_volume(USERCALIB_PATH);

    return 0;
}

class DefaultDevice : public Device {
  public:
    DefaultDevice() :
        ui(new ScreenRecoveryUI) {
    }

    RecoveryUI* GetUI() { return ui; }

    int HandleMenuKey(int key, int visible) {
        if (visible) {
            switch (key) {
              case KEY_DOWN:
              case KEY_VOLUMEDOWN:
                return kHighlightDown;

              case KEY_UP:
              case KEY_VOLUMEUP:
                return kHighlightUp;

              case KEY_ENTER:
              case KEY_POWER:
                return kInvokeItem;
            }
        }

        return kNoAction;
    }

    BuiltinAction InvokeMenuItem(int menu_position) {
        switch (menu_position) {
          case 0: return REBOOT;
          case 1: return APPLY_ADB_SIDELOAD;
          case 2: return APPLY_EXT;
          case 3: return WIPE_DATA;
          case 4: return WIPE_CACHE;
          case 5: return REBOOT_BOOTLOADER;
          case 6: return SHUTDOWN;
          default: return NO_ACTION;
        }
    }

    int WipeData() {
        erase_usercalibration_partition();
        return 0;
    }

    const char* const* GetMenuHeaders() { return HEADERS; }
    const char* const* GetMenuItems() { return ITEMS; }

  private:
    RecoveryUI* ui;
};

Device* make_device() {
    return new DefaultDevice();
}

