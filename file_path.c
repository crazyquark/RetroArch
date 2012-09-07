/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "file.h"
#include "general.h"
#include <stdlib.h>
#include "boolean.h"
#include <string.h>
#include <time.h>
#include "compat/strl.h"
#include "compat/posix_string.h"

#ifdef __CELLOS_LV2__
#include <unistd.h> //stat() is defined here
#define S_ISDIR(x) (x & CELL_FS_S_IFDIR)
#endif

#ifdef _XBOX
#include <xtl.h>
#endif

#if defined(_WIN32) && !defined(_XBOX)
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#ifdef _MSC_VER
#define setmode _setmode
#endif
#elif defined(_XBOX)
#define setmode _setmode
#define INVALID_FILE_ATTRIBUTES -1
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

static void string_list_free(struct string_list *list)
{
   if (!list)
      return;

   for (size_t i = 0; i < list->size; i++)
      free(list->elems[i].data);
   free(list->elems);
   free(list);
}

static bool string_list_capacity(struct string_list *list, size_t cap)
{
   rarch_assert(cap > list->size);

   struct string_list_elem *new_data = (struct string_list_elem*)realloc(list->elems, cap * sizeof(*new_data));
   if (!new_data)
      return false;

   list->elems = new_data;
   list->cap   = cap;
   return true;
}

static struct string_list *string_list_new(void)
{
   struct string_list *list = (struct string_list*)calloc(1, sizeof(*list));
   if (!list)
      return NULL;

   if (!string_list_capacity(list, 32))
   {
      string_list_free(list);
      return NULL;
   }

   return list;
}

static bool string_list_append(struct string_list *list, const char *elem, union string_list_elem_attr attr)
{
   if (list->size >= list->cap &&
         !string_list_capacity(list, list->cap * 2))
      return false;

   char *dup = strdup(elem);
   if (!dup)
      return false;

   list->elems[list->size].data = dup;
   list->elems[list->size].attr = attr;

   list->size++;
   return true;
}

struct string_list *string_split(const char *str, const char *delim)
{
   char *copy      = NULL;
   const char *tmp = NULL;

   struct string_list *list = string_list_new();
   if (!list)
      goto error;

   copy = strdup(str);
   if (!copy)
      goto error;

   char *save;
   tmp = strtok_r(copy, delim, &save);
   while (tmp)
   {
      union string_list_elem_attr attr;
      memset(&attr, 0, sizeof(attr));

      if (!string_list_append(list, tmp, attr))
         goto error;

      tmp = strtok_r(NULL, delim, &save);
   }

   free(copy);
   return list;

error:
   string_list_free(list);
   free(copy);
   return NULL;
}

bool string_list_find_elem(const struct string_list *list, const char *elem)
{
   if (!list)
      return false;

   for (size_t i = 0; i < list->size; i++)
   {
      if (strcmp(list->elems[i].data, elem) == 0)
         return true;
   }

   return false;
}

bool string_list_find_elem_prefix(const struct string_list *list, const char *prefix, const char *elem)
{
   if (!list)
      return false;

   char prefixed[PATH_MAX];
   snprintf(prefixed, sizeof(prefixed), "%s%s", prefix, elem);

   for (size_t i = 0; i < list->size; i++)
   {
      if (strcmp(list->elems[i].data, elem) == 0 ||
            strcmp(list->elems[i].data, prefixed) == 0)
         return true;
   }

   return false;
}

const char *path_get_extension(const char *path)
{
   const char *ext = strrchr(path, '.');
   if (ext)
      return ext + 1;
   else
      return "";
}

static int qstrcmp_plain(const void *a_, const void *b_)
{
   const struct string_list_elem *a = (const struct string_list_elem*)a_; 
   const struct string_list_elem *b = (const struct string_list_elem*)b_; 

   return strcasecmp(a->data, b->data);
}

static int qstrcmp_dir(const void *a_, const void *b_)
{
   const struct string_list_elem *a = (const struct string_list_elem*)a_; 
   const struct string_list_elem *b = (const struct string_list_elem*)b_; 

   // Sort directories before files.
   int a_dir = a->attr.b;
   int b_dir = b->attr.b;
   if (a_dir != b_dir)
      return b_dir - a_dir;
   else
      return strcasecmp(a->data, b->data);
}

void dir_list_sort(struct string_list *list, bool dir_first)
{
   if (!list)
      return;

   qsort(list->elems, list->size, sizeof(struct string_list_elem),
         dir_first ? qstrcmp_dir : qstrcmp_plain);
}

#ifdef _WIN32 // Because the API is just fucked up ...
struct string_list *dir_list_new(const char *dir, const char *ext, bool include_dirs)
{
   struct string_list *list = string_list_new();
   if (!list)
      return NULL;

   HANDLE hFind = INVALID_HANDLE_VALUE;
   WIN32_FIND_DATA ffd;

   char path_buf[PATH_MAX];
   snprintf(path_buf, sizeof(path_buf), "%s\\*", dir);

   struct string_list *ext_list = NULL;
   if (ext)
      ext_list = string_split(ext, "|");

   hFind = FindFirstFile(path_buf, &ffd);
   if (hFind == INVALID_HANDLE_VALUE)
      goto error;

