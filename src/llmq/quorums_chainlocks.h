// Copyright (c) 2019-2020 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SYSCOIN_LLMQ_QUORUMS_CHAINLOCKS_H
#define SYSCOIN_LLMQ_QUORUMS_CHAINLOCKS_H

#include <llmq/quorums_signing.h>
#include <atomic>
#include <evo/evodb.h>
#include <kernel/cs_main.h>
class CBlockIndex;
class CConnman;
class PeerManager;
class CScheduler;
class ChainstateManager;
class BlockValidationState;
namespace llmq
{

class CChainLockSig
{
public:
    int32_t nHeight{-1};
    uint256 blockHash;
    uint256 prevCLBlockHash;
    CBLSSignature sig;
    std::vector<bool> signers;

public:
    CChainLockSig(int32_t nHeight, const uint256& blockHash, const uint256& prevCLBlockHash, const CBLSSignature& sig, const std::vector<bool> signers) :
        nHeight(nHeight),
        blockHash(blockHash),
        prevCLBlockHash(prevCLBlockHash),
        sig(sig),
        signers(signers)
    {}
    CChainLockSig() = default;
    SERIALIZE_METHODS(CChainLockSig, obj) {
        READWRITE(obj.nHeight, obj.blockHash, obj.prevCLBlockHash, obj.sig);
        READWRITE(DYNBITSET(obj.signers));
    }
    // Equality operator
    bool operator==(const CChainLockSig& other) const
    {
        return nHeight == other.nHeight &&
               blockHash == other.blockHash &&
               prevCLBlockHash == other.prevCLBlockHash &&
               sig == other.sig &&
               signers == other.signers;
    }

    // Inequality operator
    bool operator!=(const CChainLockSig& other) const
    {
        return !(*this == other);
    }
    bool IsNull() const;
    std::string ToString() const;
};

typedef std::shared_ptr<const CChainLockSig> CChainLockSigCPtr;

struct ReverseHeightComparator
{
    bool operator()(const int h1, const int h2) const {
        return h1 > h2;
    }
};

class CChainLocksHandler : public CRecoveredSigsListener
{
    static const int64_t CLEANUP_INTERVAL = 1000 * 30;
    static const int64_t CLEANUP_SEEN_TIMEOUT = 24 * 60 * 60 * 1000;

private:
    CScheduler* scheduler;
    std::thread* scheduler_thread;
    bool isEnabled GUARDED_BY(cs) {false};
    bool isEnforced GUARDED_BY(cs) {false};
    std::atomic_bool tryLockChainTipScheduled {false};

    CChainLockSig mostRecentChainLockShare GUARDED_BY(cs);
    CChainLockSig bestChainLockWithKnownBlock GUARDED_BY(cs);
    CChainLockSig bestChainLockWithKnownBlockPrev GUARDED_BY(cs);
    const CBlockIndex* bestChainLockBlockIndex {nullptr};
    const CBlockIndex* bestChainLockBlockIndexPrev {nullptr};
    // Keep best chainlock shares and candidates, sorted by height (highest heght first).
    std::map<int, std::map<CQuorumCPtr, CChainLockSigCPtr>, ReverseHeightComparator> bestChainLockShares GUARDED_BY(cs);
    std::map<int, CChainLockSigCPtr, ReverseHeightComparator> bestChainLockCandidates GUARDED_BY(cs);

    std::map<uint256, std::pair<int, uint256> > mapSignedRequestIds GUARDED_BY(cs);
    std::map<uint256, int64_t> seenChainLocks GUARDED_BY(cs);
    int64_t lastCleanupTime GUARDED_BY(cs) {0};

public:
    Mutex cs;
    CConnman& connman;
    PeerManager& peerman;
    ChainstateManager& chainman;
    std::unique_ptr<CEvoDB<uint256, CChainLockSig>> m_clDb;
    explicit CChainLocksHandler(const DBParams& db_params, CConnman &connman, PeerManager& peerman, ChainstateManager& chainman);
    ~CChainLocksHandler();

