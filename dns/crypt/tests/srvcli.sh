#! /bin/sh
# @todo rewrite in cmake language?
set -e
echo "Server - Client Test"
export cdns_TESTING_TIMEOUT=5
exe="$1"
echo Executables: $1, $2
ddir="$3"
tdir="$4"
echo "Data dir: $ddir"
echo "Test data dir: $tdir"

"$exe" -6 -a 127.0.0.1:3053 -m 7 --server-name "2.dnscrypt-cert" \
	-L "$ddir/dns-resolvers.txt" -P "$tdir/public.key" -S "$tdir/crypt_secret.key" \
	-C "$tdir/dnscrypt.cert" >& /tmp/log1 &
sleep 1
echo "Client"
"$exe" -a 127.0.0.1:4053 -m 7  --crypt-resolvers "${tdir}/resolvers-local1.csv" \
	-W "$ddir/whitelist.txt" -B "$ddir/blacklist.txt" -H "$ddir/hosts" >& /tmp/log2 &
sleep 1
echo "Drill"
exe="$2"
"$exe"  xxx.com @127.0.0.1:4053 >& /tmp/log3 &
"$exe"  www.archlinux.org @127.0.0.1:4053 >& /tmp/log4 &
"$exe"  "retracker.local" @127.0.0.1:4053 >& /tmp/log5 &
"$exe"  www.skype.com @127.0.0.1:4053 >& /tmp/log6 &
wait # Always exits with 0, even if waited for processes have failed
echo "Scanning server output"
grep -Hn "Requests total: 3, processed: 2, blacklisted: 0, cached: 0" /tmp/log1
# iterations is about 5
grep -Hn "Requests still queued: 1, upstream: 1, iterations: ." /tmp/log1

echo "Scanning client output"
grep -Hn "Requests total: 4, processed: 4, blacklisted: 1, cached: 0" /tmp/log2
# queued, upstream: 1-2, iterations: 7-8
grep -Hn "Requests still queued: ., upstream: ., iterations: ." /tmp/log2
echo "Scanning drill output"
grep -Hn "rcode: REFUSED" /tmp/log3
grep -Hn "rcode: SERVFAIL" /tmp/log4
grep -Hn "rcode: NOERROR" /tmp/log5
grep -Hn "rcode: SERVFAIL" /tmp/log6
echo "All Ok"
