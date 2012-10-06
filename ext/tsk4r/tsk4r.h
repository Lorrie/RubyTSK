/*
 *  sleuthkit.h
 *  sleuthkit
 *
 *  Created by Matthew H. Stephens on 9/14/11.
 *  Copyright 2011 Matthew H. Stephens. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define VERSION "0.0.4"
#ifndef TSK4R_H
#define TSK4R_H

#include "image.h"
#include "volume.h"
#include "file_system.h"
#include "fs_dir.h"
#include "fs_file.h"

// Prototype for the initialization method - Ruby calls this, not you
void Init_tsk4r();
void klassify(VALUE obj, char * name);


// Prototype for the module and classes directly below it
VALUE rb_mtsk4r;
VALUE rb_mtsk4r_v;
VALUE rb_mtsk4r_fs;

VALUE rb_cTSKImage;
VALUE rb_cTSKVolumeSystem;
VALUE rb_cTSKVolumePart;
VALUE rb_cTSKFileSystem;
VALUE rb_cTSKFileSystemDir;
VALUE rb_cTSKFileSystemFileData;
VALUE rb_cTSKFileSystemFileMeta;
VALUE rb_cTSKFileSystemFileName;


VALUE allocate_image(VALUE klass);

#endif