/**
 * @file rpc.cpp
 *
 * This file contains RPC calls for data retrieval.
 */

#include <omnicore/rpc.h>

#include <omnicore/activation.h>
#include <omnicore/consensushash.h>
#include <omnicore/convert.h>
#include <omnicore/dbspinfo.h>
#include <omnicore/dbstolist.h>
#include <omnicore/dbtxlist.h>
#include <omnicore/dex.h>
#include <omnicore/errors.h>
#include <omnicore/log.h>
#include <omnicore/notifications.h>
#include <omnicore/omnicore.h>
#include <omnicore/parsing.h>
#include <omnicore/rpcrequirements.h>
#include <omnicore/rpctxobject.h>
#include <omnicore/rpcvalues.h>
#include <omnicore/rules.h>
#include <omnicore/sp.h>
#include <omnicore/sto.h>
#include <omnicore/tally.h>
#include <omnicore/tx.h>
#include <omnicore/nftdb.h>
#include <omnicore/utilsbitcoin.h>
#include <omnicore/version.h>
#include <omnicore/walletfetchtxs.h>
#include <omnicore/walletutils.h>

#include <amount.h>
#include <base58.h>
#include <chainparams.h>
#include <init.h>
#include <index/txindex.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <validation.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <tinyformat.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <wallet/rpcwallet.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

#include <univalue.h>

#include <stdint.h>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>

#include <boost/algorithm/string.hpp> // boost::split

using std::runtime_error;
using namespace mastercore;

/**
 * Throws a JSONRPCError, depending on error code.
 */
void PopulateFailure(int error)
{
    switch (error) {
        case MP_TX_NOT_FOUND:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");
        case MP_TX_UNCONFIRMED:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unconfirmed transactions are not supported");
        case MP_BLOCK_NOT_IN_CHAIN:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not part of the active chain");
        case MP_CROWDSALE_WITHOUT_PROPERTY:
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Potential database corruption: \
                                                  \"Crowdsale Purchase\" without valid property identifier");
        case MP_INVALID_TX_IN_DB_FOUND:
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Potential database corruption: Invalid transaction found");
        case MP_TX_IS_NOT_OMNI_PROTOCOL:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No Omni Layer Protocol transaction");
        case MP_TXINDEX_STILL_SYNCING:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No such mempool transaction. Blockchain transactions are still in the process of being indexed.");
        case MP_RPC_DECODE_INPUTS_MISSING:
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction inputs were not found. Please provide inputs explicitly (see help description) or fully synchronize node.");

    }
    throw JSONRPCError(RPC_INTERNAL_ERROR, "Generic transaction population failure");
}

void PropertyToJSON(const CMPSPInfo::Entry& sProperty, UniValue& property_obj)
{
    property_obj.pushKV("name", sProperty.name);
    property_obj.pushKV("category", sProperty.category);
    property_obj.pushKV("subcategory", sProperty.subcategory);
    property_obj.pushKV("data", sProperty.data);
    property_obj.pushKV("url", sProperty.url);
    property_obj.pushKV("divisible", sProperty.isDivisible());
    property_obj.pushKV("issuer", sProperty.issuer);
    property_obj.pushKV("creationtxid", sProperty.txid.GetHex());
    property_obj.pushKV("fixedissuance", sProperty.fixed);
    property_obj.pushKV("managedissuance", sProperty.manual);
    property_obj.pushKV("non-fungibletoken", sProperty.unique);
}

bool BalanceToJSON(const std::string& address, uint32_t property, UniValue& balance_obj, bool divisible)
{
    // confirmed balance minus unconfirmed, spent amounts
    int64_t nAvailable = GetAvailableTokenBalance(address, property);
    int64_t nReserved = GetReservedTokenBalance(address, property);
    int64_t nFrozen = GetFrozenTokenBalance(address, property);

    if (divisible) {
        balance_obj.pushKV("balance", FormatDivisibleMP(nAvailable));
        balance_obj.pushKV("reserved", FormatDivisibleMP(nReserved));
        balance_obj.pushKV("frozen", FormatDivisibleMP(nFrozen));
    } else {
        balance_obj.pushKV("balance", FormatIndivisibleMP(nAvailable));
        balance_obj.pushKV("reserved", FormatIndivisibleMP(nReserved));
        balance_obj.pushKV("frozen", FormatIndivisibleMP(nFrozen));
    }

    return (nAvailable || nReserved || nFrozen);
}

// display the non-fungible tokens owned by an address for a property
UniValue omni_getnonfungibletokens(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw runtime_error(
                RPCHelpMan{"omni_getnonfungibletokens",
                   "\nReturns the non-fungible tokens for a given address. Optional property ID filter.\n",
                   {
                       {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "the address"},
                       {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "the property identifier"},
                   },
                   RPCResult{
                        RPCResult::Type::ARR, "", "", 
                        {
                            {
                                RPCResult::Type::OBJ, "", "",
                                {
                                    {RPCResult::Type::NUM, "propertyid", "The property identifier"},
                                    {RPCResult::Type::ARR, "tokens", "Array of token details",
                                        {
                                            {
                                                RPCResult::Type::OBJ, "", "",
                                                {
                                                    {RPCResult::Type::NUM, "tokenstart", "The first token in this range"},
                                                    {RPCResult::Type::NUM, "tokenend", "The last token in this range"},
                                                    {RPCResult::Type::NUM, "amount", "The amount of tokens in the range"},
                                                }
                                            },
                                        }
                                    },
                                }
                            },
                        },
                   },
                   RPCExamples{
                       HelpExampleCli("omni_getnonfungibletokens", "\"LTceXoduS2cetpWJSe47M25i5oKjEccN1h\" 1")
                       + HelpExampleRpc("omni_getnonfungibletokens", "\"LTceXoduS2cetpWJSe47M25i5oKjEccN1h\", 1")
                   }
                }.ToString());

    std::string address = ParseAddress(request.params[0]);
    uint32_t propertyId{0};
    if (!request.params[1].isNull()) {
        propertyId = ParsePropertyId(request.params[1]);
        RequireExistingProperty(propertyId);
        RequireNonFungibleProperty(propertyId);
    }

    UniValue propertyRanges(UniValue::VARR);

    const auto uniqueRanges = pDbNFT->GetAddressNonFungibleTokens(propertyId, address);

    for (const auto& range : uniqueRanges) {
        UniValue property(UniValue::VOBJ);
        property.pushKV("propertyid", static_cast<uint64_t>(range.first));
        UniValue tokenRanges(UniValue::VARR);

        for (const auto& subRange : range.second) {
            UniValue tokenRange(UniValue::VOBJ);
            tokenRange.pushKV("tokenstart", subRange.first);
            tokenRange.pushKV("tokenend", subRange.second);
            int64_t amount = (subRange.second - subRange.first) + 1;
            tokenRange.pushKV("amount", amount);
            tokenRanges.push_back(tokenRange);
        }

        property.pushKV("tokens", tokenRanges);
        propertyRanges.push_back(property);
    }

    return propertyRanges;
}

// provides all data for a specific token
UniValue omni_getnonfungibletokendata(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 3)
        throw runtime_error(
                RPCHelpMan{"omni_getnonfungibletokendata",
                   "\nReturns owner and all data set in a non-fungible token. If looking\n"
                   "up a single token on tokenidstart can be specified only.\n",
                   {
                       {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the property identifier"},
                       {"tokenidstart", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "the first non-fungible token in range"},
                       {"tokenidend", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "the last non-fungible token in range"},
                   },
                   RPCResult{
                        RPCResult::Type::ARR, "", "",
                        {
                            {RPCResult::Type::OBJ, "", "",
                                {
                                    {RPCResult::Type::NUM, "index", "The unique index of the token"},
                                    {RPCResult::Type::STR, "owner", "The Litecoin address of the owner"},
                                    {RPCResult::Type::STR, "grantdata", "Contents of the grant data field"},
                                    {RPCResult::Type::STR, "issuerdata", "Contents of the issuer data field"},
                                    {RPCResult::Type::STR, "holderdata", "Contents of the holder data field"},
                                }
                            },
                        }
                    },
                   RPCExamples{
                       HelpExampleCli("omni_getnonfungibletokendata", "1 10 20")
                       + HelpExampleRpc("omni_getnonfungibletokendata", "1, 10, 20")
                   }
                }.ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    RequireExistingProperty(propertyId);
    RequireNonFungibleProperty(propertyId);

    // Range empty, return null.
    auto range = pDbNFT->GetNonFungibleTokenRanges(propertyId);
    if (range.begin() == range.end()) {
        return NullUniValue;
    }

    // Get token range
    int64_t start{1};
    int64_t end = std::prev(range.end())->second.second;

    if (!request.params[1].isNull()) {
        start = request.params[1].get_int();

        // Make sure start sane.
        if (start < 1) {
            start = 1;
        } else if (start > end) {
            start = end;
        }

        // Allow token start to return single token if end not provided
        if (request.params[2].isNull()) {
            end = start;
        }
    }

    if (!request.params[2].isNull()) {
        end = request.params[2].get_int();

        // Make sure end sane.
        if (end < start) {
            end = start;
        } else if (end > std::prev(range.end())->second.second) {
            end = std::prev(range.end())->second.second;
        }
    }

    UniValue result(UniValue::VARR);
    for (; start <= end; ++start)
    {
        auto owner = pDbNFT->GetNonFungibleTokenOwner(propertyId, start);
        auto grantData = pDbNFT->GetNonFungibleTokenData(propertyId, start, NonFungibleStorage::GrantData);
        auto issuerData = pDbNFT->GetNonFungibleTokenData(propertyId, start, NonFungibleStorage::IssuerData);
        auto holderData = pDbNFT->GetNonFungibleTokenData(propertyId, start, NonFungibleStorage::HolderData);

        UniValue rpcObj(UniValue::VOBJ);
        rpcObj.pushKV("index", start);
        rpcObj.pushKV("owner", owner);
        rpcObj.pushKV("grantdata", grantData);
        rpcObj.pushKV("issuerdata", issuerData);
        rpcObj.pushKV("holderdata", holderData);
        result.push_back(rpcObj);
    }

    return result;
}

// displays all the ranges and their addresses for a property
UniValue omni_getnonfungibletokenranges(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
                RPCHelpMan{"omni_getnonfungibletokenranges",
                   "\nReturns the ranges and their addresses for a non-fungible token property.\n",
                   {
                       {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the property identifier"},
                   },
                   RPCResult{
                        RPCResult::Type::ARR, "", "",
                        {
                            {RPCResult::Type::OBJ, "", "",
                            {
                                {RPCResult::Type::STR, "address", "The address"},
                                {RPCResult::Type::NUM, "tokenstart", "The first token in this range"},
                                {RPCResult::Type::NUM, "tokenend", "The last token in this range"},
                                {RPCResult::Type::NUM, "amount", "The amount of tokens in the range"},
                            }},
                        }
                    },
                   RPCExamples{
                       HelpExampleCli("omni_getnonfungibletokenranges", "1")
                       + HelpExampleRpc("omni_getnonfungibletokenranges", "1")
                   }
                }.ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    RequireExistingProperty(propertyId);
    RequireNonFungibleProperty(propertyId);

    UniValue response(UniValue::VARR);

    std::vector<std::pair<std::string,std::pair<int64_t,int64_t> > > rangeMap = pDbNFT->GetNonFungibleTokenRanges(propertyId);

    for (std::vector<std::pair<std::string,std::pair<int64_t,int64_t> > >::iterator it = rangeMap.begin(); it!= rangeMap.end(); ++it) {
        std::pair<std::string,std::pair<int64_t,int64_t> > entry = *it;
        std::string address = entry.first;
        std::pair<int64_t,int64_t> range = entry.second;
        int64_t amount = (range.second - range.first) + 1;

        UniValue uniqueRangeObj(UniValue::VOBJ);
        uniqueRangeObj.pushKV("address", address);
        uniqueRangeObj.pushKV("tokenstart", range.first);
        uniqueRangeObj.pushKV("tokenend", range.second);
        uniqueRangeObj.pushKV("amount", amount);

        response.push_back(uniqueRangeObj);
    }

    return response;
}

// obtain the payload for a transaction
static UniValue omni_getpayload(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_getpayload",
                       "\nGet the payload for an Omni transaction.\n",
                       {
                           {"txid", RPCArg::Type::STR, RPCArg::Optional::NO, "the hash of the transaction to retrieve payload\n"},
                       },
                       RPCResult{
                           RPCResult::Type::OBJ, "", "",
                           {
                                {RPCResult::Type::STR, "payload", "The decoded Omni payload message"},
                                {RPCResult::Type::NUM, "payloadsize", "The size of the payload"},
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_getpayload", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"") + HelpExampleRpc("omni_getpayload", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")}}
                .ToString());

    uint256 txid = ParseHashV(request.params[0], "txid");

    bool f_txindex_ready = false;
    if (g_txindex) {
        f_txindex_ready = g_txindex->BlockUntilSyncedToCurrentChain();
    }

    CTransactionRef tx;
    uint256 blockHash;
    if (!GetTransaction(txid, tx, Params().GetConsensus(), blockHash)) {
        if (!f_txindex_ready) {
            PopulateFailure(MP_TXINDEX_STILL_SYNCING);
        } else {
            PopulateFailure(MP_TX_NOT_FOUND);
        }
    }

    int blockTime = 0;
    int blockHeight = GetHeight();
    if (!blockHash.IsNull()) {
        CBlockIndex* pBlockIndex = GetBlockIndex(blockHash);
        if (nullptr != pBlockIndex) {
            blockTime = pBlockIndex->nTime;
            blockHeight = pBlockIndex->nHeight;
        }
    }

    CMPTransaction mp_obj;
    int parseRC = ParseTransaction(*tx, blockHeight, 0, mp_obj, blockTime);
    if (parseRC < 0) PopulateFailure(MP_TX_IS_NOT_OMNI_PROTOCOL);

    UniValue payloadObj(UniValue::VOBJ);
    payloadObj.pushKV("payload", mp_obj.getPayload());
    payloadObj.pushKV("payloadsize", mp_obj.getPayloadSize());
    return payloadObj;
}

