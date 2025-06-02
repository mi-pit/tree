#include "../CLibs/Dev/errors.h"        /* RVs, warn, terminal colors, PATH_MAX */
#include "../CLibs/misc.h"              /* countof */
#include "../CLibs/string_utils.h"      /* types */
#include "../CLibs/Structs/dynarr.h"    /* List */
#include "../CLibs/Structs/dynstring.h" /* dynstr */

#include <dirent.h>   /* directory stuff */
#include <fcntl.h>    /* open, close */
#include <stdarg.h>   /* va_list */
#include <stdio.h>    /* fprintf */
#include <stdlib.h>   /* exit */
#include <string.h>   /* strcmp */
#include <sys/stat.h> /* stat */
#include <unistd.h>   /* readlink */

_Static_assert( EXIT_SUCCESS == RV_SUCCESS, "Values must be equal" );

#define COLOR_RESET "\033[0m"
#define COLOR_DIR   "\033[1;34m" // bold blue
#define COLOR_LNK   FOREGROUND_MAGENTA
#define COLOR_EXE   FOREGROUND_RED
#define COLOR_FIFO  FOREGROUND_YELLOW
#define COLOR_SOCK  FOREGROUND_GREEN


#define DEPTH_OPT "--depth="
#define HELP_OPT  "--help"


/// Help message for the user
const string_t HELP_MESSAGE_MAP[][ 2 ] = {
    { "-a", "Include directory entries whose names begin with a dot." },
    { "-s", "Display the size of each file." },
    { "-c", "Only use ASCII characters." },
    { "-e", "Print an error message to stderr when failing to open/stat/... a file." },
    { "-l", "Acts on the target of a symlink instead of the symlink itself." },
    { DEPTH_OPT "%i", "where %i is a positive integer; Only goes %i levels deep (the"
                      " starting directory is level 0)." },
    { HELP_OPT, "Display this message." }
};


#define HELP_MESSAGE                                                            \
    "Displays a directory and its sub-directories as a tree, kinda like 'tree'" \
    " on windows\n"                                                             \
    "Options:\n"


#define COLUMN_UTF "│"
#define ROW_UTF    "─"
#define CORNER_UTF "└"
#define JOINT_UTF  "├"

#define COLUMN_ASCII "|"
#define ROW_ASCII    "-"
#define CORNER_ASCII "\\"
#define JOINT_ASCII  "|"


const string_t UTF_CHARSET[ 4 ] = {
    COLUMN_UTF,
    ROW_UTF,
    CORNER_UTF,
    JOINT_UTF,
};

const string_t ASCII_CHARSET[ 4 ] = {
    COLUMN_ASCII,
    ROW_ASCII,
    CORNER_ASCII,
    JOINT_ASCII,
};

_Static_assert( countof( UTF_CHARSET ) == 4 );
_Static_assert( countof( ASCII_CHARSET ) == 4 );


enum characters {
    CHAR_COLUMN,
    CHAR_ROW,
    CHAR_CORNER,
    CHAR_JOINT,
};


struct options {
    bool all;                // -a
    bool size;               // -s
    bool warn_on_fail;       // -e
    bool follow_links;       // -l
    const string_t *charset; // -c
                             // default UTF; -c => ASCII
    size_t max_depth;        // --depth
};


/* ================================ Helpers ================================ */

/// Format string for `-s`
#define PRINT_SIZE_FMTSTR " [%zu bytes]"

/// Fetches a character (string, really) from the charset in OPTS
#define get_character( ENUM_CHAR, OPTS_PTR ) ( ( OPTS_PTR )->charset[ ( ENUM_CHAR ) ] )


/// Skips ".", ".." and, unless `-a` is specified, ".*"
static inline bool should_skip_entry( const string_t name,
                                      const struct options *const flags )
{
    if ( strcmp( name, "." ) == 0 || strcmp( name, ".." ) == 0 )
        return true;

    return !flags->all && name[ 0 ] == '.';
}

/// Compares strings; takes pointers to strings (char **)
static inline int stringp_cmp( const void *d1, const void *d2 )
{
    return strcmp( *( string_t * ) d1, *( string_t * ) d2 );
}

/// Prints a warning if `-e` is specified
static inline void warn_if_not_silent( const struct options *options,
                                       const string_t fmt,
                                       ... )
{
    if ( !options->warn_on_fail )
        return;

    va_list vaList;
    va_start( vaList, fmt );
    vwarn( fmt, vaList );
    va_end( vaList );
}


