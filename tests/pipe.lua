
require('pipe')

pcopy(pexec('echo hello', {stdout=true}), pexec('cat >hello.txt', {stdin=true}))

