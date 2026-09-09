/* 
 *    Programmed By: Mohammed Isam [mohammed_isam1984@yahoo.com]
 *    Copyright 2026 (c)
 * 
 *    file: procfs_acpi.c
 *    This file is part of LaylaOS.
 *
 *    LaylaOS is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    LaylaOS is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with LaylaOS.  If not, see <http://www.gnu.org/licenses/>.
 */    

/**
 *  \file procfs_acpi.c
 *
 *  This file implements some procfs filesystem functions (mainly the ones
 *  used to read files in the /proc/acpi subdirectory).
 *  Functions implementing filesystem operations are exported to the rest of
 *  the kernel via the \ref procfs_ops structure, which is defined in procfs.c.
 */

//#define __DEBUG

#include <errno.h>
#include <string.h>
#include <kernel/laylaos.h>
#include <kernel/acpi.h>
#include <kernel/ksymtab.h>
#include <mm/kheap.h>
#include <fs/procfs.h>

/*
 * Read /proc/acpi/BAT0/charge_full.
 */
size_t get_bat_charge_full(char **buf, void *arg)
{
    struct one_battery_info_t *batinfo = arg;

    PR_MALLOC(*buf, 32);
    ksprintf(*buf, 32, "%llu\n", batinfo->full_capacity);

    return strlen(*buf);
}


/*
 * Read /proc/acpi/BAT0/charge_full_design.
 */
size_t get_bat_charge_full_design(char **buf, void *arg)
{
    struct one_battery_info_t *batinfo = arg;

    PR_MALLOC(*buf, 32);
    ksprintf(*buf, 32, "%llu\n", batinfo->design_capacity);

    return strlen(*buf);
}


/*
 * Read /proc/acpi/BAT0/charge_now.
 */
size_t get_bat_charge_now(char **buf, void *arg)
{
    struct one_battery_info_t *batinfo = arg;

    PR_MALLOC(*buf, 32);
    ksprintf(*buf, 32, "%llu\n", batinfo->capacity);

    return strlen(*buf);
}


/*
 * Read /proc/acpi/BAT0/capacity.
 */
size_t get_bat_capacity(char **buf, void *arg)
{
    struct one_battery_info_t *batinfo = arg;
    uint64_t cap;

    PR_MALLOC(*buf, 32);

    cap = (batinfo->capacity / batinfo->full_capacity) * 100;
    ksprintf(*buf, 32, "%llu\n", cap);

    return strlen(*buf);
}


/*
 * Read /proc/acpi/BAT0/model_name.
 */
size_t get_bat_model_name(char **buf, void *arg)
{
    struct one_battery_info_t *batinfo = arg;

    PR_MALLOC(*buf, 128);
    ksprintf(*buf, 128, "%s\n", batinfo->model);

    return strlen(*buf);
}


/*
 * Read /proc/acpi/BAT0/serial_number.
 */
size_t get_bat_serial_number(char **buf, void *arg)
{
    struct one_battery_info_t *batinfo = arg;

    PR_MALLOC(*buf, 128);
    ksprintf(*buf, 128, "%s\n", batinfo->serial);

    return strlen(*buf);
}


/*
 * Read /proc/acpi/BAT0/manufacturer.
 */
size_t get_bat_manufacturer(char **buf, void *arg)
{
    struct one_battery_info_t *batinfo = arg;

    PR_MALLOC(*buf, 128);
    ksprintf(*buf, 128, "%s\n", batinfo->oem_info);

    return strlen(*buf);
}


/*
 * Read /proc/acpi/BAT0/technology.
 */
size_t get_bat_technology(char **buf, void *arg)
{
    struct one_battery_info_t *batinfo = arg;

    PR_MALLOC(*buf, 32);
    ksprintf(*buf, 32, "%s\n", batinfo->technology);

    return strlen(*buf);
}


/*
 * Read /proc/acpi/BAT0/type.
 */
size_t get_bat_type(char **buf, void *arg)
{
    UNUSED(arg);

    PR_MALLOC(*buf, 32);
    ksprintf(*buf, 32, "%s\n", "Battery");

    return strlen(*buf);
}


