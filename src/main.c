/* tree */
#include "args_parse.h"

/* libs */
#include "../lib/CLibs/src/headers/errors.h"    /* RVs, warn, colors, PATH_MAX */
#include "../lib/CLibs/src/string_utils.h"      /* types */
#include "../lib/CLibs/src/structs/dynarr.h"    /* List */
#include "../lib/CLibs/src/structs/dynstring.h" /* String */
#include "../lib/CLibs/src/structs/sets.h"      /* Set */

/* foreach_set; must be after #include "sets.h" */
#include "../lib/CLibs/src/headers/foreach.h"

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


#define COLOR_DIR  FOREGROUND_BLUE
#define COLOR_LNK  FOREGROUND_MAGENTA
#define COLOR_EXE  FOREGROUND_RED
#define COLOR_FIFO FOREGROUND_YELLOW
#define COLOR_SOCK FOREGROUND_GREEN


/* ================================ Helpers ================================ */

struct DirectoryEntry {
    str_t name;
    off_t size;
    mode_t mode;
};


/// Fetches a character (string, really) from the charset in OPTS
#define get_character( ENUM_CHAR, OPTS_PTR ) ( ( OPTS_PTR )->charset[ ( ENUM_CHAR ) ] )


/// Compares strings; takes pointers to strings (char **)
static inline int dirent_cmp( const void *d1, const void *d2 )
{
    const struct DirectoryEntry *e1 = ( struct DirectoryEntry * ) d1;
    const struct DirectoryEntry *e2 = ( struct DirectoryEntry * ) d2;

    return strcmp( e1->name, e2->name );
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

/// Skips ".", ".." and, unless `-a` is specified, ".*"
static inline bool should_skip_entry( const struct DirectoryEntry *const dirent,
                                      const struct options *const options )
{
    const string_t name = dirent->name;
    const mode_t mode   = dirent->mode;

    if ( strcmp( name, "." ) == 0 || strcmp( name, ".." ) == 0 )
        return true;

    if ( options->only_dirs && !S_ISDIR( mode ) )
        return true;

    return !options->all && name[ 0 ] == '.';
}

/**
 * Gets an alphabetically sorted list of directory entries
 *
 * @param dir_fd    directory to go through (fd)
 * @param directory directory to go through (DIR struct)
 * @param options   options for \code should_skip_entry\endcode and \code warn_if_not_silent\endcode
 * @return List of directory-entries (\code struct DirectoryEntry\endcode)
 */
static List *get_entries_sorted( const int dir_fd,
                                 DIR *const directory,
                                 const struct options *const options )
{
    List *const entries = list_init_type( struct DirectoryEntry );
    if ( entries == NULL )
        return ( void * ) f_stack_trace( NULL );

    const struct dirent *dirent;
    while ( ( dirent = readdir( directory ) ) != NULL )
    {
        struct stat stat_data;
        if ( fstatat( dir_fd,
                      dirent->d_name,
                      &stat_data,
                      options->follow_links ? 0 : AT_SYMLINK_NOFOLLOW ) != RV_SUCCESS )
        {
            warn_if_not_silent( options, "could not stat '%s'", dirent->d_name );
            continue;
        }

        const str_t name = strdup( dirent->d_name );
        if ( name == NULL )
            err( EXIT_FAILURE, "strdup" );

        struct DirectoryEntry entry = { .name = name,
                                        .size = stat_data.st_size,
                                        .mode = stat_data.st_mode };

        if ( should_skip_entry( &entry, options ) )
            continue;

        if ( list_append( entries, &entry ) != RV_SUCCESS )
            return ( void * ) f_stack_trace( NULL );
    }

    rewinddir( directory );

    list_sort( entries, dirent_cmp );

    return entries;
}


#define KILOBYTE UINT64_C( 1024 )
#define MEGABYTE ( KILOBYTE * KILOBYTE )
#define GIGABYTE ( MEGABYTE * KILOBYTE )
#define TERABYTE ( GIGABYTE * KILOBYTE )

/**
 * Prints the number of bytes in a file in a human-readable way (like KiB)
 *
 * @param nbytes number of bytes (file size)
 */
static inline void write_size_human_readable( const uint64_t nbytes )
{
    if ( nbytes < KILOBYTE )
        printf( "%llu B", nbytes );
    else if ( nbytes < MEGABYTE )
        printf( "%llu KiB", nbytes / KILOBYTE );
    else if ( nbytes < GIGABYTE )
        printf( "%llu MiB", nbytes / MEGABYTE );
    else if ( nbytes < TERABYTE )
        printf( "%llu GiB", nbytes / GIGABYTE );

    else
        printf( "%llu TiB", nbytes / TERABYTE );
}

/**
 * Prints the directory entry -- tree structure + name (in color) + [optionally] file size
 *
 * @param is_last true if dirent is the last printable entry in the directory
 * @param dirent_name   name of the entry
 * @param dirent_color  color string for the specific type
 *                      (see \code CLibs/headers/terminal_colors.h\endcode)
 * @param pre           tree structure
 * @param options       options (-c, -s)
 * @param f_nbytes      size of file
 */
static inline void write_dirent( const bool is_last,
                                 const string_t dirent_name,
                                 const string_t dirent_color,
                                 const DynString *const pre,
                                 const struct options *const options,
                                 const uint64_t f_nbytes )
{
    printf( "%s", dynstr_data( pre ) );
    printf( "%s%s%s ",
            get_character( is_last ? CHAR_CORNER : CHAR_JOINT, options ),
            get_character( CHAR_ROW, options ),
            get_character( CHAR_ROW, options ) );

    PrintInColor( stdout, dirent_color, "%s", dirent_name );

    if ( options->size == SIZEOPT_OFF )
        return;

    printf( " [" );

    if ( options->size == SIZEOPT_BYTES )
        printf( "%llu bytes", f_nbytes );
    else
        write_size_human_readable( f_nbytes );

    printf( "]" );
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
          DynString *const pre )
{
    DIR *const directory = fdopendir( dir_fd );
    if ( directory == NULL )
        return fflwarn_ret( RV_ERROR, "fdopendir from fd=%i at lvl=%zu", dir_fd, level );

    rewinddir( directory );

    int rv = RV_SUCCESS;

    List *entries = get_entries_sorted( dir_fd, directory, options );
    if ( entries == NULL )
        return f_stack_trace( RV_ERROR );

    struct DirectoryEntry dirent;
    for ( size_t index = 0; index < list_size( entries ); ++index, free( dirent.name ) )
    {
        dirent = list_fetch( entries, index, struct DirectoryEntry );

        if ( !S_ISDIR( dirent.mode ) && options->only_dirs )
            continue;

        const bool is_last = index == list_size( entries ) - 1;

        const string_t dirent_type = get_dirent_color( dirent.mode );

        write_dirent( is_last, dirent.name, dirent_type, pre, options, dirent.size );

        if ( S_ISLNK( dirent.mode ) )
            print_link_target( dir_fd, dirent.name );

        printf( "\n" );

        if ( !S_ISDIR( dirent.mode ) || level == options->max_depth ||
             dirent_is_in_excluded( options, dirent.name ) )
            continue;

        const int subdir_fd = openat( dir_fd, dirent.name, O_DIRECTORY | O_RDONLY );
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

        if ( dynstr_slice_e( pre, -char_count - 1 ) != RV_SUCCESS )
            exit( EXIT_FAILURE );
    }

    list_destroy( entries );
    closedir( directory );
    return rv;
}


int main( const int argc, const char *const *const argv )
{
    List *paths            = list_init_type( string_t );
    struct options options = options_init();
    parse_args( argc, argv, paths, &options );

    DynString *dynstr = dynstr_init();
    if ( dynstr == NULL )
        exit( f_stack_trace( EXIT_FAILURE ) );

    int rv = RV_SUCCESS;
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

        if ( ( rv = dive( dir_fd, &options, 1, dynstr ) ) != RV_SUCCESS )
            // dive closes dir_fd (for "optimization")
            break;

        if ( i < list_size( paths ) - 1 ) // to separate args
            printf( "\n" );
    }

    dynstr_destroy( dynstr );
    list_destroy( paths );
    set_destroy( options.excluded_dirs );

    return -rv; // RV_E* are negative
}
