#ifndef _CONFIG_PARSE_INC_
#define _CONFIG_PARSE_INC_

#include <linux/module.h>
#include <linux/fs.h>

static inline int __init conf_toupper(int c){

if( c>='a' && c<='z') c += ('A' - 'a');
        return c;

}

extern int __init config_file_get_line  (struct file * f  , char * buf, int bsz   ,loff_t * pos);
extern int __init config_line_get_param (const char  * buf, int vcount, const char **vnames, int * results);


#endif

