// Copyright (c) 2017-2019 The Syscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SYSCOIN_NODE_TRANSACTION_H
#define SYSCOIN_NODE_TRANSACTION_H

#include <attributes.h>
#include <primitives/transaction.h>
#include <uint256.h>
#include <util/error.h>

/**
 * Broadcast a transaction
 *
 * @param[in]  tx the transaction to broadcast
 * @param[out] &err_string reference to std::string to fill with error string if available
 * @param[in]  max_tx_fee reject txs with fees higher than this (if 0, accept any fee)
 * @param[in]  relay flag if both mempool insertion and p2p relay are requested
 * @param[in]  wait_callback, wait until callbacks have been processed to avoid stale result due to a sequentially RPC.
 * It MUST NOT be set while cs_main, cs_mempool or cs_wallet are held to avoid deadlock
 * return error
 */
NODISCARD TransactionError BroadcastTransaction(CTransactionRef tx, std::string& err_string, const CAmount& max_tx_fee, bool relay, bool wait_callback);

#endif // SYSCOIN_NODE_TRANSACTION_H
