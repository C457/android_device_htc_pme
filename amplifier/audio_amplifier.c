/*
 * Copyright (C) 2016, The CyanogenMod Project
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

#define LOG_TAG "audio_amplifier"
//#define LOG_NDEBUG 0

#include <stdio.h>
#include <stdlib.h>
#include <cutils/log.h>
#include <cutils/str_parms.h>

#include <hardware/audio_amplifier.h>
#include <system/audio.h>

#include "tfa.h"
#include "tfa-cont.h"

#define UNUSED __attribute__ ((unused))

typedef struct amp_device {
    amplifier_device_t amp_dev;
    tfa_t *tfa;
    tfa_cont_t *tc;
    audio_mode_t mode;
    struct pcm *pcm;
} amp_device_t;

static amp_device_t *amp_dev = NULL;

static int amp_set_mode(struct amplifier_device *device, audio_mode_t mode)
{
    int ret = 0;
    amp_device_t *dev = (amp_device_t *) device;

    dev->mode = mode;
    return ret;
}

#define TFA_DEVICE_MASK (AUDIO_DEVICE_OUT_EARPIECE | AUDIO_DEVICE_OUT_SPEAKER)

static int amp_enable_output_devices(struct amplifier_device *device, uint32_t devices, bool enable)
{
    amp_device_t *dev = (amp_device_t *) device;
    int ret;

    if ((devices & TFA_DEVICE_MASK) != 0) {
        if (enable && !dev->pcm) {
            // TODO figure out the right profile based on the output devices
            dev->pcm = tfa_clocks_on(dev->tfa);
            tfa_start(dev->tfa, dev->tc, 0, 0);
        } else {
            tfa_clocks_off(dev->tfa, dev->pcm);
            tfa_stop(dev->tfa);
            dev->pcm = NULL;
        }
    }

    return 0;
}

static int amp_dev_close(hw_device_t *device)
{
    amp_device_t *dev = (amp_device_t *) device;

    tfa_destroy(dev->tfa);
    tfa_cont_destroy(dev->tc);

    free(dev);

    return 0;
}

static void init(void)
{
    struct pcm *pcm;

    pcm = tfa_clocks_on(amp_dev->tfa);
    tfa_start(amp_dev->tfa, amp_dev->tc, 0, 0);
    tfa_clocks_off(amp_dev->tfa, pcm);

    tfa_stop(amp_dev->tfa);
}

static int amp_module_open(const hw_module_t *module, const char *name UNUSED,
        hw_device_t **device)
{
    int ret;
    tfa_t *tfa;
    tfa_cont_t *tc;

    if (amp_dev) {
        ALOGE("%s:%d: Unable to open second instance of the amplifier\n", __func__, __LINE__);
        return -EBUSY;
    }

    tfa = tfa_new();
    if (!tfa) {
        ALOGE("%s:%d: Unable to tfa lib\n", __func__, __LINE__);
        return -ENOENT;
    }
    tc = tfa_cont_new("/system/etc/Tfa98xx.cnt");
    if (!tc) {
        ALOGE("%s:%d: Unable to open tfa container\n", __func__, __LINE__);
        tfa_destroy(tfa);
        return -ENOENT;
    }

    amp_dev = calloc(1, sizeof(amp_device_t));
    if (!amp_dev) {
        ALOGE("%s:%d: Unable to allocate memory for amplifier device\n", __func__, __LINE__);
        tfa_destroy(tfa);
        return -ENOMEM;
    }

    amp_dev->amp_dev.common.tag = HARDWARE_DEVICE_TAG;
    amp_dev->amp_dev.common.module = (hw_module_t *) module;
    amp_dev->amp_dev.common.version = HARDWARE_DEVICE_API_VERSION(1, 0);
    amp_dev->amp_dev.common.close = amp_dev_close;

    amp_dev->amp_dev.set_input_devices = NULL;
    amp_dev->amp_dev.set_output_devices = NULL;
    amp_dev->amp_dev.enable_input_devices = NULL;
    amp_dev->amp_dev.enable_output_devices = amp_enable_output_devices;
    amp_dev->amp_dev.set_mode = amp_set_mode;
    amp_dev->amp_dev.output_stream_start = NULL;
    amp_dev->amp_dev.input_stream_start = NULL;
    amp_dev->amp_dev.output_stream_standby = NULL;
    amp_dev->amp_dev.input_stream_standby = NULL;

    amp_dev->tfa = tfa;
    amp_dev->tc  = tc;

    init();

    *device = (hw_device_t *) amp_dev;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = amp_module_open,
};

amplifier_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AMPLIFIER_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AMPLIFIER_HARDWARE_MODULE_ID,
        .name = "Kiwi audio amplifier HAL",
        .author = "The CyanogenMod Open Source Project",
        .methods = &hal_module_methods,
    },
};
