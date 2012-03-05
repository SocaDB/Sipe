// #include "Sipe/Language_C.h"
#include "Sipe/Engine.h"
#include <string.h>

int usage( const char *prg, const char *msg, int res ) {
    if ( msg )
        cerrn << msg;
    cerrn << "Usage:";
    cerrn << "  " << prg << " [ options ] source [ input_files ]*";
    cerrn << "Possible options are:";
    cerrn << "  -o filename: file name for code output";
    cerrn << "  -l language: output langage (in C, C++, Javascript)";
    cerrn << "  -l machine: oentry point ('main'' by default)";
    cerrn << "  -dl: to display the lexem graph";
    cerrn << "  -di: to display the instruction graph";
    cerrn << "  -ds: to display the state graph";
    cerrn << "  -ws: to write the state steps";
    cerrn << "If no language is specified, " << prg << " will execute the machine on the input_files, with instructions considered as shell commands. If no input files, it reads the std input.";
    return res;
}

int main( int argc, char **argv ) {
    // default values
    Engine e;
    int first_input = 0;
    const char *source = 0;
    const char *output = 0;
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
        } else if ( strcmp( argv[ i ], "-dl" ) == 0 ) {
            e.dl = true;
        } else if ( strcmp( argv[ i ], "-di" ) == 0 ) {
            e.di = true;
        } else if ( strcmp( argv[ i ], "-ds" ) == 0 ) {
            e.ds = true;
        } else if ( strcmp( argv[ i ], "-ws" ) == 0 ) {
            e.ws = true;
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

    e.read( source );
    Instruction *inst = e.make_inst( "main" );
    // State *state = e.make_state_seq( machine );

    //    Language_C l( true );
    //    l.write( std::cout, state );
}
