#pragma once
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <string>
#include <cwctype>

namespace osm {
static std::atomic<unsigned> movie_files{0};
static std::mutex movie_mutex;
static std::unordered_set<HANDLE> movie_handles;
static HANDLE (WINAPI *original_open_a)(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
static HANDLE (WINAPI *original_open_w)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
static BOOL (WINAPI *original_close)(HANDLE);
static char movie_log[MAX_PATH];
static bool movie_name(const std::wstring& path) {
    if(path.size()<4)return false;
    std::wstring suffix=path.substr(path.size()-4);for(auto& c:suffix)c=static_cast<wchar_t>(std::towlower(c));
    return suffix==L".mpg";
}
static void movie_open(HANDLE handle,const std::wstring& path) {
    if(handle==INVALID_HANDLE_VALUE || !movie_name(path))return;
    {std::lock_guard<std::mutex> lock(movie_mutex);
    movie_handles.insert(handle);movie_files=static_cast<unsigned>(movie_handles.size());}
    FILE* file=std::fopen(movie_log,"a");if(file){std::fprintf(file,"MOVIE_OPEN handle=%p active=%u\n",handle,movie_files.load());std::fclose(file);}
}
static HANDLE WINAPI open_a(LPCSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,DWORD creation,DWORD flags,HANDLE template_file) {
    HANDLE handle=original_open_a(path,access,share,security,creation,flags,template_file);
    if(path){std::string value(path);movie_open(handle,std::wstring(value.begin(),value.end()));}return handle;
}
static HANDLE WINAPI open_w(LPCWSTR path,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES security,DWORD creation,DWORD flags,HANDLE template_file) {
    HANDLE handle=original_open_w(path,access,share,security,creation,flags,template_file);
    if(path)movie_open(handle,path);return handle;
}
static BOOL WINAPI close_file(HANDLE handle) {
    if(movie_files.load()) {
        bool changed=false;
        {std::lock_guard<std::mutex> lock(movie_mutex);
        if(movie_handles.erase(handle)) {
            movie_files=static_cast<unsigned>(movie_handles.size());
            changed=true;
        }}
        if(changed){FILE* file=std::fopen(movie_log,"a");if(file){std::fprintf(file,"MOVIE_CLOSE active=%u\n",movie_files.load());std::fclose(file);}}
    }
    return original_close(handle);
}
static void start_movies(const char* directory) {
    std::snprintf(movie_log,MAX_PATH,"%sopensoccermania-movies.log",directory);
    MH_CreateHook(reinterpret_cast<void*>(CreateFileA),reinterpret_cast<void*>(open_a),reinterpret_cast<void**>(&original_open_a));
    MH_CreateHook(reinterpret_cast<void*>(CreateFileW),reinterpret_cast<void*>(open_w),reinterpret_cast<void**>(&original_open_w));
    MH_CreateHook(reinterpret_cast<void*>(CloseHandle),reinterpret_cast<void*>(close_file),reinterpret_cast<void**>(&original_close));
}
}
