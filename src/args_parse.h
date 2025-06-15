//
// Created by Michal Pitner on 14.06.2025.
//

#ifndef ARGS_PARSE_H
#define ARGS_PARSE_H

#include "../lib/CLibs/src/structs/dynarr.h" /* List */
#include "../lib/CLibs/src/structs/sets.h"   /* Set */


#define DEPTH_OPT   "--depth="
#define HELP_OPT    "--help"
#define EXCLUDE_OPT "--exclude="


#define COLUMN_UTF "│"
#define ROW_UTF    "─"
#define CORNER_UTF "└"
#define JOINT_UTF  "├"

#define COLUMN_ASCII "|"
#define ROW_ASCII    "-"
#define CORNER_ASCII "`"
#define JOINT_ASCII  "|"


enum characters {
    CHAR_COLUMN,
    CHAR_ROW,
    CHAR_CORNER,
    CHAR_JOINT,
};


enum SizeOption {
    SIZEOPT_OFF,
    SIZEOPT_BYTES,
    SIZEOPT_HUMAN_READABLE,
};
_Static_assert( SIZEOPT_OFF == 0, "default enum value" );


struct options {
    bool all;                   // -a
    enum SizeOption size;       // -s | -S
    bool warn_on_fail;          // -e
    bool follow_links;          // -l
    const char *const *charset; // -c
                                // default UTF; -c => ASCII
    size_t max_depth;           // --depth
    Set *excluded_dirs;         // --exclude
};


void parse_args( int argc,
                 const char *const *argv,
                 List *paths,
                 struct options *options );

struct options options_init( void );


#endif //ARGS_PARSE_H
