#ifndef __W1_DS18B20_H
#define __W1_DS18B20_H

#include <asm/types.h>

#include <linux/device.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/w1.h>

//#include "w1_therm_base.h"

#define W1_THERM_DS18B20	0x28

/*
 * Copyright (c) 2020 Akira Corp. <akira215corp@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


/*      --------------Defines-----------------         */

#define W1_RECALL_EEPROM	0xB8    /* This command should be in public header w1.h but is not */

#define W1_THERM_MAX_TRY		    5	/* Nb of try for an operation */
#define W1_THERM_RETRY_DELAY	    20	/* Delay in ms to retry to acquire bus mutex */


#define W1_THERM_EEPROM_WRITE_DELAY	10	/* Delay in ms to write in EEPROM */

#define EEPROM_CMD_WRITE    "write" /* command to be written in the eeprom sysfs */
#define EEPROM_CMD_READ     "read"  /* to trigger device EEPROM operations */
/*      --------------Structs-----------------         */

/*
 * w1_therm_family_data 
 * rom : data
 * refcnt : ref count
 * external_powered : 1 - device powered externally, 
 *					 0 - device parasite powered, 
 *					-x - error or undefined
 * resolution : resolution in bit of the device, negative value are error code
*/
struct w1_therm_family_data {
	uint8_t rom[9];
	atomic_t refcnt;
	int external_powered;
	int resolution;
};

struct therm_info {
	u8 rom[9];
	u8 crc;
	u8 verdict;
};

/*      --------------Macros-----------------         */

/* return the address of the refcnt in the family data */
#define THERM_REFCNT(family_data) \
	(&((struct w1_therm_family_data *)family_data)->refcnt)

/* return the power mode of the sl slave : 1-ext, 0-parasite, <0 unknown 
	always test family data existance before*/
#define SLAVE_POWERMODE(sl) \
	(((struct w1_therm_family_data *)(sl->family_data))->external_powered)

/* return the resolution in bit of the sl slave : <0 unknown 
	always test family data existance before*/
#define SLAVE_RESOLUTION(sl) \
	(((struct w1_therm_family_data *)(sl->family_data))->resolution)

/*      --------------Interface sysfs-----------------         */

static ssize_t w1_slave_show(struct device *device,
	struct device_attribute *attr, char *buf);

static ssize_t w1_slave_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size);

static ssize_t w1_seq_show(struct device *device,
	struct device_attribute *attr, char *buf);

// ASH new files ///////////////////////////////////////////////////
/*
* temperature_show
* read temperature and return the result in the sys file 
* Main differences with w1_slave :
*	- No hardware check ()
*/
static ssize_t temperature_show(struct device *device,
	struct device_attribute *attr, char *buf);

/** @brief A callback function to output the power mode of the device
 *	Ask the device to get its powering mode
 * 	Once done, it is stored in the sl->family_data to avoid doing the test
 * 	during data read. Negative results are kernel error code
 *  @param device represents the device
 *  @param attr the pointer to the kobj_attribute struct
 *  @param buf the buffer to which to write the resolution
 *  @return return the total number of characters written to the buffer (excluding null)
 */
static ssize_t ext_power_show(struct device *device,
	struct device_attribute *attr, char *buf);

/** @brief A callback function to output the resolution of the device
 *  @param device represents the device
 *  @param attr the pointer to the kobj_attribute struct
 *  @param buf the buffer to which to write the resolution
 *  @return return the total number of characters written to the buffer (excluding null)
 */
static ssize_t resolution_show(struct device *device,
	struct device_attribute *attr, char *buf);

/** @brief A callback function to store the user resolution in the device RAM
 *  @param device represents the device
 *  @param attr the pointer to the kobj_attribute struct
 *  @param buf the buffer from which to read resolution to be set
 *  @param size the number characters in the buffer
 *  @return return should return the total number of characters used from the buffer
 */
static ssize_t resolution_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size);

/** @brief A callback function to let the user read/write device EEPROM
 *  @param device represents the device
 *  @param attr the pointer to the kobj_attribute struct
 *  @param buf the buffer from which the instruction (direction) will be read
 *              'write' -> device write RAM to EEPROM, 
 *              'read' -> device read EEPROM and put to RAM
 *  @param size the number characters in the buffer
 *  @return return should return the total number of characters used from the buffer
 */
static ssize_t eeprom_store(struct device *device,
	struct device_attribute *attr, const char *buf, size_t size);

/*      --------------Attributes declarations-----------------         */

static DEVICE_ATTR_RW(w1_slave);
static DEVICE_ATTR_RO(w1_seq);

