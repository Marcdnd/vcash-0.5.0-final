/*
 * Copyright (c) 2008-2015 John Connor (BM-NC49AxAjcqVcF5jNPu85Rb8MJ2d9JqZt)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <database/ecdhe.hpp>

EC_DHE * EC_DHE_new(int EC_NID)
{
    EC_DHE * ec_dhe = (EC_DHE *)calloc(1, sizeof(*ec_dhe));
    
    ec_dhe->EC_NID = EC_NID;
    
    return ec_dhe;
}

void EC_DHE_free(EC_DHE * ec_dhe)
{
    if (ec_dhe->ctx_params != 0)
    {
        EVP_PKEY_CTX_free(ec_dhe->ctx_params);
    }
    
    if (ec_dhe->ctx_keygen != 0)
    {
        EVP_PKEY_CTX_free(ec_dhe->ctx_keygen);
    }
    
    if (ec_dhe->ctx_derive != 0)
    {
        EVP_PKEY_CTX_free(ec_dhe->ctx_derive);
    }
    
    if (ec_dhe->privkey != 0)
    {
        EVP_PKEY_free(ec_dhe->privkey);
    }
    
    if (ec_dhe->peerkey != 0)
    {
        EVP_PKEY_free(ec_dhe->peerkey);
    }
    
    if (ec_dhe->params != 0)
    {
        EVP_PKEY_free(ec_dhe->params);
    }
    
    if (ec_dhe->publicKey != 0)
    {
        ec_dhe->publicKey[0] = '\0', free(ec_dhe->publicKey);
    }
    
    if (ec_dhe->sharedSecret != 0)
    {
        ec_dhe->sharedSecret[0] = '\0', free(ec_dhe->sharedSecret);
    }
    
    free(ec_dhe);
}

char * EC_DHE_getPublicKey(EC_DHE * ec_dhe, int * public_key_len)
{
	if (0 == (ec_dhe->ctx_params = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, 0)))
    {
        EC_DHE_handleErrors("Could not create EC_DHE contexts.");
        
        return 0;
    }
    
	if (1 != EVP_PKEY_paramgen_init(ec_dhe->ctx_params))
    {
        EC_DHE_handleErrors("Could not intialize parameter generation.");
        
        return 0;
    }
    
	if (
        1 != EVP_PKEY_CTX_set_ec_paramgen_curve_nid(
        ec_dhe->ctx_params, ec_dhe->EC_NID)
        )
    {
        EC_DHE_handleErrors("Likely unknown elliptical curve ID specified.");
        
        return 0;
    }
    
	if (!EVP_PKEY_paramgen(ec_dhe->ctx_params, &ec_dhe->params))
    {
        EC_DHE_handleErrors("Could not create parameter object parameters.");
        return 0;
    }
    
	if (0 == (ec_dhe->ctx_keygen = EVP_PKEY_CTX_new(ec_dhe->params, 0)))
    {
        EC_DHE_handleErrors(
            "Could not create the context for the key generation"
        );
        
        return 0;
    }
    
	
	if (1 != EVP_PKEY_keygen_init(ec_dhe->ctx_keygen))
    {
        EC_DHE_handleErrors("Could not init context for key generation.");
        
        return 0;
    }
    
	if (1 != EVP_PKEY_keygen(ec_dhe->ctx_keygen, &ec_dhe->privkey))
    {
        EC_DHE_handleErrors("Could not generate DHE keys in final step");
        
        return 0;
    }

    BIO * bp = BIO_new(BIO_s_mem());
    
    if (1 !=  PEM_write_bio_PUBKEY(bp, ec_dhe->privkey))
    {
        EC_DHE_handleErrors("Could not write public key to memory");
        
        return 0;
    }
    
    BUF_MEM * bptr;

    BIO_get_mem_ptr(bp, &bptr);

    ec_dhe->publicKey = (char *)calloc(1, bptr->length);
    memcpy(ec_dhe->publicKey, bptr->data, bptr->length);
    
    (*public_key_len) = bptr->length;
    
    BIO_free(bp);
    
    return ec_dhe->publicKey;
}

unsigned char * EC_DHE_deriveSecretKey(
    EC_DHE * ec_dhe, const char *peerPublicKey, int peerPublicKeyLength,
    int * shared_secret_len
    )
{
    BUF_MEM * bptr = BUF_MEM_new();
    
    BUF_MEM_grow(bptr, peerPublicKeyLength);

    BIO * bp = BIO_new(BIO_s_mem());
    
    memcpy(bptr->data, peerPublicKey, peerPublicKeyLength);
    
    BIO_set_mem_buf(bp, bptr, BIO_NOCLOSE);
    
    ec_dhe->peerkey = PEM_read_bio_PUBKEY(bp, 0, 0, 0);
    
    BIO_free(bp);
    BUF_MEM_free(bptr);
    
    size_t secret_len = 0;
    
	if (0 == (ec_dhe->ctx_derive = EVP_PKEY_CTX_new(ec_dhe->privkey, 0)))
    {
        EC_DHE_handleErrors(
            "Could not create the context for the shared secret derivation"
        );
        
        return 0;
    }
    
	if (1 != EVP_PKEY_derive_init(ec_dhe->ctx_derive))
    {
        EC_DHE_handleErrors("Could not init derivation context");
        
        return 0;
    }
    
	if (1 != EVP_PKEY_derive_set_peer(ec_dhe->ctx_derive, ec_dhe->peerkey))
    {
        EC_DHE_handleErrors(
            "Could not set the peer key into derivation context"
        );
        
        return 0;
    }
    
	if (1 != EVP_PKEY_derive(ec_dhe->ctx_derive, 0, &secret_len))
    {
        EC_DHE_handleErrors(
            "Could not determine buffer length for shared secret"
        );
        
        return 0;
    }
    
	if (
        0 == (ec_dhe->sharedSecret =
        (unsigned char *)OPENSSL_malloc(secret_len))
        )
    {
        EC_DHE_handleErrors("Could not create the sharedSecret buffer");
        
        return 0;
    }
    
	if (
        1 != (EVP_PKEY_derive(ec_dhe->ctx_derive, ec_dhe->sharedSecret,
        &secret_len))
        )
    {
        EC_DHE_handleErrors("Could not dervive the shared secret");
        
        return 0;
    }
    
    (*shared_secret_len) = (int)secret_len;

	return ec_dhe->sharedSecret;
}

static void EC_DHE_handleErrors(const char * msg)
{
    if (msg != 0)
    {
        printf("%s", msg);
    }
}

int EC_DHE_run_test()
{
    printf("ECDHE Key Generation\n");
    
    int NIDs[] =
    {
        NID_X9_62_c2pnb163v1, NID_X9_62_c2pnb163v2, NID_X9_62_c2pnb163v3,
        NID_X9_62_c2pnb176v1,NID_X9_62_c2tnb191v1,  NID_X9_62_c2tnb191v2,
        NID_X9_62_c2tnb191v3, NID_X9_62_c2pnb208w1, NID_X9_62_c2tnb239v1,
        NID_X9_62_c2tnb239v2, NID_X9_62_c2tnb239v3, NID_X9_62_c2pnb272w1,
        NID_X9_62_c2pnb304w1, NID_X9_62_c2tnb359v1, NID_X9_62_c2pnb368w1,
        NID_X9_62_c2tnb431r1, NID_X9_62_prime256v1, NID_secp112r1,
        NID_secp112r2,NID_secp128r1, NID_secp128r2, NID_secp160k1,
        NID_secp160r1, NID_secp160r2, NID_secp192k1, NID_secp224k1,
        NID_secp224r1, NID_secp256k1, NID_secp384r1, NID_secp521r1,
        NID_sect113r1, NID_sect113r2, NID_sect131r1, NID_sect131r2,
        NID_sect163k1, NID_sect163r1, NID_sect163r2 , NID_sect193r1,
        NID_sect193r2, NID_sect233k1, NID_sect233r1, NID_sect239k1,
        NID_sect283k1, NID_sect283r1, NID_sect409k1, NID_sect409r1,
        NID_sect571k1, NID_sect571r1, NID_wap_wsg_idm_ecid_wtls1,
        NID_wap_wsg_idm_ecid_wtls3, NID_wap_wsg_idm_ecid_wtls4,
        NID_wap_wsg_idm_ecid_wtls5, NID_wap_wsg_idm_ecid_wtls7,
        NID_wap_wsg_idm_ecid_wtls8, NID_wap_wsg_idm_ecid_wtls9,
        NID_wap_wsg_idm_ecid_wtls10, NID_wap_wsg_idm_ecid_wtls11,
        NID_wap_wsg_idm_ecid_wtls12
    };

    for (auto a = 0; a < sizeof(NIDs) / sizeof(int); a++)
    {
        printf("Trying Curve with ID: %d\n", NIDs[a]);
        
        int EC_Curve_ID = NIDs[a];
        
        EC_DHE * ec_dhe = EC_DHE_new(EC_Curve_ID);
        
        int public_key_len = 0;
        
        char * publicKey = EC_DHE_getPublicKey(ec_dhe, &public_key_len);
        
        printf("My Public Key:\n%s", publicKey);

        EC_DHE * ec_dhePeer = EC_DHE_new(EC_Curve_ID);
        
        int peer_key_len = 0;
        
        char * peerKey = EC_DHE_getPublicKey(ec_dhePeer, &peer_key_len);
        
        printf("Peer Public Key:\n%s", peerKey);
        
        int shared_secret_len = 0;
        
        unsigned char * shared_secret = EC_DHE_deriveSecretKey(
            ec_dhe, peerKey, peer_key_len, &shared_secret_len
        );
        
        printf("Shared Secret:\n");
        
        for(auto i = 0; i < shared_secret_len; i++)
        {
            printf("%X", shared_secret[i]);
        }
        
        printf("\n");

        EC_DHE_free(ec_dhe);
        EC_DHE_free(ec_dhePeer);
    }
    
    return 0;
}

