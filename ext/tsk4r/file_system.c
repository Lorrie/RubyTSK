/*
 *  file_system.c: File System classes for tsk4r
 *
 *  tsk4r: The SleuthKit 4 Ruby
 *
 *  Created by Matthew H. Stephens
 *  Copyright 2011,2012 Matthew H. Stephens. All rights reserved.
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
 *
 * Binding to the SleuthKit (libtsk3) library copyright 2003,2012 by Brian Carrier
 */

#include <stdio.h>
#include <ruby.h>
#include "file_system.h"

extern VALUE rb_cTSKImage;
extern VALUE rb_cTSKVolumeSystem;
extern VALUE rb_cTSKVolumePart;
extern VALUE rb_cTSKFileSystemDir;
extern VALUE rb_cTSKFileSystemFileData;


//struct tsk4r_img {
//  TSK_IMG_INFO * image;
//  char * fn_given;
//};
// prototypes (private)
TSK_FS_TYPE_ENUM * get_fs_flag();
void populate_instance_variables(VALUE self);

// functions

VALUE allocate_filesystem(VALUE klass){
  struct tsk4r_fs_wrapper * ptr;
  //    ptr = ALLOC(struct tsk4r_fs_wrapper);
  return Data_Make_Struct(klass, struct tsk4r_fs_wrapper, 0, deallocate_filesystem, ptr);
}

void deallocate_filesystem(struct tsk4r_fs_wrapper * ptr){
  TSK_FS_INFO *filesystem = ptr->filesystem;
  tsk_fs_close(filesystem);
  xfree(ptr);
}

VALUE initialize_filesystem(int argc, VALUE *args, VALUE self){
  VALUE source_obj; VALUE opts;
  rb_scan_args(argc, args, "11", &source_obj, &opts);
  
  if ( RTEST(opts) != rb_cHash){
    opts = rb_hash_new();
  }
  opts = rb_funcall(self, rb_intern("parse_opts"), 1, opts);
  
  open_filesystem(self, source_obj, opts);
  
  return self;
}


VALUE open_filesystem(VALUE self, VALUE parent_obj, VALUE opts) {
  struct tsk4r_fs_wrapper * fs_ptr;
  Data_Get_Struct(self, struct tsk4r_fs_wrapper, fs_ptr);
  
  if (rb_obj_is_kind_of((VALUE)parent_obj, rb_cTSKImage)) {
    open_fs_from_image(self, parent_obj, opts);

  } else if (rb_obj_is_kind_of((VALUE)parent_obj, rb_cTSKVolumeSystem)) {
    open_fs_from_volume(self, parent_obj, opts);
    
  } else if (rb_obj_is_kind_of((VALUE)parent_obj, rb_cTSKVolumePart)) {
    open_fs_from_partition(self, parent_obj, opts);
    
  } else {
    rb_raise(rb_eTypeError, "arg1 must be a SleuthKit::Image, SleuthKit::Volume::System or SleuthKit::Volume::Partition object.");
  }
  
  if (fs_ptr->filesystem != NULL) {

    populate_instance_variables(self);
    VALUE my_description = get_filesystem_type(self);
    rb_iv_set(self, "@description", my_description);
    rb_iv_set(self, "@parent", parent_obj);
    
  } else {
    rb_funcall(self, rb_intern("taint"), 0, NULL);
    rb_warn("filesystem pointer is NULL\n");
  }
  return self;
}

VALUE open_fs_from_image(VALUE self, VALUE image_obj, VALUE opts) {
  struct tsk4r_img * rb_image; struct tsk4r_fs_wrapper * my_pointer;
  TSK_OFF_T offset = 0;
  VALUE fs_type_flag = rb_hash_aref(opts, rb_symname_p("type_flag"));
  TSK_FS_TYPE_ENUM * type_flag_num = get_fs_flag(fs_type_flag);

  Data_Get_Struct(image_obj, struct tsk4r_img, rb_image);
  Data_Get_Struct(self, struct tsk4r_fs_wrapper, my_pointer);
  TSK_IMG_INFO * disk = rb_image->image;
  my_pointer->filesystem = tsk_fs_open_img(disk, offset, (TSK_FS_TYPE_ENUM)type_flag_num);
  return self;
}

