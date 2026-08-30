/*
* @file general_config.h
* @brief General configuration file.
*
* This file contains the general configuration parameters for the bootloader.
*/

#ifndef GENERAL_CONFIG_H
#define GENERAL_CONFIG_H


#define BOOTLOADER_VERSION_MAJOR 0
#define BOOTLOADER_VERSION_MINOR 0  
#define BOOTLOADER_VERSION_PATCH 1

#define BOOTLOADER_VERSION_STRING BOOTLOADER_VERSION_MAJOR "." BOOTLOADER_VERSION_MINOR "." BOOTLOADER_VERSION_PATCH


#define BOOTLOADER_COMM_USE_USB ///< @todo need to move to CMAKE
/* Select the bootloader transport from CMake via BOOTLOADER_COMM_TRANSPORT. */
#if defined(BOOTLOADER_COMM_USE_USB)
#define BOOTLOADER_COMM_TRANSPORT_USB 1U
#elif defined(BOOTLOADER_COMM_USE_UART)
#define BOOTLOADER_COMM_TRANSPORT_UART 1U
#endif

#endif // GENERAL_CONFIG_H