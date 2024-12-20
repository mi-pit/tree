#include "../CLibs/dynarr.h"       /* List */
#include "../CLibs/dynstring.h"    /* dynstr */
#include "../CLibs/errors.h"       /* RVs, warn() */
#include "../CLibs/string_utils.h" /* types, get_file_name() */

#include <dirent.h>   /* directory stuff */
#include <fcntl.h>    /* open(), close() */
#include <stdlib.h>   /* exit() */
#include <string.h>   /* strcmp() */
#include <sys/stat.h> /* stat */


#define HELP_MESSAGE                                                                \
    "Displays a directory and its sub-directories as a tree, kinda like 'tree' on " \
    "windows\n"                                                                     \
    "Options:\n"                                                                    \
    "    -a        \tInclude directory entries whose names begin with a dot.\n"     \
    "    -s        \tDisplay the size of each file.\n"                              \
    "    -c        \tOnly use ASCII characters.\n"                                  \
    "    -e        \tSuppress messages to stderr when failing to"                   \
    " open/stat/... a file.\n"                                                      \
    "              \tWarning messages still appear when given invalid args"         \
    " or when memory allocation fails.\n"                                           \
    "    --depth=%i\twhere %i is a positive integer; Only goes %i levels deep"      \
    " (the starting directory is level 0)\n"                                        \
    "    --help    \tDisplay this message.\n"


#define COLUMN_UTF "│"
#define ROW_UTF    "─"
#define CORNER_UTF "└"
#define JOINT_UTF  "├"

#define COLUMN_ASCII "|"
#define ROW_ASCII    "-"
#define CORNER_ASCII "\\"
#define JOINT_ASCII  "|"

#define SPACE_STR " "

string_t UTF_CHARSET[ 5 ] = {
    SPACE_STR, COLUMN_UTF, ROW_UTF, CORNER_UTF, JOINT_UTF,
};

string_t ASCII_CHARSET[ 5 ] = {
    SPACE_STR, COLUMN_ASCII, ROW_ASCII, CORNER_ASCII, JOINT_ASCII,
};

enum characters {
    CHAR_SPACE __unused = 0,

    CHAR_COLUMN = 1,
    CHAR_ROW,
    CHAR_CORNER,
    CHAR_JOINT,
};


struct options {
    bool all;          // -a
    bool size;         // -s
    bool warn_on_fail; // -e
    bool buffered;     // -B
    string_t *charset; // -c
    size_t max_depth;  // --depth
};

#define PRINT_SIZE_FMTSTR " [%lld bytes]"


DynamicString OutputBuffer;
#define BUFFER_SIZE 1024


static inline string_t get_character( enum characters ch,
                                      const struct options *const opts )
{
    return opts->charset[ ch ];
}

static inline bool should_skip_entry( string_t const name,
                                      const struct options *const flags )
{
    if ( strcmp( name, "." ) == 0 || strcmp( name, ".." ) == 0 )
        return true;

    if ( flags->all || name[ 0 ] != '.' )
        return false;

    return true;
}

static inline int stringp_cmp( const void *d1, const void *d2 )
{
    return strcmp( *( string_t * ) d1, *( string_t * ) d2 );
}

static inline void warn_if_not_silent( const struct options *flags, string_t fmt, ... )
{
    if ( !flags->warn_on_fail )
        return;

    va_list vaList;
    va_start( vaList, fmt );
    vwarn( fmt, vaList );
    va_end( vaList );
}

//
//