VALUE open_fs_from_partition(VALUE self, VALUE vpart_obj, VALUE opts) {
  struct tsk4r_vs_part * rb_partition; struct tsk4r_fs_wrapper * my_pointer;
  Data_Get_Struct(vpart_obj, struct tsk4r_vs_part, rb_partition);
  Data_Get_Struct(self, struct tsk4r_fs_wrapper, my_pointer);

  VALUE fs_type_flag = rb_hash_aref(opts, rb_symname_p("type_flag"));
  TSK_FS_TYPE_ENUM * type_flag_num = get_fs_flag(fs_type_flag);
  
  my_pointer->filesystem = tsk_fs_open_vol(rb_partition->volume_part, (TSK_FS_TYPE_ENUM)type_flag_num);
  
  return self;
}

// should warn user when there is more than one partition available
// with a readable filesystem.  Right now, this simply returns
// the last one found.
VALUE open_fs_from_volume(VALUE self, VALUE vs_obj, VALUE opts) {
  struct tsk4r_vs * rb_volumesystem; struct tsk4r_fs_wrapper * my_pointer;
  Data_Get_Struct(vs_obj, struct tsk4r_vs, rb_volumesystem);
  Data_Get_Struct(self, struct tsk4r_fs_wrapper, my_pointer);

  VALUE fs_type_flag = rb_hash_aref(opts, rb_symname_p("type_flag"));
  TSK_FS_TYPE_ENUM * type_flag_num = get_fs_flag(fs_type_flag);
  
  TSK_PNUM_T c = 0;
  while (c < rb_volumesystem->volume->part_count) {
    const TSK_VS_PART_INFO * partition = tsk_vs_part_get(rb_volumesystem->volume, c);
    my_pointer->filesystem = tsk_fs_open_vol(partition, (TSK_FS_TYPE_ENUM)type_flag_num);
    if (my_pointer->filesystem != NULL) { break; }
    c++;
  }
  return self;
}

// directory read functions
VALUE open_directory_by_name(int argc, VALUE *args, VALUE self) {
  VALUE name; VALUE opts; TSK_FS_DIR * tsk_dir; struct tsk4r_fs_wrapper * fs_ptr;
  Data_Get_Struct(self, struct tsk4r_fs_wrapper, fs_ptr);

  rb_scan_args(argc, args, "11", &name, &opts);
  VALUE new_obj;
  tsk_dir = tsk_fs_dir_open(fs_ptr->filesystem, StringValuePtr(name));
  if (tsk_dir != NULL ) {
    printf("We are getting somewhere (open_directory_by_name)!!\n");
    new_obj = rb_funcall(rb_cTSKFileSystemDir, rb_intern("new"), 2, self, name);
  } else {
    new_obj = Qnil;
  }
  return new_obj;
}
VALUE open_directory_by_inum(int argc, VALUE *args, VALUE self) {
  VALUE inum; VALUE opts; struct tsk4r_fs_wrapper * fs_ptr;
  VALUE new_obj;
  
  Data_Get_Struct(self, struct tsk4r_fs_wrapper, fs_ptr);
  rb_scan_args(argc, args, "11", &inum, &opts);
  if ( ! rb_obj_is_kind_of(inum, rb_cInteger) ) { inum = INT2FIX(0); }

  new_obj = rb_funcall(rb_cTSKFileSystemDir, rb_intern("new"), 2, self, inum);

  if ( ! OBJ_TAINTED(new_obj) ) {
    printf("inum = %lu\n", FIX2LONG(rb_iv_get(new_obj, "@inum")));
    printf("names_used = %lu\n", FIX2LONG(rb_iv_get(new_obj, "@names_used")));
    printf("names_alloc = %lu\n", FIX2LONG(rb_iv_get(new_obj, "@names_alloc")));
  } else {
    new_obj = Qnil;
  }
  return new_obj;
}

