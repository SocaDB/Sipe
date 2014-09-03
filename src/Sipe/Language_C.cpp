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


#include "Language_C.h"
#include "StreamSep.h"
#include "assert.h"
#include <sstream>


Language_C::Language_C( bool cpp ) : cpp( cpp ) {
    buffer_length = "2048";
    ptr_buf = 0;
}

Language_C::~Language_C() {
}

void Language_C::write( std::ostream &os, const CodeParm &_cp, const State *state, bool write_main ) {
    // helpers
    cp = &_cp;
    StreamSepMaker<std::ostream> on( os );
    f_suf = cpp ? "" : "_" + cp->struct_name;

    // unfold and get label
    Block *block = _unfold( state );
    if ( db )
        _display_dot( block );
    _make_labels( block );

    // hum...
    if ( not ptr_buf ) {
        if ( write_main ) {
            on << "#ifdef SIPE_MAIN";
            on << "#include <unistd.h>";
            on << "#include <stdio.h>";
            on << "#include <fcntl.h>";
            on << "#include <string>";
            on << "#endif // SIPE_MAIN";
        }
        on << "#ifndef SIPE_CHARP";
        on << "#define SIPE_CHARP const char *";
        on << "#endif // SIPE_CHARP";
    }

    //
    _write_preliminaries( os );

    _write_declarations( os );

    _write_parse_func( os );

    if ( not ptr_buf ) {
        if ( not cpp ) {
            on << "void init" << f_suf << "( " << cp->struct_name << " *sipe_data ) {";
            _write_init_func( os, "    ", "sipe_data->" );
            on << "}";

            on << "void dest" << f_suf << "( " << cp->struct_name << " *sipe_data ) {";
            _write_dest_func( os, "    ", "sipe_data->" );
            on << "}";
        }

        on << "#ifdef SIPE_CLASS";
        on << "int parse( SIPE_CHARP beg, SIPE_CHARP end ) {";
        on << "    return parse( &sipe_data, beg, end );";
        on << "}";
        on << "SipeData sipe_data;";
        on << "#endif // SIPE_CLASS";


        //
        if ( write_main ) {
            StreamSepMaker<std::ostream> on( os );
            on << "#ifdef SIPE_MAIN";
            _write_parse_file_func( os );
            _write_main_func( os );
            on << "#endif // SIPE_MAIN";
        }
    }
}

void Language_C::_write_preliminaries( std::ostream &os ) {
    StreamSepMaker<std::ostream> on( os );

    // includes, ...
    for( int i = 0; i < cp->preliminaries.size(); ++i )
        on << cp->preliminaries[ i ];

    //
    if ( not cpp ) {
        on << "struct " << cp->struct_name << ";";
        on << "void init" << f_suf << "( " << cp->struct_name << " *sipe_data );";
        on << "void dest" << f_suf << "( " << cp->struct_name << " *sipe_data );";
    }
}

void Language_C::_write_declarations( std::ostream &os ) {
    StreamSepMaker<std::ostream> on( os );

    // struct
    on << "#ifndef DONT_WANT_SIPE_STRUCT";
    on << "struct " << cp->struct_name << " {";
    if ( cpp ) {
        on << "    " << cp->struct_name << "() {";
        _write_init_func( os, "        ", "" );
        on << "    }";
        on << "    ~" << cp->struct_name << "() {";
        _write_dest_func( os, "        ", "" );
        on << "    }";
        on << "";
    }
    on << "    void *_inp_cont;";
    if ( nb_marks ) {
        if ( ptr_buf ) {
            on << "    " << ptr_buf << " _beg_mark[ " << nb_marks << " ];";
            on << "    " << ptr_buf << " _end_mark[ " << nb_marks << " ];";
            on << "    int _off_mark[ " << nb_marks << " ];";
        } else {
            on << "    SIPE_CHARP _mark[ " << nb_marks << " ];";
            on << "    std::string _mark_data[ " << nb_marks << " ];";
        }
    }
    for( int i = 0; i < cp->attributes.size(); ++i )
        on << "    " << cp->attributes[ i ].decl;
    on << "};";
    on << "#endif // DONT_WANT_SIPE_STRUCT";
}

void Language_C::_write_init_func( std::ostream &os, const char *sp, const char *sn ) {
    StreamSepMaker<std::ostream> on( os );
    on.beg = sp;

    on << sn << "_inp_cont = 0;";

    if ( not ptr_buf )
        for( int i = 0; i < nb_marks; ++i )
            on << sn << "_mark[ " << i << " ] = 0;";

    for( int i = 0, a = 0; i < cp->attributes.size(); ++i ) {
        if ( cp->attributes[ i ].init.size() ) {
            if ( cpp and not a++ )
                on << cp->struct_name << " *sipe_data = this;";
            on << cp->attributes[ i ].init << ";";
        }
    }
}

