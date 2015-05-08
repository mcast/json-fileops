#!/usr/bin/env perl
use strict;
use warnings;

=head1 NAME

strace2jsonl.pl - mash L<strace(1)> log into .jsonl

=head1 SYNOPSIS

 $ strace -o strace.log -ff -i -q -tt -T -v -x -s 256 "$@"

 $ strace-fileops/strace2jsonl.pl --path ^/nfs/bigstash strace-fileops/eg.strace.log | head -n10
 {"Op":"open","T":52071.058502,"vfd":1,"fn":"/nfs/bigstash/00000035/457682.v1.refract.bwa_mapped.bam.bai","elapsed":0.047604}
 {"Op":"fstat","T":52071.106161,"vfd":1,"elapsed":3.3e-05}
 {"bytes":[0,3402624],"_op":"sendfile","Op":"read","T":52071.1064,"vfd":1,"elapsed":0.035665}
 {"bytes":[163840,3402624],"_op":"sendfile","Op":"read","T":52071.193666,"vfd":1,"elapsed":8.8e-05}
 {"bytes":[344064,3402624],"_op":"sendfile","Op":"read","T":52071.210837,"vfd":1,"elapsed":0.008685}
 {"Op":"open","T":52071.250927,"vfd":2,"fn":"/nfs/bigstash/00000563/865736.bam.bai","elapsed":0.043914}
 {"Op":"fstat","T":52071.294942,"vfd":2,"elapsed":6.5e-05}
 {"bytes":[0,5307656],"_op":"sendfile","Op":"read","T":52071.295332,"vfd":2,"elapsed":0.045715}
 {"bytes":[540672,3402624],"_op":"sendfile","Op":"read","T":52071.363604,"vfd":1,"elapsed":0.005004}
 {"bytes":[163840,5307656],"_op":"sendfile","Op":"read","T":52071.392024,"vfd":2,"elapsed":8.3e-05}


=head1 DESCRIPTION

Parse L<strace(1)> log output, provided it was made with the expected
options - something like C<-ff -i -q -tt -T -v -x -s 256>.

Track certain (hard-wired) calls made upon file descriptors matching
the filename filter.

Emit the abridged translation in .fileops.jsonl style.

=head1 CAVEATS

This script covers just enough strace parsing to read am trace of
static files served by L<nginx(1)> with L<sendfile(2)>.

=cut


use JSON;
use Getopt::Long;
use Time::Local;


my %fd; # set of interesting fd numbers
sub close_all {
  my ($jsonl, $fdh, $why) = @_;
  foreach my $vfd (sort { $a <=> $b } values %$fdh) {
    $jsonl->({ Op => 'close', vfd => $vfd, why => $why });
  }
  %$fdh = ();
  return;
}

