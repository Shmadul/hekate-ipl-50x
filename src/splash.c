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

bool write_splash_to_framebuffer(gfx_con_t * con, char * filename) {
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
        return true;
    }

    // Failure
    return false;
}

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

void print_header(gfx_con_t * con) {
    static const char switchblade[] =
		"SwitchBlade v1.0.5 - By StevenMattera and Shmadul\n\n"
		"Based on the awesome work of naehrwert, st4rk\n"
		"Thanks to: derrek, nedwill, plutoo, shuffle2, smea, thexyz, yellows8\n"
		"Greetings to: fincs, hexkyz, SciresM, Shiny Quagsire, WinterMute\n"
		"Open source and free packages used:\n"
		" - FatFs R0.13a (Copyright (C) 2017, ChaN)\n"
		" - bcl-1.2.0 (Copyright (c) 2003-2006 Marcus Geelnard)\n\n";

	gfx_printf(con, switchblade);
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

    size_t directorySize = strlen(directory) * sizeof(char);
    size_t slashSize = sizeof(char);
    size_t nameSize = strlen(current->name) * sizeof(char);

    // Construct filename.
    char * filename = malloc(directorySize + slashSize + nameSize);
    memcpy(filename, directory, directorySize);
    memcpy(&filename[directorySize], "/", slashSize);
    memcpy(&filename[directorySize + slashSize], current->name, nameSize);

    return filename;
}

void clean_up_file_list(flist_t * head) {
    flist_t * current = head;
    flist_t * next = NULL;

    for (;;) {
        if (next == NULL) {
            return;
        }

        current = next;
        next = current->next;
        free(current);
    }
}

bool draw_splash(gfx_con_t * con) {
    if (write_splash_to_framebuffer(con, "splash.bin") == true) {
        return false;
    }

    flist_t * head = read_splashes_from_directory("splashes");
    if (head == NULL) {
        print_header(con);
        return true;
    }

    char * filename = randomly_choose_splash(head, "splashes");
    write_splash_to_framebuffer(con, filename);

    // Clean up.
    clean_up_file_list(head);
    free(filename);

    return false;
}