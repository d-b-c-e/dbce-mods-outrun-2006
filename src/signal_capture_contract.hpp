#pragma once
#include <Windows.h>
#include <wincrypt.h>
#include <array>
#include <string>
#include <string_view>
#include <cstdint>
#pragma comment(lib, "Advapi32.lib")

namespace OutRunSignalCapture {
inline constexpr char Schema[]="outrun2006.software-force-capture@1";
inline constexpr std::size_t MaxRequestBytes=2048;
struct Request { std::string id, source, proxySha, lease; int seconds=0; long long expires=0; };
inline bool Hex(std::string_view s,std::size_t n) {
    if(s.size()!=n)return false;
    for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
inline bool Integer(std::string_view s,long long& out) {
    if(s.empty()||s.size()>12||(s.size()>1&&s[0]=='0'))return false;
    long long v=0;for(char c:s){if(c<'0'||c>'9')return false;v=v*10+c-'0';}out=v;return true;
}
inline std::string Parse(std::string_view text,long long now,std::string_view gameSha,
    std::string_view proxySha,std::string_view lease,Request& out) {
    if(text.empty()||text.size()>MaxRequestBytes)return "request byte bound";
    std::array<std::string,8> v;
    constexpr std::string_view keys[]={"action","id","seconds","expiresUnix","gameSha256","proxySha256","sourceCommit","leaseToken"};
    for(size_t start=0;start<text.size();) {
        size_t end=text.find('\n',start);if(end==text.npos)end=text.size();
        auto line=text.substr(start,end-start);start=end+1;
        if(!line.empty()&&line.back()=='\r')line.remove_suffix(1);
        if(line.empty())continue;
        size_t eq=line.find('=');if(eq==line.npos)return "malformed request";
        auto key=line.substr(0,eq),value=line.substr(eq+1);size_t k=0;
        for(;k<v.size()&&keys[k]!=key;k++);if(k==v.size())return "unknown key";
        if(value.empty()||!v[k].empty())return "empty or repeated key";
        for(char c:value)if((unsigned char)c<32||(unsigned char)c>126)return "non-ASCII request";
        v[k]=value;
    }
    long long seconds=0,expiry=0;
    if(v[0]!="record-legacy-muted")return "unsupported action";
    if(!Hex(v[1],32)||!Hex(v[4],64)||!Hex(v[5],64)||!Hex(v[6],40))return "identity format";
    if(!Integer(v[2],seconds)||seconds<10||seconds>120)return "seconds range";
    if(!Integer(v[3],expiry)||expiry<=now||expiry>now+600)return "expired or excessive expiry";
    if(v[4]!=gameSha||v[5]!=proxySha)return "runtime identity mismatch";
    if(v[7].empty()||v[7].size()>512||v[7]!=lease)return "rig lease mismatch";
    out={v[1],v[6],v[5],v[7],int(seconds),expiry};return {};
}
inline std::string FileSha256(const wchar_t* path) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return {};
    LARGE_INTEGER size{};bool ok=GetFileSizeEx(file,&size)&&size.QuadPart>0&&size.QuadPart<=128*1024*1024;
    HCRYPTPROV provider=0;HCRYPTHASH hash=0;
    ok=ok&&CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT);
    ok=ok&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash);
    std::array<BYTE,16384> buffer{};DWORD n=0;
    while(ok) {if(!ReadFile(file,buffer.data(),DWORD(buffer.size()),&n,nullptr)){ok=false;break;}if(!n)break;
        ok=CryptHashData(hash,buffer.data(),n,0)!=FALSE;}
    BYTE digest[32]{};DWORD length=32;ok=ok&&CryptGetHashParam(hash,HP_HASHVAL,digest,&length,0)&&length==32;
    if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);
    std::string result;if(ok){const char*hex="0123456789abcdef";for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}}
    return result;
}
inline std::string ReadSmall(const std::wstring& path) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return {};
    char bytes[MaxRequestBytes+1];DWORD n=0;const bool ok=ReadFile(file,bytes,sizeof(bytes),&n,nullptr)!=FALSE;
    CloseHandle(file);if(!ok||n>MaxRequestBytes)return {};return std::string(bytes,n);
}
inline std::string LeaseText(const std::wstring& path) {
    auto text=ReadSmall(path);while(!text.empty()&&(text.back()=='\r'||text.back()=='\n'))text.pop_back();return text;
}
inline bool Exists(const std::wstring& path) {auto a=GetFileAttributesW(path.c_str());return a!=INVALID_FILE_ATTRIBUTES&&!(a&FILE_ATTRIBUTE_DIRECTORY);}
// New file then non-replacing rename. An outcome is written only after the data
// file has been flushed and committed; failed files never look complete.
inline bool WriteNew(const std::wstring& path,const void* bytes,size_t size) {
    const auto temp=path+L".tmp";
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;bool ok=size<=MAXDWORD&&WriteFile(file,bytes,DWORD(size),&written,nullptr)&&written==size;
    ok=FlushFileBuffers(file)&&ok;CloseHandle(file);
    if(ok)ok=MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!ok)DeleteFileW(temp.c_str());return ok;
}
inline bool WriteNew(const std::wstring& path,const std::string& text) {return WriteNew(path,text.data(),text.size());}
}
