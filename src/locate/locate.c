/*
 *
 * Copyright (C) 2021 BigfootACA <bigfoot@classfun.cn>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 */

#include "Library/PrintLib.h"
#include"internal.h"
static list*locate_cache=NULL;

static void init_locate(locate_dest*loc,const char*tag,EFI_HANDLE*hand){
	ZeroMem(loc,sizeof(locate_dest));
	AsciiStrCpyS(loc->tag,sizeof(loc->tag)-1,tag);
	loc->file_hand=hand;
	gBS->HandleProtocol(
		hand,
		&gEfiPartitionInfoProtocolGuid,
		(VOID**)&loc->part_proto
	);
	gBS->HandleProtocol(
		hand,
		&gEfiSimpleFileSystemProtocolGuid,
		(VOID**)&loc->file_proto
	);
	gBS->HandleProtocol(
		hand,
		&gEfiBlockIoProtocolGuid,
		(VOID**)&loc->block_proto
	);
	if(loc->file_proto)loc->file_proto->OpenVolume(
		loc->file_proto,
		&loc->root
	);
}

static locate_dest*new_locate(const char*tag,EFI_HANDLE*hand){
	locate_dest*loc=malloc(sizeof(locate_dest));
	if(!loc)return NULL;
	init_locate(loc,tag,hand);
	return loc;
}

static int init_default(){
	EFI_LOADED_IMAGE_PROTOCOL*li=NULL;
	EFI_STATUS st=gBS->OpenProtocol(
		gImageHandle,
		&gEfiLoadedImageProtocolGuid,
		(VOID**)&li,
		gImageHandle,
		NULL,
		EFI_OPEN_PROTOCOL_GET_PROTOCOL
	);
	if(EFI_ERROR(st)){
		tlog_error(
			"open protocol failed: %s",
			efi_status_to_string(st)
		);
		return -1;
	}
	locate_dest*loc=new_locate("default",li->DeviceHandle);
	if(!loc){
		tlog_error("create default locate failed");
		return -1;
	}
	list_obj_add_new(&locate_cache,loc);
	return 0;
}

static bool try_protocol(const char*tag,locate_dest*loc,EFI_GUID*protocol){
	UINTN cnt=0;
	EFI_STATUS st;
	EFI_HANDLE*hands=NULL;
	CHAR16*dpt=NULL;
	char dpx[PATH_MAX],guid[64];
	EFI_DEVICE_PATH_PROTOCOL*dp=NULL;
	st=gBS->LocateHandleBuffer(
		ByProtocol,
		protocol,
		NULL,&cnt,&hands
	);
	if(EFI_ERROR(st)){
		AsciiSPrint(guid,sizeof(guid),"%g",protocol);
		tlog_warn(
			"locate protocol %s failed: %s",
			guid,efi_status_to_string(st)
		);
		return false;
	}
	for(UINTN i=0;i<cnt;i++){
		init_locate(loc,tag,hands[i]);
		for(UINTN s=0;locate_matches[s];s++){
			switch(locate_matches[s](loc)){
				case MATCH_NONE:
				case MATCH_SKIP:
				case MATCH_SUCCESS:continue;
				case MATCH_INVALID:return false;
				case MATCH_FAILED:goto match_fail;
			}
		}
		ZeroMem(dpx,sizeof(dpx));
		if(
			(dp=DevicePathFromHandle(hands[i]))&&
			(dpt=ConvertDevicePathToText(dp,TRUE,FALSE))
		){
			UnicodeStrToAsciiStrS(dpt,dpx,sizeof(dpx)-1);
			FreePool(dpt);
		}
		if(!dpx[0])AsciiStrCpyS(dpx,sizeof(dpx)-1,"(Unknown)");
		tlog_info("found locate %s",dpx);
		return true;
		match_fail:;
	}
	return false;
}

static locate_dest*load_locate(const char*tag){
	locate_dest*loc=AllocatePool(sizeof(locate_dest));
	if(!loc)return NULL;
	if(try_protocol(tag,loc,&gEfiPartitionInfoProtocolGuid))return loc;
	if(try_protocol(tag,loc,&gEfiSimpleFileSystemProtocolGuid))return loc;
	if(try_protocol(tag,loc,&gEfiBlockIoProtocolGuid))return loc;
	if(loc)FreePool(loc);
	return NULL;

}

