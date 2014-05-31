#ifndef _PHPREDIS_CLUSTER_LIBRARY_H
#define _PHPREDIS_CLUSTER_LIBRARY_H

#include "common.h"

#ifdef ZTS
#include "TSRM.h"
#endif

/* Redis cluster hash slots and N-1 which we'll use to find it */
#define REDIS_CLUSTER_SLOTS 16384
#define REDIS_CLUSTER_MOD   (REDIS_CLUSTER_SLOTS-1)

/* Minimum valid CLUSTER NODES line element count
 * and the minimum we expect if there are slots */
#define CLUSTER_MIN_NODE_LINE     8
#define CLUSTER_MIN_SLOTS_COUNT   9

/* Length of a cluster name */
#define CLUSTER_NAME_LEN 40

/* The parts for our cluster nodes command */
#define CLUSTER_NODES_HASH        0
#define CLUSTER_NODES_HOST_PORT   1
#define CLUSTER_NODES_TYPE        2
#define CLUSTER_NODES_MASTER_HASH 3
#define CLUSTER_NODES_PING        4
#define CLUSTER_NODES_PONG        5
#define CLUSTER_NODES_EPOCH       6
#define CLUSTER_NODES_CONNECTED   7
#define CLUSTER_SLOTS             8

/* Cluster redirection enum */
typedef enum CLUSTER_REDIR_TYPE {
    REDIR_NONE,
    REDIR_MOVED,
    REDIR_ASK
} CLUSTER_REDIR_TYPE;

/* MOVED/ASK comparison macros */
#define IS_MOVED(p) (p[0]=='M' && p[1]=='O' && p[2]=='V' && p[3]=='E' && \
                     p[4]=='D' && p[5]==' ')
#define IS_ASK(p)   (p[0]=='A' && p[1]=='S' && p[3]=='K' && p[4]==' ')

/* MOVED/ASK lengths */
#define MOVED_LEN (sizeof("MOVED ")-1)
#define ASK_LEN   (sizeof("ASK ")-1)

/* Slot/RedisSock/RedisSock->stream macros */
#define SLOT(c,s) (c->master[s])
#define SLOT_SOCK(c,s) (SLOT(c,s)->sock)
#define SLOT_STREAM(c,s) (SLOT_SOCK(c,s)->stream)

/* Compare redirection slot information with what we have */
#define CLUSTER_REDIR_CMP(c) \
    (SLOT_SOCK(c,c->redir_slot)->port != c->redir_port || \
    strlen(SLOT_SOCK(c,c->redir_slot)->host) != c->redir_host_len || \
    memcmp(SLOT_SOCK(c,c->redir_slot)->host,c->redir_host,c->redir_host_len))

/* Lazy connect logic */
#define CLUSTER_LAZY_CONNECT(s) \
    if(s->lazy_connect) { \
        s->lazy_connect = 0; \
        redis_sock_server_open(s, 1 TSRMLS_CC); \
    }

/* Clear out our "last error" */
#define CLUSTER_CLEAR_ERROR(c) \
    if(c->err) { \
        efree(c->err); \
        c->err = NULL; \
        c->err_len = 0; \
    }

/* Specific destructor to free a cluster object */
// void redis_destructor_redis_cluster(zend_rsrc_list_entry *rsrc TSRMLS_DC);

/* Slot range structure */
typedef struct clusterSlotRange {
    unsigned short start, end;
} clusterSlotRange;

/* Bits related to CLUSTER NODES output */
typedef struct clusterNodeInfo {
    char *name, *master_name;

    short seed;

    char *host;
    int host_len;

    unsigned short port;
    
    unsigned short slave;

    clusterSlotRange *slots;
    size_t slots_size;
} clusterNodeInfo;

/* A Redis Cluster master node */
typedef struct redisClusterNode {
    /* Our cluster ID and master ID */
    char *name;
    char *master_name;

    /* Our Redis socket in question */
    RedisSock *sock;

    /* Contiguous slots we serve */
    clusterSlotRange *slots;
    size_t slots_size;

    /* Is this a slave node */
    unsigned short slave;

    /* A HashTable containing any slaves */
    HashTable *slaves;
} redisClusterNode;

/* RedisCluster implementation structure */
typedef struct redisCluster {
    /* Object reference for Zend */
    zend_object std;

    /* Timeout and read timeout */
    double timeout;
    double read_timeout;

    /* Hash table of seed host/ports */
    HashTable *seeds;

    /* RedisCluster masters, by direct slot */
    redisClusterNode *master[REDIS_CLUSTER_SLOTS];

    /* All RedisCluster objects we've created/are connected to */
    HashTable *nodes;

    /* How many failures have we had in a row */
    int failures;

    /* The last ERROR we encountered */
    char *err;
    int err_len;

    /* The slot where we should read replies */
    short reply_slot;

    /* One RedisSock* object for serialization and prefix information */
    RedisSock *flags;

    /* The last reply length we got, which we'll use to parse and
     * format our replies to the client. */
    int reply_len;

    /* Last MOVED or ASK redirection response information */
    CLUSTER_REDIR_TYPE redir_type;
    char               redir_host[255];
    int                redir_host_len;
    unsigned short     redir_slot;
    unsigned short     redir_port;
} redisCluster;

/* Hash a key to it's slot, using the Redis Cluster hash algorithm */
unsigned short cluster_hash_key_zval(zval *key);
unsigned short cluster_hash_key(const char *key, int len);

/* Send a command to where we think the key(s) should live and redirect when 
 * needed */
PHPAPI short cluster_send_command(redisCluster *cluster, short slot,
                                const char *cmd, int cmd_len TSRMLS_DC);

PHPAPI int cluster_init_seeds(redisCluster *cluster, HashTable *ht_seeds);
PHPAPI int cluster_map_keyspace(redisCluster *cluster TSRMLS_DC);
PHPAPI void cluster_free_node(redisClusterNode *node);

PHPAPI char **cluster_sock_read_multibulk_reply(RedisSock *redis_sock,
                                                int *len TSRMLS_DC);

PHPAPI int cluster_node_add_slave(redisCluster *cluster,
                                  redisClusterNode *master,
                                  clusterNodeInfo *slave TSRMLS_DC);

/*
 * Redis Cluster response handlers.  All of our response handlers take the
 * following form:
 *      PHPAPI void handler(INTERNAL_FUNCTION_PARAMETERS, redisCluster *c)
 *
 * Reply handlers are responsible for setting the PHP return value (either to
 * something valid, or FALSE in the case of some failures).
 */

PHPAPI void cluster_bool_resp(INTERNAL_FUNCTION_PARAMETERS, redisCluster *c);
PHPAPI void cluster_bulk_resp(INTERNAL_FUNCTION_PARAMETERS, redisCluster *c);

#endif
