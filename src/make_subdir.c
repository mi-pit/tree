//
// Created by MacBook on 21.12.2024.
//

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static void unlink_if_exists( const char *name )
{
    if ( unlinkat( AT_FDCWD, name, 0 ) == -1 && errno != ENOENT )
        err( 2, "unlinking %s", name );
}

int main( int argc, char **argv )
{
    system( "rm -rf ./sub-0" );

    size_t n_sub = argc == 1 ? 10 : strtoll( argv[ 1 ], NULL, 10 );

    for ( size_t i = 0; i < n_sub; ++i )
    {
        char name[ 1024 ] = { 0 };
        snprintf( name, 1024, "sub-%zu", i );

        if ( mkdir( name, 0777 ) == -1 )
        {
            warn( "mkdir" );
            return EXIT_FAILURE;
        }

        int dir_fd = open( name, O_DIRECTORY );
        if ( dir_fd == -1 )
        {
            warn( "open '%s'", name );
            return EXIT_FAILURE;
        }

        if ( fchdir( dir_fd ) != 0 )
        {
            warn( "fchdir" );
            return EXIT_FAILURE;
        }

        unlink_if_exists( "a_file.txt" );
        unlink_if_exists( "z_file.txt" );
        if ( close( open( "a_file.txt", O_CREAT | O_TRUNC | O_WRONLY ) ) != 0 )
            warn( "close" );
        if ( close( open( "z_file.txt", O_CREAT | O_TRUNC | O_WRONLY ) ) != 0 )
            warn( "close" );
    }

    return EXIT_SUCCESS;
}
