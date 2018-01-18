use strict; 

my $prev = 0;
my $proto = "Tcp";

my $startTime;
my $from;
my $to;
my $bytes;
my $rate;

for (my $i = 0; $i < 1000; $i++) 
{
    $startTime = $prev + rand(0.2);
    $prev = $startTime;
    $from = int(rand(160));
    while (($to = int(rand(160))) == $from){};
    $bytes = int(rand(1000000000));
    $rate = rand(1) + 0.01;
    
    printf "%.3f %d %d %s %d %.2fGbps\n", 
        $startTime,
        $from,
        $to,
        $proto,
        $bytes,
        $rate;
}


