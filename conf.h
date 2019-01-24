#ifndef _CONF_H
#define _CONF_H

#include "sha1.h"

#ifdef WIN32

#define MKDIR(p)		mkdir(p)

#else

#define O_BINARY		0
#define MKDIR(p)		mkdir(p, 0755)

#endif

#define HASH_SIZE		(20)		//160
#define HASH_CTX		SHA1_CTX
#define HASH_Init		SHA1_Init
#define HASH_Update		SHA1_Update
#define HASH_Final		SHA1_Final


#define ENABLE_DEL		(1)
#define ENABLE_FINGERPRINT	(1)
#define FINGERPRINT_NAME	"_fingerprint"



#endif

