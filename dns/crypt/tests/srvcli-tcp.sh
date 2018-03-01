#! /bin/sh
# @todo rewrite in cmake language?
set -e
echo "Server - Client Test"
export cdns_TESTING_TIMEOUT=5
exe="$1"
echo Executables: $1, $2
ddir="$3"
tdir="$4"
srv="127.0.0.1:3050"
cli="127.0.0.1:4050"
echo "Data dir: $ddir"
echo "Test data dir: $tdir"

# valgrind
"$exe" -6 -t -a $srv  -m 7 --server-name "2.dnscrypt-cert" \
	-L "$ddir/dns-resolvers.txt" -P "$tdir/public.key" -S "$tdir/crypt_secret.key" \
	-C "$tdir/dnscrypt.cert" >& /tmp/log1-tcp &
sleep 1
echo "Client"
sed -e 's/3053/3050/' "${tdir}/resolvers-local1.csv" > tst-tcp-resolvers.csv
"$exe" -t -a $cli  -m 7  --crypt-resolvers tst-tcp-resolvers.csv \
	-W "$ddir/whitelist.txt" -B "$ddir/blacklist.txt" \
	-H "$ddir/hosts" >& /tmp/log2-tcp &
sleep 1
echo "Drill"
exe="$2"
"$exe"  -t xxx.com @$cli >& /tmp/log3-tcp &
"$exe"  -t www.archlinux.org @$cli >& /tmp/log4-tcp &
"$exe"  -t "retracker.local" @$cli >& /tmp/log5-tcp &
"$exe"  -t www.skype.com @$cli >& /tmp/log6-tcp &
wait
echo "Scanning server output"
grep -Hn "Requests total: 3, processed: 2, blacklisted: 0, cached: 0" /tmp/log1-tcp
# iterations are around 10
# queued is 1 - 2
grep -Hn "Requests still queued: ., upstream: 0, iterations: " /tmp/log1-tcp

echo "Scanning client output"
grep -Hn "Requests total: 4, processed: 4, blacklisted: 1, cached: 0" /tmp/log2-tcp
# iteratins are about 16
# grep -Hn "Requests still queued: 1, upstream: 2, iterations: " /tmp/log2-tcp
grep -Hn "Requests still queued: ., upstream: ., iterations: " /tmp/log2-tcp
echo "Scanning drill output"
grep -Hn "rcode: REFUSED" /tmp/log3-tcp
grep -n "rcode: SERVFAIL" /tmp/log4-tcp
grep -Hn "rcode: NOERROR" /tmp/log5-tcp
grep -Hn "rcode: SERVFAIL" /tmp/log6-tcp
echo "All Ok"
