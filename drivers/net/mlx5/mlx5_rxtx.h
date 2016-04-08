/*-
 *   BSD LICENSE
 *
 *   Copyright 2015 6WIND S.A.
 *   Copyright 2015 Mellanox.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND S.A. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTE_PMD_MLX5_RXTX_H_
#define RTE_PMD_MLX5_RXTX_H_

#include <stddef.h>
#include <stdint.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/mlx5_hw.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

/* DPDK headers don't like -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-pedantic"
#endif
#include <rte_mbuf.h>
#include <rte_mempool.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-pedantic"
#endif

#include "mlx5_utils.h"
#include "mlx5.h"
#include "mlx5_autoconf.h"
#include "mlx5_defs.h"

struct mlx5_rxq_stats {
	unsigned int idx; /**< Mapping index. */
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint64_t ipackets; /**< Total of successfully received packets. */
	uint64_t ibytes; /**< Total of successfully received bytes. */
#endif
	uint64_t idropped; /**< Total of packets dropped when RX ring full. */
	uint64_t rx_nombuf; /**< Total of RX mbuf allocation failures. */
};

struct mlx5_txq_stats {
	unsigned int idx; /**< Mapping index. */
#ifdef MLX5_PMD_SOFT_COUNTERS
	uint64_t opackets; /**< Total of successfully sent packets. */
	uint64_t obytes; /**< Total of successfully sent bytes. */
#endif
	uint64_t odropped; /**< Total of packets not sent when TX ring full. */
};

/* Flow director queue structure. */
struct fdir_queue {
	struct ibv_qp *qp; /* Associated RX QP. */
	struct ibv_exp_rwq_ind_table *ind_table; /* Indirection table. */
};

struct priv;

struct rxq_zip {
	uint16_t ai; /* Array index. */
	uint16_t ca; /* Current array index. */
	uint16_t na; /* Next array index. */
	uint16_t cq_ci; /* The next CQE. */
	uint32_t cqe_cnt;
};

/* RX queue descriptor. */
struct frxq {
	uint16_t idx;
	uint16_t elts_n;
	uint16_t rq_ci;
	uint16_t cq_ci;
	uint16_t cqe_cnt;
	uint16_t port_id;
	struct rte_mbuf *elt;
	struct rxq_zip zip; /* Compressed context. */
	volatile struct mlx5_wqe_data_seg (*wqes)[];
	volatile struct mlx5_cqe64 (*cqes)[];
	volatile uint32_t *rq_db;
	volatile uint32_t *cq_db;
	struct rte_mempool *mp;
	unsigned int csum:1; /* Enable checksum offloading. */
	unsigned int vlan_strip:1; /* Enable VLAN stripping. */
	unsigned int crc_present:1; /* CRC must be subtracted. */
	struct mlx5_rxq_stats stats;
} __attribute__((aligned(64)));

/* RX queue descriptor. */
struct rxq {
	struct priv *priv; /* Back pointer to private data. */
	struct ibv_cq *cq; /* Completion Queue. */
	struct ibv_exp_wq *wq; /* Work Queue. */
	unsigned int socket; /* CPU socket ID for allocations. */
	struct mlx5_rxq_stats stats; /* RX queue counters. */
	struct ibv_exp_res_domain *rd; /* Resource Domain. */
	struct fdir_queue fdir_queue; /* Flow director queue. */
	struct ibv_mr *mr; /* Memory Region (for mp). */
	struct ibv_exp_wq_family *if_wq; /* WQ burst interface. */
	struct ibv_exp_cq_family *if_cq; /* CQ interface. */
	struct frxq frxq;
};

/* Hash RX queue types. */
enum hash_rxq_type {
	HASH_RXQ_TCPV4,
	HASH_RXQ_UDPV4,
	HASH_RXQ_IPV4,
#ifdef HAVE_FLOW_SPEC_IPV6
	HASH_RXQ_TCPV6,
	HASH_RXQ_UDPV6,
	HASH_RXQ_IPV6,
#endif /* HAVE_FLOW_SPEC_IPV6 */
	HASH_RXQ_ETH,
};

/* Flow structure with Ethernet specification. It is packed to prevent padding
 * between attr and spec as this layout is expected by libibverbs. */
struct flow_attr_spec_eth {
	struct ibv_exp_flow_attr attr;
	struct ibv_exp_flow_spec_eth spec;
} __attribute__((packed));