List get_entries_sorted( DIR *directory, const struct options *flags )
{
    List entries = list_init_type( str_t );
    if ( entries == NULL )
    {
        f_stack_trace();
        return NULL;
    }

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

int write_buffered( bool is_last,
                    string_t dirent_name,
                    ConstDynamicString pre,
                    const struct options *const options,
                    const long long f_nbytes )
{
    if ( !options->buffered )
    {
        printf( "%s", dynstr_data( pre ) );
        printf( "%s%s%s %s",
                get_character( is_last ? CHAR_CORNER : CHAR_JOINT, options ),
                get_character( CHAR_ROW, options ),
                get_character( CHAR_ROW, options ),
                dirent_name );
        if ( options->size )
            printf( PRINT_SIZE_FMTSTR, f_nbytes );
        printf( "\n" );

        return RV_SUCCESS;
    }

    return_on_fail( dynstr_append( OutputBuffer, dynstr_data( pre ) ) );
    return_on_fail(
            dynstr_appendf( OutputBuffer,
                            "%s%s%s %s",
                            get_character( is_last ? CHAR_CORNER : CHAR_JOINT, options ),
                            get_character( CHAR_ROW, options ),
                            get_character( CHAR_ROW, options ),
                            dirent_name ) );
    if ( options->size )
        return_on_fail( dynstr_appendf( OutputBuffer, PRINT_SIZE_FMTSTR, f_nbytes ) );
    return_on_fail( dynstr_append( OutputBuffer, "\n" ) );

    if ( dynstr_len( OutputBuffer ) > BUFFER_SIZE )
    {
        printf( "%s", dynstr_data( OutputBuffer ) );
        return_on_fail( dynstr_reset( OutputBuffer ) );
    }
    return RV_SUCCESS;
}

int dive( const int dir_fd,
          const struct options *const options,
          const size_t level,
          DynamicString pre )
{
    DIR *directory = fdopendir( dir_fd );
    if ( directory == NULL )
        return fwarn_ret( RV_ERROR, "fdopendir from fd=%i", dir_fd );

    rewinddir( directory );

    int rv = RV_SUCCESS;

    List entries = get_entries_sorted( directory, options );

    struct stat stat_data;
    str_t dirent;
    for ( size_t index = 0; index < list_size( entries ); ++index, free( dirent ) )
    {
        dirent = list_access( entries, index, str_t );

        if ( ( rv = fstatat( dir_fd, dirent, &stat_data, AT_SYMLINK_NOFOLLOW ) ) !=
             RV_SUCCESS )
        {
            warn_if_not_silent( options, "fstatat for '%s'", dirent );
            continue;
        }
        bool is_last = index == list_size( entries ) - 1;

        if ( write_buffered(
                     is_last, dirent, pre, options, ( long long ) stat_data.st_size ) !=
             RV_SUCCESS )
        {
            f_stack_trace();
            return RV_ERROR;
        }

        // ======= //

        if ( !S_ISDIR( stat_data.st_mode ) || level == options->max_depth )
            continue;

        int subdir_fd = openat( dir_fd, dirent, O_DIRECTORY | O_RDONLY | O_SYMLINK );
        if ( subdir_fd == RV_ERROR )
        {
            warn_if_not_silent( options, "trying to open '%s'", dirent );
            continue;
        }

        char buf[ 10 ];
        size_t char_count = snprintf(
                buf, 10, "%s   ", is_last ? " " : get_character( CHAR_COLUMN, options ) );

        if ( ( rv = dynstr_append( pre, buf ) ) != RV_SUCCESS )
        {
            f_stack_trace();
            break;
        }

        if ( ( rv = dive( subdir_fd, options, level + 1, pre ) ) != RV_SUCCESS )
            break;

        if ( ( rv = dynstr_slice_e( pre,
                                    ( ssize_t ) ( dynstr_len( pre ) - char_count ) ) ) !=
             RV_SUCCESS )
            break;
    }

    list_destroy( entries );
    closedir( directory );
    return rv;
}

// ==== // ==== //

void parse_special( string_t arg, struct options *const options )
{
    if ( strstr( arg, "--depth=" ) == arg )
    {
        const char *num_str = arg + 8;
        options->max_depth  = strtoll( num_str, NULL, 10 );
        if ( errno != E_OK )
            err( EXIT_FAILURE, "invalid depth: %s", num_str );
        return;
    }

    if ( strcmp( arg, "--help" ) == 0 )
    {
        printf( "%s", HELP_MESSAGE );
        return;
    }
}

void parse_options( string_t opts, struct options *options )
{
    size_t str_len = strlen( opts );
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
            case 'B':
                options->buffered = true;
                break;

            case '-':
                parse_special( opts, options );
                return;

            default:
                errx( EXIT_FAILURE, "invalid option: '%c'", opts[ i ] );
        }
    }
}

void parse_args( int argc, const char *const *argv, List paths, struct options *options )
{
    for ( int i = 1; i < argc; ++i )
    {
        string_t const arg = argv[ i ];

        if ( arg[ 0 ] == '-' )
        {
            parse_options( arg, options );
        }
        else if ( list_append( paths, &arg ) != RV_SUCCESS )
        {
            f_stack_trace();
            exit( EXIT_FAILURE );
        }
    }

    if ( list_size( paths ) == 0 )
    {
        string_t curr_dir = ".";
        if ( list_append( paths, &curr_dir ) != RV_SUCCESS )
        {
            f_stack_trace();
            exit( EXIT_FAILURE );
        }
    }
}

int main( int argc, const char *const *const argv )
{
    if ( argc > 1 && strcmp( argv[ 1 ], "--help" ) == 0 )
    {
        printf( "%s", HELP_MESSAGE );
        exit( EXIT_SUCCESS );
    }

    List paths             = list_init_type( str_t );
    struct options options = { 0 };
    options.charset        = UTF_CHARSET;
    options.max_depth      = SIZE_MAX;
    parse_args( argc, argv, paths, &options );

    OutputBuffer = dynstr_init_cap( BUFFER_SIZE * 2 );

    DynamicString dynstr = dynstr_init();
    if ( dynstr == NULL )
        err( EXIT_FAILURE, "fatal error" );

    int rv;
    for ( size_t i = 0; i < list_size( paths ); ++i )
    {
        string_t path = list_access( paths, i, string_t );

        int dir_fd = open( path, O_DIRECTORY | O_RDONLY );
        if ( dir_fd == RV_ERROR )
            err( EXIT_FAILURE, "could not open '%s'", path );
        printf( "%s\n", path );
        if ( options.max_depth == 0 )
            continue;

        rv = dive( dir_fd, &options, 1, dynstr );
        // dive closes dir_fd

        if ( i < list_size( paths ) - 1 )
            printf( "\n" );

        if ( rv != RV_SUCCESS )
            break;
    }
    if ( dynstr_len( OutputBuffer ) > 0 )
        printf( "%s", dynstr_data( OutputBuffer ) );

    dynstr_destroy( OutputBuffer );
    dynstr_destroy( dynstr );
    list_destroy( paths );

    return rv == RV_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
