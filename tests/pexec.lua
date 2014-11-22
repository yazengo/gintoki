
w = pexec('cat >out', 'w')
r = pexec('ls -l .', 'r')
-- sox -t raw -r 64k -c 1 -e unsigned -b 8 - -d 
pcopy(r, w)

