/***************************************************************************
 * This file is part of NUSspli.                                           *
 * Copyright (c) 2019-2020 Pokes303                                        *
 * Copyright (c) 2020-2022 V10lator <v10lator@myway.de>                    *
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

#include <crypto.h>
#include <file.h>
#include <filesystem.h>
#include <input.h>
#include <list.h>
#include <localisation.h>
#include <menu/filebrowser.h>
#include <menu/utils.h>
#include <renderer.h>
#include <state.h>

#include <coreinit/memdefaultheap.h>
#include <coreinit/memory.h>

#include <dirent.h>
#include <stdbool.h>
#include <string.h>

#define MAX_FILEBROWSER_LINES (MAX_LINES - 5)

static void drawFBMenuFrame(const char *path, LIST *folders, size_t pos, const size_t cursor, const NUSDEV activeDevice, bool usbMounted)
{
    startNewFrame();
    textToFrame(0, 6, gettext("Select a folder:"));

    boxToFrame(1, MAX_LINES - 3);

    char *toWrite = getToFrameBuffer();
    strcpy(toWrite, gettext("Press " BUTTON_A " to select"));
    strcat(toWrite, " || ");
    strcat(toWrite, gettext(BUTTON_B " to return"));
    strcat(toWrite, " || ");

    char *l = getStaticLineBuffer();
    strcpy(l, BUTTON_X " to switch to ");
    strcat(l, activeDevice == NUSDEV_USB ? "SD" : activeDevice == NUSDEV_SD ? "NAND"
            : usbMounted                                                    ? "USB"
                                                                            : "SD");
    strcat(toWrite, gettext(l));
    textToFrame(MAX_LINES - 2, ALIGNED_CENTER, toWrite);

    strcpy(toWrite, gettext("Searching on"));
    strcat(toWrite, " => ");
    strcat(toWrite, prettyDir(path));
    textToFrame(MAX_LINES - 1, ALIGNED_CENTER, toWrite);

    char *folder;
    int i = 0;
    forEachListEntry(folders, folder)
    {
        if(pos)
        {
            --pos;
            continue;
        }

        if(cursor == i)
            arrowToFrame(i + 2, 1);

        textToFrame(i + 2, 5, folder);

        if(++i == MAX_FILEBROWSER_LINES)
            break;
    }

    drawFrame();
}

char *fileBrowserMenu()
{
    LIST *folders = createList();
    if(folders == NULL)
        return false;

    size_t cursor, pos;
    NUSDEV usbMounted = getUSB();
    NUSDEV activeDevice = usbMounted ? NUSDEV_USB : NUSDEV_SD;
    bool mov;
    FSDirectoryHandle dir;
    bool ret = false;
    char *path = getStaticPathBuffer(2);

refreshVOlList:
    strcpy(path, (activeDevice & NUSDEV_USB) ? (usbMounted == NUSDEV_USB01 ? INSTALL_DIR_USB1 : INSTALL_DIR_USB2) : (activeDevice == NUSDEV_SD ? INSTALL_DIR_SD : INSTALL_DIR_MLC));
refreshDirList:
    OSTime t = OSGetTime();
    clearList(folders, true);
    char *name = MEMAllocFromDefaultHeap(strlen("../") + 1);
    if(name == NULL)
        goto exitFileBrowserMenu;

    OSBlockMove(name, "../", strlen("../") + 1, false);
    if(!addToListEnd(folders, name))
    {
        MEMFreeToDefaultHeap(name);
        goto exitFileBrowserMenu;
    }

    cursor = pos = 0;

    if(FSOpenDir(__wut_devoptab_fs_client, getCmdBlk(), path, &dir, FS_ERROR_FLAG_ALL) == FS_STATUS_OK)
    {
        size_t len;
        FSDirectoryEntry entry;
        while(FSReadDir(__wut_devoptab_fs_client, getCmdBlk(), dir, &entry, FS_ERROR_FLAG_ALL) == FS_STATUS_OK)
            if(entry.info.flags & FS_STAT_DIRECTORY && entry.name[0] != '.') // Check if it's a directory
            {
                len = strlen(entry.name);
                name = MEMAllocFromDefaultHeap(len + 2);
                if(name == NULL)
                {
                    FSCloseDir(__wut_devoptab_fs_client, getCmdBlk(), dir, FS_ERROR_FLAG_ALL);
                    goto exitFileBrowserMenu;
                }

                OSBlockMove(name, entry.name, len, false);
                name[len] = '/';
                name[++len] = '\0';
                if(!addToListEnd(folders, name))
                {
                    MEMFreeToDefaultHeap(name);
                    FSCloseDir(__wut_devoptab_fs_client, getCmdBlk(), dir, FS_ERROR_FLAG_ALL);
                    goto exitFileBrowserMenu;
                }
            }

        FSCloseDir(__wut_devoptab_fs_client, getCmdBlk(), dir, FS_ERROR_FLAG_ALL);
    }

    t = OSGetTime() - t;
    addEntropy(&t, sizeof(OSTime));

    mov = getListSize(folders) >= MAX_FILEBROWSER_LINES;
    bool redraw = true;
    uint32_t oldHold = 0;
    size_t frameCount = 0;
    bool dpadAction;
    while(AppRunning(true))
    {
        if(app == APP_STATE_BACKGROUND)
            continue;
        if(app == APP_STATE_RETURNING)
            redraw = true;

        if(redraw)
        {
            drawFBMenuFrame(path, folders, pos, cursor, activeDevice, usbMounted);
            redraw = false;
        }
        showFrame();

        if(vpad.trigger & VPAD_BUTTON_B)
            goto exitFileBrowserMenu;
        if(vpad.trigger & VPAD_BUTTON_A)
        {
            if(cursor + pos == 0)
            {
                char *last = strstr(path + strlen("/vol/"), "/");
                char *cur = strstr(last + 1, "/");
                if(cur != NULL)
                {
                    char *next = strstr(cur + 1, "/");
                    while(next != NULL)
                    {
                        last = cur;
                        cur = next;
                        next = strstr(cur + 1, "/");
                    }

                    *++last = '\0';
                    goto refreshDirList;
                }
            }
            else
            {
                strcat(path, getContent(folders, cursor + pos));
                size_t pos = strlen(path);
                strcpy(path + pos, "title.tmd");
                ret = fileExists(path);
                path[pos] = '\0';
                if(ret)
                    goto exitFileBrowserMenu;

                goto refreshDirList;
            }
        }

        if(vpad.hold & VPAD_BUTTON_UP)
        {
            if(oldHold != VPAD_BUTTON_UP)
            {
                oldHold = VPAD_BUTTON_UP;
                frameCount = 30;
                dpadAction = true;
            }
            else if(frameCount == 0)
                dpadAction = true;
            else
            {
                --frameCount;
                dpadAction = false;
            }

            if(dpadAction)
            {
                if(cursor)
                    cursor--;
                else
                {
                    if(mov)
                    {
                        if(pos)
                            pos--;
                        else
                        {
                            cursor = MAX_FILEBROWSER_LINES - 1;
                            pos = getListSize(folders) - MAX_FILEBROWSER_LINES;
                        }
                    }
                    else
                        cursor = getListSize(folders) - 1;
                }

                redraw = true;
            }
        }
        else if(vpad.hold & VPAD_BUTTON_DOWN)
        {
            if(oldHold != VPAD_BUTTON_DOWN)
            {
                oldHold = VPAD_BUTTON_DOWN;
                frameCount = 30;
                dpadAction = true;
            }
            else if(frameCount == 0)
                dpadAction = true;
            else
            {
                --frameCount;
                dpadAction = false;
            }

            if(dpadAction)
            {
                if(cursor + pos >= getListSize(folders) - 1 || cursor >= MAX_FILEBROWSER_LINES - 1)
                {
                    if(!mov || ++pos + cursor >= getListSize(folders))
                        cursor = pos = 0;
                }
                else
                    ++cursor;

                redraw = true;
            }
        }
        else if(mov)
        {
            if(vpad.hold & VPAD_BUTTON_RIGHT)
            {
                if(oldHold != VPAD_BUTTON_RIGHT)
                {
                    oldHold = VPAD_BUTTON_RIGHT;
                    frameCount = 30;
                    dpadAction = true;
                }
                else if(frameCount == 0)
                    dpadAction = true;
                else
                {
                    --frameCount;
                    dpadAction = false;
                }

                if(dpadAction)
                {
                    pos += MAX_FILEBROWSER_LINES;
                    if(pos >= getListSize(folders))
                        pos = 0;
                    cursor = 0;
                    redraw = true;
                }
            }
            else if(vpad.hold & VPAD_BUTTON_LEFT)
            {
                if(oldHold != VPAD_BUTTON_LEFT)
                {
                    oldHold = VPAD_BUTTON_LEFT;
                    frameCount = 30;
                    dpadAction = true;
                }
                else if(frameCount == 0)
                    dpadAction = true;
                else
                {
                    --frameCount;
                    dpadAction = false;
                }

                if(dpadAction)
                {
                    if(pos >= MAX_FILEBROWSER_LINES)
                        pos -= MAX_FILEBROWSER_LINES;
                    else
                        pos = getListSize(folders) - MAX_FILEBROWSER_LINES;
                    cursor = 0;
                    redraw = true;
                }
            }
        }

        if(vpad.trigger & VPAD_BUTTON_X)
        {
            switch((int)activeDevice)
            {
                case NUSDEV_USB:
                    activeDevice = NUSDEV_SD;
                    break;
                case NUSDEV_SD:
                    activeDevice = NUSDEV_MLC;
                    break;
                case NUSDEV_MLC:
                    activeDevice = usbMounted ? NUSDEV_USB : NUSDEV_SD;
            }
            goto refreshVOlList;
        }

        if(oldHold && !(vpad.hold & (VPAD_BUTTON_UP | VPAD_BUTTON_DOWN | VPAD_BUTTON_LEFT | VPAD_BUTTON_RIGHT)))
            oldHold = 0;
    }

exitFileBrowserMenu:
    destroyList(folders, true);
    return ret ? path : NULL;
}
