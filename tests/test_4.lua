
-- test radio

print('-- radio test --')

info(cjson.encode(muno.info()))

info(cjson.encode(radio.info()))
radio.next()
info(cjson.encode(radio.info()))
radio.next()
info(cjson.encode(radio.info()))
radio.next()
info(cjson.encode(radio.info()))

