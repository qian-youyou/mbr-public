# Master Banker Relay

The Master Banker Relay (MBR) is a relay/sharding process that allow to
have more than one [RTBKit](http://www.rtbkit.org) Master Banker (MB) running
at the same time.

## Building

**Depencies**

* Ubuntu 12.04 / Ubuntu 14.04
* cmake (Tested with 2.8.15)
* gflags (latest) <https://github.com/gflags/gflags>
* glog v0.3.3 <https://github.com/google/glog>
* libevent2 <http://libevent.org/>
* boost 1.52 or higher


**Clone the repo and build**

```
$ git clone git@github.com:Motrixi/mbr-public.git mbr
$ cd mbr
$ mkdir build
$ cd build
```

If you want to use a specific boost version (ie the one installed by RTBKit's
platform deps set) or if CMake has trouble finding it, set the following environment
variables before running CMake.

```
$ export CMAKE_LIBRARY_PATH="/home/some_user/local/lib:/usr/local/lib:$CMAKE_LIBRARY_PATH"
$ export CMAKE_INCLUDE_PATH="/path/to/platform-deps/boost-svn/:/usr/local/include:$CMAKE_INCLUDE_PATH"
```

and then run cmake and build :

```
mbr/build $ cmake ..
mbr/build $ make
```

you can also run *make install* if you'd like the MBR to be installed system-wide.

In case you want to compile with **a lot** of info log (don't do this on production) run cmake 
and build like this : 

```
mbr/build $ cmake .. -DVERBOSELOGHIGH:BOOLEAN=true
mbr/build $ make
```
**This is highly recommended when you are testing the MBR and want to see what's going on.**

## Configuring the MBR and the MBs

For the sake of clarity will set up the MBs (MB1 and MB2) and a MBR. You can then
extend the example to whatever number of MBs and MBRs. This example also assumes
that there are no accounst created.

* MBR will listen to HTTP requests on port 7000
* MB1 will listen to HTTP requests on port 9985 using redis DB 0
* MB2 will listen to HTTP requests on port 9984 using redis DB 1

**1**. We need to have 2 different bootstrap files for each MB :
MB1.bootstrap.json
``` 
<---  config --->
{
    ...
    "banker-uri": "127.0.0.1:7000",
    "portRanges": {
        ...
        "banker.http":              9985,
        ...
    }
}

``` 
Here we have set that the banker-uri is 127.0.0.1:7000 wich is the address
for the MBR and we've set 9985 as the HTTP listen port for MB1.

MB2.bootstrap.json
``` 
<--- MB2 config --->
{
    ...
    "banker-uri": "127.0.0.1:7000",
    "portRanges": {
        ...
        "banker.http":              9984,
        ...
    }
}
```
Here we have set that the banker-uri is 127.0.0.1:7000 wich is the address
for the MBR and we've set 9984 as the HTTP listen port for MB2.

This way all the PALs and Routers will shoot the MBR when synching all the
slave accounts.

**2**. Start the PAL/s and Router/s adding the following parameters :
```
--use-http-banker --http-connections 128  --banker-http-timeouts 1000
```
This tells the processes to use the HTTP banker interface

**3**. Start MB1
```
$ banker_service_runner -B /path/to/mb1.bootstrap.json -r 127.0.0.1:6379 -N masterBanker1 -d 0
```
This tells the banker to start and push masterBanker1 into zookeeper and use the DB 0 on redis

**4**. Start MB2
```
$ banker_service_runner -B /path/to/mb1.bootstrap.json -r 127.0.0.1:6379 -N masterBanker2 -d 1
```
This tells the banker to start and push masterBanker2 into zookeeper and use the DB 1 on redis

**5**. Start MBR
First we need to create the config file for the MBR. Create a *config.json* file that looks like this:
```
[
    {"shard":0, "endpoint":"127.0.0.1:9985"},
    {"shard":1, "endpoint":"127.0.0.1:9984"}
]
```
Here whe are declaring that shard 0 is MB1 and that shard 1 is MB2

```
src/master_banker_relay --logtostderr=1 --relay_config=config.json --http_port=7000
```
Start MBR and log to stderr, in order to get all de CLI flags (there are plenty for
logging config) use --help.

**6**. You are all set now. Every call that modifies the state of any account must be done
using the MBR.

## Verifying everything is ok.

**1**. Start 2 agents:
* Set on agent 1 account_hello
* Set on agent 2 account_bla
* account_hello ends up in shard 0, thus if you connect to redis and check DB 0 you should see
the keys related to the account, give a few seconds since the MBs sync every few secs.
* account_bla ends up in shard 1, thus if you connect to redis and check DB 1 you should see
the keys related to the account, give a few seconds since the MBs sync every few secs.

**IMPORTANT** be sure to use the bootsrap file that sets the MBR as banker-uri for the agents.

After this has happened you'll see the PALs and Routers shooting the MBR and the MBR relaying
those HTTP requests.

**2**. Push some money into the accounts :
```
$ curl http://localhost:7000/v1/accounts/account_hello/budget -d '{ "USD/1M": 123456789 }'
$ curl http://localhost:7000/v1/accounts/account_bla/budget -d '{ "USD/1M": 123456789 }'
```

**3**. It's always a good idea wo use wireshark/tcpdump to check that the relaying is done
correctly

## Supported endpoints

Supported: S

Not supported: NS

*Root calls*
* NS GET */* : desc of the API
* NS GET */ping* : should return pong
* NS GET */v1* : version of the api
* S  GET */v1/accounts* : return a list of all accounts
* S  GET */v1/activeaccounts* : return a list of all active accounts
* S  GET */v1/summary* : return a summary of all the accounts.

*Account Specific* :
* S POST */v1/accounts* : the account name to be created comes in the body.
* S GET */v1/accounts/<accountName>*
* S POST,PUT */v1/accounts/<accountName>/adjustment*
* S POST,PUT */v1/accounts/<accountName>/balance*
* S POST,PUT */v1/accounts/<accountName>/budget*
* S GET */v1/accounts/<accountName>/children*
* S GET */v1/accounts/<accountName>/close*
* S POST,PUT */v1/accounts/<accountName>/shadow*
* S GET */v1/accounts/<accountName>/subtree*
* S GET */v1/accounts/<accountName>/subtree*
* S GET */v1/accounts/<accountName>/summary*

*Batched calls*:
* NS POST,PUT */v1/accounts/balance* : accounts to be balance comes in the request body.
* NS POST,PUT */v1/accounts/shadow* : accounts to be sync  comes in the request body.

## Migrations

The code includes a sharding script for existing accounts. This is what you need to use in case
you are either :

* Moving from an MB signleton to a mutliple MB scheme using the MBR
* Adding more MB instances

The [sharding](https://github.com/Motrixi/mbr-public/blob/develop/scripts/shard.py) script 
is coded in python and python-redis needs to be installed

Here is how it works :
```
$ python shard.py --help
usage: shard.py [-h] -f FROM_REDIS [FROM_REDIS ...] -t TO_REDIS [TO_REDIS ...]
                [--delete_from] [--dry_run]

Process NGINX access logs

optional arguments:
  -h, --help            show this help message and exit
  -f FROM_REDIS [FROM_REDIS ...], --from_redis FROM_REDIS [FROM_REDIS ...]
                        redis hosts to read from <host>:<port>:<db>
  -t TO_REDIS [TO_REDIS ...], --to_redis TO_REDIS [TO_REDIS ...]
                        redis destinations <host>:<port>:<db>:<shard>
  --delete_from         delete the keys from the origin redis
  --dry_run             don't do anything just print
```

You need to specify the redis DBs where the current accounts are using *-f* and then
set the destination DBs for the new shards us -t. You can also specify --dry_run to
do a dry run or --delete_from if you want to clean up the DBs set using *-f*.

**IMPORTANT** DO BACKUP YOUR REDIS DBs before running this.

**Some examples**

**1**. 
* MB has N accounts and we are moving to a scheme with MB1 and MB2
* MB used redis DB 0
* MB1 (shard 0) will use redis DB 1
* MB2 (shard 1) will use redis DB 2

* We do not want to clean up the MB db
* We first want to do a dry run

```
python shard.py --from_redis 127.0.0.1:6379:0 --to_redis 127.0.0.1:6379:1:0 127.0.0.1:6379:2:1 --dry_run
```
This will take all accounts and take redis 127.0.0.1:6379 DB 1 as shard 0 and 127.0.0.1:6379 DB 2 
as shard 1 (check the format at the help of the the command). One we are sure that it works we 
actually run it :
```
python shard.py --from_redis 127.0.0.1:6379:0 --to_redis 127.0.0.1:6379:1:0 127.0.0.1:6379:2:1
```

**2**. 
* We have MB1 and MB2 and we want to add MB3
* MB1 (shard 0) used redis DB 0 and will keep using that DB
* MB2 (shard 1) used redis DB 1 and will keep using that DB
* MB3 (shard 2) will use redis DB 2
* We need to clean up the MB1 and MB2 db since they will be reused
```
python shard.py --from_redis 127.0.0.1:6379:0 127.0.0.1:6379:1 --to_redis 127.0.0.1:6379:0:0 127.0.0.1:6379:1:1 127.0.0.1:6379:2:2 --delete_from
```


