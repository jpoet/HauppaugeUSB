#ifndef __REGISTRYIF_H_
#define __REGISTRYIF_H_

#include "winwrap.h"
#include "baseif.h"
#include "log.h"

bool wrapRegistryLoad(const char *fn);
BOOLEAN hcwGetRegistryDWORD(IN const PCHAR pValueName, LPDWORD lpdwValue);

class RegistryAccess {
public:
    static NTSTATUS readDword(const PCHAR name, PDWORD p_value);
    static NTSTATUS writeDword(const PCHAR name, DWORD value);
    static NTSTATUS readDword(const PWCHAR keyname, const PCHAR name, PDWORD p_value);
};

#endif
