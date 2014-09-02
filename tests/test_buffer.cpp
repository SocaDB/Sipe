#include <Sipe/Buffer.h>
#include <sys/types.h>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "test_buffer.h"
using namespace Sipe;

int main( int argc, char **argv ) {
    SipeData sd;

    int fd = open( argv[ 1 ], O_RDONLY );
    if ( fd < 0 )
        perror( "Opening file" );

    while ( true ) {
        Ptr<Buffer> buff = new Buffer;
        buff->used = read( fd, buff->data, buff->item_size );
        // write( 0, buff->data, buff->used );
        if ( buff->used <= 0 ) {
            // need a retry or there are more data to come
            if ( buff->used < 0 and ( errno == EAGAIN or errno == EWOULDBLOCK ) )
                continue;
            std::cout << "EOF" << std::endl;
            return 0;
        }

        // parse
        // std::cout << "size = " << buff->used << std::endl;
        if ( int res = parse( &sd, buff ) )
            return 0;
    }
    return 0;
}
