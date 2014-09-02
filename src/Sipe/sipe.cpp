/*
 Copyright 2012 Structure Computation  www.structure-computation.com
 Copyright 2012 Hugo Leclerc

 This file is part of Sipe.

 Sipe is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Sipe is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with Sipe. If not, see <http://www.gnu.org/licenses/>.
*/


#include "Sipe/Language_C.h"
#include "Sipe/Engine.h"
#include <string.h>
#include <stdlib.h>
#include <sstream>
#include <fstream>

int usage( const char *prg, const char *msg, int res ) {
    if ( msg )
        cerrn << msg;
    cerrn << "Usage:";
    cerrn << "  " << prg << " [ options ] source [ input_files ]*";
    cerrn << "Possible options are:";
    cerrn << "  -o filename: file name for code output";
    cerrn << "  -l language: output langage (in C, C++, Javascript)";
    cerrn << "  -m machine: oentry point ('main'' by default)";
    cerrn << "  -b class_name: use class_name (e.g. Ptr<Nuffer>) instead of const char *couples";
    cerrn << "  -e: compile and execute";
    cerrn << "  -L : buffer length for execution";
    cerrn << "  -dl: to display the lexem graph";
    cerrn << "  -di: to display the instruction graph";
    cerrn << "  -ds: to display the state graph";
    cerrn << "  -db: to display the block graph";
    cerrn << "  -ws: to write the state steps";
    cerrn << "If no language is specified, " << prg << " will execute the machine on the input_files, with instructions considered as shell commands. If no input files, it reads the std input.";
    return res;
}

int main( int argc, char **argv ) {
    // default values
    Engine e;
    String output;
    bool db = false;
    int first_input = 0;
    const char *buffer_length = "2048";
    bool execute = false;
    const char *source = 0;
    const char *ptr_buf = 0;
    const char *machine = 0;
    const char *language = 0;

    // arg parse
    for( int i = 1; i < argc; ++i ) {
        if ( strcmp( argv[ i ], "-h" ) == 0 ) {
            return usage( argv[ 0 ], 0, 0 );
        } else if ( strcmp( argv[ i ], "-o" ) == 0 ) {
            if ( i + 1 >= argc )
                return usage( argv[ 0 ], "-o must be followed by an argument", 1 );
            output = argv[ ++i ];
        } else if ( strcmp( argv[ i ], "-l" ) == 0 ) {
            if ( i + 1 >= argc )
                return usage( argv[ 0 ], "-l must be followed by an argument", 2 );
            language = argv[ ++i ];
        } else if ( strcmp( argv[ i ], "-m" ) == 0 ) {
            if ( i + 1 >= argc )
                return usage( argv[ 0 ], "-m must be followed by an argument", 3 );
            machine = argv[ ++i ];
        } else if ( strcmp( argv[ i ], "-b" ) == 0 ) {
            if ( i + 1 >= argc )
                return usage( argv[ 0 ], "-b must be followed by an argument", 3 );
            ptr_buf = argv[ ++i ];
        } else if ( strcmp( argv[ i ], "-L" ) == 0 ) {
            if ( i + 1 >= argc )
                return usage( argv[ 0 ], "-l must be followed by an argument", 2 );
            buffer_length = argv[ ++i ];
        } else if ( strcmp( argv[ i ], "-dl" ) == 0 ) {
            e.dl = true;
        } else if ( strcmp( argv[ i ], "-di" ) == 0 ) {
            e.di = true;
        } else if ( strcmp( argv[ i ], "-ds" ) == 0 ) {
            e.ds = true;
        } else if ( strcmp( argv[ i ], "-db" ) == 0 ) {
            db = true;
        } else if ( strcmp( argv[ i ], "-ws" ) == 0 ) {
            e.ws = true;
        } else if ( strcmp( argv[ i ], "-e" ) == 0 ) {
            execute = true;
        } else if ( not source ) {
            source = argv[ i ];
        } else if ( not first_input ) {
            first_input = i;
            break;
        }
    }

    if ( not source )
        return usage( argv[ 0 ], "Please specify a sipe file", 4 );

    if ( not machine )
        machine = "main";

    if ( not language )
        language = "cpp";

    e.read( source );
    State *state = e.make_state_seq( machine );

    //
    if ( execute and not output.size() ) {
        output = source;
        if ( int ind = output.rfind( '/' ) )
            output = output.substr( 0, ind + 1 ) + '.' + output.substr( ind + 1 ) + ".cpp";
        else
            output = '.' + output + ".cpp";
    }

    std::ostream *out = &std::cout;
    std::ofstream fout;
    if ( output.size() ) {
        fout.open( output.c_str() );
        if ( not fout ) {
            cerrn << "Impossible to open '" << output << "'.";
            return 6;
        }
        out = &fout;
    }

    Language_C l( true );
    l.db = db;
    l.ptr_buf = ptr_buf;
    l.buffer_length = buffer_length;
    l.write( *out, e.code_parm, state, true );

    if ( execute and output.size() ) {
        fout.close();

        std::ostringstream cmd;
        cmd << "metil_comp -no-env -g3 -O3 -DSIPE_MAIN " << output;
        if ( first_input )
            for( int i = first_input; i < argc; ++i )
                cmd << " '" << argv[ i ] << "'";
        return system( cmd.str().c_str() );
    }

    return e.ret();
}

