
require('pipe')

apipe(fopen('a.txt'), fopen('b.txt', 'w'))

