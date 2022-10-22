#!/usr/bin/perl -w

# test CREATE/MKNOD, LOOKUP, READDIR.

use strict;
use threads;
use threads::shared;

my $dir = $ARGV[0];
my $seq = 0;
my $files = { };
my @dead;

my $progress :shared = 0;   # to sparse crash among operations
my $crashlock :shared = 0;  # to avoid concurrency of crash and check
my $crashstat :shared = 0;  # to mark crash status
my $crashcnt = 0;           # crash times

if($#ARGV != 0) {
    print STDERR "Usage: test-lab2a-part2-a.pl directory\n";
    exit(1);
}
if (!mounted()) {
    print STDERR "Please make sure ChFS is mounted before running this test!\n";
    exit(1);
}

# killer thread, kills chfs on a regular basis
my $thr = threads->create(sub {
    for(my $iters = 0; $iters < 8; $iters++) {
        # wait for next crash signal
        {
            lock($progress);
            # until ($progress >= ($iters * 10)) {
                cond_wait($progress);
            # }
        }

        # crash and restart
        {
            lock($crashlock);
            $crashstat = 1;
            chfscrash();
            chfsrestart();
            if (!mounted()) {last;}
            $crashstat = 0;
        }
    }
});

for(my $iters = 0; $iters < 200; $iters++){
    createone();
    lock($progress);
    $progress = $iters + 1;
    if (($progress % 20) == 0) {
        cond_broadcast($progress);
    }
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
printf "Passed all tests!\n";
clearandexit(0);

sub createone {
    my $name = "file -\n-\t-";
    for(my $i = 0; $i < 40; $i++){
	$name .= sprintf("%c", ord('a') + int(rand(26)));
    }
    $name .= "-$$-" . $seq;     # $$ = current pid
    $seq = $seq + 1;
    my $contents = rand();
    print "create $name\n";

    while(!mounted() || !open(F, ">$dir/$name")) {
        # success(1), end loop
        # fail(0) without crash, error
        # fail(0) with crash, wait for restart
        errhandle("test-lab2a-part2-a: cannot create $dir/$name", $!);
    }
    close(F);
    $files->{$name} = $contents;
}

# make sure all the live files are there,
# and that all the dead files are not there.
sub dircheck {
    lock($crashlock);
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
        if(!defined($h{$f})) {
            print STDERR "test-lab2a-part2-a.pl: $f is not in the directory listing\n";
            clearandexit(1);
        }
        
        if($h{$f} > 1) {
            print STDERR "test-lab2a-part2-a.pl: $f appears more than once in the directory\n";
            clearandexit(1);
        }
    }

    foreach $f (@dead){
        if(defined($h{$f})) {
            print STDERR "test-lab2a-part2-a.pl: $f is dead but in directory listing\n";
            clearandexit(1);
        }
    }
}

sub livecheck {
    lock($crashlock);
    my @a = keys(%$files);
    return if $#a < 0;
    my $i = int(rand($#a + 1));
    my $k = $a[$i];
    print "livecheck $k\n";
    if(!open(F, "$dir/$k")) {
        print STDERR "test-lab2a-part2-a: cannot open $dir/$k : $!\n";
        clearandexit(1);
    }
    close(F);
    if( ! -f "$dir/$k" ) {
        print STDERR "test-lab2a-part2-a: $dir/$k is not of type file\n";
        clearandexit(1);
    }
    if(open(F, ">$dir/$k/xx")) {
        print STDERR "test-lab2a-part2-a: $dir/$k acts like a directory, not a file\n";
        clearandexit(1);
    }
}

sub deadcheck {
    lock($crashlock);
    my $name = "file-$$-" . $seq;
    $seq = $seq + 1;
    print "check-not-there $name\n";
    if(open(F, "$dir/$name")) {
        print STDERR "test-lab2a-part2-a: $dir/$name exists but should not\n";
        clearandexit(1);
    }
}

# to crash and restart chfs
sub chfsrestart {
    # restart
    print "===== ChFS Restart =====\n";
    system './start.sh';
    
    if (!mounted()) {
        print "Fail to restart chfs!\n";
    }
}

sub chfscrash {
    print "===== ChFS Crash $crashcnt =====\n";
    $crashcnt++;
    system './stop.sh';
    # wait for old chfs to exit on its own
    while(mounted()) { 
        print "Wait for ChFS to unmount...\n";
        sleep(0.5);
        system './stop.sh';
    }
}

sub errhandle {
    my($msg, $err) = @_;

    if ($crashstat) {
        print "$msg due to system crash: $err\n";
        while($crashstat) {
            print "Wait for ChFS to mount...\n";
            sleep(1);
        }
        # if fail to mount
        if (!mounted()) {
            print "Cannot find ChFS, process exit\n";
            exit(1);
        }
    }
    else {
        print STDERR "$msg: $err\n";
        clearandexit(1);
    }
}

sub mounted {return (`mount | grep $dir | grep -v grep | wc -l` == 1);}

sub alive {return (`ps -auw | grep $dir | grep client | wc -l` == 1);}

sub clearandexit {
    $thr->join();
    exit($_[0]);
}