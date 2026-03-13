#ifndef BITCOIN_OMNICORE_RULES_H
#define BITCOIN_OMNICORE_RULES_H

#include <uint256.h>

#include <stdint.h>
#include <string>
#include <vector>

namespace mastercore
{
//! Feature identifier to enable cross property (v1) Send To Owners
const uint16_t FEATURE_STOV1 = 10;
//! Feature identifier to activate the waiting period for enabling managed property address freezing
const uint16_t FEATURE_FREEZENOTICE = 14;
//! Feature identifier to activate trading of any token on the distributed exchange
const uint16_t FEATURE_FREEDEX = 15;
//! Feature identifier to enable non-fungible token support
const uint16_t FEATURE_NONFUNGIBLE = 16;
//! Feature identifier to enable NFT issuer data update by issuers only
const uint16_t FEATURE_NONFUNGIBLE_ISSUER = 18;

/** A structure to represent transaction restrictions.
 */
struct TransactionRestriction
{
    //! Transaction type
    uint16_t txType;
    //! Transaction version
    uint16_t txVersion;
    //! Whether the property identifier can be 0 (= BTC)
    bool allowWildcard;
    //! Block after which the feature or transaction is enabled
    int activationBlock;
};

/** A structure to represent a verification checkpoint.
 */
struct ConsensusCheckpoint
{
    int blockHeight;
    uint256 blockHash;
    uint256 consensusHash;
};

/** A structure to represent a specific transaction checkpoint.
 */
struct TransactionCheckpoint
{
    int blockHeight;
    uint256 txHash;
};

/** Base class for consensus parameters.
 */
class CConsensusParams
{
public:
    //! OmniLite genesis block
    int GENESIS_BLOCK;

    //! Minimum number of blocks to use for notice rules on activation
    int MIN_ACTIVATION_BLOCKS;
    //! Maximum number of blocks to use for notice rules on activation
    int MAX_ACTIVATION_BLOCKS;

    //! Waiting period after enabling freezing before addresses may be frozen
    int OMNI_FREEZE_WAIT_PERIOD;

    //! Block to enable pay-to-pubkey-hash support
    int PUBKEYHASH_BLOCK;
    //! Block to enable pay-to-script-hash support
    int SCRIPTHASH_BLOCK;
    //! Block to enable bare-multisig based encoding
    int MULTISIG_BLOCK;
    //! Block to enable OP_RETURN based encoding
    int NULLDATA_BLOCK;

    //! Block to enable alerts and notifications
    int MSC_ALERT_BLOCK;
    //! Block to enable simple send transactions
    int MSC_SEND_BLOCK;
    //! Block to enable DEx transactions
    int MSC_DEX_BLOCK;
    //! Block to enable smart property transactions
    int MSC_SP_BLOCK;
    //! Block to enable managed properties
    int MSC_MANUALSP_BLOCK;
    //! Block to enable send-to-owners transactions
    int MSC_STO_BLOCK;
    //! Block to enable "send all" transactions
    int MSC_SEND_ALL_BLOCK;
    //! Block to enable cross property STO (v1)
    int MSC_STOV1_BLOCK;
    //! Block to enable any data payloads
    int MSC_ANYDATA_BLOCK;
    //! Block to enable non-fungible tokens
    int MSC_NONFUNGIBLE_BLOCK;

    //! Block to activate the waiting period for enabling managed property address freezing
    int FREEZENOTICE_FEATURE_BLOCK;
    //! Block to activate the waiting period to activate trading of any token on the distributed exchange
    int FREEDEX_FEATURE_BLOCK;
    //! Block to activate the ability for only issuers to update issuer data on NFTs
    int NONFUNGIBLETOKEN_ISSUER_DATA;

    /** Returns a mapping of transaction types, and the blocks at which they are enabled. */
    virtual std::vector<TransactionRestriction> GetRestrictions() const;

    /** Returns an empty vector of consensus checkpoints. */
    virtual std::vector<ConsensusCheckpoint> GetCheckpoints() const;

    /** Returns an empty vector of transaction checkpoints. */
    virtual std::vector<TransactionCheckpoint> GetTransactions() const;

    /** Destructor. */
    virtual ~CConsensusParams() {}

protected:
    /** Constructor, only to be called from derived classes. */
    CConsensusParams() {}
};

/** Consensus parameters for mainnet.
 */
class CMainConsensusParams: public CConsensusParams
{
public:
    /** Constructor for mainnet consensus parameters. */
    CMainConsensusParams();
    /** Destructor. */
    virtual ~CMainConsensusParams() {}

    /** Returns consensus checkpoints for mainnet, used to verify transaction processing. */
    virtual std::vector<ConsensusCheckpoint> GetCheckpoints() const;

    /** Returns transactions checkpoints for mainnet, used to verify DB consistency. */
    virtual std::vector<TransactionCheckpoint> GetTransactions() const;
};

/** Consensus parameters for testnet.
 */
class CTestNetConsensusParams: public CConsensusParams
{
public:
    /** Constructor for testnet consensus parameters. */
    CTestNetConsensusParams();
    /** Destructor. */
    virtual ~CTestNetConsensusParams() {}
};

/** Consensus parameters for regtest mode.
 */
class CRegTestConsensusParams: public CConsensusParams
{
public:
    /** Constructor for regtest consensus parameters. */
    CRegTestConsensusParams();
    /** Destructor. */
    virtual ~CRegTestConsensusParams() {}
};

/** Returns consensus parameters for the given network. */
CConsensusParams& ConsensusParams(const std::string& network);
/** Returns currently active consensus parameter. */
const CConsensusParams& ConsensusParams();
/** Returns currently active mutable consensus parameter. */
CConsensusParams& MutableConsensusParams();
/** Resets consensus parameters. */
void ResetConsensusParams();


/** Gets the display name for a feature ID */
std::string GetFeatureName(uint16_t featureId);
/** Activates a feature at a specific block height. */
bool ActivateFeature(uint16_t featureId, int activationBlock, uint32_t minClientVersion, int transactionBlock);
/** Deactivates a feature immediately, authorization has already been validated. */
bool DeactivateFeature(uint16_t featureId, int transactionBlock);
/** Checks, whether a feature is activated at the given block. */
bool IsFeatureActivated(uint16_t featureId, int transactionBlock);
/** Checks, if the script type is allowed as input. */
bool IsAllowedInputType(int whichType, int nBlock);
/** Checks, if the script type qualifies as output. */
bool IsAllowedOutputType(int whichType, int nBlock);
/** Checks, if the transaction type and version is supported and enabled. */
bool IsTransactionTypeAllowed(int txBlock, uint32_t txProperty, uint16_t txType, uint16_t version);

/** Compares a supplied block, block hash and consensus hash against a hardcoded list of checkpoints. */
bool VerifyCheckpoint(int block, const uint256& blockHash);
/** Checks, if a specific transaction exists in the database. */
bool VerifyTransactionExistence(int block);
}

#endif // BITCOIN_OMNICORE_RULES_H
