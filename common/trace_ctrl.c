/**
 * Copyright 2015 Qihoo 360
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include "trace_ctrl.h"

int pt_ctrl_open(pt_ctrl_t *ctrl, const char *file)
{
    return pt_mmap_open(&ctrl->seg, file, PT_CTRL_SIZE);
}

int pt_ctrl_create(pt_ctrl_t *ctrl, const char *file)
{
    return pt_mmap_create(&ctrl->seg, file, PT_CTRL_SIZE);
}

int pt_ctrl_close(pt_ctrl_t *ctrl)
{
    return pt_mmap_close(&ctrl->seg);
}

void pt_ctrl_clean_all(pt_ctrl_t *ctrl)
{
    memset(ctrl->seg.addr, 0x00, ctrl->seg.size);
}
