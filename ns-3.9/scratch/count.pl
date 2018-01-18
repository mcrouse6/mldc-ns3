use strict;

my @data;
while (<>)
{
    my ($ts, $from, $to, $proto, $size, $rate) = split;
    $data[$ts] += $size;
}

for (my $i = 0; $i < scalar @data; $i++) 
{
    printf "%3d %10d\n", $i, $data[$i]
}