/* ================================ Dirent Stuff ================================ */

/**
 * Gets an alphabetically sorted list of directory entries
 *
 * @param directory directory to go through
 * @param flags options for \code should_skip_entry\endcode
 * @return List (struct dynamic_array) of directory-entry names
 */
static List *get_entries_sorted( DIR *const directory, const struct options *const flags )
{
    List *entries = list_init_type( str_t );
    if ( entries == NULL )
        return ( void * ) f_stack_trace( NULL );

    struct dirent *dirent;
    while ( ( dirent = readdir( directory ) ) != NULL )
    {
        if ( should_skip_entry( dirent->d_name, flags ) )
            continue;

        str_t name = strdup( dirent->d_name );
        list_append( entries, &name );
    }

    rewinddir( directory );

    list_sort( entries, stringp_cmp );

    return entries;
}

/**
 * Prints the directory entry -- tree structure + name (in color) + [optionally] file size
 *
 * @param is_last true if dirent is the last printable entry in the directory
 * @param dirent_name   name of the entry
 * @param dirent_color  color string for the specific type
 *                      (see \code CLibs/Dev/terminal_colors.h\endcode)
 * @param pre           tree structure
 * @param options       options (-c, -s)
 * @param f_nbytes      size of file
 */
static void write_dirent( const bool is_last,
                          const string_t dirent_name,
                          const string_t dirent_color,
                          const struct dynamic_string *const pre,
                          const struct options *const options,
                          const size_t f_nbytes )
{
    printf( "%s", dynstr_data( pre ) );
    printf( "%s%s%s %s%s%s",
            get_character( is_last ? CHAR_CORNER : CHAR_JOINT, options ),
            get_character( CHAR_ROW, options ),
            get_character( CHAR_ROW, options ),
            dirent_color,
            dirent_name,
            COLOR_DEFAULT );

    if ( options->size )
        printf( PRINT_SIZE_FMTSTR, f_nbytes );
}

/// prints "-> [name]" for target of a symlink
static void print_link_target( const int dir_at_fd, const string_t dirent )
{
    char buffer[ PATH_MAX ] = { 0 };
    if ( readlinkat( dir_at_fd, dirent, buffer, sizeof buffer ) == RV_ERROR )
    {
        printf( "\n" );
        warn( "readlink( %s )", dirent );
        return;
    }

    printf( " -> '%s'", buffer );
}


int dive( const int dir_fd,
          const struct options *const options,
          const size_t level,
          struct dynamic_string *const pre )
{
    DIR *const directory = fdopendir( dir_fd );
    if ( directory == NULL )
        return fflwarn_ret( RV_ERROR, "fdopendir from fd=%i at lvl=%zu", dir_fd, level );

    rewinddir( directory );

    int rv = RV_SUCCESS;

    List *entries = get_entries_sorted( directory, options );
    if ( entries == NULL )
        return f_stack_trace( NULL );

    str_t dirent;
    for ( size_t index = 0; index < list_size( entries ); ++index, free( dirent ) )
    {
        dirent = list_access( entries, index, str_t );

        struct stat stat_data;
        if ( fstatat( dir_fd,
                      dirent,
                      &stat_data,
                      options->follow_links ? 0 : AT_SYMLINK_NOFOLLOW ) != RV_SUCCESS )
        {
            warn_if_not_silent( options, "fstatat for '%s'", dirent );
            continue;
        }
        const bool is_last = index == list_size( entries ) - 1;

        const string_t dirent_type = S_ISDIR( stat_data.st_mode )    ? COLOR_DIR
                                     : S_ISFIFO( stat_data.st_mode ) ? COLOR_FIFO
                                     : S_ISSOCK( stat_data.st_mode ) ? COLOR_SOCK
                                     : S_ISLNK( stat_data.st_mode )  ? COLOR_LNK
                                     : stat_data.st_mode & ( S_IXUSR | S_IXGRP | S_IXOTH )
                                             ? COLOR_EXE
                                             : COLOR_DEFAULT;

        write_dirent( is_last, dirent, dirent_type, pre, options, stat_data.st_size );

        if ( S_ISLNK( stat_data.st_mode ) )
            print_link_target( dir_fd, dirent );

        printf( "\n" );

        if ( !S_ISDIR( stat_data.st_mode ) || level == options->max_depth )
            continue;

        const int subdir_fd = openat( dir_fd, dirent, O_DIRECTORY | O_RDONLY );
        if ( subdir_fd == RV_ERROR )
        {
            warn_if_not_silent( options, "trying to open '%s'", dirent );
            continue;
        }

        char buf[ 10 ];
        const size_t char_count =
                snprintf( buf,
                          sizeof buf,
                          "%s   ",
                          is_last ? " " : get_character( CHAR_COLUMN, options ) );
        if ( dynstr_append( pre, buf ) != RV_SUCCESS )
            exit( EXIT_FAILURE );

        if ( ( rv = dive( subdir_fd, options, level + 1, pre ) ) != RV_SUCCESS )
            break;

        if ( ( rv = dynstr_slice_e( pre, -char_count - 1 ) ) != RV_SUCCESS )
            exit( EXIT_FAILURE );
    }

