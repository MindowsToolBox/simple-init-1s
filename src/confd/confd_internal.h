#ifndef CONFD_INTERNAL_H
#define CONFD_INTERNAL_H
#include<inttypes.h>
#include"list.h"
#include"confd.h"

#define CONFD_MAGIC0 0xEF
#define CONFD_MAGIC1 0x66

// initconfd remote action
enum confd_action{
	CONF_OK           =0xAC00,
	CONF_FAIL         =0xAC01,
	CONF_QUIT         =0xAC02,
	CONF_DELETE       =0xAC03,
	CONF_DUMP         =0xAC04,
	CONF_LIST         =0xAC05,
	CONF_GET_TYPE     =0xAC06,
	CONF_GET_STRING   =0xAC21,
	CONF_GET_INTEGER  =0xAC22,
	CONF_GET_BOOLEAN  =0xAC23,
	CONF_SET_STRING   =0xAC41,
	CONF_SET_INTEGER  =0xAC42,
	CONF_SET_BOOLEAN  =0xAC43,
};

// initconfd message
struct confd_msg{
	unsigned char magic0:8,magic1:8;
	enum confd_action action:16;
	char path[4084-(MAX(
		MAX(sizeof(size_t),sizeof(int64_t)),
		MAX(sizeof(enum conf_type),sizeof(bool))
	))];
	int code:32;
	union{
		size_t data_len;
		enum conf_type type;
		int64_t integer;
		bool boolean;
	}data;
};

// config struct
struct conf{
	char name[255];
	enum conf_type type;
	struct conf*parent;
	union{
		list*keys;
		union{
			char*string;
			int64_t integer;
			bool boolean;
		}value;
	};
};

// src/confd/client.c: current confd fd
extern int confd;

// src/confd/internal.c: check message magick
extern bool confd_internal_check_magic(struct confd_msg*msg);

// src/confd/internal.c: check message magick
extern void confd_internal_init_msg(struct confd_msg*msg,enum confd_action action);

// src/confd/internal.c: send message
extern int confd_internal_send(int fd,struct confd_msg*msg);

// src/confd/internal.c: send code
extern int confd_internal_send_code(int fd,enum confd_action action,int code);

// src/confd/internal.c: read message
extern int confd_internal_read_msg(int fd,struct confd_msg*buff);

// src/confd/internal.c: convert action to string
extern const char*confd_action2name(enum confd_action action);

// src/confd/dump.c: dump config store to loggerd
extern int conf_dump_store();

// src/confd/store.c: get config store root struct
extern struct conf*conf_get_store();

// src/confd/store.c: convert config item type to string
extern const char*conf_type2string(enum conf_type type);

// src/confd/store.c: get config item type by path
extern enum conf_type conf_get_type(const char*path);

// src/confd/store.c: get config item type string by path
extern const char*conf_get_type_string(const char*path);

// src/confd/store.c: list config item keys
extern const char**conf_ls(const char*path);

// src/confd/store.c: delete config item and all children
extern int conf_del(const char*path);

// config item value
#define VALUE_STRING(conf)conf->value.string
#define VALUE_BOOLEAN(conf)conf->value.boolean
#define VALUE_INTEGER(conf)conf->value.integer

// src/confd/store.c: get or set config item value
#define DECLARE_CONF_GET_SET(_tag,_type,_func) \
	extern int conf_set_##_func(const char*path,_type data);\
	extern _type conf_get_##_func(const char*path,_type def);

DECLARE_CONF_GET_SET(STRING,char*,string)
DECLARE_CONF_GET_SET(INTEGER,int64_t,integer)
DECLARE_CONF_GET_SET(BOOLEAN,bool,boolean)

#endif
