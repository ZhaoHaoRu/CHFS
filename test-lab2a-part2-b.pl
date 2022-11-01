#!/usr/bin/perl -w

# Case 1: Single file system
# 1. create new files
# 2. open an existing file
# 3. write to a file
#      - write to the middle
#      - append
#      - write from middle and beyond file length
#      - seek beyond file length and write
# 4. read file content
#      from each case above
#
# Make sure file handles/file type for new files are correct.

# Case 2: Two file systems mounted under same root dir.
# 0. Start two fs with same rootdir
# 1. create files in dir1
# 2. read the files from dir2
#
#

use strict;
use threads;
use threads::shared;
$| = 1;

my $dir1 = $ARGV[0];
my $f1 = "a$$";
my $f2 = "b$$";
my $files = { };

my $progress :shared = 0;   # to sparse crash among operations
my $crashlock :shared = 0;  # to avoid concurrency of crash and check
my $crashstat :shared = 0;  # to mark crash status
my $crashcnt = 0;           # crash times

if($#ARGV != 0){
    print STDERR "Usage: test-lab2a-part2-b.pl directory1\n";
    exit(1);
}

if (!mounted()) {
    print STDERR "Please make sure ChFS is mounted before running this test!\n";
    exit(1);
}

print "Write and read one file: ";
writeone($dir1, $f1, 600);
checkcontent($dir1, $f1);
print "OK\n";

print "Write and read a second file: ";
writeone($dir1, $f2, 4111);
checkcontent($dir1, $f2);
checkcontent($dir1, $f1);
print "OK\n";

