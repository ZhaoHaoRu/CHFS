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
$| = 1;

if($#ARGV != 0){
    print STDERR "Usage: test-lab2a-part3-a.pl directory1\n";
    exit(1);
}
my $dir1 = $ARGV[0];
my $f1 = "a$$";
my $files = { };

my $logfile = "log/logdata.bin";
my $snpfile = "log/checkpoint.bin";
my $MAX_LOG_SZ = 131072;
my $MAX_SNP_SZ = 16777216;

for(my $iters = 0; $iters < 30; $iters++){
    print "Write and read one file: ";
    writeone($dir1, $f1, 6000);
    checkcontent($dir1, $f1);
    print "OK\n";
}

print "Check directory listing: ";
dircheck($dir1);
print "OK\n";

chfscrash();

# check the size of logfile and checkpoint
print "Check logfile and checkpoint size: \n";
my $logsize = checksize($logfile);
my $snpsize = checksize($snpfile);
print "\tlogfile $logsize bytes, checkpoint $snpsize bytes\n";
if ($logsize > $MAX_LOG_SZ) {
    print "Logfile too big, try to do checkpoint!\n";
    exit(0);
}
if ($snpsize > $MAX_SNP_SZ) {
    print "Checkpoint too big, try to do some optimization!\n";
    exit(0);
}

chfsrestart();
overallcheck();

print "Passed all tests\n";
exit(0);

sub writeone {
    my($d, $name, $len) = @_;
    my $contents = "";

    my $f = $d . "/" . $name;

    use FileHandle;
    sysopen F, $f, O_TRUNC|O_RDWR|O_CREAT
	or die "cannot create $f\n";

    while(length($contents) < $len){
	$contents .= rand();
    }
    $contents = substr($contents, 0, $len);
    $files->{$name} = $contents;

    syswrite F, $files->{$name}, length($files->{$name}) 
	or die "cannot write to $f";
    close(F);
}

sub checkcontent {
    my($d, $name) = @_;

    my $f = $d . "/" . $name;

    open F, "$f" or die "could not open $f for reading";
    my $c2 = "";
    while(<F>) {
      $c2 .= $_;
    }
    close(F);
    $files->{$name} eq $c2 or die "content of $f is incorrect\n";
}

sub checknot {
    my($d, $name) = @_;

    my $f = $d . "/" . $name;

    my $x = open(F, $f);
    if(defined($x)){
        print STDERR "$x exists but should not\n";
        exit(1);
    }
}

sub dircheck {
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

sub checksize {
    my($f) = @_;
    if (!(-e $f)) {
        # print "Cannot find file $f.\n";
        return 0;
    }

    my @meta = stat($f);
    return $meta[7];
}

sub overallcheck {
    checkcontent($dir1, $f1);
    checknot($dir1, "z-$$-z");
    dircheck($dir1);
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
    # `pkill -SIGUSR1 chfs_client`;
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
    return (`mount | grep $dir1 | grep -v grep | wc -l` == 1);
}