void Language_C::_write_dest_func( std::ostream &os, const char *sp, const char *sn ) {
    StreamSepMaker<std::ostream> on( os );
    on.beg = sp;

    for( int i = 0, a = 0; i < cp->attributes.size(); ++i ) {
        if ( cp->attributes[ i ].dest.size() ) {
            if ( cpp and not a++ )
                on << cp->struct_name << " *sipe_data = this;";
            os << cp->attributes[ i ].dest << ";";
        }
    }
}

static int nb_digits( int val ) {
    if ( val < 0 ) return 1 + nb_digits( -val );
    if ( val < 10 ) return 1;
    return 1 + nb_digits( val / 10 );
}

void Language_C::_write_parse_func( std::ostream &os ) {
    StreamSepMaker<std::ostream> on( os );
    int nb_spaces = 5;
    String sp( nb_spaces, ' ' );
    on.beg = sp.c_str();
    os << "#ifndef DONT_WANT_SIPE_PARSE\n";

    os << "#ifndef SIPE_PARSE_PRELIM\n";
    os << "#define SIPE_PARSE_PRELIM\n";
    os << "#endif // SIPE_PARSE_PRELIM\n";

    // parse
    if ( ptr_buf ) {
        os << "int SIPE_PARSE_PRELIM parse" << f_suf << "( " << cp->struct_name << " *sipe_data, " << ptr_buf << " buffer ) {\n";
        os << "     while ( not buffer->used ) {\n";
        os << "         buffer = buffer->next;\n";
        os << "         if ( not buffer )\n";
        os << "             return 0; // want more\n";
        os << "     }\n";
        os << "     \n";
        os << "     unsigned char *data = buffer->data, *end = data + buffer->used;\n";
        os << "     \n";
    } else {
        os << "int SIPE_PARSE_PRELIM parse" << f_suf << "( " << cp->struct_name << " *sipe_data, SIPE_CHARP data, SIPE_CHARP end ) {\n";
        if ( nb_marks )
            on << "SIPE_CHARP beg_data = data;\n";
    }
    on << "if ( sipe_data->_inp_cont )";
    on << "    goto *sipe_data->_inp_cont;";
    on << "";

    // blocks
    for( int i = 0; i < block_seq.size(); ++i ) {
        Block *b = block_seq[ i ];
        if ( not b->write )
            continue;

        //
        if ( b->label >= 0 ) {
            os << "l_" << b->label << ":";
            on.first_beg = on.beg + std::min( nb_spaces, 3 + nb_digits( b->label ) );
        }

        //
        if ( b->state ) {
            // action
            if ( b->state->action )
                b->state->action->write_code( on, this );

            // end ?
            if ( b->state->end ) {
                on << "sipe_data->_inp_cont = &&l_" << b->label << ";";
                on << "return " << b->state->end << ";";
            }

            //
            if ( b->state->set_mark ) {
                if ( ptr_buf ) {
                    on << "sipe_data->_beg_mark[ " << marks[ b->state ] << " ] = buffer;";
                    on << "sipe_data->_end_mark[ " << marks[ b->state ] << " ] = buffer;";
                    on << "sipe_data->_off_mark[ " << marks[ b->state ] << " ] = data - buffer->data;";
                } else {
                    on << "sipe_data->_mark[ " << marks[ b->state ] << " ] = data;";
                    on << "sipe_data->_mark_data[ " << marks[ b->state ] << " ].clear();";
                }
            }

            // rewind and exec following expressions blocks
            if ( b->state->use_mark ) {
                int nm = marks[ b->state->use_mark ];
                if ( ptr_buf ) {
                    on << "buffer = sipe_data->_beg_mark[ " << nm << " ];";
                    on << "data = buffer->data + sipe_data->_off_mark[ " << nm << " ];";
                    on << "end = buffer->data + buffer->used;";
                } else {
                    on << "if ( sipe_data->_mark[ " << nm << " ] ) {";
                    on << "    data = sipe_data->_mark[ " << nm << " ];";
                    on << "} else {";
                    on << "    sipe_data->_inp_cont = &&cnt_mark_" << b << ";";
                    on << "    SIPE_CHARP beg = (SIPE_CHARP)sipe_data->_mark_data[ " << nm << " ].data();";
                    on << "    int res = parse( sipe_data, beg, beg + sipe_data->_mark_data[ " << nm << " ].size() );";
                    on << "    if ( res )";
                    on << "        return res;";
                    on << "    data = beg_data;";
                    on << "    goto *sipe_data->_inp_cont;";
                    on << "}";
                }
                os << "cnt_mark_" << b << ":\n";
            }

            //
            if ( b->state->rem_mark ) {
            }

            //
            if ( b->state->incc ) {
                on << "if ( ++data >= end ) goto p_" << cnt.size() << ";";
                os << "c_" << cnt.size() << ":\n";

                Cnt c;
                c.block = b;
                c.label = b->label;
                cnt << c;
            }
        }

        //
        if ( b->ko ) {
            if ( b->t_ok ) { // if ( cond ) goto ok;
                String cond = b->cond.ok_cpp( "*data", &b->not_in );
                on << "if ( " << cond << " ) goto l_" << b->ok->label << ";";
            } else { // if ( not cond ) goto ko;
                String cond = b->cond.ko_cpp( "*data", &b->not_in );
                on << "if ( " << cond << " ) goto l_" << b->ko->label << ";";
            }
        }

        //
        if ( b->write_goto )
            on << "goto l_" << b->write_goto->label << ";";
    }

    // cnt
    on.beg = "";
    for( int i = 0; i < cnt.size(); ++i ) {
        if ( ptr_buf ) {
            on << "p_" << i << ": ";
            on << "    while ( buffer->next ) {";
            on << "        buffer = buffer->next;";
            on << "        if ( buffer->used ) {";
            on << "            data = buffer->data;";
            on << "            end = data + buffer->used;";
            on << "            goto c_" << i << ";";
            on << "        }";
            on << "   }";
            if ( const State *m = cnt[ i ].block->mark ) {
                on << "   sipe_data->_inp_cont = &&m_" << i << ";";
                on << "   return 0;";
                on << "m_" << i << ": ";
                on << "    sipe_data->_end_mark[ " << marks[ m ] << " ]->next = buffer;";
                on << "    sipe_data->_end_mark[ " << marks[ m ] << " ] = buffer;";
                on << "    goto c_" << i << ";";
            } else {
                on << "   sipe_data->_inp_cont = &&c_" << i << ";";
                on << "   return 0;";
            }
        } else {
            os << "p_" << i << ":";
            if ( const State *m = cnt[ i ].block->mark ) {
                on << "sipe_data->_mark_data[ " << marks[ m ] << " ].append( sipe_data->_mark[ " << marks[ m ] << " ] ? sipe_data->_mark[ " << marks[ m ] << " ] : beg_data, data );";
                on << "sipe_data->_mark[ " << marks[ m ] << " ] = 0;";
            } else
                on.first_beg = on.beg + std::min( nb_spaces, 3 + nb_digits( i ) );
            on << "sipe_data->_inp_cont = &&c_" << i << "; return 0;";
        }
    }

    os << "}\n";
    os << "#endif // DONT_WANT_SIPE_PARSE\n";
}

