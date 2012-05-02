//------------------------------------------------------------------------------
// Copyright (C) 2012, Alexander Rössler
// All rights reserved.
//
//
//------------------------------------------------------------------------------

/**
 * @file   mbusread.h
 * 
 * @brief  main file od mbusread
 *
 */

#ifndef _MBUSREAD_H_
#define _MBUSREAD_H_

#define PARAM_F 0
#define PARAM_X 1

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <inttypes.h>

#include <mbus/mbus-protocol.h>
#include <mbus/mbus.h>
#include "mbus_csv.h"

void process_hexdump(char* hexdumpfile_name);
void process_config(char* configfile_name);
mbus_handle* connect_device(char* device, uint baudrate);
int request_primary(mbus_handle* handle, uint address);
int request_secondary(mbus_handle* handle, uint address);
int receive_frame(mbus_handle* handle, FILE *file, uint address);
int compress_file(char* filename, char* zip_filename);
char* generate_filename();
void log_error(char* error_msg, const char* function_name);
int join_files(char* old_filename, char* new_filename);

#endif