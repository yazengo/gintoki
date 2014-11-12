
info(randomhexstring(8))

r = version_cmp('^NightlyBuild%-(%d+)%-(%d+)$', 'NigthlyBuild-1111-1115', 'NightlyBuild-20130101-1100')
info(r)

r = version_cmp('^(%d+)%.(%d+)$', '3.0', '3.1')
info(r)

r = version_cmp('^(%d+)%.(%d+)$', '3.2', '3.1')
info(r)