void Language_C::_write_parse_file_func( std::ostream &os ) {
    StreamSepMaker<std::ostream> on( os );

    // function for execution
    on << "int parse_file( int fd ) {";
    on << "    " << cp->struct_name << " sd;";
    if ( not cpp )
        on << "    init( &sd );";

    on << "    char buffer[ " << buffer_length << " ];";
    on << "    while ( true ) {";
    on << "        int r = read( fd, buffer, " << buffer_length << " );";
    // on << "        printf( \"r=%i\\n\", r );";
    on << "        if ( r == 0 )";
    on << "            return 0;";
    on << "        if ( int res = parse" << f_suf << "( &sd, buffer, buffer + r ) )";
    on << "            return res;";
    on << "    }";

    if ( not cpp )
        on << "    dest( &sd );";
    on << "    return 0;";
    on << "}";
}

void Language_C::_write_main_func( std::ostream &os ) {
    StreamSepMaker<std::ostream> on( os );
    on << "int main( int argc, char **argv ) {";
    on << "    if ( argc == 1 )";
    on << "        return parse_file( 0 );";
    on << "    for( int i = 1; i < argc; ++i ) {";
    on << "        int fd = open( argv[ i ], O_RDONLY );";
    on << "        if ( fd < 0 )";
    on << "            perror( \"Opening file\" );";
    on << "        else";
    on << "            parse_file( fd );";
    on << "    }";
    on << "    return 0;";
    on << "}";
}
