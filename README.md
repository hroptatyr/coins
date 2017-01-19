coins
=====

A bunch of snarfers that connect to bitcoin and FX exchanges
via websocket or FIX.  Each exchange has its own binary.

The snarfers are running 24/7 to collect trade and quote data
for offline use.  Tools to convert the raw data stream to a
common format are provided.

The tools rely on openssl and libev.

The following exchanges are supported:
- coinbase
- bitfinex
- bitstamp
- poloniex
- okcoin
- hitbtc
- cex
- gemini
- btcc
- therocktrading
- bitmex
- deribit
- coinfloor
- fastmatch
- bitso
- huobi
- spacebtc

The following data providers are supported:
- kaiko
- forexfactory
