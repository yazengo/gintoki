
import sys

res = []

for i in sys.stdin.readlines():
	if i[0] == '#':
		continue
	l = i.strip().split()
	if len(l) > 1:
		for t in l[1:]:
			res.append((t, l[0]))

res.sort()

print '''
#define MIMES_NR %d

typedef struct {
	char *ext;
	char *type;
} mime_t;

static mime_t mimes[MIMES_NR] = {''' % (len(res))
for i in res:
	print '\t{"%s", "%s"},' % i
print '};'

