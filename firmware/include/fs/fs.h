#pragma once

#include <LittleFS.h>
#include "log/logger.h"

/// @brief Initializes the LittleFS file system
/// @return Whether or not initialization failed
bool fs_init();

/// @brief Checks if a file (`name`) exists in LittleFS
/// @param name
/// @return Whether or not the file exists
bool fs_exists(const char* name);

/// @brief Reads a file (`name`) from LittleFS and returns it as a char*; Must be freed after use
/// @param name
/// @return A cstring representing all of the data inside of the file
const char* fs_read(const char* name, bool terminate = true);

/// @brief Writes `content` to a file (`name`) in LittleFS; If the file already exists, it will be
/// overwritten
/// @param name
/// @param content
/// @param unsafe Whether or not to write the file, even if the file contains blank content; If
/// false, the file will not be written if `content` is empty
/// @return Whether or not the file was successfully written
bool fs_write(const char* name, const char* content, bool unsafe = false);