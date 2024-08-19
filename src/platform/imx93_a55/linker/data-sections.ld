SECTION_PROLOGUE(.static_uuid_entries,,)
{
	*(*.static_uuids)
} GROUP_ROM_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)

SECTION_PROLOGUE(.static_log_entries,,)
{
	*(*.static_log*)
} GROUP_ROM_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)

SECTION_PROLOGUE(.fw_metadata,,)
{
	KEEP (*(*.fw_metadata))
	. = ALIGN(16);
} GROUP_ROM_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)
