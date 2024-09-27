// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/pureheader.h>

#include <chainparams.h>
#include <crypto/scrypt.h>
#include <hash.h>
#include <util/strencodings.h>
#include <logging.h>

void CPureBlockHeader::SetBaseVersion(int32_t nBaseVersion, int32_t nChainId)
{
    LogPrintf("%s: DEBUG: SetBaseVersion start\n", __func__);
    LogPrintf("%s: DEBUG: nBaseVersion = %d\n", __func__, nBaseVersion);
    auto testValue = VERSION_AUXPOW;
    LogPrintf("%s: DEBUG: VERSION_AUXPOW = %d\n", __func__, testValue);
    assert(nBaseVersion >= 1 && nBaseVersion < VERSION_AUXPOW);
    LogPrintf("%s: DEBUG: SetBaseVersion nBaseVersion < VERSION_AUXPOW) \n", __func__);
    assert(!IsAuxpow());
    LogPrintf("%s: DEBUG: SetBaseVersion !IsAuxpow() \n", __func__);
    nVersion = nBaseVersion | (nChainId * VERSION_CHAIN_START);
    LogPrintf("%s: DEBUG: SetBaseVersion end\n", __func__);
}

uint256 CPureBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CPureBlockHeader::GetPoWHash() const
{
    uint256 thash;
    scrypt_1024_1_1_256(BEGIN(nVersion), BEGIN(thash));
    return thash;
}
