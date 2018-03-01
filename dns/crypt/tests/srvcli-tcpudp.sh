#! /bin/sh
# @todo rewrite in cmake language?
set -e
echo "Server - Client Test TCP&UDP"
export cdns_TESTING_TIMEOUT=5
exe="$1"
echo Executables: $1, $2
ddir="$3"
tdir="$4"
srv="127.0.0.1:3051"
cli="127.0.0.1:4051"
logs="-tcpudp.txt"
logp="/tmp/log"
cryptresolv=tst-tcpudp-resolvers.csv
echo "Data dir: $ddir"
echo "Test data dir: $tdir"

# valgrind
"$exe" -6 -t -a $srv  -m 7 --server-name "2.dnscrypt-cert" \
	-L "$ddir/dns-resolvers.txt" -P "$tdir/public.key" -S "$tdir/crypt_secret.key" \
	-C "$tdir/dnscrypt.cert" >& ${logp}1$logs  &
sleep 1
echo "Client"
sed -e 's/3053/3051/' "${tdir}/resolvers-local1.csv" > ${cryptresolv}
"$exe" -a $cli  -m 7  --crypt-resolvers ${cryptresolv} \
	-W "$ddir/whitelist.txt" -B "$ddir/blacklist.txt" \
	-H "$ddir/hosts" >& ${logp}2$logs &
sleep 1
echo "Drill"
exe="$2"
"$exe"  xxx.com @$cli >& ${logp}3$logs  &
"$exe"  www.archlinux.org @$cli >& ${logp}4$logs &
"$exe"  "retracker.local" @$cli >& ${logp}5$logs &
"$exe"  www.skype.com @$cli >& ${logp}6$logs &
wait
echo "Scanning server output"
grep -Hn "Requests total: 3, processed: 2, blacklisted: 0, cached: 0" ${logp}1$logs
# iterations are around 10
# queued is 1 - 2
grep -Hn "Requests still queued: ., upstream: 0, iterations: " ${logp}1$logs

echo "Scanning client output"
grep -Hn "Requests total: 4, processed: 4, blacklisted: 1, cached: 0" ${logp}2$logs
# iteratins are about 16
# grep -Hn "Requests still queued: 1, upstream: 2, iterations: " ${logp}2$logs
grep -Hn "Requests still queued: ., upstream: ., iterations: " ${logp}2$logs
echo "Scanning drill output"
grep -Hn "rcode: REFUSED" ${logp}3$logs
grep -n "rcode: SERVFAIL" ${logp}4$logs
grep -Hn "rcode: NOERROR" ${logp}5$logs
grep -Hn "rcode: SERVFAIL" ${logp}6$logs
echo "All Ok"
