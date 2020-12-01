# Threadbank application

A simple server client application. 

Build using 'make'.
After building run:
Server with './server'
Client with './client'

Bank balance stored in ''bank_balance.txt'

server accepts following commands:
  "d 1 2" = "deposit 2 to account 1"
  "w 1 10" = "withdraw 10 from account 1"
  "l 1" = "give balance of account 1"
  "t 1 2 10" = "transfer amount 10 from account 1 to account 2"


please run first as this command gives server clients socket information,
so client can read information from its own terminal window
Also: If not done in this order there will be problems with sending the message.
I.e. will print random bits to servers terminal window,
This will not affect servers functionality.
