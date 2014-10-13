#ifndef ND_RAND_HPP
#define ND_RAND_HPP

#include <random>

#include <windows.h>

#ifdef __MINGW32__
    inline std::random_device::result_type nd_rand()
    {
        std::random_device::result_type rv;
        HCRYPTPROV hp {};

        CryptAcquireContext(&hp, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
        CryptGenRandom(hp, sizeof(rv), reinterpret_cast<unsigned char*>(&rv));
        CryptReleaseContext(hp, 0);

        return rv;
    }

#else
    inline std::random_device::result_type nd_rand()
    {
        return std::random_device{}();
    }
#endif // __MINGW32__

#endif // ND_RAND_HPP
