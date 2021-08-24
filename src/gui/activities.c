#ifdef ENABLE_GUI
#include"activity.h"
extern struct gui_register guireg_guipm_disk_select;
extern struct gui_register guireg_guipm_partitions;
extern struct gui_register guireg_uefi_bootmenu;
extern struct gui_register guireg_backlight;
extern struct gui_register guireg_logviewer;
extern struct gui_register guireg_language;
extern struct gui_register guireg_filemgr;
extern struct gui_register guireg_reboot;
extern struct gui_register guireg_guiapp;
struct gui_register*guiact_register[]={
	#ifdef ENABLE_UEFI
	&guireg_uefi_bootmenu,
	#else
	#ifdef ENABLE_FDISK
	&guireg_guipm_disk_select,
	&guireg_guipm_partitions,
	#endif
	&guireg_filemgr,
	&guireg_backlight,
	&guireg_logviewer,
	#endif
	&guireg_language,
	&guireg_reboot,
	&guireg_guiapp,
	NULL
};
#endif