VALUE open_file_by_inum(int argc, VALUE *args, VALUE self) {
  VALUE inum; VALUE opts; VALUE new_obj;
  
  rb_scan_args(argc, args, "11", &inum, &opts);
  if ( ! rb_obj_is_kind_of(inum, rb_cInteger) ) { inum = INT2FIX(0); }
  
  new_obj = rb_funcall(rb_cTSKFileSystemFileData, rb_intern("new"), 2, self, inum);
  if ( OBJ_TAINTED(new_obj)) {
    new_obj = Qnil;
  }
  
  return new_obj;
}

VALUE open_file_by_name(int argc, VALUE *args, VALUE self) {
  VALUE name; VALUE opts; VALUE new_obj;
  
  rb_scan_args(argc, args, "11", &name, &opts);
  if ( ! rb_obj_is_kind_of(name, rb_cString) ) { name = rb_funcall(name, rb_intern("to_s"), 0); }
  
  new_obj = rb_funcall(rb_cTSKFileSystemFileData, rb_intern("new"), 2, self, name);
  if ( OBJ_TAINTED(new_obj)) {
    new_obj = Qnil;
  }
  
  return new_obj;
}


// utility functions
VALUE get_filesystem_type(VALUE self) {
  const char * mytype;
  struct tsk4r_fs_wrapper * fs_ptr;
  Data_Get_Struct(self, struct tsk4r_fs_wrapper, fs_ptr);
  mytype = tsk_fs_type_toname(fs_ptr->filesystem->ftype);
  rb_iv_set(self, "@name", rb_str_new2(mytype));
  return rb_str_new2(mytype);
}

// want to call this function from ruby
// uint8_t(*fsstat) (TSK_FS_INFO * fs, FILE * hFile);
VALUE call_tsk_fsstat(VALUE self, VALUE io){
  if (! rb_obj_is_kind_of(io, rb_cIO) ) {
    rb_raise(rb_eArgError, "Method did not recieve IO object");
  }
  int fd = FIX2LONG(rb_funcall(io, rb_intern("fileno"), 0));
  FILE * hFile = fdopen((int)fd, "w");

  struct tsk4r_fs_wrapper * fs_ptr;
  Data_Get_Struct(self, struct tsk4r_fs_wrapper, fs_ptr);
  
  // this accesses the function pointer in TSK_FS_INFO
  // the function dumps a status report (text)
  // to the file handle specified by hFile
  if (fs_ptr->filesystem != NULL) {
    uint8_t(*myfunc) (TSK_FS_INFO * fs, FILE * hFile);
    myfunc = fs_ptr->filesystem->fsstat;
    int r = myfunc(fs_ptr->filesystem, hFile);
    fflush(hFile); // clear file buffer, completing write
    if (r != 0 ) { rb_raise(rb_eRuntimeError, "TSK function: fsstat exited with an error."); }
  }

  return self;
}

