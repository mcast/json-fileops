#! /usr/bin/env perl
use strict;
use warnings;

=head1 NAME

clfx2jsonl.pl - mash extended-CLF nginx access logs into .jsonl

=head1 SYNOPSIS

 # translate access.log to json, examine first request object
 $ web-fileops/clfx2jsonl.pl --web web-fileops/eg.access.log | head -n1 | jq .
 {
   "remoteIP": "10.0.0.31",
   "time": "30/Mar/2015:16:24:53 +0100",
   "timeHi": 1427729093.652,
   "method": "GET",
   "request": "/BEGIN",
   "httpv": "1.1",
   "status": 404,
   "bodyBytes": 168,
   "referer": "-",
   "userAgent": "libwww-perl/6.03",
   "vhost": "mysite:8080",
   "_ORIG": "10.0.0.31 - - [30/Mar/2015:16:24:53 +0100] \"GET /BEGIN HTTP/1.1\" 404 168 \"-\" \"libwww-perl/6.03\" io<145in 317out 0.007sec T1427729093.652> con<3108wpid OK. 1475#con 1#req> 206<-> fwd<-> mysite:8080\n",
   "byteIn": 145,
   "byteOut": 317,
   "reqTime": 0.007,
   "range": null,
   "fwd": "-",
   "_CON": "3108wpid OK. 1475#con 1#req",
   "_SRC": "web-fileops/eg.access.log",
   "_LINE": 1
 }
 # keys manually sorted in the example, for ease of comprehension

 # fileop output; ignore requests unless matched vhost && htdocs-mount
 $ web-fileops/clfx2jsonl.pl -F web-fileops/eg.access.log --vhost mysite:8080 --mount '/./autoconf/nfs'=/nfs | tail -n1 | jq .
 {
   "elapsed": 0.134,
   "vfd": [
     { "fn": "/nfs/bigstash/00000171/1929736.bam",
       "T": 1430210770.582,
       "Op": "open" },
     { "Op": "fstat" },
     { "Op": "read",
       "bytes": [ 122454016, 122748927 ] },
     { "Op": "close" }
   ]
 }


=head1 DESCRIPTION

A quick mess to digest some CLF with extended debug data.

Field names based on L<http://mark-kay.net/2015/03/17/web-server-logs-convert-to-json-and-upload-to-logentries/>.
It is just the first hit I found giving JSONified access_log, but
still better than inventing a new set of field names.

(CLFX is my acronym for CLF plus stuff.)

=head1 RATIONALE

The L<Common Log Format|https://en.wikipedia.org/wiki/Common_Log_Format> or L<Combined Log Format|https://httpd.apache.org/docs/trunk/logs.html#combined> are the de-facto standards.

=over 2

=item * CLF lacks the necessary temporal precision for my purposes, so I need to extend it.

=item * There is no standard way to extend it.

L<Extended Log Format|http://www.w3.org/TR/WD-logfile> is an alternative format.  L<(Acronym collsion!)|https://en.wikipedia.org/wiki/ELF#Computing>

=back

I just throw some extra text on the end of each CLF line.
I already have yet another CLFX (CLF + stuff) parser...

=head2 Config for nginx

 # For nginx.conf
 log_format combinedio '$remote_addr - $remote_user [$time_local] '
   '"$request" $status $body_bytes_sent '
   '"$http_referer" "$http_user_agent" '
   'io<${request_length}in ${bytes_sent}out ${request_time}sec T${msec}> con<${pid}wpid $request_completion$pipe ${connection}#con ${connection_requests}#req> 206<$sent_http_content_range> fwd<$http_x_forwarded_for> $server_name:$server_port';
 access_log logs/access.log combinedio;

=head1 SEE ALSO

=over 2

=item * L<http://logstash.net/>

=item * L<Fluentd vs Logstash|http://jasonwilder.com/blog/2013/11/19/fluentd-vs-logstash/>

=item * L<http://goaccess.io/>