static bool locate_cmp(list*l,void*d){
	LIST_DATA_DECLARE(x,l,locate_dest*);
	return d&&x&&AsciiStrCmp(x->tag,(char*)d)==0;
}

static locate_dest*get_locate(const char*tag){
	list*l;
	if(!tag||!*tag)return NULL;
	if(!locate_cache)init_default();
	if((l=list_search_one(locate_cache,locate_cmp,(void*)tag)))
		return LIST_DATA(l,locate_dest*);
	if(confd_get_type_base(BASE,tag)!=TYPE_KEY)
		return NULL;
	locate_dest*loc=load_locate(tag);
	if(loc)list_obj_add_new(&locate_cache,loc);
	return loc;
}

static void dump_block_info(char*tag,EFI_BLOCK_IO_PROTOCOL*bi){
	char buf[64];
	if(!tag||!bi||!bi->Media)return;
	EFI_BLOCK_IO_MEDIA*m=bi->Media;
	UINT64 size=m->BlockSize*(m->LastBlock+1);
	tlog_verbose("locate %s block media id: %d",tag,m->MediaId);
	tlog_verbose("locate %s block read only: %s",tag,BOOL2STR(m->ReadOnly));
	tlog_verbose("locate %s block sector size: %d",tag,m->BlockSize);
	tlog_verbose("locate %s block total sectors: %lld",tag,m->LastBlock+1);
	tlog_verbose(
		"locate %s block total size: %lld (%s)",tag,size,
		make_readable_str_buf(buf,sizeof(buf),size,1,0)
	);
}

static void dump_fs_info(char*tag,EFI_FILE_PROTOCOL*root){
	UINTN bs=0;
	EFI_STATUS st;
	EFI_FILE_SYSTEM_INFO*fi=NULL;
	char buf[64],xn[PATH_MAX];
	if(!tag||!root)return;
	st=root->GetInfo(root,&gEfiFileSystemInfoGuid,&bs,fi);
	if(st==EFI_BUFFER_TOO_SMALL&&(fi=AllocateZeroPool(bs)))
		st=root->GetInfo(root,&gEfiFileSystemInfoGuid,&bs,fi);
	if(EFI_ERROR(st))return;
	ZeroMem(xn,sizeof(xn));
	UnicodeStrToAsciiStrS(fi->VolumeLabel,xn,sizeof(xn));
	tlog_verbose("locate %s file system sector size: %d",tag,fi->BlockSize);
	tlog_verbose("locate %s file system read only: %s",tag,BOOL2STR(fi->ReadOnly));
	tlog_verbose("locate %s file system volume label: %s",tag,xn);
	tlog_verbose(
		"locate %s file system total size: %lld (%s)",tag,fi->VolumeSize,
		make_readable_str_buf(buf,sizeof(buf),fi->VolumeSize,1,0)
	);
	tlog_verbose(
		"locate %s file system free space: %lld (%s)",tag,fi->FreeSpace,
		make_readable_str_buf(buf,sizeof(buf),fi->FreeSpace,1,0)
	);
}

static void dump_locate(locate_dest*loc){
	if(!loc||loc->dump)return;
	if(loc->root)dump_fs_info(loc->tag,loc->root);
	if(loc->block_proto)dump_block_info(loc->tag,loc->block_proto);
	loc->dump=true;
}

