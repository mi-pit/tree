#include "../lib/CLibs/src/Dev/assert_that.h"   /* assert_that */
#include "../lib/CLibs/src/Dev/errors.h"        /* RVs, warn, terminal colors, PATH_MAX */
#include "../lib/CLibs/src/misc.h"              /* countof */
#include "../lib/CLibs/src/string_utils.h"      /* types */
#include "../lib/CLibs/src/Structs/dynarr.h"    /* List */
#include "../lib/CLibs/src/Structs/dynstring.h" /* String */
#include "../lib/CLibs/src/Structs/sets.h"      /* Set */

/* foreach_set */
#include "../lib/CLibs/src/Dev/foreach.h"

#include <dirent.h>   /* directory stuff */
#include <fcntl.h>    /* open, close */
#include <fnmatch.h>  /* --exclude globbing */
#include <stdarg.h>   /* va_list */
#include <stdio.h>    /* fprintf */
#include <stdlib.h>   /* exit */
#include <string.h>   /* strcmp */
#include <sys/stat.h> /* stat */
#include <unistd.h>   /* readlink */


_Static_assert( EXIT_SUCCESS == RV_SUCCESS, "Values must be equal" );


#define COLOR_DIR  "\033[1;34m" // bold blue
#define COLOR_LNK  FOREGROUND_MAGENTA
#define COLOR_EXE  FOREGROUND_RED
#define COLOR_FIFO FOREGROUND_YELLOW
#define COLOR_SOCK FOREGROUND_GREEN


#define DEPTH_OPT   "--depth="
#define HELP_OPT    "--help"
#define EXCLUDE_OPT "--exclude="


/// Help message for the user
const string_t HELP_MESSAGE_MAP[][ 2 ] = {
    { "-a", "Include directory entries whose names begin with a dot." },
    { "-s", "Display the size of each file." },
    { "-c", "Only use ASCII characters." },
    { "-e", "Print an error message to stderr when failing to open/stat/... a file." },
    { "-l", "Acts on the target of a symlink instead of the symlink itself." },
    { DEPTH_OPT "%i", "where %i is a non-negative integer; Only goes %i levels deep (the"
                      " starting directory is level 0)." },
    { EXCLUDE_OPT "%s[,%s]*",
      "Don't dive into these directories. Strings (names) separated by `,'. "
      "Supports globbing (must be wrapped in quotes, otherwise the args get separated by "
      "the shell)" },
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
#define CORNER_ASCII "`"
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
    Set *excluded_dirs;      // --exclude
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

/// prints "-> ‹name›" for target of a symlink
static void print_link_target( const int dir_at_fd, const string_t dirent )
{
    char buffer[ PATH_MAX ] = { 0 };
    if ( readlinkat( dir_at_fd, dirent, buffer, sizeof buffer ) == RV_ERROR )
    {
        printf( "\n" );
        warn( "readlink( %s )", dirent );
        return;
    }

    printf( " -> %s", buffer );
}

/// Returns a color based on the file type
static inline string_t get_dirent_color( const int st_mode )
{
    return S_ISDIR( st_mode )                          ? COLOR_DIR
           : S_ISLNK( st_mode )                        ? COLOR_LNK
           : S_ISSOCK( st_mode )                       ? COLOR_SOCK
           : S_ISFIFO( st_mode )                       ? COLOR_FIFO
           : st_mode & ( S_IXUSR | S_IXGRP | S_IXOTH ) ? COLOR_EXE
                                                       : COLOR_DEFAULT;
}


bool dirent_is_in_excluded( const struct options *const options, const string_t dirent )
{
    foreach_set ( options->excluded_dirs )
        if ( fnmatch( entry.item->data, dirent, 0 ) == 0 )
            return true;
    return false;
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
        dirent = list_fetch( entries, index, str_t );

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

        const string_t dirent_type = get_dirent_color( stat_data.st_mode );

        write_dirent( is_last, dirent, dirent_type, pre, options, stat_data.st_size );

        if ( S_ISLNK( stat_data.st_mode ) )
            print_link_target( dir_fd, dirent );

        printf( "\n" );

        if ( !S_ISDIR( stat_data.st_mode ) || level == options->max_depth ||
             dirent_is_in_excluded( options, dirent ) )
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

NoReturn static void print_help_message( void )
{
    printf( HELP_MESSAGE );
    for ( size_t i = 0; i < countof( HELP_MESSAGE_MAP ); ++i )
    {
        if ( strlen( HELP_MESSAGE_MAP[ i ][ 0 ] ) >= 4 /* tab width */ )
            printf( "\t%s\n\t\t %s\n",
                    HELP_MESSAGE_MAP[ i ][ 0 ],
                    HELP_MESSAGE_MAP[ i ][ 1 ] );
        else
            printf( "\t%s\t %s\n",
                    HELP_MESSAGE_MAP[ i ][ 0 ],
                    HELP_MESSAGE_MAP[ i ][ 1 ] );
    }
    exit( EXIT_SUCCESS );
}

static void parse_special( const string_t arg, struct options *const options )
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

static void parse_options( const string_t opts, struct options *options )
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
                errx( EXIT_FAILURE, "invalid option: '%c'", opt_char );
        }

        contained_anything = true;
    }

    if ( !contained_anything )
        errx( EXIT_FAILURE, "invalid option '%s'", opts );
}

// Exits on error
static void parse_args( const int argc,
                        const char *const *argv,
                        List *const paths,
                        struct options *const options )
{
    for ( int i = 1; i < argc; ++i )
    {
        const string_t arg = argv[ i ];

        if ( arg[ 0 ] == '-' )
            parse_options( arg, options );
        else if ( list_append( paths, &arg ) != RV_SUCCESS )
            exit( f_stack_trace( EXIT_FAILURE ) );
    }

    if ( list_is_empty( paths ) )
    {
        const string_t curr_dir = ".";
        if ( list_append( paths, &curr_dir ) != RV_SUCCESS )
            exit( f_stack_trace( EXIT_FAILURE ) );
    }
}

int main( const int argc, const char *const *const argv )
{
    List *paths            = list_init_type( string_t );
    struct options options = { 0 };
    if ( ( options.excluded_dirs = set_init() ) == NULL )
        err( EXIT_FAILURE, "set_init() failed" );
    options.charset   = UTF_CHARSET;
    options.max_depth = SIZE_MAX;
    parse_args( argc, argv, paths, &options );

    DynString *dynstr = dynstr_init();
    if ( dynstr == NULL )
        exit( f_stack_trace( EXIT_FAILURE ) );

    for ( size_t i = 0; i < list_size( paths ); ++i )
    {
        const string_t path = list_fetch( paths, i, string_t );

        if ( set_search( options.excluded_dirs, path, strlen( path ) ) )
            continue;

        const int dir_fd = open( path, O_DIRECTORY | O_RDONLY );
        if ( dir_fd == RV_ERROR )
        {
            warn( "could not open '%s'", path );
            continue;
        }
        PrintInColor( stdout, COLOR_DIR, "%s", path );
        printf( "\n" );

        if ( options.max_depth == 0 )
            continue;

        if ( dive( dir_fd, &options, 1, dynstr ) != RV_SUCCESS )
            // dive closes dir_fd (for "optimization")
            break;

        if ( i < list_size( paths ) - 1 ) // to separate args
            printf( "\n" );
    }

    dynstr_destroy( dynstr );
    list_destroy( paths );
    set_destroy( options.excluded_dirs );

    return -RV_EXCEPTION; // RV_E* are negative
}