    void Start() EXCLUSIVE_LOCKS_REQUIRED(!cs);
    void Stop();

    bool AlreadyHave(const uint256& hash) EXCLUSIVE_LOCKS_REQUIRED(!cs);
    bool GetChainLockByHash(const uint256& hash, CChainLockSig& ret) EXCLUSIVE_LOCKS_REQUIRED(!cs);
    CChainLockSig GetMostRecentChainLock() EXCLUSIVE_LOCKS_REQUIRED(!cs);
    CChainLockSig GetBestChainLock() EXCLUSIVE_LOCKS_REQUIRED(!cs);
    CChainLockSig GetBestChainLockPrev() EXCLUSIVE_LOCKS_REQUIRED(!cs);
    const CBlockIndex* GetPreviousChainLock() EXCLUSIVE_LOCKS_REQUIRED(!cs);
    std::map<CQuorumCPtr, CChainLockSigCPtr> GetBestChainLockShares() EXCLUSIVE_LOCKS_REQUIRED(!cs);

    void ProcessMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv) EXCLUSIVE_LOCKS_REQUIRED(!cs);
    bool ProcessNewChainLock(NodeId from, CChainLockSig& clsig, BlockValidationState& state, const uint256& hash, const uint256& idIn = uint256()) EXCLUSIVE_LOCKS_REQUIRED(!cs);
    void NotifyHeaderTip(const CBlockIndex* pindexNew) EXCLUSIVE_LOCKS_REQUIRED(!cs);
    void UpdatedBlockTip(const CBlockIndex* pindexNew, bool fInitialDownload);
    void CheckActiveState() EXCLUSIVE_LOCKS_REQUIRED(!cs);
    void TrySignChainTip() EXCLUSIVE_LOCKS_REQUIRED(!cs);
    void HandleNewRecoveredSig(const CRecoveredSig& recoveredSig) override EXCLUSIVE_LOCKS_REQUIRED(!cs);

    bool HasChainLock(int nHeight, const uint256& blockHash) EXCLUSIVE_LOCKS_REQUIRED(!cs);
    bool HasConflictingChainLock(int nHeight, const uint256& blockHash) EXCLUSIVE_LOCKS_REQUIRED(!cs);
    void SetToPreviousChainLock() EXCLUSIVE_LOCKS_REQUIRED(!cs);
    bool VerifyAggregatedChainLock(const CChainLockSig& clsig, const CBlockIndex* pindexScan) EXCLUSIVE_LOCKS_REQUIRED(!cs);
    bool FlushCacheToDisk();
    bool GetChainLockFromDB(const uint256& hash, llmq::CChainLockSig& ret);
    bool AlreadyHaveDB(const uint256& hash);
    uint256 GetLastRequestedBlockHash();
    void RestoreState() EXCLUSIVE_LOCKS_REQUIRED(cs_main, cs);
private:
    void CheckState(bool bSignTip = true) EXCLUSIVE_LOCKS_REQUIRED(!cs);
    // these require locks to be held already
    bool InternalHasChainLock(int nHeight, const uint256& blockHash) const EXCLUSIVE_LOCKS_REQUIRED(cs);
    bool InternalHasConflictingChainLock(int nHeight, const uint256& blockHash) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    bool TryUpdateBestChainLock(const CBlockIndex* pindex) EXCLUSIVE_LOCKS_REQUIRED(cs);
    bool VerifyChainLockShare(const CChainLockSig& clsig, const CBlockIndex* pindexScan, const uint256& idIn, std::pair<int, CQuorumCPtr>& ret) EXCLUSIVE_LOCKS_REQUIRED(!cs);
    void Cleanup() EXCLUSIVE_LOCKS_REQUIRED(!cs);
};

extern CChainLocksHandler* chainLocksHandler;

bool AreChainLocksEnabled();

} // namespace llmq

#endif // SYSCOIN_LLMQ_QUORUMS_CHAINLOCKS_H