static locate_dest*init_locate_ret(locate_ret*ret,const char*file){
	locate_dest*loc;
	char buf[PATH_MAX],*tag=NULL,*path=NULL;
	if(!ret||!file||!*file)return NULL;
	ZeroMem(ret,sizeof(locate_ret));
	ZeroMem(buf,sizeof(buf));
	AsciiStrCpyS(buf,sizeof(buf)-1,file);
	ret->type=LOCATE_FILE;
	if(buf[0]=='@'){
		tag=buf+1;
		char*d=strchr(buf,':');
		if(d)*d=0,path=d+1;
	}else if(buf[0]=='#'){
		tag=buf+1;
		tlog_info("tag %s",tag);
		ret->type=LOCATE_BLOCK;
	}else path=buf,tag="default";
	AsciiStrCpyS(
		ret->tag,
		sizeof(ret->tag)-1,
		tag
	);
	if(path)AsciiStrCpyS(
		ret->path,
		sizeof(ret->path)-1,
		path
	);
	if(!(loc=get_locate(tag)))return NULL;
	dump_locate(loc);
	switch(ret->type){
		case LOCATE_FILE:
			ret->hand=loc->file_hand;
			ret->fs=loc->file_proto;
			ret->root=loc->root;
		break;
		case LOCATE_BLOCK:
			ret->block=loc->block_proto;
		break;
		default:;
	}
	ret->part=loc->part_proto;
	return loc;
}

static bool locate_open_file(locate_dest*loc,locate_ret*ret,UINT64 mode){
	EFI_STATUS st;
	if(!ret->path[0]){
		tlog_warn("no path specified");
		return false;
	}
	if(!loc->root){
		tlog_warn(
			"locate '%s' have no file system",
			ret->tag
		);
		return false;
	}
	ZeroMem(ret->path16,sizeof(ret->path16));
	AsciiStrToUnicodeStrS(
		ret->path,ret->path16,
		sizeof(ret->path16)/sizeof(CHAR16)
	);
	st=loc->root->Open(loc->root,&ret->file,ret->path16,mode,0);
	if(EFI_ERROR(st)){
		tlog_error(
			"open file '%s' failed: %s",
			ret->path,efi_status_to_string(st)
		);
		return false;
	}
	if(!ret->file){
		tlog_error(
			"open file '%s' failed",
			ret->path
		);
		return false;
	}
	if(!(ret->device=FileDevicePath(loc->file_hand,ret->path16))){
		tlog_error(
			"get file '%s' device path failed",
			ret->path
		);
		return false;
	}
	return true;
}

static bool locate_open_block(locate_dest*loc,locate_ret*ret){
	if(!(ret->device=DevicePathFromHandle(loc->file_hand))){
		tlog_error(
			"get block '%s' device path failed",
			ret->tag
		);
		return false;
	}
	return true;
}

bool boot_locate(locate_ret*ret,const char*file){
	locate_dest*loc=init_locate_ret(ret,file);
	if(loc)switch(ret->type){
		case LOCATE_FILE:return locate_open_file(loc,ret,EFI_FILE_MODE_READ);
		case LOCATE_BLOCK:return locate_open_block(loc,ret);
		default:;
	}
	return false;
}

bool boot_locate_create_file(locate_ret*ret,const char*file){
	locate_dest*loc=init_locate_ret(ret,file);
	return loc&&ret->type==LOCATE_FILE&&locate_open_file(loc,ret,
		EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE
	);
}

char*locate_find_name(char*buf,size_t len){
	CHAR8 name[255];
	ZeroMem(buf,len);
	for(int i=0;i<4096;i++){
		ZeroMem(name,sizeof(name));
		AsciiSPrint(name,sizeof(name),"auto-%d",i);
		if(confd_get_type_base("locates",name)==TYPE_KEY)continue;
		AsciiStrCpyS(buf,len-1,name);
		return buf;
	}
	tlog_warn("no available locate name found");
	return NULL;
}

bool locate_add_by_device_path(char*tag,bool save,EFI_DEVICE_PATH_PROTOCOL*dp){
	CHAR16*dpt=NULL;
	CHAR8 xpt[PATH_MAX];
	if(!tag||!dp||get_locate(tag))return false;
	if(!(dpt=ConvertDevicePathToText(dp,FALSE,FALSE)))return false;
	confd_add_key_base(BASE,tag);
	confd_set_save_base(BASE,tag,save);
	ZeroMem(xpt,sizeof(xpt));
	UnicodeStrToAsciiStrS(dpt,xpt,sizeof(xpt));
	confd_set_string_dict(BASE,tag,"by_device_path",xpt);
	FreePool(dpt);
	return true;
}

bool locate_auto_add_by_device_path(char*buf,size_t len,EFI_DEVICE_PATH_PROTOCOL*dp){
	return locate_add_by_device_path(locate_find_name(buf,len),false,dp);
}