# killer thread, kills chfs on a regular basis
my $thr = threads->create(sub {
    for(my $iters = 0; $iters < 10; $iters++) {
        # wait for next crash signal
        {
            lock($progress);
            until ($progress > ($iters * 10)) {
                cond_wait($progress);
            }
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

for(my $iters = 0; $iters < 100; $iters++){
    print "round $iters\n";
    my $r = rand();
    if($r < 0.2){
        print "Write and read one file: ";
        writeone($dir1, $f1, int(rand(1024)));
        checkcontent($dir1, $f1);
        checkcontent($dir1, $f2);
        print "OK\n";
    }
    if ($r >= 0.2 and $r < 0.4) {
        print "Write and read second file: ";
        writeone($dir1, $f2, int(rand(1024)));
        checkcontent($dir1, $f1);
        checkcontent($dir1, $f2);
        print "OK\n";
    }
    
    if ($r >= 0.4 and $r < 0.65) {
        print "Write into the middle of one file: ";
        writeat($dir1, $f1, int(rand(length($files->{$f1}))));
        checkcontent($dir1, $f1);
        print "OK\n";
    }
    if ($r >= 0.65 and $r < 0.7) {
        print "Write into the middle of second file: ";
        writeat($dir1, $f2, int(rand(length($files->{$f2}))));
        checkcontent($dir1, $f2);
        print "OK\n";
    }

    if ($r >= 0.7 and $r < 0.85) {
        print "Append to one file: ";
        append($dir1, $f1, int(rand(1024)));
        checkcontent($dir1, $f1);
        print "OK\n";
    }
    if ($r >= 0.85) {
        print "Append to second file: ";
        append($dir1, $f2, int(rand(1024)));
        checkcontent($dir1, $f2);
        print "OK\n";
    }

    lock($progress);
    $progress = $iters;
    if (($progress % 9) == 1) {
        cond_broadcast($progress);
    }
}

print "Passed all tests\n";
clearandexit(0);

sub writeone {
    my($d, $name, $len) = @_;
    my $f = $d . "/" . $name;

    # prepare for write contents
    my $contents = "";
    while(length($contents) < $len){
	$contents .= rand();
    }
    $contents = substr($contents, 0, $len);
    $files->{$name} = $contents;

    use FileHandle;

    # write file
    while(1) {
        while(!mounted() || !sysopen F, $f, O_TRUNC|O_RDWR|O_CREAT) {
            errhandle("test-lab2a-part2-b: cannot create $f", $!);
        }

        if (!mounted() || !defined(syswrite F, $files->{$name}, length($files->{$name}))) {
            errhandle("test-lab2a-part2-b: cannot write to $f", $!);
            close(F);
        } else {
            last;
        }
    }

    close(F);
}

sub append {
    my($d, $name, $n) = @_;
    my $f = $d . "/" . $name;
    my $end = length($files->{$name});

    # prepare for write contents
    my $contents = "";
    while(length($contents) < $n){
	$contents .= rand();
    }
    $contents = substr($contents, 0, $n);
    $files->{$name} .= $contents; ## Append the file content
    # my $sz = length($files->{$name});
    # print("file $f will be $sz");

    use FileHandle;

    # write file
    while(1) {
        while(!mounted() || !sysopen F, $f, O_RDWR) {
            errhandle("test-lab2a-part2-b: cannot open $f for append", $!);
        }

        # goto end of file
        if (!mounted() || !seek(F, $end, 0)) {
            close(F);
            next;
        }

        my $sz = length($contents);
        if (!mounted() || !defined(syswrite F, $contents, $sz, 0)) {
            errhandle("test-lab2a-part2-b: cannot append $sz bytes to $f", $!);
            close(F);
        } else {
            last;
        }
    }

    close(F);
}

sub writeat {
    my($d, $name, $off) = @_;
    my $f = $d . "/" . $name;

    # prepare for write contents
    my $contents = rand();
    my $x = $files->{$name};
    if (length($x) < $off + length($contents)) {
      my $nappend = $off + length($contents) - length($x);
      for (my $i=0; $i < $nappend; $i++) {
        $x .= "\0";
      }
    }
    substr($x, $off, length($contents)) = $contents;
    $files->{$name} = $x;

    use FileHandle;

    # write file
    while(1) {
        while(!mounted() || !sysopen F, $f, O_RDWR) {
            errhandle("test-lab2a-part2-b: cannot open $f for read/write", $!);
        }

        # goto end of file
        if (!mounted() || !seek(F, $off, 0)) {
            close(F);
            next;
        }

        if (!mounted() || !defined(syswrite(F, $contents, length($contents), 0))) {
            errhandle("test-lab2a-part2-b: cannot write $f at offset $off", $!);
            close(F);
        } else {
            last;
        }
    }

    close(F);
}

# make sure all the live files are there,
# and that all the dead files are not there.
sub checkcontent {
    lock($crashlock);
    my($d, $name) = @_;

    my $f = $d . "/" . $name;

    open F, "$f" or die "could not open $f for reading";
    my $c2 = "";
    while(<F>) {
      $c2 .= $_;
    }
    close(F);
    my $sz = length($files->{$name});
    $files->{$name} eq $c2 or die "content of $f is incorrect, should be size $sz: \n$files->{$name}\n";
}

sub checknot {
    lock($crashlock);
    my($d, $name) = @_;

    my $f = $d . "/" . $name;

    my $x = open(F, $f);
    if(defined($x)){
        print STDERR "$x exists but should not\n";
        exit(1);
    }
}

sub dircheck {
    lock($crashlock);
    my($dir) = @_;

    opendir(D, $dir);
    my %h;
    my $f;
    while(defined($f = readdir(D))){
        if(defined($h{$f})){
            print STDERR "$f appears more than once in directory $dir\n";
            exit(1);
        }
        $h{$f} = 1;
    }
    closedir(D);

    foreach $f (keys(%$files)){
        if(!defined($h{$f})){
            print STDERR "$f is missing from directory $dir\n";
            exit(1);
        }
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

sub mounted {return (`mount | grep $dir1 | grep -v grep | wc -l` == 1);}

sub alive {return (`ps -auw | grep $dir1 | grep client | wc -l` == 1);}

sub clearandexit {
    $thr->join();
    exit($_[0]);
}