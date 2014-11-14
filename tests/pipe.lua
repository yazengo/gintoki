
require('pipe')

pcopy(pexec('echo hello', {stdout=true}).stdout, pexec('cat >hello.txt', {stdin=true}).stdin)

