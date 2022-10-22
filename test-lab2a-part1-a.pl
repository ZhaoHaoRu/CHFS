#!/usr/bin/perl -w

# test CREATE/MKNOD, LOOKUP, READDIR.

use strict;

if($#ARGV != 0){
    print STDERR "Usage: test-lab2a-part1-a.pl directory\n";
    exit(1);
}
my $dir = $ARGV[0];

my $seq = 0;

my $files = { };
my @dead;

for(my $i = 0; $i < 3; $i++){
    for(my $iters = 0; $iters < 100; $iters++){
        createone();
    }

    for(my $iters = 0; $iters < 100; $iters++){
        if(rand() < 0.1){
            livecheck();
        }
        if(rand() < 0.1){
            deadcheck();
        }
        if(rand() < 0.02){
            dircheck();
        }
        if(rand() < 0.5){
            createone();
        }
    }

    dircheck();

    chfscrash();
    chfsrestart();
    overallcheck();
}

printf "Passed all tests!\n";
exit(0);

sub createone {
    my $name = "file -\n-\t-";
    for(my $i = 0; $i < 40; $i++){
	$name .= sprintf("%c", ord('a') + int(rand(26)));
    }
    $name .= "-$$-" . $seq;
    $seq = $seq + 1;
    my $contents = rand();
    print "create $name\n";
    if(!open(F, ">$dir/$name")){
        print STDERR "test-lab2a-part1-a: cannot create $dir/$name : $!\n";
        exit(1);
    }
    close(F);
    $files->{$name} = $contents;
}

# make sure all the live files are there,
# and that all the dead files are not there.
sub dircheck {
    print "dircheck\n";
    opendir(D, $dir);
    my %h;
    my $f;
    while(defined($f = readdir(D))){
        if(!defined($h{$f})){
            $h{$f} = 0;
        }
        $h{$f} = $h{$f} + 1;
    }
    closedir(D);

    foreach $f (keys(%$files)){
        if(!defined($h{$f})){
            print STDERR "test-lab2a-part1-a.pl: $f is not in the directory listing\n";
            exit(1);
        }
        if($h{$f} > 1){
            print STDERR "test-lab2a-part1-a.pl: $f appears more than once in the directory\n";
            exit(1);
        }
    }

    foreach $f (@dead){
        if(defined($h{$f})){
            print STDERR "test-lab2a-part1-a.pl: $f is dead but in directory listing\n";
            exit(1);
        }
    }
}

sub livecheck {
    my @a = keys(%$files);
    return if $#a < 0;
    my $i = int(rand($#a + 1));
    my $k = $a[$i];
    print "livecheck $k\n";
    if(!open(F, "$dir/$k")){
        print STDERR "test-lab2a-part1-a: cannot open $dir/$k : $!\n";
        exit(1);
    }
    close(F);
    if( ! -f "$dir/$k" ){
	print STDERR "test-lab2a-part1-a: $dir/$k is not of type file\n";
	exit(1);
    }
    if(open(F, ">$dir/$k/xx")){
	print STDERR "test-lab2a-part1-a: $dir/$k acts like a directory, not a file\n";
        exit(1);
    }
}

sub deadcheck {
    my $name = "file-$$-" . $seq;
    $seq = $seq + 1;
    print "check-not-there $name\n";
    if(open(F, "$dir/$name")){
        print STDERR "test-lab2a-part1-a: $dir/$name exists but should not\n";
        exit(1);
    }
}

sub overallcheck {
    for(my $iters = 0; $iters < 50; $iters++){
        livecheck();
    }
    deadcheck();
    dircheck();
}

# to crash and restart chfs
sub chfsrestart {
    # restart
    print "===== ChFS Restart =====\n";
    system './start.sh';

    # wait until chfs restart
    my $time = 5;
    while($time--) {
        print "Wait for ChFS to mount...\n";
        if(mounted()) { # mounted successfully
            last;       # eq to 'break' in C
        } else {        # wait for mounting
            sleep(0.1);
        };
    }
    
    if (!mounted()) {
        print "Fail to restart chfs!\n";
        exit(1);
    }
}

sub chfscrash {
    print "===== ChFS Crash =====\n";
    system './stop.sh';
    if ($? == -1) {
        print "Failed to crash ChFS: $!\n";
    }
    # wait for old chfs to exit on its own
    while(mounted()) { 
        print "Wait for ChFS to unmount...\n";
        sleep(0.1);
    }
}

sub mounted {
    return (`mount | grep $dir | grep -v grep | wc -l` == 1);
}