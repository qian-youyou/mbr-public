import sys
import argparse
import redis

class Sharder(object):

    def __init__(self, from_redis, to_redis, delete_from, dry_run):
        self.from_redis = from_redis
        self.to_redis = to_redis
        self.delete_from = delete_from
        self.dry_run = dry_run

    def SDBMHash(self, key):
        hash = 0
        for i in range(len(key)):
            hash = ord(key[i]) + (hash << 6) + (hash << 16) - hash;
        return (hash & 0x7FFFFFFF)

    def run(self):
        # get all the accounts
        accounts = self.get_existing_accounts()
        self.push_accounts(accounts)
        if self.delete_from:
            self.clean_from_redis()

    def get_existing_accounts(self):
        accounts = []
        for c in self.from_redis:
            acs = c.smembers('banker:accounts')
            for a in acs:
                # get the value of each account/key
                value = c.get('banker-%s' % a)
                accounts.append((a, value))
        return accounts

    def push_accounts(self, accounts):
        for k,v in accounts:
            shard = self.get_shard(k)
            print 'pushing banker-%s -> shard %d' % (k, shard)
            if not self.dry_run:
                self.to_redis[shard].set('banker-%s' % k, v)
                self.to_redis[shard].sadd('banker:accounts', k)

    def get_shard(self, key):
        hash = self.SDBMHash(key.split(':')[0])
        shard = hash % len(self.to_redis)
        return shard

    def clean_from_redis(self):
        for c in self.from_redis:
            keys = c.keys('banker-*')
            for k in keys:
                print 'deleting %s' % k
                if not self.dry_run:
                    c.delete(k)
            print 'deleting banker:accounts'
            if not self.dry_run:
                c.delete('banker:accounts')

def from_redis(redis_endpoints):
    conns = []
    for ep in redis_endpoints:
        parts = ep.split(':')
        host = parts[0]
        port = int(parts[1])
        db = int(parts[2])

        c = redis.StrictRedis(host=host, port=port, db=db)
        conns.append(c)
        print '<-- %s:%d:%d' % (host, port, db)
    return conns


def to_redis(redis_endpoints):
    conns = {}
    for ep in redis_endpoints:
        parts = ep.split(':')
        host = parts[0]
        port = int(parts[1])
        db = int(parts[2])
        shard = int(parts[3])

        c = redis.StrictRedis(host=host, port=port, db=db)
        conns[shard] = c
        print '--> shard : %d = %s:%d:%d' % (shard, host, port, db)
    return conns


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
            description='Process NGINX access logs')
    parser.add_argument('-f', '--from_redis', nargs='+', required=True,
            help='redis hosts to read from <host>:<port>:<db>')
    parser.add_argument('-t', '--to_redis', nargs='+', required=True,
            help='redis destinations <host>:<port>:<db>:<shard>')
    parser.add_argument('--delete_from', action='store_true', default=False,
            help='delete the keys from the origin redis')
    parser.add_argument('--dry_run', action='store_true', default=False,
            help='don\'t do anything just print')
    args = parser.parse_args()

    # create all connections
    print 'pulling from redis'
    f = from_redis(args.from_redis)

    print 'pushing to redis'
    t = to_redis(args.to_redis)

    Sharder(f, t, args.delete_from, args.dry_run).run()

