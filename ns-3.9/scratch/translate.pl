use strict;

while (<>)
{
    my ($ts, $srcrack, $src, $dstrack, $dst, $size) = split;
    ($srcrack == $dstrack) && next;
    $ts += 0.001;
    print "$ts $srcrack $dstrack Tcp $size 10Gbps\n"
}
