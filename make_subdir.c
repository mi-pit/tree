//
// Created by MacBook on 21.12.2024.
//

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>


int main( int argc, char **argv )
{
    system( "rm -r ./sub-0" );

    size_t n_sub;
    if ( argc == 1 )
        n_sub = 10;
    else
        n_sub = strtoll( argv[ 1 ], NULL, 10 );

    for ( size_t i = 0; i < n_sub; ++i )
    {
        char name[ 1024 ] = { 0 };
        snprintf( name, 1024, "sub-%zu", i );


        if ( mkdir( name, 0777 ) == -1 )
        {
            warn( "mkdir" );
            return EXIT_FAILURE;
        }

        int dir_fd = open( name, O_DIRECTORY, 0777 );
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
    }
}
