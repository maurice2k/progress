progress
========

cat/zcat with progress information and load management

Ever wished to see how long it takes MySQL to import a big dump?
Curious whether GnuPG finishes encrypting your 4GB file within this century?

Then progress might be the tool of your choice. It reads in a file given as parameter
and prints progress information to STDERR while it outputs the file itself to STDOUT.
progress also supports gzipped files (extracts them automatically).

An example:
<pre>
# progress --max-load=3 really-big-dump.sql.gz | mysql -D mydb -u myuser
progress: 00:00 -- really-big-dump.sql.gz -- starting (5368342015 bytes)...
progress: 00:03 -- really-big-dump.sql.gz --   1.0% (12.1 MB/s) -- ETA 06:59
progress: 00:10 -- really-big-dump.sql.gz --   2.0% (10.2 MB/s) -- ETA 08:13
progress: 00:18 -- really-big-dump.sql.gz --   3.0% (7.9 MB/s) -- ETA 10:31
progress: 00:18 -- really-big-dump.sql.gz -- cpu load exceeded (3.06), sleeping for 5 seconds
progress: 00:23 -- really-big-dump.sql.gz -- cpu load exceeded (3.02), sleeping for 5 seconds
...
progress: 01:31 -- really-big-dump.sql.gz --   4.0% (4.2 MB/s) -- ETA 17:10
progress: 01:33 -- really-big-dump.sql.gz --   5.0% (4.2 MB/s) -- ETA 16:57
progress: 01:35 -- really-big-dump.sql.gz --   6.0% (4.5 MB/s) -- ETA 15:44
progress: 01:41 -- really-big-dump.sql.gz --   7.0% (4.9 MB/s) -- ETA 15:14
...
progress: 17:13 -- really-big-dump.sql.gz -- finished.
</pre>
