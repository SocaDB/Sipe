#include "StateCloner.h"
#include "StateMaker.h"
#include "Smp.h"

#include <string.h>
#include <map>

// used by a_cond_can_be_added for the map
struct SortVecInst {
    bool operator()( const Vec<const Instruction *> &a, const Vec<const Instruction *> &b ) const {
        for( int i = 0; i < std::max( a.size(), b.size() ); ++i ) {
            const Instruction *va = i < a.size() ? a[ i ] : 0;
            const Instruction *vb = i < b.size() ? b[ i ] : 0;
            if ( va < vb ) return true;
            if ( va > vb ) return false;
        }
        return false;
    }
};


StateMaker::StateMaker( Vec<State *> &to_del ) : to_del( to_del ) {
}

State *StateMaker::make( const Instruction *inst, bool ws ) {
    Smp p;
    p.allow_incc = false;
    p.ok << inst;
    p.display_steps = ws;
    State *res = _make_rec( p, "init" );

    // restart state seqs
//    while ( use_mark_stack.size() ) {
//        State *dst = use_mark_stack.pop();
//        State *src = dst->use_mark->next[ 0 ].s; // after the set_mark

//        StateCloner sc( to_del, use_mark_stack );
//        State *nst = sc.make( src, dst );
//        if ( nst->has_something_to_execute( dst->has_something_to_execute( false ) ) ) {
//            dst->insert_between_this_and_next( nst );
//        } else {
//            dst->rem_mark = dst->use_mark;
//            dst->use_mark = 0;
//        }

//        //        std::ostringstream ss;
//        //        ss << ".state_" << nst << ".dot";
//        //        nst->display_dot( ss.str().c_str() );
//    }

    return res->simplified();
}

State *StateMaker::_make_rec( Smp &p, const char *step ) {
    if ( p.display_steps )
        coutn << p.display_prefix << step << String( std::max( 0, 8 - int( strlen( step ) ) ), ' ' ) << " -> " << p;


    // already created ?
    if ( State *res = _same_bid( p ) ) return res;

    // the same inst appears twice ?
    if ( State *res = _rm_twice( p ) ) return res;

    // we have pending code / func and we know that all the branches will go to the end
    if ( State *res = _use_pend( p ) ) return res;

    // no more work -> exit
    if ( State *res = _only_end( p ) ) return res;

    // if a branch leads to the end, the following (in priority order) should be removed
    if ( State *res = _prior_br( p ) ) return res;

    // cond are treated at the beginning because they can reduce the number of branches
    // (which permits to decrease the number of marks...)
    if ( State *res = _has_cond( p ) ) return res;

    // if some branches can be removed due to cond in p
    if ( State *res = _rm_bcond( p ) ) return res;

    // if we have a cond in p and in p.ok and this cond is ok
    if ( State *res = _jmp_cond( p ) ) return res;

    // if we have code
    if ( State *res = _jmp_code( p ) ) return res;

    // if we have only incc (or ends)
    if ( State *res = _jmp_incc( p ) ) return res;

    // else, go next
    if ( State *res = _use_next( p ) ) return res;

    P( "nase" );
    return new State();
}

State *StateMaker::_new_State( const Smp &p ) {
    State *res = new State;
    res->visited = p.visited;
    res->instructions = p.ok;
    created[ p.bid() ] = res;
    return res;
}

State *StateMaker::_same_bid( Smp &p ) {
    TC::iterator iter = created.find( p.bid() );
    if ( p.display_steps and iter != created.end() )
        coutn << p.display_prefix << "found same bid " << p.bid();

    if ( iter != created.end() ) {
        //for( std::set<const Instruction *>::const_iterator v = p.visited.begin(); v != p.visited.end(); ++v )
        //    iter->second->visited.insert( *v );
        return iter->second;
    }

    return 0;
}

State *StateMaker::_rm_twice( Smp &p ) {
    for( int i = 1; i < p.ok.size(); ++i ) {
        for( int j = 0; j < i; ++j ) {
            if ( p.ok[ j ] == p.ok[ i ] ) {
                p.join_branches( j, i );
                return _make_rec( p, "rm_twice" );
            }
        }
    }

    return 0;
}

State *StateMaker::_use_pend( Smp &p ) {
    if ( p.pending and p.ok.size() == 1 ) {
        State *res = _new_State( p );
        res->use_mark = p.pending;
        p.pending = 0;
        res->add_next( _make_rec( p, "use_pend" ) );
        use_mark_stack << res;
        return res;
    }

    return 0;
}


State *StateMaker::_only_end( Smp &p ) {
    if ( p.ok.size() == 1 and p.ok[ 0 ]->end ) {
        if ( p.display_steps )
            coutn << p.display_prefix << "end";
        State *res = _new_State( p );
        res->end = p.ok[ 0 ]->end;
        return res;
    }

    return 0;
}

State *StateMaker::_prior_br( Smp &p ) {
    for( int i = 0; i < p.ok.size() - 1; ++i ) {
        if ( p.surely_leads_to_the_end( i ) ) {
            for( int j = p.ok.size() - 1; j > i; --j )
                p.remove_branch( j );
            return _make_rec( p, "prior_br" );
        }
    }

    return 0;
}