sub main {
  $| = 1;
  my $J = JSON->new->utf8->indent(0)->space_before(0)->space_after(0);
  # $J->canonical(1);  # sort keys?  don't need
  my $jsonl = sub { print $J->encode(@_), "\n" };

  my %opt;
  GetOptions(\%opt,
             'help|h',
             'debug|d',
             'toffset|T=f', # not convinced this is useful here
             'path=s@',
            ) or $opt{help} = 1;
  die "Syntax: $0 [ --path <path_regexp> ]* [ --debug ] <strace.log>+\n" if $opt{help};
  my @want_path = map {qr{$_}} @{ $opt{path} || [] };

  # know about functions we're likely to see, and keep quiet about them
  my %ignore_fn = map {($_ => 1)} _dull_fns();

  my $T_IP_RE = qr{(\d\d:\d\d:\d\d\.\d+) \[ *([0-9a-f]+)\] };
  while (<>) {
    next if /^$/; # blank lines are in ./eg.strace.log

    my $show = 0;
    if (my ($t,$ip, $fn, $args, $rc, $fn_t) =
        m{^$T_IP_RE(\w+)\((.*?)\) = (.*?)(?: <(\d+\.\d+)>)?$}) {

      if ($rc eq '?' || !defined $fn_t) {
        # ($rc,$fn_t)=('?',undef) when function didn't return
        if ($fn =~ m{^exit(_group)?$}) {
          close_all($jsonl, \%fd, [ $fn => $ARGV, $. ]);
          next;
        } else {
          die "fn $fn didn't return";
        }
      }

      my $tfrac = tstr($t);
#      $opt{toffset} = $tfrac if !defined $opt{toffset}; # makes picosecond noise in output
      $tfrac -= $opt{toffset} if defined $opt{toffset};
      die "Negative relative time $t - $opt{toffset} = $tfrac" if $tfrac < 0;
      my @Op = (elapsed => $fn_t+0, T => $tfrac, 'Op');
      unshift @Op, (_ORIG => $_, _SRC => $ARGV, _LINE => $.) if $opt{debug};

      if ($fn eq 'open') {
        my @arg = argstr($args);
        next unless grep { $arg[0] =~ $_ } @want_path;
        my %op = (@Op => $fn, fn => $arg[0]);
        if ($rc =~ /^-\d+( |$)/) {
          $op{err} = $rc; # error
        } elsif ($rc =~ /^\d+$/) {
          $op{vfd} = $fd{$rc} = vfd();
        } else {
          die "Unexpected return from $fn($arg[0]...)=$rc";
        }
        $jsonl->(\%op);

      } elsif ($fn eq 'sendfile') {
        my ($outfd, $fd, $offset, $count) = my @arg = argstr($args);
        next unless defined $fd{$fd};
        my %op = (@Op => 'read', _op => $fn, vfd => $fd{$fd});
        die "offset was $offset ?" unless ref($offset) eq 'ARRAY';
        if ("@$offset" eq 'const NULL') {
          # read from file pos
          $op{count} = $count + 0;
        } elsif (1 == @$offset && (my $o = $offset->[0]) =~ /^\d+$/) {
          # read independent of file pos
          $op{bytes} = [ $o + 0, $o + $rc ];
        } else {
          die "Incomprehensible $fn".$J->encode(\@arg);
        }
        $jsonl->(\%op);

      } elsif ($fn =~ m{^(close|fstat|lseek|ioctl|fcntl|p?writev?)$}) {
        my ($fd) = my @arg = argstr($args);
        next unless defined $fd{$fd};
        my %op = (@Op => $fn, vfd => $fd{$fd});

        if ($fn eq 'close') {
          delete $fd{$fd};
        } elsif ($fn eq 'fstat') {
          # enough
        } elsif ($fn eq 'lseek') {
          my $whence = $arg[2];
          if (ref($whence) && "@$whence" =~ /^const SEEK\S+$/) {
            $whence = $whence->[1];
          }
          @op{qw{ offset whence }} = ($arg[1], $whence);
        } else {
          die "fn $fn($args) is not yet translated\n".$J->encode(\@arg);
        }

        $jsonl->(\%op);

      } elsif ($fn =~ m{^p?(read)v?$}) {
        my ($fd, undef, $want) = my @arg = argstr($args);
        next unless defined $fd{$fd};
        my %op = (@Op => $fn, vfd => $fd{$fd},
                  want => $want, got => $rc);
        $jsonl->(\%op);

      } elsif ($fn =~ m{^f(seek|tell|[gs]etpos|eof|error)$|^(rewind|clearerr|fileno)$}) {
        # stream operations - not yet supported
        my ($stream) = my @arg = argstr($args);
        die "fn $fn($args) is not yet translated\n".$J->encode(\@arg);

      } elsif ($fn =~ m{^f(read|write|(|d|re)open)$}) {
        # more stream operations - not yet supported
        die "fn $fn($args) is not yet translated\n";

      } elsif ($fn eq 'mmap') {
        my ($addr, $length, $prot, $flags, $fd, $offset) = argstr($args);
        next unless defined $fd{$fd};
        die "fn $fn($args) cannot be handled";
        # how would we know what bytes were fetched?

      } else {
        warn "Ignored fn=$fn\n" unless exists $ignore_fn{$fn};
      }

      next; # reusing ($t, $ip) in next case
    }

    if (my ($t, $ip, $sig, $info) =
             m{^$T_IP_RE--- (SIG\w+) (.*) ---$}) {
      # ignore arrival of signal
      next;
    }

    die "Did not understand $ARGV:$.:$_";

  } continue {
    if (eof) {
      close_all($jsonl, \%fd, [ EOF => $ARGV, $. ]);
      close ARGV;
    }
  }
  return 0;
}

exit main();

my $_vfd; # a "virtual" file descriptor
sub vfd {
  $_vfd = 1 if !$_vfd;
  return $_vfd++;
}