/* Define a struct flow_attr_spec_eth object as an array of at least
 * "size" bytes. Room after the first index is normally used to store
 * extra flow specifications. */
#define FLOW_ATTR_SPEC_ETH(name, size) \
	struct flow_attr_spec_eth name \
		[((size) / sizeof(struct flow_attr_spec_eth)) + \
		 !!((size) % sizeof(struct flow_attr_spec_eth))]

/* Initialization data for hash RX queue. */
struct hash_rxq_init {
	uint64_t hash_fields; /* Fields that participate in the hash. */
	uint64_t dpdk_rss_hf; /* Matching DPDK RSS hash fields. */
	unsigned int flow_priority; /* Flow priority to use. */
	union {
		struct {
			enum ibv_exp_flow_spec_type type;
			uint16_t size;
		} hdr;
		struct ibv_exp_flow_spec_tcp_udp tcp_udp;
		struct ibv_exp_flow_spec_ipv4 ipv4;
#ifdef HAVE_FLOW_SPEC_IPV6
		struct ibv_exp_flow_spec_ipv6 ipv6;
#endif /* HAVE_FLOW_SPEC_IPV6 */
		struct ibv_exp_flow_spec_eth eth;
	} flow_spec; /* Flow specification template. */
	const struct hash_rxq_init *underlayer; /* Pointer to underlayer. */
};

/* Initialization data for indirection table. */
struct ind_table_init {
	unsigned int max_size; /* Maximum number of WQs. */
	/* Hash RX queues using this table. */
	unsigned int hash_types;
	unsigned int hash_types_n;
};

/* Initialization data for special flows. */
struct special_flow_init {
	uint8_t dst_mac_val[6];
	uint8_t dst_mac_mask[6];
	unsigned int hash_types;
	unsigned int per_vlan:1;
};

enum hash_rxq_flow_type {
	HASH_RXQ_FLOW_TYPE_PROMISC,
	HASH_RXQ_FLOW_TYPE_ALLMULTI,
	HASH_RXQ_FLOW_TYPE_BROADCAST,
	HASH_RXQ_FLOW_TYPE_IPV6MULTI,
	HASH_RXQ_FLOW_TYPE_MAC,
};

#ifndef NDEBUG
static inline const char *
hash_rxq_flow_type_str(enum hash_rxq_flow_type flow_type)
{
	switch (flow_type) {
	case HASH_RXQ_FLOW_TYPE_PROMISC:
		return "promiscuous";
	case HASH_RXQ_FLOW_TYPE_ALLMULTI:
		return "allmulticast";
	case HASH_RXQ_FLOW_TYPE_BROADCAST:
		return "broadcast";
	case HASH_RXQ_FLOW_TYPE_IPV6MULTI:
		return "IPv6 multicast";
	case HASH_RXQ_FLOW_TYPE_MAC:
		return "MAC";
	}
	return NULL;
}
#endif /* NDEBUG */

struct hash_rxq {
	struct priv *priv; /* Back pointer to private data. */
	struct ibv_qp *qp; /* Hash RX QP. */
	enum hash_rxq_type type; /* Hash RX queue type. */
	/* MAC flow steering rules, one per VLAN ID. */
	struct ibv_exp_flow *mac_flow[MLX5_MAX_MAC_ADDRESSES][MLX5_MAX_VLAN_IDS];
	struct ibv_exp_flow *special_flow[MLX5_MAX_SPECIAL_FLOWS][MLX5_MAX_VLAN_IDS];
};

struct mlx5_wqe64 {
	union {
		struct mlx5_wqe_ctrl_seg ctrl;
		uint32_t data[4];
	} ctrl;
	struct mlx5_wqe_eth_seg eseg;
	struct mlx5_wqe_data_seg dseg;
};

#define MLX5_WQE64_INL_DATA 12
#define MLX5_WQE64_INL_DATA_OFFSET 52

struct mlx5_wqe64_inl {
	union {
		struct mlx5_wqe64 wqe;
		char data[64];
	} wqe;
};