    list_destroy( entries );
    closedir( directory );
    return rv;
}


/* ==== OPTS ==== */


static void parse_special( const string_t arg, struct options *const options )
{
    if ( strstr( arg, DEPTH_OPT ) == arg )
    {
        const char *num_str = arg + STRLEN( DEPTH_OPT );
        options->max_depth  = strtoll( num_str, NULL, 10 );
        if ( errno != E_OK )
            err( EXIT_FAILURE, "invalid depth: '%s'", num_str );
    }
    else if ( strcmp( arg, HELP_OPT ) == 0 )
    {
        printf( HELP_MESSAGE );
        for ( size_t i = 0; i < countof( HELP_MESSAGE_MAP ); ++i )
        {
            printf( "\t%s\t %s\n",
                    HELP_MESSAGE_MAP[ i ][ 0 ],
                    HELP_MESSAGE_MAP[ i ][ 1 ] );
        }
        exit( EXIT_SUCCESS );
    }
    else
    {
        errx( EXIT_FAILURE, "unknown option '%s'", arg );
    }
}

static void parse_options( const string_t opts, struct options *options )
{
    const size_t str_len = strlen( opts );
    bool invalid         = true;
    for ( size_t i = 1; i < str_len; ++i )
    {
        switch ( opts[ i ] )
        {
            case 'a':
                options->all = true;
                break;
            case 's':
                options->size = true;
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
                errx( EXIT_FAILURE, "invalid option: '%c'", opts[ i ] );
        }
        invalid = false;
    }

    if ( invalid )
        errx( EXIT_FAILURE, "unknown option '%s'", opts );
}

static void parse_args( const int argc,
                        const char *const *argv,
                        List *paths,
                        struct options *options )
{
    for ( int i = 1; i < argc; ++i )
    {
        const string_t arg = argv[ i ];

        if ( arg[ 0 ] == '-' )
            parse_options( arg, options );

        else if ( list_append( paths, &arg ) != RV_SUCCESS )
        {
            f_stack_trace( 0 );
            exit( EXIT_FAILURE );
        }
    }

    if ( list_size( paths ) == 0 )
    {
        const string_t curr_dir = ".";
        if ( list_append( paths, &curr_dir ) != RV_SUCCESS )
        {
            f_stack_trace( 0 );
            exit( EXIT_FAILURE );
        }
    }
}

int main( const int argc, const char *const *const argv )
{
    List *paths            = list_init_type( str_t );
    struct options options = { 0 };
    options.charset        = UTF_CHARSET;
    options.max_depth      = PATH_MAX; // depth really shouldn't be more than this
    parse_args( argc, argv, paths, &options );

    struct dynamic_string *dynstr = dynstr_init();
    if ( dynstr == NULL )
        exit( f_stack_trace( EXIT_FAILURE ) );

    int rv = RV_EXCEPTION; // value used if no path is specified
    for ( size_t i = 0; i < list_size( paths ); ++i )
    {
        const string_t path = list_access( paths, i, string_t );

        const int dir_fd = open( path, O_DIRECTORY | O_RDONLY );
        if ( dir_fd == RV_ERROR )
        {
            warn( "could not open '%s'", path );
            continue;
        }
        printf( "%s\n", path );
        if ( options.max_depth == 0 )
            continue;

        rv = dive( dir_fd, &options, 1, dynstr );
        // dive closes dir_fd (for "optimization")

        if ( i < list_size( paths ) - 1 ) // to separate args
            printf( "\n" );

        if ( rv != RV_SUCCESS )
            break;
    }

    dynstr_destroy( dynstr );
    list_destroy( paths );

    return -rv; // RV_E* are negative
}
