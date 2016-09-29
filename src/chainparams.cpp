// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "assert.h"

#include "chainparams.h"
#include "main.h"
#include "util.h"

#include <boost/assign/list_of.hpp>

using namespace boost::assign;

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparamsseeds.h"

//
// Main network
//

// Convert the pnSeeds6 array into usable address objects.
static void convertSeed6(std::vector<CAddress> &vSeedsOut, const SeedSpec6 *data, unsigned int count)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7*24*60*60;
    for (unsigned int i = 0; i < count; i++)
    {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

class CMainParams : public CChainParams {
public:
    CMainParams() {
        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        pchMessageStart[0] = 0xa5;
        pchMessageStart[1] = 0x4d;
        pchMessageStart[2] = 0x7e;
        pchMessageStart[3] = 0x9f;
        vAlertPubKey = ParseHex("040d379156cf512ff52c3d7e53a638a93cc5cba24312123dc8e9dc07636535b583cf188aa941b9d228e560e673e21c412ffa39b5479827892caddd22eea1183959");
        nDefaultPort = 12710;
        nRPCPort = 12720;
        bnProofOfWorkLimit = CBigNum(~uint256(0) >> 20);


        const char* pszTimestamp = "000000000000000002ceb63a6f2eed08765fef70b085d80a5d837db5a8e03db5";
        std::vector<CTxIn> vin;
        vin.resize(1);
        vin[0].scriptSig = CScript() << 0 << CBigNum(42) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        std::vector<CTxOut> vout;
        vout.resize(1);
        vout[0].SetEmpty();
        CTransaction txNew(1, 1474908761, vin, vout, 0);
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime    = 1474908761;
        genesis.nBits    = bnProofOfWorkLimit.GetCompact();
        genesis.nNonce   = 28763773;


        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x00000a58fa6b22b3ae1ac66b82c5e748f73d65e6d9fa97c0ccf34bfdf041e393"));
        assert(genesis.hashMerkleRoot == uint256("0x8583b303cd5b0cd8688ed2063141611bcefcdc29d4c155e9248dd1b1481b6586"));

        vSeeds.push_back(CDNSSeedData("185.144.156.201", "185.144.156.201"));
        
        base58Prefixes[PUBKEY_ADDRESS] = list_of(70);
        base58Prefixes[SCRIPT_ADDRESS] = list_of(85);
        base58Prefixes[SECRET_KEY] =     list_of(153);
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x04)(0x88)(0xB2)(0x1E);
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x04)(0x88)(0xAD)(0xE4);

        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

    }

    virtual const CBlock& GenesisBlock() const { return genesis; }
    virtual Network NetworkID() const { return CChainParams::MAIN; }

    virtual const vector<CAddress>& FixedSeeds() const {
        return vFixedSeeds;
    }
protected:
    CBlock genesis;
    vector<CAddress> vFixedSeeds;
};
static CMainParams mainParams;


//
// Testnet
//

class CTestNetParams : public CMainParams {
public:
    CTestNetParams() {
        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        pchMessageStart[0] = 0xa3;
        pchMessageStart[1] = 0xe2;
        pchMessageStart[2] = 0x7c;
        pchMessageStart[3] = 0x54;
        bnProofOfWorkLimit = CBigNum(~uint256(0) >> 1);
        vAlertPubKey = ParseHex("04dd07c7d96f9505adc3022eeaf3d9bb11951b49790e2bdd245099f705305d5b0e413f54c7c3546dc7983df54162e1083f1bb20031e5fa763d4806301afbc7dc04");
        nDefaultPort = 22710;
        nRPCPort = 22720;
        strDataDir = "testnet";

        // Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nBits  = bnProofOfWorkLimit.GetCompact();
        genesis.nNonce = 6546570;

        hashGenesisBlock = genesis.GetHash();
        assert(hashGenesisBlock == uint256("0x6c13316078f4dcb31c46937109abdb9082859c57b71e9455d71c770925933826"));

        vFixedSeeds.clear();
        vSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = list_of(70);
        base58Prefixes[SCRIPT_ADDRESS] = list_of(196);
        base58Prefixes[SECRET_KEY]     = list_of(239);
        base58Prefixes[EXT_PUBLIC_KEY] = list_of(0x04)(0x35)(0x87)(0xCF);
        base58Prefixes[EXT_SECRET_KEY] = list_of(0x04)(0x35)(0x83)(0x94);

        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

    }
    virtual Network NetworkID() const { return CChainParams::TESTNET; }
};
static CTestNetParams testNetParams;

static CChainParams *pCurrentParams = &mainParams;

const CChainParams &Params() {
    return *pCurrentParams;
}

void SelectParams(CChainParams::Network network) {
    switch (network) {
        case CChainParams::MAIN:
            pCurrentParams = &mainParams;
            break;
        case CChainParams::TESTNET:
            pCurrentParams = &testNetParams;
            break;
        default:
            assert(false && "Unimplemented network");
            return;
    }
}

bool SelectParamsFromCommandLine() {
    bool fTestNet = GetBoolArg("-testnet", false);

    if (fTestNet) {
        SelectParams(CChainParams::TESTNET);
    } else {
        SelectParams(CChainParams::MAIN);
    }
    return true;
}