# we probably lack a date, leave it relative to 1970-01-01 UTC
sub tstr {
  my ($tstr) = @_;
  my ($y,$m,$d) = (1970,1,1);
  my ($H,$M,$S,$us) =
    $tstr =~ m{^(\d{2}):(\d{2}):(\d{2})\.(\d{6})$}
    or die "Parse tstr($tstr) failed";
  my $t = timegm($S,$M,$H, $d, $m-1, $y-1900);
  $t .= ".$us";
  return $t + 0;
}

sub argstr {
  my ($in_txt, $alt_end) = @_;
  my $txt = ref($in_txt) ? $$in_txt : $in_txt;
  my $orig = $txt;
  my @out;
  my $end = $alt_end ? qr{(?=$alt_end)} : qr{$};

  until ($txt =~ m{^$end}) {
    if ($txt =~ s/^"//) {
      # "text" ends with (\.{3})?(,|$end)
      my $item = '';
      while(1) {
        if ($txt =~ s/^"((?:\.{3})?)(, |$end)//) {
          $item .= "[...]" if $1 eq '...'; # XXX: no oob way to represent the truncation here
          last;
        }

        if ($txt =~ s/^([^\\"]+)//) {
          $item .= $1;
        } elsif ($txt =~ s/^\\(r|n|t|\\|")//) {
          $item .= { r => "\r", n => "\n", t => "\t", "\x5C" => "\x5C", '"' => '"' }->{$1};
        } elsif ($txt =~ s/^\\x([a-f0-9]{2})//) {
          $item .= chr(hex($1));
        } else {
          $item =~ s{([^ -~])}{sprintf("\\x%02x", ord($1))}eg;
          die "Cannot parse argstr($orig) after q{$item>>HERE>>$txt}";
        }
      }
      push @out, $item;
    } elsif ($txt =~ s/^(-?\d+(?:\.\d+)?)(, |$end)//) {
      push @out, $1+0;
    } elsif ($txt =~ s/^([A-Z_|]+)(?:, |$end)//) {
      push @out, [ const => split / *(\|) */, $1 ];

    } elsif ($txt =~ s{^(\{st_dev=[- (),/0-9:=A-Z_a-z|]+\})(, |$end)}{}) {
      # XXX: struct is not parsed
      push @out, $1;

    } elsif ($txt =~ s{^\[}{}) {
      # pointer to something
      my @item = argstr(\$txt, qr{\](, |$end)});
      push @out, \@item;

    } elsif ($txt =~ s{^\{}{}) {
      # some kind of struct? writev(, iovec,) has them
      my @item = argstr(\$txt, qr{\}(, |$end)});
      push @out, \@item; # XXX: might need to change

    } else {
      die "Cannot parse argstr($orig|$end) HERE>>$txt";
    }
  }

  if (ref($in_txt)) {
    # send remainder back to recursing calls
    $txt =~ s{^$alt_end}{} or die "argstr($orig|$alt_end): unclosed HERE>>$txt";
    $$in_txt = $txt;
  }

  return @out;
}


sub _dull_fns {
  my @dull;

  # functions which aren't poking file descriptors or files
  push @dull, (<<DULLFNS =~ m{(\w+)}g);

 arch_prctl
 brk
 getegid
 geteuid
 getgid
 getpgrp
 getpid
 getppid
 getrlimit
 getuid
 mprotect
 prctl
 rt_sigaction
 rt_sigprocmask
 rt_sigreturn
 rt_sigsuspend
 set_tid_address
 set_robust_list
 setitimer
 setsid
 uname
 umask
 wait4
 futex

DULLFNS

  # functions touch files or file descriptors, but not in a way we're
  # interested in now
  push @dull, (<<DULLFNS =~ m{(\w+)}g);

 accept4
 bind
 connect
 getsockname
 getsockopt
 listen
 recvfrom
 recvmsg
 sendmsg
 sendto
 setsockopt
 shutdown
 socket
 socketpair

 epoll_create
 epoll_ctl
 epoll_wait

 poll
 select

 access
 clone
 dup2
 execve
 fadvise64
 fork
 getdents
 ioctl
 mkdir
 munmap
 openat
 splice
 stat
 unlink
 vfork

DULLFNS

  return @dull;
}