struct ftxq {
	uint16_t elts_head; /* Current index in (*elts)[]. */
	uint16_t elts_tail; /* First element awaiting completion. */
	uint16_t elts_comp_cd_init; /* Initial value for countdown. */
	uint16_t elts_comp; /* number of completion per ring. */
	uint16_t elts_n;
	uint16_t cq_ci;
	uint16_t cqe_cnt;
	uint16_t wqe_ci;
	uint16_t wqe_cnt;
	uint16_t bf_offset;
	uint16_t bf_buf_size;
	volatile struct mlx5_cqe64 (*cqes)[];
	volatile struct mlx5_wqe64 (*wqes)[];
	volatile uint32_t *qp_db;
	volatile uint32_t *cq_db;
	volatile void *bf_reg;
	struct {
		const struct rte_mempool *mp; /* Cached Memory Pool. */
		struct ibv_mr *mr; /* Memory Region (for mp). */
		uint32_t lkey; /* mr->lkey */
	} mp2mr[MLX5_PMD_TX_MP_CACHE]; /* MP to MR translation table. */
	struct rte_mbuf *(*elts)[];
	struct mlx5_txq_stats stats; /* TX queue counters. */
	uint32_t qp_num_8s; /* QP number shifted by 8. */
} __attribute__((aligned(64)));

/* TX queue descriptor. */
struct txq {
	struct priv *priv; /* Back pointer to private data. */
	struct ibv_cq *cq; /* Completion Queue. */
	struct ibv_qp *qp; /* Queue Pair. */
	/* Elements used only for init part are here. */
	struct ibv_exp_qp_burst_family *if_qp; /* QP burst interface. */
	struct ibv_exp_cq_family *if_cq; /* CQ interface. */
	struct ibv_exp_res_domain *rd; /* Resource Domain. */
	unsigned int socket; /* CPU socket ID for allocations. */
	struct ftxq ftxq;
};

/* mlx5_rxq.c */

extern const struct hash_rxq_init hash_rxq_init[];
extern const unsigned int hash_rxq_init_n;

extern uint8_t rss_hash_default_key[];
extern const size_t rss_hash_default_key_len;

size_t priv_flow_attr(struct priv *, struct ibv_exp_flow_attr *,
		      size_t, enum hash_rxq_type);
int priv_create_hash_rxqs(struct priv *);
void priv_destroy_hash_rxqs(struct priv *);
int priv_allow_flow_type(struct priv *, enum hash_rxq_flow_type);
int priv_rehash_flows(struct priv *);
void rxq_cleanup(struct rxq *);
int rxq_rehash(struct rte_eth_dev *, struct rxq *);
int rxq_setup(struct rte_eth_dev *, struct rxq *, uint16_t, unsigned int,
	      const struct rte_eth_rxconf *, struct rte_mempool *);
int mlx5_rx_queue_setup(struct rte_eth_dev *, uint16_t, uint16_t, unsigned int,
			const struct rte_eth_rxconf *, struct rte_mempool *);
void mlx5_rx_queue_release(void *);
uint16_t mlx5_rx_burst_secondary_setup(void *dpdk_rxq, struct rte_mbuf **pkts,
			      uint16_t pkts_n);


/* mlx5_txq.c */

void txq_cleanup(struct txq *);
int txq_setup(struct rte_eth_dev *dev, struct txq *txq, uint16_t desc,
	  unsigned int socket, const struct rte_eth_txconf *conf);
int txq_min_queue_inline(void);

int mlx5_tx_queue_setup(struct rte_eth_dev *, uint16_t, uint16_t, unsigned int,
			const struct rte_eth_txconf *);
void mlx5_tx_queue_release(void *);
uint16_t mlx5_tx_burst_secondary_setup(void *dpdk_txq, struct rte_mbuf **pkts,
			      uint16_t pkts_n);

/* mlx5_rxtx.c */

uint16_t mlx5_tx_burst(void *, struct rte_mbuf **, uint16_t);
#if MLX5_PMD_MAX_INLINE > 0
uint16_t mlx5_tx_burst_inline(void *, struct rte_mbuf **, uint16_t);
#endif /* MLX5_PMD_MAX_INLINE > 0 */
uint16_t mlx5_rx_burst_sp(void *, struct rte_mbuf **, uint16_t);
uint16_t mlx5_rx_burst(void *, struct rte_mbuf **, uint16_t);
uint16_t removed_tx_burst(void *, struct rte_mbuf **, uint16_t);
uint16_t removed_rx_burst(void *, struct rte_mbuf **, uint16_t);

/* mlx5_rxtx_mr.c */
struct ibv_mr *mlx5_mp2mr(struct ibv_pd *, const struct rte_mempool *);
void txq_mp2mr_iter(const struct rte_mempool *, void *);
uint32_t txq_mp2mr_reg(struct ftxq *, const struct rte_mempool *, unsigned int);

#endif /* RTE_PMD_MLX5_RXTX_H_ */