// determine whether to automatically commit transactions
#ifdef ENABLE_WALLET
static UniValue omni_setautocommit(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_setautocommit",
                       "\nSets the global flag that determines whether transactions are automatically committed and broadcast.\n",
                       {
                           {"flag", RPCArg::Type::BOOL, RPCArg::Optional::NO, "the flag\n"},
                       },
                       RPCResult{
                           RPCResult::Type::BOOL, "", "The updated flag status (true or false)"
                        },
                       RPCExamples{
                           HelpExampleCli("omni_setautocommit", "false") + HelpExampleRpc("omni_setautocommit", "false")}}
                .ToString());

    LOCK(cs_tally);

    autoCommit = request.params[0].get_bool();
    return autoCommit;
}
#endif

// display the tally map & the offer/accept list(s)
static UniValue mscrpc(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();
#endif

    int extra = 0;
    int extra2 = 0, extra3 = 0;

    if (request.fHelp || request.params.size() > 3)
        throw runtime_error(
            RPCHelpMan{"mscrpc",
                       "\nReturns the number of blocks in the longest block chain.\n",
                       {},
                       RPCResult{
                           RPCResult::Type::NUM, "", "The current block count"
                        },
                       RPCExamples{
                           HelpExampleCli("mscrpc", "") + HelpExampleRpc("mscrpc", "")}}
                .ToString());

    if (0 < request.params.size()) extra = atoi(request.params[0].get_str());
    if (1 < request.params.size()) extra2 = atoi(request.params[1].get_str());
    if (2 < request.params.size()) extra3 = atoi(request.params[2].get_str());

    PrintToConsole("%s(extra=%d,extra2=%d,extra3=%d)\n", __FUNCTION__, extra, extra2, extra3);

    bool bDivisible = isPropertyDivisible(extra2);

    // various extra tests
    switch (extra) {
        case 0:
        {
            LOCK(cs_tally);
            int64_t total = 0;
            // display all balances
            for (std::unordered_map<std::string, CMPTally>::iterator my_it = mp_tally_map.begin(); my_it != mp_tally_map.end(); ++my_it) {
                PrintToConsole("%34s => ", my_it->first);
                total += (my_it->second).print(extra2, bDivisible);
            }
            PrintToConsole("total for property %d  = %X is %s\n", extra2, extra2, FormatDivisibleMP(total));
            break;
        }
        case 1:
        {
            LOCK(cs_tally);
            // display the whole CMPTxList (leveldb)
            pDbTransactionList->printAll();
            pDbTransactionList->printStats();
            break;
        }
        case 2:
        {
            LOCK(cs_tally);
            // display smart properties
            pDbSpInfo->printAll();
            break;
        }
        case 3:
        {
            LOCK(cs_tally);
            uint32_t id = 0;
            // for each address display all currencies it holds
            for (std::unordered_map<std::string, CMPTally>::iterator my_it = mp_tally_map.begin(); my_it != mp_tally_map.end(); ++my_it) {
                PrintToConsole("%34s => ", my_it->first);
                (my_it->second).print(extra2);
                (my_it->second).init();
                while (0 != (id = (my_it->second).next())) {
                    PrintToConsole("Id: %u=0x%X ", id, id);
                }
                PrintToConsole("\n");
            }
            break;
        }
        case 4:
        {
            LOCK(cs_tally);
            for (CrowdMap::const_iterator it = my_crowds.begin(); it != my_crowds.end(); ++it) {
                (it->second).print(it->first);
            }
            break;
        }
        case 5:
        {
            LOCK(cs_tally);
            PrintToConsole("isMPinBlockRange(%d,%d)=%s\n", extra2, extra3, pDbTransactionList->isMPinBlockRange(extra2, extra3, false) ? "YES" : "NO");
            break;
        }
        case 6:
        {
            LOCK(cs_tally);
            // display the STO receive list
            pDbStoList->printAll();
            pDbStoList->printStats();
            break;
        }
        case 7:
        {
            PrintToConsole("Locking cs_tally for %d milliseconds..\n", extra2);
            LOCK(cs_tally);
            MilliSleep(extra2);
            PrintToConsole("Unlocking cs_tally now\n");
            break;
        }
        case 8:
        {
            PrintToConsole("Locking cs_main for %d milliseconds..\n", extra2);
            LOCK(cs_main);
            MilliSleep(extra2);
            PrintToConsole("Unlocking cs_main now\n");
            break;
        }
#ifdef ENABLE_WALLET
        case 9:
        {
            PrintToConsole("Locking pwallet->cs_wallet for %d milliseconds..\n", extra2);
            LOCK(pwallet->cs_wallet);
            MilliSleep(extra2);
            PrintToConsole("Unlocking pwallet->cs_wallet now\n");
            break;
        }
#endif
        case 13:
        {
            // dump the non-fungible tokens database
            pDbNFT->printAll();
            pDbNFT->printStats();
            break;
        }
        default:
            break;
    }

    return GetHeight();
}

// display an MP balance via RPC
static UniValue omni_getbalance(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            RPCHelpMan{"omni_getbalance",
                       "\nReturns the token balance for a given address and property.\n",
                       {
                           {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "the address\n"},
                           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the property identifier\n"},
                       },
                       RPCResult{
                           RPCResult::Type::OBJ, "", "",
                           {
                                {RPCResult::Type::STR_AMOUNT, "balance", "The available balance of the address"},
                                {RPCResult::Type::STR_AMOUNT, "reserved", "The amount reserved by sell offers and accepts"},
                                {RPCResult::Type::STR_AMOUNT, "frozen", "The amount frozen by the issuer (applies to managed properties only)"},
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_getbalance", "\"LTceXoduS2cetpWJSe47M25i5oKjEccN1h\" 1") + HelpExampleRpc("omni_getbalance", "\"LTceXoduS2cetpWJSe47M25i5oKjEccN1h\", 1")}}
                .ToString());

    std::string address = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);

    RequireExistingProperty(propertyId);

    UniValue balanceObj(UniValue::VOBJ);
    BalanceToJSON(address, propertyId, balanceObj, isPropertyDivisible(propertyId));

    return balanceObj;
}

