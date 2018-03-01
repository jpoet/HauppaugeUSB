#include "registryif.h"

#include <string.h>
#include <stdlib.h>
#include <map>
#include <string>

static std::map<std::string, DWORD> g_reg;

bool wrapRegistryLoad(const char *fn) {
        char *buf;
        size_t len;
        wrapLogDebug("Reading parameters...");
        if(wrapFileLoad(fn, (void**)&buf, &len) != WRAPOS_OK) {
                return false;
        }
        wrapLogDebug("Parsing parameters...");
        g_reg.clear();
        char *p = buf;
        size_t l = len;
        char *n;
        char *v;
        do {
                n = p;
                p = (char*)memchr(n, '\n', l);
                size_t nl;
                if(!p) {
                        nl = l;
                        p = n + l;
                        l = 0;
                } else {
                        nl = p - n;
                        ++p;
                        l -= nl + 1;
                }
                v = (char*)memchr(n, '=', nl);
                if(!v) {
                        continue;
                }
                ++v;
                size_t vl = nl - (v - n);
                nl = v - n - 1;
                wrapLogDebug("%.*s=%.*s", nl, n, vl, v);
                g_reg.insert(std::pair<std::string, DWORD>(std::string(n, nl), strtoul(std::string(v, vl).c_str(), NULL, 10)));
        } while(l);
        wrapHeapFree(buf);
        wrapLogDebug("Done");
        return true;
}

BOOLEAN hcwGetRegistryDWORD(IN const PCHAR pValueName, LPDWORD lpdwValue) {
        std::map<std::string, DWORD>::iterator i = g_reg.find(pValueName);
        if(i == g_reg.end()) {
            wrapLogDebug("g_reg >>> %s not in reg", pValueName);
            return FALSE;
        }
        wrapLogDebug("g_reg >>> %s=%d", pValueName, i->second);
        if(lpdwValue) *lpdwValue = i->second;
        return TRUE;
}

NTSTATUS RegistryAccess::readDword(const PCHAR name, PDWORD p_value) {
        return hcwGetRegistryDWORD(name, p_value) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

NTSTATUS RegistryAccess::writeDword(const PCHAR name, DWORD value) {
        g_reg.insert(std::pair<std::string, DWORD>(std::string(name), value));
        return STATUS_SUCCESS;
}

NTSTATUS RegistryAccess::readDword(const PWCHAR keyname, const PCHAR name, PDWORD p_value) {
        return readDword(name, p_value);
}