static DEVICE_ATTR_RO(ext_power);
static DEVICE_ATTR_RW(resolution); // TODO implement
static DEVICE_ATTR_RO(temperature);
//static DEVICE_ATTR_RW(alarms);	// TODO implement
static DEVICE_ATTR_WO(eeprom);	// TODO implement


/*      --------------- Helpers Functions-----------------         */

/* get_convertion_time() get the Tconv fo the device
 * @param: lock: w1 bus mutex to get
 * return value : true is mutex is acquired and lock, false otherwise
*/
extern bool w1_get_bus_mutex_lock(struct mutex *);

/* get_convertion_time() get the Tconv fo the device
 * @param: sl: device to get the conversion time
 * return value : positive value is conversion time in ms, negative values kernel error code otherwise
*/
static inline int get_convertion_time(struct w1_slave *sl);

/*      ---------------Hardware Functions-----------------         */

/**
 * reset_select_slave() - reset and select a slave
 * @sl:		the slave to select
 *
 * Resets the bus and then selects the slave by sending either a ROM MATCH.  
 * w1_reset_select_slave() from w1_io.c could not be used
 * here because a SKIP ROM command is sent if only one device is on the line.
 * At the beginning of the such process, sl->master->slave_count is 1 even if 
 * more devices are on the line, causing collision on the line.
 * The w1 master lock must be held.
 *
 * Return:	0 if success, negative kernel error code otherwise
 */
static int reset_select_slave(struct w1_slave *sl);

/* read_scratchpad()
 * @sl: 	pointer to the slave to read
 * @info: 	pointer to a structure to store the read results
 * return value: 0 if success, -kernel error code otherwise
 */
static int read_scratchpad(struct w1_slave *sl, struct therm_info *info);

/* write_scratchpad()
 * @sl: 				pointer to the slave to read
 * @data: 				pointer to an array of 3 bytes, as 3 bytes MUST be written
 * @pullup_duration: 	duration in ms of pullup, only for parasited powered devices
 * return value: 0 if success, -kernel error code otherwise
 */
static int write_scratchpad(struct w1_slave *sl, const u8 *data);

/* convert_t()
 * @sl: 				pointer to the slave to read
 * @info: 	pointer to a structure to store the read results
 * return value: 0 if success, -kernel error code otherwise
 */
static int convert_t(struct w1_slave *sl, struct therm_info *info);

/* copy_scratchpad() - Copy the content of scratchpad in device EEPROM
 * @sl:		slave involved
 * return value : 0 if success, -kernel error code otherwise
 */
static int copy_scratchpad(struct w1_slave *sl);

/* recall_eeprom() - retrieve EEPROM data to device RAM 
 * @sl:		slave involved
 * return value : 0 if success, -kernel error code otherwise
 */
static int recall_eeprom(struct w1_slave *sl);

/* read_powermode() - Ask the device to get its power mode {external, parasite}
 * @sl:		slave to be interrogated
 * return value :
 * 0 - parasite powered device
 * 1 - externally powered device
 * <0 - kernel error code
 */
static int read_powermode(struct w1_slave *sl);

/*      ---------------Interface Functions-----------------         */

/* w1_therm_add_slave() - Called each time a search discover a new device
 * used to initialized slave (family datas)
 * @sl:	slave just discovered
 * return value : 0 - If success, negative kernel code otherwise
 */
static int w1_therm_add_slave(struct w1_slave *sl);

/* w1_therm_remove_slave() - Called each time a slave is removed
 * used to free memory
 * @sl:	slave to be removed
 */
static void w1_therm_remove_slave(struct w1_slave *sl);

/* w1_DS18B20_set_resolution() write new resolution to the RAM device
 * @param: slave: device to set the resolution
 * @param: val: new resolution in bit [9..12]
 * return value : 0 if success, negative kernel error code otherwise
*/
static inline int w1_DS18B20_set_resolution(struct w1_slave *sl, int val);

/* w1_DS18S20_set_resolution() write new resolution to the RAM device
 * @param: device: device to set the resolution
 * @param: val: new resolution in bit [9..12]
 * return value : 0 if success, negative kernel error code otherwise
*/
static inline int w1_DS18S20_set_resolution(struct w1_slave *sl, int val);

/* w1_DS18B20_get_resolution() read the device RAM to get its resolution setting
 * @param: device: device to get the resolution form
 * return value : resolution in bit [9..12] or negative kernel error code
*/
static inline int w1_DS18B20_get_resolution(struct w1_slave *sl);

/* w1_DS18B20_get_resolution() read the device RAM to get its resolution setting
 * @param: device: device to get the rsolution form
 * return value : resolution in bit [9..12] or negative kernel error code
*/
static inline int w1_DS18S20_get_resolution(struct w1_slave *sl);

#endif  /* __W1_DS18B20_H */