State *StateMaker::_has_cond( Smp &p ) {
    if ( not p.cond ) {
        // if only one branch -> we know that p will be ok
        if ( p.ok.size() == 1 and p.ok[ 0 ]->cond ) {
            p.next( 0 );
            return _make_rec( p, "aut_cond" );
        }

        // get all the coming conditions
        Vec<const Instruction *> nc;
        for( int i = 0; i < p.ok.size(); ++i )
            p.ok[ i ]->get_next_conds( nc, p.allow_incc == false );

        if ( nc.size() ) {
            // list of inst for each char num
            Vec<const Instruction *> inst_set_vs_n_ch[ Cond::p_size ];
            for( int i = 0; i < Cond::p_size; ++i )
                for( int j = 0; j < nc.size(); ++j )
                    if ( ( *nc[ j ]->cond )[ i ] )
                        inst_set_vs_n_ch[ i ] << nc[ j ];

            // cond vs instruction set
            typedef std::map<Vec<const Instruction *>,Cond,SortVecInst> TM;
            TM m;
            for( int i = 0; i < Cond::p_size; ++i )
                m[ inst_set_vs_n_ch[ i ] ] << i;

            // make the branches
            State *res = _new_State( p );
            res->next.reserve( m.size() );
            for( TM::iterator iter = m.begin(); iter != m.end(); ++iter ) {
                double freq = 0;
                for( int i = 0; i < iter->first.size(); ++i )
                    freq += iter->first[ i ]->freq;

                Smp n = p;
                n.cond = iter->second;
                n.display_prefix += "  ";
                if ( n.display_steps and res->next.size() )
                    coutn << "";

                State::Next nx;
                nx.freq = freq;
                nx.cond = iter->second;
                nx.s = _make_rec( n, "has_cond" );

                // append nx
                res->add_next( nx );

                //
                //                for( int i = 0; ; ++i ) {
                //                    if ( i == res->next.size() ) {
                //                        break;
                //                    }
                //                    if ( res->next[ i ].s == nx.s ) {
                //                        res->next[ i ].cond |= nx.cond;
                //                        res->next[ i ].freq += nx.freq;
                //                        break;
                //                    }
                //                }
            }

            return res;
        }
    }

    return 0;
}

State *StateMaker::_rm_bcond( Smp &p ) {
    if ( p.cond ) {
        for( int i = 0; i < p.ok.size(); ++i ) {
            Vec<const Instruction *> nc;
            if ( not p.ok[ i ]->get_next_conds( nc, p.allow_incc == false ) )
                continue;

            if ( nc.size() ) {
                bool ok = false;
                for( int j = 0; j < nc.size() and not ok; ++j ) {
                    const Cond *cond = nc[ j ]->cond.get_ptr();
                    ok |= p.cond->included_in( *cond );
                }

                if ( not ok ) {
                    p.remove_branch( i );
                    return _make_rec( p, "rm_bcond" );
                }
            }
        }
    }

    return 0;
}

State *StateMaker::_jmp_cond( Smp &p ) {
    if ( p.cond ) {
        for( int i = 0; i < p.ok.size(); ++i ) {
            const Cond *cond = p.ok[ i ]->cond.get_ptr();
            if ( cond and p.cond->included_in( *cond ) ) {
                p.next( i );
                return _make_rec( p, "jmp_cond" );
            }
        }
    }

    return 0;
}

State *StateMaker::_jmp_code( Smp &p ) {
    for( int i = 0; i < p.ok.size(); ++i ) {
        if ( p.ok[ i ]->is_an_action() ) {
            // add a mark if necessary (-> p.pending)
            if ( p.ok.size() > 1 and not p.pending ) {
                State *res = _new_State( p );
                res->set_mark = true;
                p.pending = res;
                res->add_next( _make_rec( p, "mk pend" ) );
                return res;
            }

            // else (or after that), create a state with the instructions
            State *res = _new_State( p );
            res->action_num = i;
            p.next( i );

            res->add_next( _make_rec( p, "jmp_code" ) );
            return res;
        }
    }

    return 0;
}

State *StateMaker::_jmp_incc( Smp &p ) {
    bool mi = true, ma = false;
    for( int i = 0; i < p.ok.size(); ++i ) {
        if ( p.ok[ i ]->next.size() ) {
            mi &= p.ok[ i ]->incc;
            ma |= p.ok[ i ]->incc;
        }
    }

    if ( mi and ma ) {
        // incc not allowed ?
        if ( not p.allow_incc ) {
            for( int i = 0; i < p.ok.size(); ++i )
                if ( p.ok[ i ]->next.size() )
                    i += p.next( i ) - 1;
            p.allow_incc = true;
            return _make_rec( p, "jmp_incd" );
        }

        State *s = _new_State( p );
        for( int i = 0; i < p.ok.size(); ++i )
            if ( p.ok[ i ]->next.size() )
                i += p.next( i ) - 1;
        p.cond.clear();

        s->add_next( _make_rec( p, "jmp_incc" ) );
        s->incc = 1;
        return s;
    }

    return 0;
}

State *StateMaker::_use_next( Smp &p ) {
    for( int i = 0; i < p.ok.size(); ++i ) {
        if ( p.ok[ i ]->branching_only() and not p.ok[ i ]->end ) {
            p.next( i );
            return _make_rec( p, "use_next" );
        }
    }

    return 0;
}
