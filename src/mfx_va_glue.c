/*
 * Copyright 2013 Luca Barbato. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * - Neither the name of Intel Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mfx/mfxvideo.h>
#include "mfx_dispatcher_log.h"

#ifdef HAVE_LIBVA_DRM
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <va/va_drm.h>
#include <va/va_drmcommon.h>
#include <va/va_backend.h>

#define MFX_DISPLAY_CONTROLLER_CLASS 0x03
#define MFX_VENDORID_INTEL 0x8086
#define MFX_DRM_DEV_CARD   "card"
#define MFX_DRM_DEV_RENDER "renderD"
#define MFX_DRI_DIR        "/dev/dri/"
#define MFX_PCI_DIR        "/sys/bus/pci/devices"

struct mfx_disp_adapters
{
    mfxU32 vendor_id;
    mfxU32 device_id;
    char   card[32]; // sizeof(MFX_DRI_DIR) = 9, sizeof(MFX_CARD) = 4, sizeof(CARD_NUMBER_LEN) = 3
    char   render[32]; //sizeof(MFX_DRI_DIR) = 9, sizeof(MFX_RENDER) = 7, sizeof(CARD_NUMBER_LEN) = 3
};

static int dir_filter(const struct dirent* dir_ent)
{
    if (!dir_ent)
        return 0;
    if (!strcmp(dir_ent->d_name, "."))
        return 0;
    if (!strcmp(dir_ent->d_name, ".."))
        return 0;
    return 1;
}

typedef int (*fsort)(const struct dirent**, const struct dirent**);

static int mfx_search_disp_adapter_from_pci(struct mfx_disp_adapters** p_adapters, int* adapters_num)
{
    int i                              = 0;
    int j                              = 0;
    int entries_num                    = 0;
    int drm_entries_num                = 0;
    struct mfx_disp_adapters* adapters = NULL;
    struct dirent** dir_entries        = NULL;
    struct dirent** drm_dir_entries    = NULL;
    char card[16]                      = {0};
    char render[16]                    = {0};
    char file_name[300]                = {0};
    // sizeof(MFX_PCI_DIR) = 20, sizeof(pci_bus_id) = 12(?), sizeof(dirent::d_name) <= 256, sizeof(vendor|class|drm) = 6
    char str[16]                       = {0};
    FILE* file                         = NULL;

    entries_num = scandir(MFX_PCI_DIR, &dir_entries, dir_filter, (fsort)alphasort);

    for (i = 0; i < entries_num; i++) {
        long int class_id = 0, vendor_id = 0, device_id = 0;

        if (!dir_entries[i])
            continue;
        // class
        snprintf(file_name, sizeof(file_name), "%s/%s/%s", MFX_PCI_DIR,
                 dir_entries[i]->d_name, "class");
        file = fopen(file_name, "r");
        if (file) {
            if (fgets(str, sizeof(str), file)) {
                class_id = strtol(str, NULL, 16);
            }
            fclose(file);
            if (MFX_DISPLAY_CONTROLLER_CLASS == (class_id >> 16)) {
                 // vendor
                snprintf(file_name, sizeof(file_name), "%s/%s/%s", MFX_PCI_DIR,
                         dir_entries[i]->d_name, "vendor");
                file = fopen(file_name, "r");
                if (file) {
                    if (fgets(str, sizeof(str), file)) {
                        vendor_id = strtol(str, NULL, 16);
                    }
                    fclose(file);
                }
                // device
                snprintf(file_name, sizeof(file_name), "%s/%s/%s", MFX_PCI_DIR,
                         dir_entries[i]->d_name, "device");
                file = fopen(file_name, "r");
                if (file) {
                    if (fgets(str, sizeof(str), file)) {
                        device_id = strtol(str, NULL, 16);
                    }
                    fclose(file);
                }
                // check DRM info
                card[0]   = '\0';
                render[0] = '\0';
                snprintf(file_name, sizeof(file_name), "%s/%s/%s", MFX_PCI_DIR,
                         dir_entries[i]->d_name, "drm");
                drm_entries_num = scandir(file_name, &drm_dir_entries, dir_filter,
                                          (fsort)alphasort);
                for (j=0; j < drm_entries_num; j++)
                {
                    if (!drm_dir_entries[j])
                        continue;
                    if (!strncmp(MFX_DRM_DEV_CARD, drm_dir_entries[j]->d_name,
                                 strlen(MFX_DRM_DEV_CARD))) {
                        // card Number
                        if (card[0] != '\0') {
                            free(drm_dir_entries[j]);
                            continue;
                        }
                        snprintf(card, sizeof(card), "%s", drm_dir_entries[j]->d_name);
                    } else if (!strncmp(MFX_DRM_DEV_RENDER, drm_dir_entries[j]->d_name,
                                strlen(MFX_DRM_DEV_RENDER))) {
                        // render Number
                        if (render[0] != '\0') {
                            free(drm_dir_entries[j]);
                            continue;
                        }
                        snprintf(render, sizeof(render), "%s", drm_dir_entries[j]->d_name);
                    }
                    free(drm_dir_entries[j]);
                }
                // adding valid adapter to the list
                if (vendor_id && device_id) {
                    struct mfx_disp_adapters* tmp_adapters = NULL;
                    tmp_adapters = (struct mfx_disp_adapters*)realloc(adapters,
                                                               (*adapters_num+1)*sizeof(struct mfx_disp_adapters));

                    if (tmp_adapters) {
                        adapters = tmp_adapters;
                        adapters[*adapters_num].vendor_id = vendor_id;
                        adapters[*adapters_num].device_id = device_id;
                        if (strlen(card))
                            snprintf(adapters[*adapters_num].card,
                                     sizeof(adapters[*adapters_num].card),
                                     "%s%s", MFX_DRI_DIR, card);
                        else
                            adapters[*adapters_num].card[0] = '\0';
                        if (strlen(render))
                            snprintf(adapters[*adapters_num].render,
                                     sizeof(adapters[*adapters_num].render),
                                     "%s%s", MFX_DRI_DIR, render);
                        else
                            adapters[*adapters_num].card[0] = '\0';
                        *adapters_num                     = *adapters_num+1;
                    } else
                        goto nomem;
                }
            }
        }
        free(dir_entries[i]);
    }
    if (drm_entries_num)
       free(drm_dir_entries);
    if (entries_num)
       free(dir_entries);
    if (p_adapters)
        *p_adapters = adapters;
    return MFX_ERR_NONE;

 nomem:
    if (drm_entries_num)
       free(drm_dir_entries);
    if (entries_num)
       free(dir_entries);
    if (p_adapters)
        *p_adapters = adapters;
    return MFX_ERR_MEMORY_ALLOC;
}


mfxStatus mfx_allocate_va(mfxSession session)
{
    char *card = getenv("MFX_DRM_CARD");
    int ret, fd;
    int minor, major;
    int i, j;
    int adapters_num = 0;
    struct mfx_disp_adapters *adapters;
    char filename[300] = {0};
    // sizeof(MFX_PCI_DIR) = 20, sizeof(pci_bus_id) = 12(?), sizeof(dirent::d_name) <= 256, sizeof("drm") = 4
    VADisplay display;

    if (!card) {
        ret = mfx_search_disp_adapter_from_pci(&adapters, &adapters_num);
        if (ret != MFX_ERR_NONE)
            goto fail;
        for (i=0; i < adapters_num; i++) {
            if (adapters[i].vendor_id == MFX_VENDORID_INTEL) {
                if ( adapters[i].render[0] != '\0' ) {
                    card = adapters[i].render; // renderDN : iHD
                    break;
                } else if( adapters[i].card[0] != '\0' ) {
                    card = adapters[i].card; // cardN : i965
                    break;
                }
            }
        }
        if (!card) {
            // Intel VGA card not found on the system.
            DISPATCHER_LOG_ERROR(("No Intel VGA card found.\n"));
            ret = MFX_ERR_NOT_INITIALIZED;
            goto fail;
        }
    }
    if ((fd = open(card, O_RDWR)) < 0) {
        ret = MFX_ERR_NOT_INITIALIZED;
        goto fail;
    }

    if (!(display = vaGetDisplayDRM(fd))) {
        ret = MFX_ERR_NULL_PTR;
        goto fail;
     }

    if (vaInitialize(display, &major, &minor) < 0) {
        ret = MFX_ERR_NOT_INITIALIZED;
        goto fail;
    }
 
    if(!adapters)
        free(adapters);

    return MFXVideoCORE_SetHandle(session, MFX_HANDLE_VA_DISPLAY, display);
fail:
    if(!adapters)
        free(adapters);
    close(fd);
    return ret;
}

void mfx_deallocate_va(mfxSession session)
{
    VADisplayContextP display;
    VADriverContextP driver;
    VADisplay disp;
    struct drm_state *state;
    int ret;

    ret = MFXVideoCORE_GetHandle(session, MFX_HANDLE_VA_DISPLAY,
                                 &disp);
    if (ret != MFX_ERR_NONE)
        return;

    display = disp;

    driver = display->pDriverContext;

    state = driver->drm_state;

    close(state->fd);

    vaTerminate(disp);
}
#endif
