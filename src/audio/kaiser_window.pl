#!/usr/bin/perl -w

use warnings;
use strict;

# The resampling algorithm: https://ccrma.stanford.edu/~jos/resample/
# https://www.mathworks.com/help/signal/ref/kaiser.html
# "Thus kaiser(L,beta) is equivalent to
#    besseli(0,beta*sqrt(1-(((0:L-1)-(L-1)/2)/((L-1)/2)).^2))/besseli(0,beta)."
# Matlab kaiser calls besseli():
# https://www.mathworks.com/help/matlab/ref/besseli.htm
# https://en.wikipedia.org/wiki/Bessel_function

sub print_table {
    my $tableref = shift;
    my $name = shift;
    my @table = @{$tableref};
    my $comma = '';
    my $count = 0;
    print("static const float $name = {\n    ");
    foreach (@table) {
        print("$comma$_");
        #print(sprintf("%.6f\n", $_));
        if (++$count > 4) {
            $count = 0;
            print(",\n    ");
            $comma = '';
        } else {
            $comma = ', ';
        }
    }
    print("\n};\n\n");
}


use POSIX ();

# This is a "modified" bessel function, so you can't use POSIX j0()
sub bessel {
    my $x = shift;

    my $i0 = 1;
    my $f = 1;
    my $i = 1;

    while (1) {
        my $diff = POSIX::pow($x / 2.0, $i * 2) / POSIX::pow($f, 2);
        last if ($diff < 1.0e-21);
        $i0 += $diff;
        $i++;
        $f *= $i;
    }

    return $i0;
}

sub kaiser {
    my $L = shift;
    my $beta = shift;
    my @retval;

    #print("L=$L, beta=$beta\n"); exit(0);

    for (my $i = 0; $i < $L; $i++) {
        my $val = bessel($beta * sqrt(1.0 - 
        POSIX::pow(
            (
                (
                    ($i-($L-1.0))
                ) / 2.0
            ) / (($L-1)/2.0), 2.0 ))
        ) / bessel($beta);

        unshift @retval, $val;
    }
    return @retval;
}


my $zero_crossings = 5;
my $bits_per_sample = 16;
my $samples_per_zero_crossing = 1 << (($bits_per_sample / 2) + 1);
my $kaiser_window_table_size = ($samples_per_zero_crossing * $zero_crossings) + 1;

# if dB > 50: 0.1102 * ($db - 8.7)
my $db = 80.0;
my $beta = 0.1102 * ($db - 8.7);

my @table = kaiser($kaiser_window_table_size, $beta);

print_table(\@table, 'kaiser_window');

# Kaiser window has "sinc function" ("cardinal sine") applied to it:
#   sin(pi * x) / (pi * x)
# "For example, to use the ideal lowpass filter, the table would contain
#  h(l) = sinc(l/L)."

use Math::Trig ':pi';
for (my $i = 1; $i < $kaiser_window_table_size; $i++) {
    my $x = $i / $samples_per_zero_crossing;
    $table[$i] *= sin($x * pi) / ($x * pi);
}

print_table(\@table, 'with_sinc');

# "Our implementation also stores a table of differences ¯h(l) = h(l + 1) − h(l) between successive
# FIR sample values in order to speed up the linear interpolation. The length of each table is
# Nh = LNz + 1, including the endpoint definition ¯h(Nh) = 0."

my @differences = ();
for (my $i = 1; $i < $kaiser_window_table_size; $i++) {
    push @differences, $table[$i] - $table[$i - 1];
}
push @differences, 0;

print_table(\@differences, 'differences');


# Might as well use this code as a test harness...

use autodie;
my $fnamein = shift @ARGV;
my $fnameout = shift @ARGV;
my $inrate = shift @ARGV;
my $outrate = shift @ARGV;

print("Resampling $fnamein (freq=$inrate) to $fnameout (freq=$outrate).\n");

open(IN, '<:raw', $fnamein);
my @src = ();

# this assumes mono Sint16 raw data since we aren't parsing .wav files.
# !!! FIXME: deal with multichannel audio.
my $channels = 1;

# this is inefficient, but this is just throwaway code...
while (read(IN, my $bytes, 2) == 2) {
    my ($samp) = unpack('s', $bytes);
    push @src, $samp;
}

close(IN);

my $ratio = $outrate / $inrate;
my $sample_frames_in = scalar(@src) / $channels;
my $sample_frames_out = $sample_frames_in * $ratio;

my $outsamples = $sample_frames_out * $channels;
#my @dst = (0) x ($outsamples);
my @dst = ();
print("Resampling $sample_frames_in input frames to $sample_frames_out output (ratio=$ratio).\n");


my $inv_spzc = int(POSIX::ceil(($samples_per_zero_crossing * $inrate) / $outrate));
my $padding_len;
if ($ratio < 1.0) {
    $padding_len = int(POSIX::ceil(($samples_per_zero_crossing * $inrate) / $outrate));
} else {
    $padding_len = $samples_per_zero_crossing;
}

# You need to pad the input or we'll get buffer overflows.
# !!! FIXME: deal with multichannel audio.
for (my $i = 0; $i < $padding_len; $i++) {
    push @src, 0;
    unshift @src, 0;
}

# !!! FIXME: deal with multichannel audio.
my $time = 0.0;
for (my $i = 0; $i < $outsamples; $i++) {
    my $srcindex = int($time * $inrate);  # !!! FIXME: truncate or round?

    my $ftime = $srcindex / $inrate;  # this would be $time if we didn't convert $srcindex to int.
    my $fnexttime = ($srcindex + 1) / $inrate;

    # do this twice to calculate the sample, once for the "left wing" and then same for the right.
    my $sample = 0;
    my $interpolation = 1.0 - ($fnexttime - $time) / ($fnexttime - $ftime);
    my $filterindex = int($interpolation * $samples_per_zero_crossing);

    $srcindex += $padding_len;

    for (my $j = 0; ($filterindex + ($j * $samples_per_zero_crossing)) < $kaiser_window_table_size; $j++) {
        $sample += int($src[$srcindex - $j] * ($table[$filterindex + $j * $samples_per_zero_crossing] + $interpolation * $differences[$filterindex + $j * $samples_per_zero_crossing]));
    }

    $interpolation = 1 - $interpolation;
    $filterindex = $interpolation * $samples_per_zero_crossing;
    for (my $j = 0; ($filterindex + ($j * $samples_per_zero_crossing)) < $kaiser_window_table_size; $j++) {
        $sample += int($src[$srcindex + 1 + $j] * ($table[$filterindex + $j * $samples_per_zero_crossing] + $interpolation * $differences[$filterindex + $j * $samples_per_zero_crossing]));
    }

    push @dst, $sample;

    # "After each output sample is computed, the time register is incremented by 2nl+nÎ· /Ï (i.e., time is incremented by 1/Ï in fixed-point format)."
    $time += 1.0 / $outrate;
}

open(OUT, '>:raw', $fnameout);

# this is inefficient, but this is just throwaway code...
foreach (@dst) {
    print OUT pack('s', $_);
}

close(OUT);

print("Done.\n");

