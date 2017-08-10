// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILEAPI_LOCAL_FILE_UTIL_H_
#define STORAGE_BROWSER_FILEAPI_LOCAL_FILE_UTIL_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "storage/browser/fileapi/file_system_file_util.h"
#include "storage/browser/storage_browser_export.h"

namespace base {
class Time;
}

class GURL;

namespace storage {

class FileSystemOperationContext;
class FileSystemURL;

// An instance of this class is created and owned by *FileSystemBackend.
class STORAGE_EXPORT LocalFileUtil
    : public FileSystemFileUtil {
 public:
  LocalFileUtil();
  virtual ~LocalFileUtil();

  virtual base::File CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int file_flags) OVERRIDE;
  virtual base::File::Error EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url, bool* created) OVERRIDE;
  virtual base::File::Error CreateDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      bool exclusive,
      bool recursive) OVERRIDE;
  virtual base::File::Error GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::File::Info* file_info,
      base::FilePath* platform_file) OVERRIDE;
  virtual scoped_ptr<AbstractFileEnumerator> CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemURL& root_url) OVERRIDE;
  virtual base::File::Error GetLocalFilePath(
      FileSystemOperationContext* context,
      const FileSystemURL& file_system_url,
      base::FilePath* local_file_path) OVERRIDE;
  virtual base::File::Error Touch(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) OVERRIDE;
  virtual base::File::Error Truncate(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int64 length) OVERRIDE;
  virtual base::File::Error CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      CopyOrMoveOption option,
      bool copy) OVERRIDE;
  virtual base::File::Error CopyInForeignFile(
      FileSystemOperationContext* context,
      const base::FilePath& src_file_path,
      const FileSystemURL& dest_url) OVERRIDE;
  virtual base::File::Error DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
  virtual base::File::Error DeleteDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
  virtual storage::ScopedFile CreateSnapshotFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::File::Error* error,
      base::File::Info* file_info,
      base::FilePath* platform_path) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalFileUtil);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILEAPI_LOCAL_FILE_UTIL_H_
