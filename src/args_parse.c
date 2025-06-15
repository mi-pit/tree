//
// Created by Michal Pitner on 14.06.2025.
//

#include "args_parse.h"

#include "../lib/CLibs/src/Dev/attributes.h" /* NoReturn */
#include "../lib/CLibs/src/Dev/errors.h"     /* err, errno, RV_ */
#include "../lib/CLibs/src/Dev/foreach.h"    /* foreach */
#include "../lib/CLibs/src/misc.h"           /* countof */
#include "../lib/CLibs/src/string_utils.h"   /* string_split */
#include "../lib/CLibs/src/Structs/dynarr.h" /* List */
#include "../lib/CLibs/src/Structs/sets.h"   /* Set */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/// Help message for the user
const char *const HELP_MESSAGE_MAP[][ 2 ] = {
    { "-a", "Include directory entries whose names begin with a dot." },
    { "-s", "Display the size of each file." },
    { "-S", "Display the size of each file in bytes." },
    { "-c", "Only use ASCII characters." },
    { "-e", "Print an error message to stderr when failing to open/stat/... a file." },
    { "-l", "Acts on the target of a symlink instead of the symlink itself." },
    { DEPTH_OPT "%i", "where %i is a non-negative integer; Only goes %i levels deep (the"
                      " starting directory is level 0)." },
    { EXCLUDE_OPT "%s[,%s]*",
      "Don't dive into these directories. Strings (names) separated by `,'. "
      "Supports globbing (must be wrapped in quotes, otherwise the args get separated by "
      "the shell)" },
    { HELP_OPT, "Display this message." },
    { "--", "All following arguments are taken as file names, no matter their format." },
};


#define HELP_MESSAGE                                                            \
    "Displays a directory and its sub-directories as a tree, kinda like 'tree'" \
    " on windows\n"                                                             \
    "Options:\n"


const char *const UTF_CHARSET[ 4 ] = {
    COLUMN_UTF,
    ROW_UTF,
    CORNER_UTF,
    JOINT_UTF,
};

const char *const ASCII_CHARSET[ 4 ] = {
    COLUMN_ASCII,
    ROW_ASCII,
    CORNER_ASCII,
    JOINT_ASCII,
};

_Static_assert( countof( UTF_CHARSET ) == 4 );
_Static_assert( countof( ASCII_CHARSET ) == 4 );


NoReturn void print_help_message( void )
{
    printf( HELP_MESSAGE );
    for ( size_t i = 0; i < countof( HELP_MESSAGE_MAP ); ++i )
    {
        printf( "\t`%s`%s\t %s\n",
                HELP_MESSAGE_MAP[ i ][ 0 ],
                strlen( HELP_MESSAGE_MAP[ i ][ 0 ] ) + 2 >= 8 ? "\n\t" : "",
                HELP_MESSAGE_MAP[ i ][ 1 ] );
    }
    exit( EXIT_SUCCESS );
}

void parse_special( const string_t arg, struct options *const options )
{
    if ( strncmp( arg, DEPTH_OPT, STRLEN( DEPTH_OPT ) ) == 0 )
    {
        assert( errno == E_OK );
        const char *num_str = arg + STRLEN( DEPTH_OPT );
        char *endptr;
        const long long ll = strtoll( num_str, &endptr, 10 );

        if ( errno != E_OK )
            err( EXIT_FAILURE, "invalid depth: '%s'", num_str );
        if ( *endptr != '\0' ) // has other stuff after the number
            errx( EXIT_FAILURE, "invalid depth: '%s'", num_str );
        if ( ll < 0 )
            errx( EXIT_FAILURE, "depth must be a positive integer (was given %lli)", ll );

        options->max_depth = ( size_t ) ll;
    }
    else if ( strncmp( arg, EXCLUDE_OPT, STRLEN( EXCLUDE_OPT ) ) == 0 )
    {
        const char *ex_dirs_str = arg + STRLEN( EXCLUDE_OPT );

        str_t *spl;
        const ssize_t spl_count = string_split(
                &spl, ex_dirs_str, ",", STRSPLIT_STRIP_RESULTS | STRSPLIT_EXCLUDE_EMPTY );
        if ( spl_count < 0 )
            err( EXIT_FAILURE, "error in string split" );
        if ( spl_count == 0 )
            errx( EXIT_FAILURE, "exclude ('%s') must contain valid file names", arg );

        foreach_arr ( str_t const, s, spl, spl_count )
        {
            switch ( set_insert_f( options->excluded_dirs, s, strlen( s ),
                                   print_string_direct ) )
            {
                case RV_ERROR:
                case RV_EXCEPTION:
                    errx( EXIT_FAILURE, "set_insert_f failed" );

                case SETINSERT_WAS_IN:
                    warnx( "duplicate --exclude argument: '%s'", s );
                    break;

                default:
                    break;
            }
        }

        string_split_destroy( spl_count, &spl );
    }
    else if ( strcmp( arg, HELP_OPT ) == 0 )
        print_help_message();
    else
        errx( EXIT_FAILURE, "unknown option '%s'", arg );
}

void parse_options( const string_t opts, struct options *options )
{
    bool contained_anything = false;
    foreach_str( opt_char, opts + 1 )
    {
        switch ( opt_char )
        {
            case 'a':
                options->all = true;
                break;
            case 's':
                options->size = SIZEOPT_HUMAN_READABLE;
                break;
            case 'S':
                options->size = SIZEOPT_BYTES;
                break;
            case 'c':
                options->charset = ASCII_CHARSET;
                break;
            case 'e':
                options->warn_on_fail = true;
                break;
            case 'l':
                options->follow_links = true;
                break;

            case '-':
                parse_special( opts, options );
                return;

            default:
                errx( EXIT_FAILURE, "invalid option: '%c'", opt_char );
        }

        contained_anything = true;
    }

    if ( !contained_anything )
        errx( EXIT_FAILURE, "invalid option '%s'", opts );
}

// Exits on error
void parse_args( const int argc,
                 const char *const *argv,
                 List *const paths,
                 struct options *const options )
{
    bool stop_options = false;
    for ( int i = 1; i < argc; ++i )
    {
        const string_t arg = argv[ i ];

        if ( strcmp( arg, "--" ) == 0 && !stop_options )
            stop_options = true;
        else if ( arg[ 0 ] == '-' && !stop_options )
            parse_options( arg, options );
        else
        {
            if ( list_append( paths, &arg ) != RV_SUCCESS )
                exit( f_stack_trace( EXIT_FAILURE ) );
        }
    }

    if ( list_is_empty( paths ) )
    {
        const string_t curr_dir = ".";
        if ( list_append( paths, &curr_dir ) != RV_SUCCESS )
            exit( f_stack_trace( EXIT_FAILURE ) );
    }
}


struct options options_init( void )
{
    Set *excluded_dirs_set = set_init();
    if ( excluded_dirs_set == NULL )
        err( EXIT_FAILURE, "failed to initialize excluded dirs" );

    struct options options = { 0 };
    options.charset        = UTF_CHARSET;
    options.max_depth      = SIZE_MAX;
    options.excluded_dirs  = excluded_dirs_set;

    return options;
}
