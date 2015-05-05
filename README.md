# json-fileops: Utilities for recording file info

* I need these utilities for some work I'm doing.
* I remember doing similar things before, with ad-hoc scripts and
arbitrary file formats.
* Wish I had thought of using http://jsonlines.org / http://ndjson.org last time!
* I'm sure I would use such utilities again, if I were more careful
about making them reusable, and therefore so would you.

This starts as a specification, *i.e.* pure vapourware.

## `*.fileops.json`: Record of operations upon a set of files
* Inputs
  * [ ] Web server logs
  * [ ] strace logs
  * [ ] Random read operations, taken from the set of file
* Outputs
  * [ ] Summary statistics for access speeds and concurrency
  * [ ] Re-do the operations
	* If the underlying filesystem were going to go *bang!* as a
	  result, this should be a nice way to reproduce the effect.
	* Aiming to start each at the same relative time
	* Emit updated timing info

There are two interchangeable ways to record this, I'm not yet sure
whether I want to deal with both.

### Per-filehandle record style

An strace-like format, where events are logged in chronological order
and so you need a (virtual) file descriptor number to tie the later
operations back to the initial `open`.

### Per-object record style

In which we name a file, then do some operations on it at various
times.

## `*.fileprops.json`: Record of file checksums

The initial application here is a large POSIX filesystem, for which we
currently have no checksums or integrity checks.  File may get updated
from time to time, and (at this time) I don't want to hook into
everything that touches them.

* Properties to store
  * File pathname
	* Ignoring any implicit root, to which names may be relative.
	* Ignoring the implicit hostname or filesystem mappings.
  * Whole file checksums, of any type (md5, sha1, sha2-512 ...)
  * Metadata
	* Size
	* mtime, ctime, atime
	* dev, inode, nlinks
  * Segmental file checksums?
	* As a reversible translation of (hashdeep)[https://github.com/jessek/hashdeep] output.
	* The advantage of segmental sums is that you don't have to read the entire file when making a random validation.
* Operations possible
  * Scan a directory tree and collect fileprops (metadata).  *Because actually reading the files to get the checksums may take weeks, and we will want to run that in parallel.*
  * Read fileprops and scan the directory tree.
	* check and update metadata, reporting changes and dropping stale checksums
	* fill in missing checksums
* Tools that solve part of the problem
  * (GNU findutils)[http://savannah.gnu.org/projects/findutils/], (GNU coreutils)[http://gnu.org/software/coreutils]
	* `find /stuff -type f -print0 | xargs -r0 -n5 -P4 sha1sum`
	* No way to detect intentional changes.
	* No way to keep several types of checksum, except in multiple files `scans.md5`, `scans.sha1` etc..
	* There are (other line-based plaintext formats)[https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man1/md5.1.html] which represent similar results in different ways.
  * (Summain)[http://liw.fi/summain/] does file scanning and checksumming, and has `--output-format json`.
	* You have to choose what to scan, and then decide whether the results are still fresh.
  * (Rsync)[https://rsync.samba.org/] does file change detection, based on metadata comparison; we have to be aware that
	* "dev" and/or "inode" may not be stable on some filesystem types.  *I'm using an NFS which makes the `Dev` field useless.*
	* timestamp granularity varies with filesystem type

### Sample data
I took the output style (Summain)[http://liw.fi/summain/] as my standard, and [made some changes](https://gitlab.com/mcast/summain) to the details.
```
$ summain -I --exclude={Username,Group,Atime,Dev} -c nil          -f jsonl todo.jq | tee sum0
{"Ctime":"2015-05-04 19:39:55.198461000 +0000","Gid":"808","Ino":"34399918","Mode":"100640","Mtime":"2015-05-04 19:39:55.198461000 +0000","Name":"todo.jq","Nlink":"1","Size":"357","Uid":"11179"}
$ summain -I --exclude={Username,Group,Atime,Dev} -c md5 -c sha256 -f jsonl todo.jq | tee sum2
{"Ctime":"2015-05-04 19:39:55.198461000 +0000","Gid":"808","Ino":"34399918","MD5":"efe45c3ac876bf4ae963069c18f17a0c","Mode":"100640","Mtime":"2015-05-04 19:39:55.198461000 +0000","Name":"todo.jq","Nlink":"1","SHA256":"b98236d0b92a8e4d069ac62352e58c8ae66d553560449547f8f518d42819df2c","Size":"357","Uid":"11179"}
$ jq . sum0 sum2
{
  "Uid": "11179",
  "Ctime": "2015-05-04 19:39:55.198461000 +0000",
  "Gid": "808",
  "Ino": "34399918",
  "Mode": "100640",
  "Mtime": "2015-05-04 19:39:55.198461000 +0000",
  "Name": "todo.jq",
  "Nlink": "1",
  "Size": "357"
}
{
  "Uid": "11179",
  "Size": "357",
  "SHA256": "b98236d0b92a8e4d069ac62352e58c8ae66d553560449547f8f518d42819df2c",
  "Ctime": "2015-05-04 19:39:55.198461000 +0000",
  "Gid": "808",
  "Ino": "34399918",
  "MD5": "efe45c3ac876bf4ae963069c18f17a0c",
  "Mode": "100640",
  "Mtime": "2015-05-04 19:39:55.198461000 +0000",
  "Name": "todo.jq",
  "Nlink": "1"
}
```

MD5 and SHA2-256 because they are what (iRODS)[http://irods.org/] needs for v3 and v4.