   do
   {
      const char *name     = ffd.cFileName;
      const char *file_ext = path_get_extension(name);
      bool is_dir          = ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

      if (!include_dirs && is_dir)
         continue;

      if (!is_dir && ext_list && !string_list_find_elem_prefix(ext_list, ".", file_ext))
         continue;

      char file_path[PATH_MAX];
      snprintf(file_path, sizeof(file_path), "%s\\%s", dir, name);

      union string_list_elem_attr attr;
      attr.b = is_dir;

      if (!string_list_append(list, file_path, attr))
         goto error;
   }
   while (FindNextFile(hFind, &ffd) != 0);

   FindClose(hFind);
   string_list_free(ext_list);
   return list;

error:
   RARCH_ERR("Failed to open directory: \"%s\"\n", dir);
   if (hFind != INVALID_HANDLE_VALUE)
      FindClose(hFind);
   
   string_list_free(list);
   string_list_free(ext_list);
   return NULL;
}
#else
struct string_list *dir_list_new(const char *dir, const char *ext, bool include_dirs)
{
   struct string_list *list = string_list_new();
   if (!list)
      return NULL;

   DIR *directory = NULL;
   const struct dirent *entry = NULL;

   struct string_list *ext_list = NULL;
   if (ext)
      ext_list = string_split(ext, "|");

   directory = opendir(dir);
   if (!directory)
      goto error;

   while ((entry = readdir(directory)))
   {
      const char *name     = entry->d_name;
      const char *file_ext = path_get_extension(name);
      bool is_dir          = entry->d_type == DT_DIR;

      if (!include_dirs && is_dir)
         continue;

      if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
         continue;

      if (!is_dir && ext_list && !string_list_find_elem_prefix(ext_list, ".", file_ext))
         continue;

      char file_path[PATH_MAX];
      snprintf(file_path, sizeof(file_path), "%s/%s", dir, name);

      union string_list_elem_attr attr;
      attr.b = is_dir;

      if (!string_list_append(list, file_path, attr))
         goto error;
   }

   closedir(directory);

   string_list_free(ext_list);
   return list;

error:
   RARCH_ERR("Failed to open directory: \"%s\"\n", dir);

   if (directory)
      closedir(directory);

   string_list_free(list);
   string_list_free(ext_list);
   return NULL;
}
#endif

void dir_list_free(struct string_list *list)
{
   string_list_free(list);
}

bool path_is_directory(const char *path)
{
#ifdef _WIN32
   DWORD ret = GetFileAttributes(path);
   return (ret & FILE_ATTRIBUTE_DIRECTORY) && (ret != INVALID_FILE_ATTRIBUTES);
#else
   struct stat buf;
   if (stat(path, &buf) < 0)
      return false;

   return S_ISDIR(buf.st_mode);
#endif
}

bool path_file_exists(const char *path)
{
   FILE *dummy = fopen(path, "rb");
   if (dummy)
   {
      fclose(dummy);
      return true;
   }
   return false;
}

void fill_pathname(char *out_path, const char *in_path, const char *replace, size_t size)
{
   char tmp_path[PATH_MAX];

   rarch_assert(strlcpy(tmp_path, in_path, sizeof(tmp_path)) < sizeof(tmp_path));
   char *tok = strrchr(tmp_path, '.');
   if (tok)
      *tok = '\0';

   rarch_assert(strlcpy(out_path, tmp_path, size) < size);
   rarch_assert(strlcat(out_path, replace, size) < size);
}

void fill_pathname_noext(char *out_path, const char *in_path, const char *replace, size_t size)
{
   rarch_assert(strlcpy(out_path, in_path, size) < size);
   rarch_assert(strlcat(out_path, replace, size) < size);
}

void fill_pathname_dir(char *in_dir, const char *in_basename, const char *replace, size_t size)
{
   rarch_assert(strlcat(in_dir, "/", size) < size);

   const char *base = strrchr(in_basename, '/');
   if (!base)
      base = strrchr(in_basename, '\\');

   if (base)
      base++;
   else
      base = in_basename;

   rarch_assert(strlcat(in_dir, base, size) < size);
   rarch_assert(strlcat(in_dir, replace, size) < size);
}

void fill_pathname_base(char *out_dir, const char *in_path, size_t size)
{
   const char *ptr = strrchr(in_path, '/');
   if (!ptr)
      ptr = strrchr(in_path, '\\');

   if (ptr)
      ptr++;
   else
      ptr = in_path;

   rarch_assert(strlcpy(out_dir, ptr, size) < size);
}

void fill_pathname_basedir(char *out_dir, const char *in_path, size_t size)
{
   rarch_assert(strlcpy(out_dir, in_path, size) < size);

   char *base = strrchr(out_dir, '/');
   if (!base)
      base = strrchr(out_dir, '\\');

   if (base)
      *base = '\0';
   else if (size >= 2)
   {
      out_dir[0] = '.';
      out_dir[1] = '\0';
   }
}

void fill_pathname_shell(char *out_path, const char *in_path, size_t size)
{
#if !defined(_WIN32) && !defined(RARCH_CONSOLE)
   if (*in_path == '~')
   {
      const char *home = getenv("HOME");
      if (home)
      {
         size_t src_size = strlcpy(out_path, home, size);
         rarch_assert(src_size < size);

         out_path += src_size;
         size -= src_size;
         in_path++;
      }
   }
#endif

   rarch_assert(strlcpy(out_path, in_path, size) < size);
}

size_t convert_char_to_wchar(wchar_t *out_wchar, const char *in_char, size_t size)
{
   return mbstowcs(out_wchar, in_char, size / sizeof(wchar_t));
}

size_t convert_wchar_to_char(char *out_char, const wchar_t *in_wchar, size_t size)
{
   return wcstombs(out_char, in_wchar, size);
}