VALUE call_tsk_istat(int argc, VALUE *args, VALUE self) {
  VALUE io; VALUE inum; VALUE options;
  rb_scan_args(argc, args, "21", &inum, &io, &options);
  if (! rb_obj_is_kind_of(io, rb_cIO) ) {
    rb_raise(rb_eArgError, "Method did not recieve IO object");
  }
  
  TSK_DADDR_T numblock; int32_t sec_skew;
  VALUE block = rb_hash_aref(options, ID2SYM(rb_intern("block")));
  if (! NIL_P(block)) { numblock = (TSK_DADDR_T)NUM2ULL(block); } else { numblock = 0; }
  VALUE skew = rb_hash_aref(options, rb_symname_p("skew"));
  if (! NIL_P(skew)) {  sec_skew = (int32_t)NUM2INT(skew); } else { sec_skew = 0; }

  TSK_INUM_T inum_int;
  inum_int = NUM2INT(inum);
  int fd = FIX2LONG(rb_funcall(io, rb_intern("fileno"), 0));
  FILE * hFile = fdopen((int)fd, "w");
  
  struct tsk4r_fs_wrapper * fs_ptr;
  Data_Get_Struct(self, struct tsk4r_fs_wrapper, fs_ptr);
  
  if (fs_ptr->filesystem != NULL) {
    uint8_t(*myfunc) (TSK_FS_INFO * fs, FILE * hFile, TSK_INUM_T inum,
                      TSK_DADDR_T numblock, int32_t sec_skew);
    myfunc = fs_ptr->filesystem->istat;
    int r = myfunc(fs_ptr->filesystem, hFile, inum_int, numblock, sec_skew);
    fflush(hFile); // clear file buffer, completing write
    if (r != 0 ) { rb_raise(rb_eRuntimeError, "TSK function: fsstat exited with an error."); }

  }
  return self;
}

// assigns numerous TSK_FS_INFO values to ruby instance variables
void populate_instance_variables(VALUE self) {
  struct tsk4r_fs_wrapper * fs_ptr;
  Data_Get_Struct(self, struct tsk4r_fs_wrapper, fs_ptr);

  rb_iv_set(self, "@block_count", ULONG2NUM((unsigned long long)fs_ptr->filesystem->block_count));
  //    rb_iv_set(self, "@block_getflags", INT2NUM((int)fs_ptr->filesystem->block_getflags)); // do not impl
// These features are only in libtsk >= 3.2.2
#ifndef TSK4R_HIDE_ADVANCED_FEATURE
  rb_iv_set(self, "@block_post_size", UINT2NUM((uint)fs_ptr->filesystem->block_post_size));
  rb_iv_set(self, "@block_pre_size", UINT2NUM((uint)fs_ptr->filesystem->block_pre_size));
#endif
  rb_iv_set(self, "@block_size", UINT2NUM((uint)fs_ptr->filesystem->block_size));
  //    rb_iv_set(self, "@block_walk", fs_ptr->filesystem->block_walk);// no
  rb_iv_set(self, "@dev_bsize", UINT2NUM((uint)fs_ptr->filesystem->dev_bsize));
  rb_iv_set(self, "@data_unit_name", rb_str_new2(fs_ptr->filesystem->duname));
  rb_iv_set(self, "@endian", UINT2NUM((uint)fs_ptr->filesystem->endian));
  rb_iv_set(self, "@first_inum", ULONG2NUM((unsigned long int)fs_ptr->filesystem->first_inum));
  //    rb_iv_set(self, "@file_add_meta", fs_ptr->filesystem->file_add_meta); // do not impl
  rb_iv_set(self, "@first_block", ULONG2NUM((unsigned long int)fs_ptr->filesystem->first_block));
  rb_iv_set(self, "@flags", UINT2NUM((uint)fs_ptr->filesystem->flags));
  //    rb_iv_set(self, "@fread_owner_sid", fs_ptr->filesystem->fread_owner_sid); // do not impl
  rb_iv_set(self, "@fs_id", rb_str_new2((char *)fs_ptr->filesystem->fs_id));
  rb_iv_set(self, "@fs_id_used", UINT2NUM((uint)fs_ptr->filesystem->fs_id_used));
  //    rb_iv_set(self, "@fscheck", fs_ptr->filesystem->fscheck); // do not impl
  //    rb_iv_set(self, "@fsstat", fs_ptr->filesystem->fsstat); // do not impl
  rb_iv_set(self, "@ftype", UINT2NUM((uint)fs_ptr->filesystem->ftype));
  //    rb_iv_set(self, "@get_default_attr_type", fs_ptr->filesystem->get_default_attr_type); // do not impl
  //    rb_iv_set(self, "@inode_walk", fs_ptr->filesystem->inode_walk);//no
  rb_iv_set(self, "@inum_count", ULONG2NUM((unsigned long int)fs_ptr->filesystem->inum_count));
#ifdef TSK4R_DEPRECATED_TSK4_FEATURE
  rb_iv_set(self, "@isOrphanHunting", UINT2NUM((uint)fs_ptr->filesystem->isOrphanHunting));
#endif
  //    rb_iv_set(self, "@istat", fs_ptr->filesystem->istat); // do not impl
  //    rb_iv_set(self, "@jblk_walk", fs_ptr->filesystem->jblk_walk);//no
  //    rb_iv_set(self, "@jentry_walk", fs_ptr->filesystem->jentry_walk);//no
  //    rb_iv_set(self, "@jopen", fs_ptr->filesystem->jopen);//no
  rb_iv_set(self, "@journ_inum", ULONG2NUM((unsigned long int)fs_ptr->filesystem->journ_inum));
  rb_iv_set(self, "@last_block", ULONG2NUM((unsigned long int)fs_ptr->filesystem->last_block));
  rb_iv_set(self, "@last_block_act", ULONG2NUM((unsigned long int)fs_ptr->filesystem->last_block_act));
  rb_iv_set(self, "@last_inum", ULONG2NUM((unsigned long int)fs_ptr->filesystem->last_inum));
  //    rb_iv_set(self, "@list_inum_named", fs_ptr->filesystem->list_inum_named); // to do
  //    rb_iv_set(self, "@load_attrs", fs_ptr->filesystem->load_attrs); // do not impl
  //    rb_iv_set(self, "@name_cmp", fs_ptr->filesystem->name_cmp); // do no impl
  rb_iv_set(self, "@offset", LONG2NUM((long int)fs_ptr->filesystem->offset));
  //    rb_iv_set(self, "@orphan_dir", fs_ptr->filesystem->orphan_dir); // to do
  rb_iv_set(self, "@root_inum", ULONG2NUM((unsigned long int)fs_ptr->filesystem->root_inum));
  rb_iv_set(self, "@tag", INT2NUM(fs_ptr->filesystem->tag));
}