/*
 * Read /proc/acpi/BAT0/present.
 */
size_t get_bat_present(char **buf, void *arg)
{
    struct one_battery_info_t *batinfo = arg;

    PR_MALLOC(*buf, 32);
    ksprintf(*buf, 32, "%d\n", batinfo->is_present);

    return strlen(*buf);
}


/*
 * Read /proc/acpi/BAT0/cycle_count.
 */
size_t get_bat_cycle_count(char **buf, void *arg)
{
    struct one_battery_info_t *batinfo = arg;

    PR_MALLOC(*buf, 32);
    ksprintf(*buf, 32, "%llu\n", batinfo->cycle_count);

    return strlen(*buf);
}


/*
 * Read /proc/acpi/BAT0/status.
 */
size_t get_bat_status(char **buf, void *arg)
{
    struct one_battery_info_t *batinfo = arg;
    char *status;

    PR_MALLOC(*buf, 32);

    if(batinfo->state & ACPI_BATTERY_STATE_DISCHARGING)
    {
        status = "Discharging";
    }
    else if(batinfo->state & ACPI_BATTERY_STATE_CHARGING)
    {
        status = "Charging";
    }
    else if(batinfo->state & ACPI_BATTERY_STATE_CRITICAL)
    {
        status = "Critical";
    }
    else
    {
        status = "Unknown";
    }

    ksprintf(*buf, 32, "%s\n", status);

    return strlen(*buf);
}


/*
 * Read /proc/acpi/BAT0/info.
 */
size_t get_bat_info(char **buf, void *arg)
{
    struct one_battery_info_t *batinfo = arg;
    char *p;
    char *power_unit = batinfo->power_unit ? "mAh" : "mWh";

    PR_MALLOC(*buf, 4096);
    p = *buf;

    ksprintf(p, 4096, "present:                yes\n");
    p += strlen(p);

    ksprintf(p, 4096, "design capacity:        %llu %s\n", batinfo->design_capacity, power_unit);
    p += strlen(p);

    ksprintf(p, 4096, "last full capacity:     %llu %s\n", batinfo->full_capacity, power_unit);
    p += strlen(p);

    ksprintf(p, 4096, "battery technology:     %s\n", 
                batinfo->is_rechargeable ? "rechargeable" : "non-rechargeable");
    p += strlen(p);

    ksprintf(p, 4096, "design voltage:         %llu mV\n", batinfo->design_voltage);
    p += strlen(p);

    ksprintf(p, 4096, "design capacity waning: %llu %s\n", batinfo->design_capacity_warn, power_unit);
    p += strlen(p);

    ksprintf(p, 4096, "design capacity low:    %llu %s\n", batinfo->design_capacity_low, power_unit);
    p += strlen(p);

    ksprintf(p, 4096, "cycle count:            %llu\n", batinfo->cycle_count);
    p += strlen(p);

    ksprintf(p, 4096, "capacity granularity 1: %llu %s\n", batinfo->capacity_gran[0], power_unit);
    p += strlen(p);

    ksprintf(p, 4096, "capacity granularity 2: %llu %s\n", batinfo->capacity_gran[1], power_unit);
    p += strlen(p);

    ksprintf(p, 4096, "model number:           %s\n", batinfo->model);
    p += strlen(p);

    ksprintf(p, 4096, "serial number:          %s\n", batinfo->serial);
    p += strlen(p);

    ksprintf(p, 4096, "battery type:           %s\n", batinfo->technology);
    p += strlen(p);

    ksprintf(p, 4096, "OEM info:               %s\n", batinfo->oem_info);
    p += strlen(p);

    return (p - *buf);
}


size_t get_gpe(char **buf, void *arg)
{
    int gpe = (int)(uintptr_t)arg;

    PR_MALLOC(*buf, 256);

    /*
     * Force gcc to ignore the "void * to function pointer cast" warning
     */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

    void (*acpifunc)();

    if((acpifunc = ksym_value("acpi_get_gpe")))
    {
        acpifunc(*buf, (size_t)256, gpe);
    }

#pragma GCC diagnostic pop

    return strlen(*buf);
}