static UniValue omni_getallbalancesforid(const JSONRPCRequest& request)
{
    PrintToConsole("omni_getallbalancesforid called\n");
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_getallbalancesforid",
                       "\nReturns a list of token balances for a given currency or property identifier.\n",
                       {
                           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the property identifier\n"},
                       },
                       RPCResult{
                           RPCResult::Type::ARR, "", "", 
                           {
                                {RPCResult::Type::OBJ, "", "",
                                    {
                                        {RPCResult::Type::STR, "address", "The address"},
                                        {RPCResult::Type::STR_AMOUNT, "balance", "The available balance of the address"},
                                        {RPCResult::Type::STR_AMOUNT, "reserved", "The amount reserved by sell offers and accepts"},
                                        {RPCResult::Type::STR_AMOUNT, "frozen", "The amount frozen by the issuer (applies to managed properties only)"},
                                    }
                                },
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_getallbalancesforid", "1") + HelpExampleRpc("omni_getallbalancesforid", "1")}}
                .ToString());
    PrintToConsole("calling ParsePropertyId\n");
    uint32_t propertyId = ParsePropertyId(request.params[0]);
    PrintToConsole("calling RequireExistingProperty\n");
    RequireExistingProperty(propertyId);

    UniValue response(UniValue::VARR);
    PrintToConsole("calling isPropertyDivisible\n");
    bool isDivisible = isPropertyDivisible(propertyId); // we want to check this BEFORE the loop
    PrintToConsole("called isPropertyDivisible\n");
    LOCK(cs_tally);

    for (std::unordered_map<std::string, CMPTally>::iterator it = mp_tally_map.begin(); it != mp_tally_map.end(); ++it) {
        uint32_t id = 0;
        bool includeAddress = false;
        std::string address = it->first;
        (it->second).init();
        while (0 != (id = (it->second).next())) {
            if (id == propertyId) {
                includeAddress = true;
                break;
            }
        }
        if (!includeAddress) {
            continue; // ignore this address, has never transacted in this propertyId
        }
        UniValue balanceObj(UniValue::VOBJ);
        balanceObj.pushKV("address", address);
        bool nonEmptyBalance = BalanceToJSON(address, propertyId, balanceObj, isDivisible);

        if (nonEmptyBalance) {
            response.push_back(balanceObj);
        }
    }

    return response;
}

static UniValue omni_getallbalancesforaddress(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_getallbalancesforaddress",
                       "\nReturns a list of all token balances for a given address.\n",
                       {
                           {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "the address\n"},
                       },
                       RPCResult{
                           RPCResult::Type::ARR, "", "",
                           {
                                {
                                    RPCResult::Type::OBJ, "", "",{
                                        {RPCResult::Type::NUM, "propertyid", "The property identifier"},
                                        {RPCResult::Type::STR, "name", "The name of the property"},
                                        {RPCResult::Type::STR_AMOUNT, "balance", "The available balance of the address"},
                                        {RPCResult::Type::STR_AMOUNT, "reserved", "The amount reserved by sell offers and accepts"},
                                        {RPCResult::Type::STR_AMOUNT, "frozen", "The amount frozen by the issuer (applies to managed properties only)"},
                                    }
                                },
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_getallbalancesforaddress", "\"LTceXoduS2cetpWJSe47M25i5oKjEccN1h\"") + HelpExampleRpc("omni_getallbalancesforaddress", "\"LTceXoduS2cetpWJSe47M25i5oKjEccN1h\"")}}
                .ToString());

    std::string address = ParseAddress(request.params[0]);

    UniValue response(UniValue::VARR);

    LOCK(cs_tally);

    CMPTally* addressTally = getTally(address);

    if (nullptr == addressTally) { // addressTally object does not exist
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Address not found");
    }

    addressTally->init();

    uint32_t propertyId = 0;
    while (0 != (propertyId = addressTally->next())) {
        CMPSPInfo::Entry property;
        if (!pDbSpInfo->getSP(propertyId, property)) {
            continue;
        }

        UniValue balanceObj(UniValue::VOBJ);
        balanceObj.pushKV("propertyid", (uint64_t) propertyId);
        balanceObj.pushKV("name", property.name);

        bool nonEmptyBalance = BalanceToJSON(address, propertyId, balanceObj, property.isDivisible());

        if (nonEmptyBalance) {
            response.push_back(balanceObj);
        }
    }

    return response;
}

/** Returns all addresses that may be mine. */
static std::set<std::string> getWalletAddresses(const JSONRPCRequest& request, bool fIncludeWatchOnly)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();
#endif

    std::set<std::string> result;

#ifdef ENABLE_WALLET
    LOCK(pwallet->cs_wallet);

    for (const auto& item : pwallet->m_address_book) {
        const CTxDestination& address = item.first;
        isminetype iIsMine = pwallet->IsMine(address);

        if (iIsMine == ISMINE_SPENDABLE || (fIncludeWatchOnly && iIsMine != ISMINE_NO)) {
            result.insert(EncodeDestination(address));
        }
    }
#endif

    return result;
}

