/***************************************************************************
 * This file is part of NUSspli.                                           *
 * Copyright (c) 2021-2022 V10lator <v10lator@myway.de>                    *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include <wut-fixups.h>

#include <stdbool.h>
#include <stdint.h>

#include <crypto.h>
#include <file.h>
#include <romfs.h>
#include <state.h>
#include <staticMem.h>
#include <utils.h>

#include <coreinit/mcp.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/time.h>
#include <coreinit/title.h>

#include <mbedtls/md5.h>

#define MD5_FILES 2

static const uint32_t md5[MD5_FILES][16] = {
    { 0xcb490117, 0xda849d9f, 0x5cd4bbe8, 0xe860b181 },
    { 0xe91699c5, 0x5f10d1bb, 0x08bb8849, 0xd36f35ff },
};

static const char *md5File[MD5_FILES] = {
    ROMFS_PATH "audio/bg.mp3",
    ROMFS_PATH "textures/goodbye.png",
};

bool sanityCheck()
{
    size_t s;
    void *buf;
    uint32_t m[4];
    OSTime t = OSGetTime();

    for(int i = 0; i < MD5_FILES; ++i)
    {
        s = readFile(md5File[i], &buf);
        if(buf == NULL)
            return false;

        mbedtls_md5(buf, s, (unsigned char *)m);
        for(int j = 0; j < 4; ++j)
        {
            if(m[j] != md5[i][j])
            {
                debugPrintf("Sanity error: Data integrity error");
                MEMFreeToDefaultHeap(buf);
                return false;
            }
        }

        MEMFreeToDefaultHeap(buf);
    }

    t = OSGetTime() - t;
    addEntropy(&t, sizeof(OSTime));
    return true;
}
