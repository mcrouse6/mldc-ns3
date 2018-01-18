use strict; 

my @flyways = ();
my $startTime = 0;
my $count = 0;
while (<>)
{
    m/trial/ || next;
    if (m/valid/) {
        s/[a-zA-Z=]//g;
        #s/[A-Z]//g;
        #s/=//g;
        my ($trial, $gain, $thresh, $total, $valid, $sameEndPoint, $nodeOverlap, $tooFar, $hurtsOthers) = split;
        open OP, ">flow.$gain.$thresh.$trial.dat";
        print OP @flyways;
        close OP;
        @flyways = ();
        $startTime = 0;
        $count++;
         ($count % 100 == 0) && print "$count\n";
    }
    else {
        m/\(([0-9]+)=>([0-9]+)\)/;
        push @flyways, "$startTime $1 $2 Tcp 0 4Gbps\n";
        $startTime += 2;
    }
}