#ifdef ENABLE_WALLET
static UniValue omni_getwalletbalances(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            RPCHelpMan{"omni_getwalletbalances",
                       "\nReturns a list of the total token balances of the whole wallet.\n",
                       {
                           {"includewatchonly", RPCArg::Type::BOOL, /* default */ "false", "include balances of watchonly addresses\n"},
                       },
                       RPCResult{
                           RPCResult::Type::ARR, "", "",
                           {
                                {
                                    RPCResult::Type::OBJ, "", "", 
                                    {
                                        {RPCResult::Type::NUM, "propertyid", "The property identifier"},
                                        {RPCResult::Type::STR, "name", "The name of the token"},
                                        {RPCResult::Type::STR_AMOUNT, "balance", "The total available balance for the token"},
                                        {RPCResult::Type::STR_AMOUNT, "reserved", "The total amount reserved by sell offers and accepts"},
                                        {RPCResult::Type::STR_AMOUNT, "frozen", "The total amount frozen by the issuer (applies to managed properties only)"},
                                    }
                                },
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_getwalletbalances", "") + HelpExampleRpc("omni_getwalletbalances", "")}}
                .ToString());

    bool fIncludeWatchOnly = false;
    if (request.params.size() > 0) {
        fIncludeWatchOnly = request.params[0].get_bool();
    }

    UniValue response(UniValue::VARR);

    if (!pwallet) {
        return response;
    }

    std::set<std::string> addresses = getWalletAddresses(request, fIncludeWatchOnly);
    std::map<uint32_t, std::tuple<int64_t, int64_t, int64_t>> balances;

    LOCK(cs_tally);
    for(const std::string& address : addresses) {
        CMPTally* addressTally = getTally(address);
        if (nullptr == addressTally) {
            continue; // address doesn't have tokens
        }

        uint32_t propertyId = 0;
        addressTally->init();

        while (0 != (propertyId = addressTally->next())) {
            int64_t nAvailable = GetAvailableTokenBalance(address, propertyId);
            int64_t nReserved = GetReservedTokenBalance(address, propertyId);
            int64_t nFrozen = GetFrozenTokenBalance(address, propertyId);

            if (!nAvailable && !nReserved && !nFrozen) {
                continue;
            }

            std::tuple<int64_t, int64_t, int64_t> currentBalance{0, 0, 0};
            if (balances.find(propertyId) != balances.end()) {
                currentBalance = balances[propertyId];
            }

            currentBalance = std::make_tuple(
                    std::get<0>(currentBalance) + nAvailable,
                    std::get<1>(currentBalance) + nReserved,
                    std::get<2>(currentBalance) + nFrozen
            );

            balances[propertyId] = currentBalance;
        }
    }

    for (auto const& item : balances) {
        uint32_t propertyId = item.first;
        std::tuple<int64_t, int64_t, int64_t> balance = item.second;

        CMPSPInfo::Entry property;
        if (!pDbSpInfo->getSP(propertyId, property)) {
            continue; // token wasn't found in the DB
        }

        int64_t nAvailable = std::get<0>(balance);
        int64_t nReserved = std::get<1>(balance);
        int64_t nFrozen = std::get<2>(balance);

        UniValue objBalance(UniValue::VOBJ);
        objBalance.pushKV("propertyid", (uint64_t) propertyId);
        objBalance.pushKV("name", property.name);

        if (property.isDivisible()) {
            objBalance.pushKV("balance", FormatDivisibleMP(nAvailable));
            objBalance.pushKV("reserved", FormatDivisibleMP(nReserved));
            objBalance.pushKV("frozen", FormatDivisibleMP(nFrozen));
        } else {
            objBalance.pushKV("balance", FormatIndivisibleMP(nAvailable));
            objBalance.pushKV("reserved", FormatIndivisibleMP(nReserved));
            objBalance.pushKV("frozen", FormatIndivisibleMP(nFrozen));
        }

        response.push_back(objBalance);
    }

    return response;
}

static UniValue omni_getwalletaddressbalances(const JSONRPCRequest& request)
{

    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            RPCHelpMan{"omni_getwalletaddressbalances",
               "\nReturns a list of all token balances for every wallet address.\n",
               {
                   {"includewatchonly", RPCArg::Type::BOOL, /*default */ "false", "include balances of watchonly addresses\n"},
               },
               RPCResult{
                    RPCResult::Type::ARR, "", "",
                    {
                        {RPCResult::Type::OBJ, "", "",
                        {
                            {RPCResult::Type::STR, "address", "The address linked to the following balances"},
                            {RPCResult::Type::ARR, "balances", "Array of token balances for this address",
                            {
                                {RPCResult::Type::OBJ, "", "",
                                {
                                    {RPCResult::Type::NUM, "propertyid", "The property identifier"},
                                    {RPCResult::Type::STR, "name", "The name of the token"},
                                    {RPCResult::Type::STR_AMOUNT, "balance", "The available balance for the token"},
                                    {RPCResult::Type::STR_AMOUNT, "reserved", "The amount reserved by sell offers and accepts"},
                                    {RPCResult::Type::STR_AMOUNT, "frozen", "The amount frozen by the issuer (applies to managed properties only)"},
                                }},
                            }},
                        }},
                    }},
               RPCExamples{
                   HelpExampleCli("omni_getwalletaddressbalances", "")
                   + HelpExampleRpc("omni_getwalletaddressbalances", "")
               }
            }.ToString());

    bool fIncludeWatchOnly = false;
    if (request.params.size() > 0) {
        fIncludeWatchOnly = request.params[0].get_bool();
    }

    UniValue response(UniValue::VARR);

    if (!pwallet) {
        return response;
    }

    std::set<std::string> addresses = getWalletAddresses(request, fIncludeWatchOnly);

    LOCK(cs_tally);
    for(const std::string& address : addresses) {
        CMPTally* addressTally = getTally(address);
        if (nullptr == addressTally) {
            continue; // address doesn't have tokens
        }

        UniValue arrBalances(UniValue::VARR);
        uint32_t propertyId = 0;
        addressTally->init();

        while (0 != (propertyId = addressTally->next())) {
            CMPSPInfo::Entry property;
            if (!pDbSpInfo->getSP(propertyId, property)) {
                continue; // token wasn't found in the DB
            }

            UniValue objBalance(UniValue::VOBJ);
            objBalance.pushKV("propertyid", (uint64_t) propertyId);
            objBalance.pushKV("name", property.name);

            bool nonEmptyBalance = BalanceToJSON(address, propertyId, objBalance, property.isDivisible());

            if (nonEmptyBalance) {
                arrBalances.push_back(objBalance);
            }
        }

        if (arrBalances.size() > 0) {
            UniValue objEntry(UniValue::VOBJ);
            objEntry.pushKV("address", address);
            objEntry.pushKV("balances", arrBalances);
            response.push_back(objEntry);
        }
    }

    return response;
}
#endif

static UniValue omni_getproperty(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_getproperty",
                       "\nReturns details for about the tokens or smart property to lookup.\n",
                       {
                           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens or property"},
                       },
                       RPCResult{
                           RPCResult::Type::OBJ, "", "",
                           {
                                {RPCResult::Type::NUM, "propertyid", "The property identifier"},
                                {RPCResult::Type::STR, "name", "The name of the token"},
                                {RPCResult::Type::STR, "category", "The category used for the tokens"},
                                {RPCResult::Type::STR, "subcategory", "The subcategory used for the tokens"},
                                {RPCResult::Type::STR, "data", "Additional information or a description"},
                                {RPCResult::Type::STR, "url", "A URI, for example pointing to a website"},
                                {RPCResult::Type::BOOL, "divisible", "Whether the tokens are divisible"},
                                {RPCResult::Type::STR, "issuer", "The Litecoin address of the issuer on record"},
                                {RPCResult::Type::STR_HEX, "creationtxid", "The hex-encoded creation transaction hash"},
                                {RPCResult::Type::BOOL, "fixedissuance", "Whether the token supply is fixed"},
                                {RPCResult::Type::BOOL, "managedissuance", "Whether the token supply is managed"},
                                {RPCResult::Type::BOOL, "non-fungibletoken", "Whether the property contains non-fungible tokens"},
                                {RPCResult::Type::BOOL, "freezingenabled", "Whether freezing is enabled for the property (managed properties only)"},
                                {RPCResult::Type::STR_AMOUNT, "totaltokens", "The total number of tokens in existence"},
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_getproperty", "3") + HelpExampleRpc("omni_getproperty", "3")}}
                .ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    RequireExistingProperty(propertyId);

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (!pDbSpInfo->getSP(propertyId, sp)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
        }
    }
    int64_t nTotalTokens = getTotalTokens(propertyId);
    std::string strTotalTokens = FormatMP(propertyId, nTotalTokens);

    UniValue response(UniValue::VOBJ);
    response.pushKV("propertyid", (uint64_t) propertyId);
    PropertyToJSON(sp, response); // name, category, subcategory, ...

    if (sp.manual) {
        int currentBlock = GetHeight();
        LOCK(cs_tally);
        response.pushKV("freezingenabled", isFreezingEnabled(propertyId, currentBlock));
    }
    response.pushKV("totaltokens", strTotalTokens);

    return response;
}

static UniValue omni_listproperties(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            RPCHelpMan{"omni_listproperties",
                       "\nLists all tokens or smart properties. To get the total number of tokens, please use omni_getproperty.\n",
                       {},
                       RPCResult{
                           RPCResult::Type::ARR, "", "",
                           {
                                {RPCResult::Type::OBJ, "", "", {
                                    {RPCResult::Type::NUM, "propertyid", "The identifier of the tokens"},
                                    {RPCResult::Type::STR, "name", "The name of the tokens"},
                                    {RPCResult::Type::STR, "category", "The category used for the tokens"},
                                    {RPCResult::Type::STR, "subcategory", "The subcategory used for the tokens"},
                                    {RPCResult::Type::STR, "data", "Additional information or a description"},
                                    {RPCResult::Type::STR, "url", "A URI, for example pointing to a website"},
                                    {RPCResult::Type::BOOL, "divisible", "Whether the tokens are divisible"},
                                    {RPCResult::Type::STR, "issuer", "The Litecoin address of the issuer on record"},
                                    {RPCResult::Type::STR_HEX, "creationtxid", "The hex-encoded creation transaction hash"},
                                    {RPCResult::Type::BOOL, "fixedissuance", "Whether the token supply is fixed"},
                                    {RPCResult::Type::BOOL, "managedissuance", "Whether the token supply is managed"},
                                }},
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_listproperties", "") + HelpExampleRpc("omni_listproperties", "")}}
                .ToString());

    UniValue response(UniValue::VARR);

    LOCK(cs_tally);

    uint32_t nextSPID = pDbSpInfo->peekNextSPID(1);
    for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++) {
        CMPSPInfo::Entry sp;
        if (pDbSpInfo->getSP(propertyId, sp)) {
            UniValue propertyObj(UniValue::VOBJ);
            propertyObj.pushKV("propertyid", (uint64_t) propertyId);
            PropertyToJSON(sp, propertyObj); // name, category, subcategory, ...

            response.push_back(propertyObj);
        }
    }

    uint32_t nextTestSPID = pDbSpInfo->peekNextSPID(2);
    for (uint32_t propertyId = TEST_ECO_PROPERTY_1; propertyId < nextTestSPID; propertyId++) {
        CMPSPInfo::Entry sp;
        if (pDbSpInfo->getSP(propertyId, sp)) {
            UniValue propertyObj(UniValue::VOBJ);
            propertyObj.pushKV("propertyid", (uint64_t) propertyId);
            PropertyToJSON(sp, propertyObj); // name, category, subcategory, ...

            response.push_back(propertyObj);
        }
    }

    return response;
}

static UniValue omni_getcrowdsale(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw runtime_error(
            RPCHelpMan{"omni_getcrowdsale",
                "\nReturns information about a crowdsale.\n",
                {
                    {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the crowdsale\n"},
                    {"verbose", RPCArg::Type::BOOL, /* default */ "false", "list crowdsale participants\n"},
                },
                RPCResult{
                    RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "propertyid", "The identifier of the crowdsale"},
                        {RPCResult::Type::STR, "name", "The name of the tokens issued via the crowdsale"},
                        {RPCResult::Type::BOOL, "active", "Whether the crowdsale is still active"},
                        {RPCResult::Type::STR, "issuer", "The Litecoin address of the issuer on record"},
                        {RPCResult::Type::NUM, "propertyiddesired", "The identifier of the tokens eligible to participate in the crowdsale"},
                        {RPCResult::Type::STR_AMOUNT, "tokensperunit", "The amount of tokens granted per unit invested in the crowdsale"},
                        {RPCResult::Type::NUM, "earlybonus", "An early bird bonus for participants in percent per week"},
                        {RPCResult::Type::NUM, "percenttoissuer", "A percentage of tokens that will be granted to the issuer"},
                        {RPCResult::Type::NUM, "starttime", "The start time of the crowdsale as Unix timestamp"},
                        {RPCResult::Type::NUM, "deadline", "The deadline of the crowdsale as Unix timestamp"},
                        {RPCResult::Type::STR_AMOUNT, "amountraised", "The amount of tokens invested by participants"},
                        {RPCResult::Type::STR_AMOUNT, "tokensissued", "The total number of tokens issued via the crowdsale"},
                        {RPCResult::Type::STR_AMOUNT, "issuerbonustokens", "The amount of tokens granted to the issuer as bonus"},
                        {RPCResult::Type::STR_AMOUNT, "addedissuertokens", "The amount of issuer bonus tokens not yet emitted"},
                        {RPCResult::Type::BOOL, "closedearly", "Whether the crowdsale ended early (if not active)"},
                        {RPCResult::Type::BOOL, "maxtokens", "Whether the crowdsale ended early due to reaching the limit of max. issuable tokens (if not active)"},
                        {RPCResult::Type::NUM, "endedtime", "The time when the crowdsale ended (if closed early)"},
                        {RPCResult::Type::STR_HEX, "closetx", "The hex-encoded hash of the transaction that closed the crowdsale (if closed manually)"},
                        {RPCResult::Type::ARR, "participanttransactions", /* optional */ true, "A list of crowdsale participations (if verbose=true)", {
                                                                                                                                                        {RPCResult::Type::OBJ, "", "", {
                                                                                                                                                                                            {RPCResult::Type::STR_HEX, "txid", "The hex-encoded hash of participation transaction"},
                                                                                                                                                                                            {RPCResult::Type::STR_AMOUNT, "amountsent", "The amount of tokens invested by the participant"},
                                                                                                                                                                                            {RPCResult::Type::STR_AMOUNT, "participanttokens", "The tokens granted to the participant"},
                                                                                                                                                                                            {RPCResult::Type::STR_AMOUNT, "issuertokens", "The tokens granted to the issuer as bonus"},
                                                                                                                                                                                        }},
                                                                                                                                                    }},
                    }
                },
                RPCExamples{HelpExampleCli("omni_getcrowdsale", "3 true") + HelpExampleRpc("omni_getcrowdsale", "3, true")}}
        .ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    bool showVerbose = (request.params.size() > 1) ? request.params[1].get_bool() : false;

    RequireExistingProperty(propertyId);
    RequireCrowdsale(propertyId);

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (!pDbSpInfo->getSP(propertyId, sp)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
        }
    }

    const uint256& creationHash = sp.txid;

    bool f_txindex_ready = false;
    if (g_txindex) {
        f_txindex_ready = g_txindex->BlockUntilSyncedToCurrentChain();
    }

    CTransactionRef tx;
    uint256 hashBlock;
    if (!GetTransaction(creationHash, tx, Params().GetConsensus(), hashBlock)) {
        if (!f_txindex_ready) {
            PopulateFailure(MP_TXINDEX_STILL_SYNCING);
        } else {
            PopulateFailure(MP_TX_NOT_FOUND);
        }
    }

    UniValue response(UniValue::VOBJ);
    bool active = isCrowdsaleActive(propertyId);
    std::map<uint256, std::vector<int64_t> > database;

    if (active) {
        bool crowdFound = false;

        LOCK(cs_tally);

        for (CrowdMap::const_iterator it = my_crowds.begin(); it != my_crowds.end(); ++it) {
            const CMPCrowd& crowd = it->second;
            if (propertyId == crowd.getPropertyId()) {
                crowdFound = true;
                database = crowd.getDatabase();
            }
        }
        if (!crowdFound) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Crowdsale is flagged active but cannot be retrieved");
        }
    } else {
        database = sp.historicalData;
    }

    int64_t tokensIssued = getTotalTokens(propertyId);
    const std::string& txidClosed = sp.txid_close.GetHex();

    int64_t startTime = -1;
    if (!hashBlock.IsNull() && GetBlockIndex(hashBlock)) {
        startTime = GetBlockIndex(hashBlock)->nTime;
    }

    // note the database is already deserialized here and there is minimal performance penalty to iterate recipients to calculate amountRaised
    int64_t amountRaised = 0;
    int64_t amountIssuerTokens = 0;
    uint16_t propertyIdType = isPropertyDivisible(propertyId) ? MSC_PROPERTY_TYPE_DIVISIBLE : MSC_PROPERTY_TYPE_INDIVISIBLE;
    uint16_t desiredIdType = isPropertyDivisible(sp.property_desired) ? MSC_PROPERTY_TYPE_DIVISIBLE : MSC_PROPERTY_TYPE_INDIVISIBLE;
    std::map<std::string, UniValue> sortMap;
    for (std::map<uint256, std::vector<int64_t> >::const_iterator it = database.begin(); it != database.end(); it++) {
        UniValue participanttx(UniValue::VOBJ);
        std::string txid = it->first.GetHex();
        amountRaised += it->second.at(0);
        amountIssuerTokens += it->second.at(3);
        participanttx.pushKV("txid", txid);
        participanttx.pushKV("amountsent", FormatByType(it->second.at(0), desiredIdType));
        participanttx.pushKV("participanttokens", FormatByType(it->second.at(2), propertyIdType));
        participanttx.pushKV("issuertokens", FormatByType(it->second.at(3), propertyIdType));
        std::string sortKey = strprintf("%d-%s", it->second.at(1), txid);
        sortMap.insert(std::make_pair(sortKey, participanttx));
    }

    response.pushKV("propertyid", (uint64_t) propertyId);
    response.pushKV("name", sp.name);
    response.pushKV("active", active);
    response.pushKV("issuer", sp.issuer);
    response.pushKV("propertyiddesired", (uint64_t) sp.property_desired);
    response.pushKV("tokensperunit", FormatMP(propertyId, sp.num_tokens));
    response.pushKV("earlybonus", sp.early_bird);
    response.pushKV("percenttoissuer", sp.percentage);
    response.pushKV("starttime", startTime);
    response.pushKV("deadline", sp.deadline);
    response.pushKV("amountraised", FormatMP(sp.property_desired, amountRaised));
    response.pushKV("tokensissued", FormatMP(propertyId, tokensIssued));
    response.pushKV("issuerbonustokens", FormatMP(propertyId, amountIssuerTokens));
    response.pushKV("addedissuertokens", FormatMP(propertyId, sp.missedTokens));

    // TODO: return fields every time?
    if (!active) response.pushKV("closedearly", sp.close_early);
    if (!active) response.pushKV("maxtokens", sp.max_tokens);
    if (sp.close_early) response.pushKV("endedtime", sp.timeclosed);
    if (sp.close_early && !sp.max_tokens) response.pushKV("closetx", txidClosed);

    if (showVerbose) {
        UniValue participanttxs(UniValue::VARR);
        for (std::map<std::string, UniValue>::iterator it = sortMap.begin(); it != sortMap.end(); ++it) {
            participanttxs.push_back(it->second);
        }
        response.pushKV("participanttransactions", participanttxs);
    }

    return response;
}

static UniValue omni_getactivecrowdsales(const JSONRPCRequest& request)
{
    if (request.fHelp)
        throw runtime_error(
            RPCHelpMan{"omni_getactivecrowdsales",
                       "\nLists currently active crowdsales.\n",
                       {},
                       RPCResult{
                           RPCResult::Type::ARR, "", "",
                           {
                                {RPCResult::Type::OBJ, "", "",
                                    {
                                        {RPCResult::Type::NUM, "propertyid", "The identifier of the crowdsale"},
                                        {RPCResult::Type::STR, "name", "The name of the tokens issued via the crowdsale"},
                                        {RPCResult::Type::STR, "issuer", "The Litecoin address of the issuer on record"},
                                        {RPCResult::Type::NUM, "propertyiddesired", "The identifier of the tokens eligible to participate in the crowdsale"},
                                        {RPCResult::Type::STR_AMOUNT, "tokensperunit", "The amount of tokens granted per unit invested in the crowdsale"},
                                        {RPCResult::Type::NUM, "earlybonus", "An early bird bonus for participants in percent per week"},
                                        {RPCResult::Type::NUM, "percenttoissuer", "A percentage of tokens that will be granted to the issuer"},
                                        {RPCResult::Type::NUM, "starttime", "The start time of the crowdsale as Unix timestamp"},
                                        {RPCResult::Type::NUM, "deadline", "The deadline of the crowdsale as Unix timestamp"},
                                    }
                                },
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_getactivecrowdsales", "") + HelpExampleRpc("omni_getactivecrowdsales", "")}}
                .ToString());

    UniValue response(UniValue::VARR);

    LOCK2(cs_main, cs_tally);

    for (CrowdMap::const_iterator it = my_crowds.begin(); it != my_crowds.end(); ++it) {
        const CMPCrowd& crowd = it->second;
        uint32_t propertyId = crowd.getPropertyId();

        CMPSPInfo::Entry sp;
        if (!pDbSpInfo->getSP(propertyId, sp)) {
            continue;
        }

        const uint256& creationHash = sp.txid;

        bool f_txindex_ready = false;
        if (g_txindex) {
            f_txindex_ready = g_txindex->BlockUntilSyncedToCurrentChain();
        }

        CTransactionRef tx;
        uint256 hashBlock;
        if (!GetTransaction(creationHash, tx, Params().GetConsensus(), hashBlock)) {
            if (!f_txindex_ready) {
                PopulateFailure(MP_TXINDEX_STILL_SYNCING);
            } else {
                PopulateFailure(MP_TX_NOT_FOUND);
            }
        }

        int64_t startTime = -1;
        if (!hashBlock.IsNull() && GetBlockIndex(hashBlock)) {
            startTime = GetBlockIndex(hashBlock)->nTime;
        }

        UniValue responseObj(UniValue::VOBJ);
        responseObj.pushKV("propertyid", (uint64_t) propertyId);
        responseObj.pushKV("name", sp.name);
        responseObj.pushKV("issuer", sp.issuer);
        responseObj.pushKV("propertyiddesired", (uint64_t) sp.property_desired);
        responseObj.pushKV("tokensperunit", FormatMP(propertyId, sp.num_tokens));
        responseObj.pushKV("earlybonus", sp.early_bird);
        responseObj.pushKV("percenttoissuer", sp.percentage);
        responseObj.pushKV("starttime", startTime);
        responseObj.pushKV("deadline", sp.deadline);
        response.push_back(responseObj);
    }

    return response;
}

static UniValue omni_getgrants(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_getgrants",
                       "\nReturns information about granted and revoked units of managed tokens.\n",
                       {
                           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the managed tokens to lookup\n"},
                       },
                       RPCResult{
                           RPCResult::Type::OBJ, "", "",
                           {
                                {RPCResult::Type::NUM, "propertyid", "The identifier of the managed tokens"},
                                {RPCResult::Type::STR, "name", "The name of the tokens"},
                                {RPCResult::Type::STR, "issuer", "The Litecoin address of the issuer on record"},
                                {RPCResult::Type::STR_HEX, "creationtxid", "The hex-encoded creation transaction hash"},
                                {RPCResult::Type::STR_AMOUNT, "totaltokens", "The total number of tokens in existence"},
                                {RPCResult::Type::ARR, "issuances", "A list of the granted and revoked tokens",
                                    {
                                        {RPCResult::Type::OBJ, "", "",
                                        {
                                            {RPCResult::Type::STR_HEX, "txid", "The hash of the transaction that granted or revoked tokens"},
                                            {RPCResult::Type::STR_AMOUNT, "grant", "The number of tokens granted or revoked by this transaction"},
                                        }},
                                    }
                                },
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_getgrants", "31") + HelpExampleRpc("omni_getgrants", "31")}}
                .ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (false == pDbSpInfo->getSP(propertyId, sp)) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
        }
    }
    UniValue response(UniValue::VOBJ);
    const uint256& creationHash = sp.txid;
    int64_t totalTokens = getTotalTokens(propertyId);

    // TODO: sort by height?

    UniValue issuancetxs(UniValue::VARR);
    std::map<uint256, std::vector<int64_t> >::const_iterator it;
    for (it = sp.historicalData.begin(); it != sp.historicalData.end(); it++) {
        const std::string& txid = it->first.GetHex();
        int64_t grantedTokens = it->second.at(0);
        int64_t revokedTokens = it->second.at(1);

        if (grantedTokens > 0) {
            UniValue granttx(UniValue::VOBJ);
            granttx.pushKV("txid", txid);
            granttx.pushKV("grant", FormatMP(propertyId, grantedTokens));
            issuancetxs.push_back(granttx);
        }

        if (revokedTokens > 0) {
            UniValue revoketx(UniValue::VOBJ);
            revoketx.pushKV("txid", txid);
            revoketx.pushKV("revoke", FormatMP(propertyId, revokedTokens));
            issuancetxs.push_back(revoketx);
        }
    }

    response.pushKV("propertyid", (uint64_t) propertyId);
    response.pushKV("name", sp.name);
    response.pushKV("issuer", sp.issuer);
    response.pushKV("creationtxid", creationHash.GetHex());
    response.pushKV("totaltokens", FormatMP(propertyId, totalTokens));
    response.pushKV("issuances", issuancetxs);

    return response;
}

static UniValue omni_getactivedexsells(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            RPCHelpMan{"omni_getactivedexsells",
                       "\nReturns currently active offers on the distributed exchange.\n",
                       {
                           {"address", RPCArg::Type::STR, /* default */ "include any", "address filter\n"},
                       },
                       RPCResult{
                           RPCResult::Type::ARR, "", "",
                           {
                                {RPCResult::Type::OBJ, "", "",
                                    {
                                        {RPCResult::Type::STR_HEX, "txid", "The hash of the transaction of this offer"},
                                        {RPCResult::Type::NUM, "propertyid", "The identifier of the tokens for sale"},
                                        {RPCResult::Type::STR, "seller", "The Litecoin address of the seller"},
                                        {RPCResult::Type::STR_AMOUNT, "amountavailable", "The number of tokens still listed for sale and currently available"},
                                        {RPCResult::Type::STR_AMOUNT, "litecoindesired", "The number of litecoins desired in exchange"},
                                        {RPCResult::Type::STR_AMOUNT, "unitprice", "The unit price (LTC/token)"},
                                        {RPCResult::Type::NUM, "timelimit", "The time limit in blocks a buyer has to pay following a successful accept"},
                                        {RPCResult::Type::STR_AMOUNT, "minimumfee", "The minimum mining fee a buyer has to pay to accept this offer"},
                                        {RPCResult::Type::STR_AMOUNT, "amountaccepted", "The number of tokens currently reserved for pending \"accept\" orders"},
                                        {RPCResult::Type::ARR, "accepts", "A list of pending \"accept\" orders", {
                                                                                                                        {RPCResult::Type::OBJ, "", "", {
                                                                                                                                                        {RPCResult::Type::STR, "buyer", "The Litecoin address of the buyer"},
                                                                                                                                                        {RPCResult::Type::NUM, "block", "The index of the block that contains the \"accept\" order"},
                                                                                                                                                        {RPCResult::Type::NUM, "blocksleft", "The number of blocks left to pay"},
                                                                                                                                                        {RPCResult::Type::STR_AMOUNT, "amount", "The amount of tokens accepted and reserved"},
                                                                                                                                                        {RPCResult::Type::STR_AMOUNT, "amounttopay", "The amount in litecoins needed to finalize the trade"},
                                                                                                                                                    }},
                                                                                                                    }},
                                    }
                                },
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_getactivedexsells", "") + HelpExampleRpc("omni_getactivedexsells", "")}}
                .ToString());

    std::string addressFilter;

    if (request.params.size() > 0) {
        addressFilter = ParseAddressOrEmpty(request.params[0]);
    }

    UniValue response(UniValue::VARR);

    int curBlock = GetHeight();

    LOCK(cs_tally);

    for (OfferMap::iterator it = my_offers.begin(); it != my_offers.end(); ++it) {
        const CMPOffer& selloffer = it->second;
        std::vector<std::string> vstr;
        boost::split(vstr, it->first, boost::is_any_of("-"), boost::token_compress_on);
        std::string seller = vstr[0];

        // filtering
        if (!addressFilter.empty() && seller != addressFilter) continue;

        std::string txid = selloffer.getHash().GetHex();
        uint32_t propertyId = selloffer.getProperty();
        int64_t minFee = selloffer.getMinFee();
        uint8_t timeLimit = selloffer.getBlockTimeLimit();
        int64_t sellOfferAmount = selloffer.getOfferAmountOriginal(); //badly named - "Original" implies off the wire, but is amended amount
        int64_t sellBitcoinDesired = selloffer.getBTCDesiredOriginal(); //badly named - "Original" implies off the wire, but is amended amount
        int64_t amountAvailable = GetTokenBalance(seller, propertyId, SELLOFFER_RESERVE);
        int64_t amountAccepted = GetTokenBalance(seller, propertyId, ACCEPT_RESERVE);

        // TODO: no math, and especially no rounding here (!)
        // TODO: no math, and especially no rounding here (!)
        // TODO: no math, and especially no rounding here (!)

        // calculate unit price and updated amount of bitcoin desired
        double unitPriceFloat = 0.0;
        if ((sellOfferAmount > 0) && (sellBitcoinDesired > 0)) {
            unitPriceFloat = (double) sellBitcoinDesired / (double) sellOfferAmount; // divide by zero protection
            if (!isPropertyDivisible(propertyId)) {
                unitPriceFloat /= 100000000.0;
            }
        }
        int64_t unitPrice = rounduint64(unitPriceFloat * COIN);
        int64_t bitcoinDesired = calculateDesiredBTC(sellOfferAmount, sellBitcoinDesired, amountAvailable);

        UniValue responseObj(UniValue::VOBJ);
        responseObj.pushKV("txid", txid);
        responseObj.pushKV("propertyid", (uint64_t) propertyId);
        responseObj.pushKV("seller", seller);
        responseObj.pushKV("amountavailable", FormatMP(propertyId, amountAvailable));
        responseObj.pushKV("litecoindesired", FormatDivisibleMP(bitcoinDesired));
        responseObj.pushKV("unitprice", FormatDivisibleMP(unitPrice));
        responseObj.pushKV("timelimit", timeLimit);
        responseObj.pushKV("minimumfee", FormatDivisibleMP(minFee));

        // display info about accepts related to sell
        responseObj.pushKV("amountaccepted", FormatMP(propertyId, amountAccepted));
        UniValue acceptsMatched(UniValue::VARR);
        for (AcceptMap::const_iterator ait = my_accepts.begin(); ait != my_accepts.end(); ++ait) {
            UniValue matchedAccept(UniValue::VOBJ);
            const CMPAccept& accept = ait->second;
            const std::string& acceptCombo = ait->first;

            // does this accept match the sell?
            if (accept.getHash() == selloffer.getHash()) {
                // split acceptCombo out to get the buyer address
                std::string buyer = acceptCombo.substr((acceptCombo.find("+") + 1), (acceptCombo.size()-(acceptCombo.find("+") + 1)));
                int blockOfAccept = accept.getAcceptBlock();
                int blocksLeftToPay = (blockOfAccept + selloffer.getBlockTimeLimit()) - curBlock;
                int64_t amountAccepted = accept.getAcceptAmountRemaining();
                // TODO: don't recalculate!
                int64_t amountToPayInBTC = calculateDesiredBTC(accept.getOfferAmountOriginal(), accept.getBTCDesiredOriginal(), amountAccepted);
                matchedAccept.pushKV("buyer", buyer);
                matchedAccept.pushKV("block", blockOfAccept);
                matchedAccept.pushKV("blocksleft", blocksLeftToPay);
                matchedAccept.pushKV("amount", FormatMP(propertyId, amountAccepted));
                matchedAccept.pushKV("amounttopay", FormatDivisibleMP(amountToPayInBTC));
                acceptsMatched.push_back(matchedAccept);
            }
        }
        responseObj.pushKV("accepts", acceptsMatched);

        // add sell object into response array
        response.push_back(responseObj);
    }

    return response;
}

static UniValue omni_listblocktransactions(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_listblocktransactions",
                       "\nLists all Omni transactions in a block.\n",
                       {
                           {"index", RPCArg::Type::NUM, RPCArg::Optional::NO, "the block height or block index\n"},
                       },
                       RPCResult{
                           RPCResult::Type::ARR, "", "A list of transaction hashes in the block", {
                                {RPCResult::Type::STR_HEX, "", "The hash of the transaction"},
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_listblocktransactions", "279007") + HelpExampleRpc("omni_listblocktransactions", "279007")}}
                .ToString());

    int blockHeight = request.params[0].get_int();

    RequireHeightInChain(blockHeight);

    // next let's obtain the block for this height
    CBlock block;
    {
        LOCK(cs_main);
        CBlockIndex* pBlockIndex = ::ChainActive()[blockHeight];

        if (!ReadBlockFromDisk(block, pBlockIndex, Params().GetConsensus())) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Failed to read block from disk");
        }
    }

    UniValue response(UniValue::VARR);

    // now we want to loop through each of the transactions in the block and run against CMPTxList::exists
    // those that return positive add to our response array

    LOCK(cs_tally);

    for(const auto tx : block.vtx) {
        if (pDbTransactionList->exists(tx->GetHash())) {
            // later we can add a verbose flag to decode here, but for now callers can send returned txids into omni_gettransaction
            // add the txid into the response as it's an MP transaction
            response.push_back(tx->GetHash().GetHex());
        }
    }

    return response;
}

static UniValue omni_listblockstransactions(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            RPCHelpMan{"omni_listblocktransactions",
                       "\nLists all Omni transactions in a given range of blocks.\n",
                       {
                           {"firstblock", RPCArg::Type::NUM, RPCArg::Optional::NO, "the index of the first block to consider\n"},
                           {"lastblock", RPCArg::Type::NUM, RPCArg::Optional::NO, "the index of the last block to consider\n"},
                       },
                       RPCResult{
                           RPCResult::Type::ARR, "", "A list of transaction hashes in the specified block range", {
                                {RPCResult::Type::STR_HEX, "", "The hash of the transaction"},
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_listblocktransactions", "279007 300000") + HelpExampleRpc("omni_listblocktransactions", "279007, 300000")}}
                .ToString());

    int blockFirst = request.params[0].get_int();
    int blockLast = request.params[1].get_int();

    std::set<uint256> txs;
    UniValue response(UniValue::VARR);

    LOCK(cs_tally);
    {
        pDbTransactionList->GetOmniTxsInBlockRange(blockFirst, blockLast, txs);
    }

    for(const uint256& tx : txs) {
        response.push_back(tx.GetHex());
    }

    return response;
}

static UniValue omni_gettransaction(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pWallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pWallet;
#endif

    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_gettransaction",
                       "\nGet detailed information about an Omni transaction.\n",
                       {
                           {"txid", RPCArg::Type::STR, RPCArg::Optional::NO, "the hash of the transaction to lookup\n"},
                       },
                       RPCResult{
                           RPCResult::Type::OBJ, "", "", {
                                {RPCResult::Type::STR_HEX, "txid", "The hex-encoded hash of the transaction"},
                                {RPCResult::Type::STR, "sendingaddress", "The Litecoin address of the sender"},
                                {RPCResult::Type::STR, "referenceaddress", "A Litecoin address used as reference (if any)"},
                                {RPCResult::Type::BOOL, "ismine", "Whether the transaction involves an address in the wallet"},
                                {RPCResult::Type::NUM, "confirmations", "The number of transaction confirmations"},
                                {RPCResult::Type::STR_AMOUNT, "fee", "The transaction fee in litecoins"},
                                {RPCResult::Type::NUM, "blocktime", "The timestamp of the block that contains the transaction"},
                                {RPCResult::Type::BOOL, "valid", "Whether the transaction is valid"},
                                {RPCResult::Type::STR, "invalidreason", "If a transaction is invalid, the reason"},
                                {RPCResult::Type::NUM, "version", "The transaction version"},
                                {RPCResult::Type::NUM, "type_int", "The transaction type as a number"},
                                {RPCResult::Type::STR, "type", "The transaction type as a string"},
                                {RPCResult::Type::OBJ, "type_specific", "Other transaction type-specific properties", {}},
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"") + HelpExampleRpc("omni_gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")}}
                .ToString());

    uint256 hash = ParseHashV(request.params[0], "txid");

    UniValue txobj(UniValue::VOBJ);
    int populateResult = populateRPCTransactionObject(hash, txobj, "", false, "", pWallet.get());
    if (populateResult != 0) PopulateFailure(populateResult);

    return txobj;
}

#ifdef ENABLE_WALLET
static UniValue omni_listtransactions(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pWallet = interfaces::MakeWallet(wallet);

    if (request.fHelp || request.params.size() > 5)
        throw runtime_error(
            RPCHelpMan{"omni_listtransactions",
                       "\nList wallet transactions, optionally filtered by an address and block boundaries.\n",
                       {
                           {"address", RPCArg::Type::STR, /* default */ "\"*\"", "address filter\n"},
                           {"count", RPCArg::Type::NUM, /* default */ "10", "show at most n transactions\n"},
                           {"skip", RPCArg::Type::NUM, /* default */ "0", "skip the first n transactions\n"},
                           {"startblock", RPCArg::Type::NUM, /* default */ "0", "first block to begin the search\n"},
                           {"endblock", RPCArg::Type::NUM, /* default */ "999999999", "last block to include in the search\n"},
                       },
                       RPCResult{
                           RPCResult::Type::ARR, "", "A list of transactions matching the filter criteria", {
                                {RPCResult::Type::OBJ, "", "", {
                                    {RPCResult::Type::STR_HEX, "txid", "The hex-encoded hash of the transaction"},
                                    {RPCResult::Type::STR, "sendingaddress", "The Litecoin address of the sender"},
                                    {RPCResult::Type::STR, "referenceaddress", "A Litecoin address used as reference (if any)"},
                                    {RPCResult::Type::BOOL, "ismine", "Whether the transaction involves an address in the wallet"},
                                    {RPCResult::Type::NUM, "confirmations", "The number of transaction confirmations"},
                                    {RPCResult::Type::STR_AMOUNT, "fee", "The transaction fee in litecoins"},
                                    {RPCResult::Type::NUM, "blocktime", "The timestamp of the block that contains the transaction"},
                                    {RPCResult::Type::BOOL, "valid", "Whether the transaction is valid"},
                                    {RPCResult::Type::NUM, "version", "The transaction version"},
                                    {RPCResult::Type::NUM, "type_int", "The transaction type as a number"},
                                    {RPCResult::Type::STR, "type", "The transaction type as a string"},
                                    {RPCResult::Type::OBJ, "type_specific", "Other transaction type-specific properties", {}},
                                }},
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_listtransactions", "") + HelpExampleRpc("omni_listtransactions", "")}}
                .ToString());

    // obtains parameters - default all wallet addresses & last 10 transactions
    std::string addressParam;
    if (request.params.size() > 0) {
        if (("*" != request.params[0].get_str()) && ("" != request.params[0].get_str())) addressParam = request.params[0].get_str();
    }
    int64_t nCount = 10;
    if (request.params.size() > 1) nCount = request.params[1].get_int64();
    if (nCount < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    int64_t nFrom = 0;
    if (request.params.size() > 2) nFrom = request.params[2].get_int64();
    if (nFrom < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");
    int64_t nStartBlock = 0;
    if (request.params.size() > 3) nStartBlock = request.params[3].get_int64();
    if (nStartBlock < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative start block");
    int64_t nEndBlock = 999999999;
    if (request.params.size() > 4) nEndBlock = request.params[4].get_int64();
    if (nEndBlock < 0) throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative end block");

    // obtain a sorted list of Omni layer wallet transactions (including STO receipts and pending)
    std::map<std::string,uint256> walletTransactions = FetchWalletOmniTransactions(*pWallet, nFrom+nCount, nStartBlock, nEndBlock);

    // reverse iterate over (now ordered) transactions and populate RPC objects for each one
    UniValue response(UniValue::VARR);
    for (std::map<std::string,uint256>::reverse_iterator it = walletTransactions.rbegin(); it != walletTransactions.rend(); it++) {
        if (nFrom <= 0 && nCount > 0) {
            uint256 txHash = it->second;
            UniValue txobj(UniValue::VOBJ);
            int populateResult = populateRPCTransactionObject(txHash, txobj, addressParam, false, "", pWallet.get());
            if (0 == populateResult) {
                response.push_back(txobj);
                nCount--;
            }
        }
        nFrom--;
    }

    return response;
}
#endif

static UniValue omni_listpendingtransactions(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pWallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pWallet;
#endif

    if (request.fHelp || request.params.size() > 1)
        throw runtime_error(
            RPCHelpMan{"omni_listpendingtransactions",
                       "\nReturns a list of unconfirmed Omni transactions, pending in the memory pool.\n"
                       "\nAn optional filter can be provided to only include transactions which involve the given address.\n"
                       "\nNote: the validity of pending transactions is uncertain, and the state of the memory pool may "
                       "change at any moment. It is recommended to check transactions after confirmation, and pending "
                       "transactions should be considered as invalid.\n",
                       {
                           {"address", RPCArg::Type::STR, /* default */ "\"\" for no filter", "address filter\n"},
                       },
                       RPCResult{
                           RPCResult::Type::ARR, "", "A list of unconfirmed Omni transactions in the memory pool",
                           {
                                {RPCResult::Type::OBJ, "", "", {
                                    {RPCResult::Type::STR_HEX, "txid", "The hex-encoded hash of the transaction"},
                                    {RPCResult::Type::STR, "sendingaddress", "The Litecoin address of the sender"},
                                    {RPCResult::Type::STR, "referenceaddress", "A Litecoin address used as reference (if any)"},
                                    {RPCResult::Type::BOOL, "ismine", "Whether the transaction involves an address in the wallet"},
                                    {RPCResult::Type::STR_AMOUNT, "fee", "The transaction fee in litecoins"},
                                    {RPCResult::Type::NUM, "version", "The transaction version"},
                                    {RPCResult::Type::NUM, "type_int", "The transaction type as a number"},
                                    {RPCResult::Type::STR, "type", "The transaction type as a string"},
                                    {RPCResult::Type::OBJ, "type_specific", "Other transaction type-specific properties", {}},
                                }},
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_listpendingtransactions", "") + HelpExampleRpc("omni_listpendingtransactions", "")}}
                .ToString());

    std::string filterAddress;
    if (request.params.size() > 0) {
        filterAddress = ParseAddressOrEmpty(request.params[0]);
    }

    std::vector<uint256> vTxid;
    CTxMemPool& mempool = ::ChainstateActive().m_mempool;
    mempool.queryHashes(vTxid);

    UniValue result(UniValue::VARR);
    for(const uint256& hash : vTxid) {
        if (!IsInMarkerCache(hash)) {
            continue;
        }

        UniValue txObj(UniValue::VOBJ);
        if (populateRPCTransactionObject(hash, txObj, filterAddress, false, "", pWallet.get()) == 0) {
            result.push_back(txObj);
        }
    }

    return result;
}

static UniValue omni_getinfo(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw runtime_error(
            RPCHelpMan{"omni_getinfo",
                       "Returns various state information of the client and protocol.\n",
                       {},
                       RPCResult{
                           RPCResult::Type::OBJ, "", "", {
                                {RPCResult::Type::NUM, "omnicoreversion_int", "Client version as an integer"},
                                {RPCResult::Type::STR, "omnicoreversion", "Client version as a string"},
                                {RPCResult::Type::STR, "litecoincoreversion", "Litecoin Core version"},
                                {RPCResult::Type::NUM, "block", "Index of the last processed block"},
                                {RPCResult::Type::NUM, "blocktime", "Timestamp of the last processed block"},
                                {RPCResult::Type::NUM, "blocktransactions", "Omni transactions found in the last processed block"},
                                {RPCResult::Type::NUM, "totaltransactions", "Total number of Omni transactions processed"},
                                {RPCResult::Type::ARR, "alerts", /* optional */ true, "Active protocol alerts (if any)", {
                                    {RPCResult::Type::OBJ, "", "", {
                                        {RPCResult::Type::NUM, "alerttypeint", "Alert type as an integer"},
                                        {RPCResult::Type::STR, "alerttype", "Alert type"},
                                        {RPCResult::Type::STR, "alertexpiry", "Expiration criteria"},
                                        {RPCResult::Type::STR, "alertmessage", "Information about the alert"},
                                    }},
                                }},
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_getinfo", "") + HelpExampleRpc("omni_getinfo", "")}}
                .ToString());

    UniValue infoResponse(UniValue::VOBJ);

    // provide the mastercore and bitcoin version
    infoResponse.pushKV("omnicoreversion_int", OMNICORE_VERSION);
    infoResponse.pushKV("omnicoreversion", OmniCoreVersion());
    infoResponse.pushKV("litecoincoreversion", BitcoinCoreVersion());

    // provide the current block details
    int block = GetHeight();
    int64_t blockTime = GetLatestBlockTime();

    LOCK(cs_tally);

    int blockMPTransactions = pDbTransactionList->getMPTransactionCountBlock(block);
    int totalMPTransactions = pDbTransactionList->getMPTransactionCountTotal();
    infoResponse.pushKV("block", block);
    infoResponse.pushKV("blocktime", blockTime);
    infoResponse.pushKV("blocktransactions", blockMPTransactions);

    // provide the number of transactions parsed
    infoResponse.pushKV("totaltransactions", totalMPTransactions);

    // handle alerts
    UniValue alerts(UniValue::VARR);
    std::vector<AlertData> omniAlerts = GetOmniCoreAlerts();
    for (std::vector<AlertData>::iterator it = omniAlerts.begin(); it != omniAlerts.end(); it++) {
        AlertData alert = *it;
        UniValue alertResponse(UniValue::VOBJ);
        std::string alertTypeStr;
        switch (alert.alert_type) {
            case 1: alertTypeStr = "alertexpiringbyblock";
            break;
            case 2: alertTypeStr = "alertexpiringbyblocktime";
            break;
            case 3: alertTypeStr = "alertexpiringbyclientversion";
            break;
            default: alertTypeStr = "error";
        }
        alertResponse.pushKV("alerttypeint", alert.alert_type);
        alertResponse.pushKV("alerttype", alertTypeStr);
        alertResponse.pushKV("alertexpiry", FormatIndivisibleMP(alert.alert_expiry));
        alertResponse.pushKV("alertmessage", alert.alert_message);
        alerts.push_back(alertResponse);
    }
    infoResponse.pushKV("alerts", alerts);

    return infoResponse;
}

static UniValue omni_getactivations(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw runtime_error(
            RPCHelpMan{"omni_getactivations",
                "Returns pending and completed feature activations.\n",
                {},
                RPCResult{
                    RPCResult::Type::OBJ, "", "", {{RPCResult::Type::ARR, "pendingactivations", "a list of pending feature activations", {
                        {RPCResult::Type::NUM, "featureid", "the id of the feature"},
                        {RPCResult::Type::STR, "featurename", "the name of the feature"},
                        {RPCResult::Type::NUM, "activationblock", "the block the feature will be activated"},
                        {RPCResult::Type::NUM, "minimumversion", "the minimum client version needed to support this feature"},
                    }},
                    {RPCResult::Type::ARR, "completedactivations", "a list of completed feature activations", {
                        {RPCResult::Type::NUM, "featureid", "the id of the feature"},
                        {RPCResult::Type::STR, "featurename", "the name of the feature"},
                        {RPCResult::Type::NUM, "activationblock", "the block the feature was activated"},
                        {RPCResult::Type::NUM, "minimumversion", "the minimum client version needed to support this feature"},
                    }}}},
                RPCExamples{HelpExampleCli("omni_getactivations", "") + HelpExampleRpc("omni_getactivations", "")}}
            .ToString());

    UniValue response(UniValue::VOBJ);

    UniValue arrayPendingActivations(UniValue::VARR);
    std::vector<FeatureActivation> vecPendingActivations = GetPendingActivations();
    for (std::vector<FeatureActivation>::iterator it = vecPendingActivations.begin(); it != vecPendingActivations.end(); ++it) {
        UniValue actObj(UniValue::VOBJ);
        FeatureActivation pendingAct = *it;
        actObj.pushKV("featureid", pendingAct.featureId);
        actObj.pushKV("featurename", pendingAct.featureName);
        actObj.pushKV("activationblock", pendingAct.activationBlock);
        actObj.pushKV("minimumversion", (uint64_t)pendingAct.minClientVersion);
        arrayPendingActivations.push_back(actObj);
    }

    UniValue arrayCompletedActivations(UniValue::VARR);
    std::vector<FeatureActivation> vecCompletedActivations = GetCompletedActivations();
    for (std::vector<FeatureActivation>::iterator it = vecCompletedActivations.begin(); it != vecCompletedActivations.end(); ++it) {
        UniValue actObj(UniValue::VOBJ);
        FeatureActivation completedAct = *it;
        actObj.pushKV("featureid", completedAct.featureId);
        actObj.pushKV("featurename", completedAct.featureName);
        actObj.pushKV("activationblock", completedAct.activationBlock);
        actObj.pushKV("minimumversion", (uint64_t)completedAct.minClientVersion);
        arrayCompletedActivations.push_back(actObj);
    }

    response.pushKV("pendingactivations", arrayPendingActivations);
    response.pushKV("completedactivations", arrayCompletedActivations);

    return response;
}

static UniValue omni_getsto(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pWallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pWallet;
#endif

    if (request.fHelp || request.params.size() < 1 || request.params.size() > 2)
        throw runtime_error(
            RPCHelpMan{"omni_getsto",
               "\nGet information and recipients of a send-to-owners transaction.\n",
               {
                   {"txid", RPCArg::Type::STR, RPCArg::Optional::NO, "the hash of the transaction to lookup\n"},
                   {"recipientfilter", RPCArg::Type::STR, /* default */ "\"*\" for all", "a filter for recipients\n"},
               },
               RPCResult{
                    RPCResult::Type::OBJ, "", "", 
                    {
                        {RPCResult::Type::STR_HEX, "txid", "the hex-encoded hash of the transaction"},
                        {RPCResult::Type::STR, "sendingaddress", "the Litecoin address of the sender"},
                        {RPCResult::Type::BOOL, "ismine", "whether the transaction involves an address in the wallet"},
                        {RPCResult::Type::NUM, "confirmations", "the number of transaction confirmations"},
                        {RPCResult::Type::STR_AMOUNT, "fee", "the transaction fee in litecoins"},
                        {RPCResult::Type::NUM, "blocktime", "the timestamp of the block that contains the transaction"},
                        {RPCResult::Type::BOOL, "valid", "whether the transaction is valid"},
                        {RPCResult::Type::NUM, "version", "the transaction version"},
                        {RPCResult::Type::NUM, "type_int", "the transaction type as number"},
                        {RPCResult::Type::STR, "type", "the transaction type as string"},
                        {RPCResult::Type::NUM, "propertyid", "the identifier of sent tokens"},
                        {RPCResult::Type::BOOL, "divisible", "whether the sent tokens are divisible"},
                        {RPCResult::Type::STR_AMOUNT, "amount", "the number of tokens sent to owners"},
                        {RPCResult::Type::STR_AMOUNT, "totalstofee", "the fee paid by the sender, nominated in OMN or TOMN"},
                        {RPCResult::Type::ARR, "recipients", "a list of recipients",
                            {
                                {RPCResult::Type::OBJ, "", "", 
                                    {
                                        {RPCResult::Type::STR, "address", "the Litecoin address of the recipient"},
                                        {RPCResult::Type::STR_AMOUNT, "amount", "the number of tokens sent to this recipient"},
                                    }
                                }
                            }
                        }
                    }
                },
               RPCExamples{
                   HelpExampleCli("omni_getsto", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" \"*\"")
                   + HelpExampleRpc("omni_getsto", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\", \"*\"")
               }
            }.ToString());

    uint256 hash = ParseHashV(request.params[0], "txid");
    std::string filterAddress;
    if (request.params.size() > 1) filterAddress = ParseAddressOrWildcard(request.params[1]);

    UniValue txobj(UniValue::VOBJ);
    int populateResult = populateRPCTransactionObject(hash, txobj, "", true, filterAddress, pWallet.get());
    if (populateResult != 0) PopulateFailure(populateResult);

    return txobj;
}

static UniValue omni_getcurrentconsensushash(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 0)
        throw runtime_error(
            RPCHelpMan{"omni_getcurrentconsensushash",
                       "\nReturns the consensus hash for all balances for the current block.\n",
                       {},
                       RPCResult{
                           RPCResult::Type::OBJ, "", "", {
                                {RPCResult::Type::NUM, "block", "the index of the block this consensus hash applies to"},
                                {RPCResult::Type::STR_HEX, "blockhash", "the hash of the corresponding block"},
                                {RPCResult::Type::STR_HEX, "consensushash", "the consensus hash for the block"},
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_getcurrentconsensushash", "") + HelpExampleRpc("omni_getcurrentconsensushash", "")}}
                .ToString());

    LOCK(cs_main); // TODO - will this ensure we don't take in a new block in the couple of ms it takes to calculate the consensus hash?

    int block = GetHeight();

    CBlockIndex* pblockindex = ::ChainActive()[block];
    uint256 blockHash = pblockindex->GetBlockHash();

    uint256 consensusHash = GetConsensusHash();

    UniValue response(UniValue::VOBJ);
    response.pushKV("block", block);
    response.pushKV("blockhash", blockHash.GetHex());
    response.pushKV("consensushash", consensusHash.GetHex());

    return response;
}

static UniValue omni_getbalanceshash(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_getbalanceshash",
                       "omni_getbalanceshash propertyid\n",
                       {
                           {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the property to hash balances for\n"},
                       },
                       RPCResult{
                           RPCResult::Type::OBJ, "", "", {
                                {RPCResult::Type::NUM, "block", "the index of the block this hash applies to"},
                                {RPCResult::Type::STR_HEX, "blockhash", "the hash of the corresponding block"},
                                {RPCResult::Type::NUM, "propertyid", "the property id of the hashed balances"},
                                {RPCResult::Type::STR_HEX, "balanceshash", "the hash for the balances"},
                            }
                        },
                       RPCExamples{HelpExampleCli("omni_getbalanceshash", "31") + HelpExampleRpc("omni_getbalanceshash", "31")}}
                .ToString());

    LOCK(cs_main);

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    RequireExistingProperty(propertyId);

    int block = GetHeight();
    CBlockIndex* pblockindex = ::ChainActive()[block];
    uint256 blockHash = pblockindex->GetBlockHash();

    uint256 balancesHash = GetBalancesHash(propertyId);

    UniValue response(UniValue::VOBJ);
    response.pushKV("block", block);
    response.pushKV("blockhash", blockHash.GetHex());
    response.pushKV("propertyid", (uint64_t)propertyId);
    response.pushKV("balanceshash", balancesHash.GetHex());

    return response;
}

static const CRPCCommand commands[] =
{ //  category                             name                            actor (function)               argNames
  //  ------------------------------------ ------------------------------- ------------------------------ ----------
    { "omni layer (data retrieval)", "omni_getinfo",                   &omni_getinfo,                    {} },
    { "omni layer (data retrieval)", "omni_getactivations",            &omni_getactivations,             {} },
    { "omni layer (data retrieval)", "omni_getallbalancesforid",       &omni_getallbalancesforid,        {"propertyid"} },
    { "omni layer (data retrieval)", "omni_getbalance",                &omni_getbalance,                 {"address", "propertyid"} },
    { "omni layer (data retrieval)", "omni_gettransaction",            &omni_gettransaction,             {"txid"} },
    { "omni layer (data retrieval)", "omni_getproperty",               &omni_getproperty,                {"propertyid"} },
    { "omni layer (data retrieval)", "omni_listproperties",            &omni_listproperties,             {} },
    { "omni layer (data retrieval)", "omni_getcrowdsale",              &omni_getcrowdsale,               {"propertyid", "verbose"} },
    { "omni layer (data retrieval)", "omni_getgrants",                 &omni_getgrants,                  {"propertyid"} },
    { "omni layer (data retrieval)", "omni_getactivedexsells",         &omni_getactivedexsells,          {"address"} },
    { "omni layer (data retrieval)", "omni_getactivecrowdsales",       &omni_getactivecrowdsales,        {} },
    { "omni layer (data retrieval)", "omni_getsto",                    &omni_getsto,                     {"txid", "recipientfilter"} },
    { "omni layer (data retrieval)", "omni_listblocktransactions",     &omni_listblocktransactions,      {"index"} },
    { "omni layer (data retrieval)", "omni_listblockstransactions",    &omni_listblockstransactions,     {"firstblock", "lastblock"} },
    { "omni layer (data retrieval)", "omni_listpendingtransactions",   &omni_listpendingtransactions,    {"address"} },
    { "omni layer (data retrieval)", "omni_getallbalancesforaddress",  &omni_getallbalancesforaddress,   {"address"} },
    { "omni layer (data retrieval)", "omni_getcurrentconsensushash",   &omni_getcurrentconsensushash,    {} },
    { "omni layer (data retrieval)", "omni_getpayload",                &omni_getpayload,                 {"txid"} },
    { "omni layer (data retrieval)", "omni_getbalanceshash",           &omni_getbalanceshash,            {"propertyid"} },
    { "omni layer (data retrieval)", "omni_getnonfungibletokens",      &omni_getnonfungibletokens,       {"address", "propertyid"} },
    { "omni layer (data retrieval)", "omni_getnonfungibletokendata",   &omni_getnonfungibletokendata,    {"propertyid", "tokenidstart", "tokenidend"} },
    { "omni layer (data retrieval)", "omni_getnonfungibletokenranges", &omni_getnonfungibletokenranges,  {"propertyid"} },
#ifdef ENABLE_WALLET
    { "omni layer (data retrieval)", "omni_listtransactions",          &omni_listtransactions,           {"address", "count", "skip", "startblock", "endblock"} },
    { "omni layer (configuration)",  "omni_setautocommit",             &omni_setautocommit,              {"flag"}  },
    { "omni layer (data retrieval)", "omni_getwalletbalances",         &omni_getwalletbalances,          {"includewatchonly"} },
    { "omni layer (data retrieval)", "omni_getwalletaddressbalances",  &omni_getwalletaddressbalances,   {"includewatchonly"} },
#endif
    { "hidden",                      "mscrpc",                         &mscrpc,                          {"extra", "extra2", "extra3"}  },
};

void RegisterOmniDataRetrievalRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
