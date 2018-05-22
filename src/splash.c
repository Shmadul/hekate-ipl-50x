/*
* Copyright (c) 2018 StevenMattera
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <string.h>

#include "splash.h"
#include "ff.h"
#include "util.h"

/* Size of the Splash Screen. */
const int SPLASH_SIZE = 3932169;

/* Linked List for the Splash Screens. */
typedef struct flist {
    TCHAR name[FF_LFN_BUF + 1];
    struct flist * next;
    int * length;
} flist_t;

flist_t * read_splashes_from_directory(char * directory) {
    int numberOfSplashes = 0;
    flist_t * head = NULL;
    flist_t * current = NULL;
    FRESULT res;
    FILINFO fno;
    DIR dp;

    // Open the directory
    res = f_opendir(&dp, directory);
    if (res != FR_OK) {
        return NULL;
    }

    // Loop through the files in the folder.
    for (;;) {
        res = f_readdir(&dp, &fno);

        // Break the loop
        if (res != FR_OK || fno.fname[0] == 0) {
            break;
        }
        // Skip directories.
        else if (fno.fattrib & AM_DIR) {
            continue;
        }

        if (current == NULL) {
            head = malloc(sizeof(flist_t));
            current = head;
        } else {
            current->next = malloc(sizeof(flist_t));
            current = current->next;
        }

        memcpy(current->name, fno.fname, sizeof(fno.fname));
        current->length = &numberOfSplashes;
        numberOfSplashes++;
    }

    // Clean up.
    f_closedir(&dp);

    return head;
}

char * randomly_choose_splash(flist_t * head, char * directory) {
    flist_t * current = NULL;

    // Seed the randomization and choose our lucky winner.
    srand(get_tmr());
    int splashChosen = rand() % *head->length;

    // Go through our linked list and get the chosen splash.
    for (int i = 0; i <= splashChosen; i++) {
        if (current == NULL) {
            current = head;
        } else {
            current = current->next;
        }
    }

    // Construct filename.
    char filename[sizeof(current->name) + 9];
    strcpy(filename, directory);
    strcat(filename, "/");
    strcat(filename, current->name);
}

int write_splash_to_framebuffer(gfx_con_t * con, char * filename) {
    FIL fp;
    char buffer[SPLASH_SIZE + 1];

    // Open the file.
    if (f_open(&fp, filename, FA_READ) == FR_OK) {
        // Read the file to the buffer.
	    f_read(&fp, buffer, SPLASH_SIZE, NULL);

        // Copy the splash from our buffer to the framebuffer.
        memcpy(con->gfx_ctxt->fb, buffer + 1, SPLASH_SIZE);

        // Clean up.
        f_close(&fp);
        
        // Success
        return 1;
    }

    // Failure
    return 0;
}

void draw_splash(gfx_con_t * con) {
    if (write_splash_to_framebuffer(con, "splash.bin") == 1) {
        return;
    }

    flist_t * head = read_splashes_from_directory("splashes");
    if (head == NULL) {
        return;
    }

    char * filename = randomly_choose_splash(head, "splashes");
    write_splash_to_framebuffer(con, filename);
}