VALUE return_tsk_fs_type_list(int argc, VALUE *args, VALUE self) {
  VALUE io; uint fd;
  rb_scan_args(argc, args, "01", &io);
  if (! rb_obj_is_kind_of(io, rb_cIO) ) {
    rb_warn("Method did not recieve IO object, using STDOUT");
    fd = 1;
  } else {
    fd = FIX2LONG(rb_funcall(io, rb_intern("fileno"), 0));
  }
  
  FILE * hFile = fdopen((int)fd, "w");
  tsk_fs_type_print(hFile);
  fflush(hFile); // clear file buffer, completing write
  
  return self;
}

// helper method to convert ruby integers to TSK_IMG_TYPE_ENUM values
TSK_FS_TYPE_ENUM * get_fs_flag(VALUE rb_obj) {
  TSK_FS_TYPE_ENUM * flag;
  switch (TYPE(rb_obj)) {
    case T_STRING:
      printf("string is %s\n", StringValuePtr(rb_obj));
      char *str = StringValuePtr(rb_obj);
      //TO DO: convert string to value of Sleuthkit::FileSystem::TSK_FS_TYPE_ENUM[string.to_sym]
      printf("flag is %s\n", str);
      flag = (TSK_FS_TYPE_ENUM *)0;
      break;
      
    case T_FIXNUM:
      printf("fs_type is %ld\n", NUM2INT(rb_obj));
      long num = NUM2INT(rb_obj);
      flag = (TSK_FS_TYPE_ENUM *)num;
      break;
      
    case T_SYMBOL:
      // TO DO
      flag = (TSK_FS_TYPE_ENUM *)0;
      break;
      
    default:
      flag = (TSK_FS_TYPE_ENUM *)0;
      break;
  }
  return flag;
}