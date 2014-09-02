import os, sys, re

def exec_cmd( cmd ):
    print cmd
    ret = os.system( cmd )
    if ret:
        print ret
        sys.exit( ret )

def write_data( beg, end, filename ):
    while sipe[ end - 1 ] in ["\n","\r"]:
        end -= 1
        
    res = ""
    for l in sipe[ beg : end ].splitlines( True ):
        if l.startswith( "# " ):
            res += l[ 2: ]
            
    f = file( filename, 'w' )
    f.write( res )
    f.flush()


for i in os.listdir( 'tests' ):
    if i.endswith( ".sipe" ):
        print "running test with", i
        exec_cmd( './sipe -b "Sipe::Ptr<Sipe::Buffer>" "tests/' + i + '" > tests/test_buffer.h' )
        exec_cmd( 'metil_comp -Isrc -ne -o tests/test_buffer tests/test_buffer.cpp' )
        
        # find test cases in .sipe
        sipe = file( 'tests/' + i ).read()
        bl = re.finditer( '# beg test\s+', sipe )
        ex = re.finditer( '# expected\s+', sipe )
        el = re.finditer( '# end test', sipe )
        for b, x, e in zip( bl, ex, el ):
            write_data( b.end(), x.start(), "tests/test_buffer.data" ) # input data
            write_data( x.end(), e.start(), "tests/test_buffer.expected" ) # expected result
            exec_cmd( 'tests/test_buffer tests/test_buffer.data > tests/test_buffer.result' ) # launch
            if os.system( 'cmp tests/test_buffer.expected tests/test_buffer.result' ):
                print "PB with 'tests/'" + i
                print "DATA:"
                os.system( "cat tests/test_buffer.data" )
                print "\nEXPECTED:"
                os.system( "cat tests/test_buffer.expected" )
                print "\nRESULT:"
                os.system( "cat tests/test_buffer.result" )
                print ""
                


            