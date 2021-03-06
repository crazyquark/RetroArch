/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "frontend_driver.h"
#include "../driver.h"
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

static const frontend_ctx_driver_t *frontend_ctx_drivers[] = {
#if defined(__CELLOS_LV2__)
   &frontend_ctx_ps3,
#endif
#if defined(_XBOX)
   &frontend_ctx_xdk,
#endif
#if defined(GEKKO)
   &frontend_ctx_gx,
#endif
#if defined(__QNX__)
   &frontend_ctx_qnx,
#endif
#if defined(IOS) || defined(OSX)
   &frontend_ctx_apple,
#endif
#if defined(ANDROID)
   &frontend_ctx_android,
#endif
#if defined(PSP)
   &frontend_ctx_psp,
#endif
#if defined(_3DS)
   &frontend_ctx_ctr,
#endif
#if defined(_WIN32) && !defined(_XBOX)
   &frontend_ctx_win32,
#endif
   &frontend_ctx_null,
   NULL
};

/**
 * frontend_ctx_find_driver:
 * @ident               : Identifier name of driver to find.
 *
 * Finds driver with @ident. Does not initialize.
 *
 * Returns: pointer to driver if successful, otherwise NULL.
 **/
const frontend_ctx_driver_t *frontend_ctx_find_driver(const char *ident)
{
   unsigned i;

   for (i = 0; frontend_ctx_drivers[i]; i++)
   {
      if (strcmp(frontend_ctx_drivers[i]->ident, ident) == 0)
         return frontend_ctx_drivers[i];
   }

   return NULL;
}

/**
 * frontend_ctx_init_first:
 *
 * Finds first suitable driver and initialize.
 *
 * Returns: pointer to first suitable driver, otherwise NULL. 
 **/
const frontend_ctx_driver_t *frontend_ctx_init_first(void)
{
   unsigned i;

   for (i = 0; frontend_ctx_drivers[i]; i++)
      return frontend_ctx_drivers[i];

   return NULL;
}

#ifndef IS_SALAMANDER
const frontend_ctx_driver_t *frontend_get_ptr(void)
{
   driver_t *driver        = driver_get_ptr();
   if (!driver)
      return NULL;
   return driver->frontend_ctx;
}
#endif