=back

=cut


use 5.010; # need %+
use Getopt::Long;
use JSON;


sub main {
  $| = 1;
  my $J = JSON->new->utf8->indent(0)->space_before(0)->space_after(0);
  # $J->canonical(1);  # sort keys?  don't need

  my %opt;
  GetOptions(\%opt,
             'help|h',
             'web|W', 'fileop|F', # mutex
             'vhost=s@',
             'mount=s%',
            ) or $opt{help} = 1;
  $opt{help} = 1 unless $opt{web} xor $opt{fileop};
  die "Syntax: $0 < --web | --fileop > [ opts... ] <access.log>+\n" if $opt{help};

  my %want_vhost = map {($_ => 1)} @{ $opt{vhost} || [] };

  # XXX: order should matter, but I didn't bother
  my @mount = map {[ qr{^$_(?<end>.*)$} => $opt{mount}{$_} ]} keys %{ $opt{mount} || {} };

  while(<>) {
    my %req;
    %req = (_ORIG => $_, _SRC => $ARGV, _LINE => $.); # DEBUG

    # Combined Log Format https://httpd.apache.org/docs/trunk/logs.html#combined
    (@req{qw{ remoteIP time method request httpv status bodyBytes referer userAgent }}, my $TAIL) =
      m{^(\S+) - - \[([^[\]]+)\] "([A-Z]+) +(\S+) +HTTP/([0-9.]+)" (\d+) (\d+) "([^"]*)" "([^"]*)" (.*)$}
      or die "Choked on CLF $ARGV:$.:$_";

    if ($TAIL ne '') {
      # CLFX: Arbitrary stuff appended to each line.
      # These are just the fields I'm using.
      my @more = $TAIL =~
        m{^io<(\d+)in (\d+)out (\d+\.\d+)sec T(\d+\.\d+)> con<([^<>]+)> 206<([^<>]*)> fwd<([^<>]*)> (\S+)$};
      if (@more) {
        @req{qw{ byteIn byteOut reqTime timeHi _CON range fwd vhost }} = @more;
        $req{range} = parse_range($req{range});
      } else {
        die "Choked on CLFX tail $ARGV:$.:$TAIL";
      }
    }

    foreach my $f (qw( status bodyBytes byteIn byteOut reqTime timeHi )) {
      next unless defined $req{$f};
      $req{$f} += 0; # numify
      # timeHi in double precision float: good for 1 microsecond until 2001-09-09, ~10us until 2286
    }

    if (keys %want_vhost) {
      next unless $want_vhost{ $req{vhost} };
    }

    if (@mount) {
      my @hit = map { $req{request} =~ $_->[0] ? ($_->[1] . $+{end}) : () } @mount;
      next unless @hit;
      $req{file} = $hit[0];
    }

    if ($opt{web}) {
      print $J->encode(\%req), "\n";

    } else {
      # Mash request into fileop style.  Expected POSIX operations,
      # ditch the other fields
      my ($start, $end) = $req{range} ? @{ $req{range}{bytes}->[0] } : (0, $req{bodyBytes} - 1);
      my %fileop =
        (vfd =>
         [ { Op => 'open', T => $req{timeHi},
             fn => $req{ defined $req{file} ? 'file' : 'request'}, },
           { Op => 'fstat' },
           { Op => 'read', bytes => [ $start, $end ] },
           { Op => 'close' } ],
         elapsed => $req{reqTime});
      print $J->encode(\%fileop), "\n";
    }

  } continue {
    close ARGV if eof; # to keep $ARGV right
  }

  return 0;
}

exit main();


sub parse_range {
  my ($in) = @_;
  if ($in eq '-') {
    return ();
  } elsif ($in =~ m{^bytes (\d+)-(\d+)/(\d+)$}) { # single range
    return +{ bytes => [ [$1+0, $2+0] ], of => $3+0 };
  } else {
    die "parse_range: failed on '$in'";
  }
}
