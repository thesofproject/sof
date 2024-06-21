/* This is a Zephyr linker "snippet"
 * See zephyr_linker_sources() cmake function
 */

/* Runtime UUID table. */
ITERABLE_SECTION_ROM(sof_uuid_entry, 4)
