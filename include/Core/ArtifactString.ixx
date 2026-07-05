module;
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <utility>
#include <new>

export module Core.ArtifactString;

export namespace ArtifactCore {

class StringView {
public:
    StringView() = default;
    StringView(const char* s, size_t len) : data_(s), len_(len) {}
    StringView(const char* s) : data_(s), len_(s ? std::strlen(s) : 0) {}
    size_t length() const { return len_; }
    bool isEmpty() const { return len_ == 0; }
    const char* data() const { return data_ ? data_ : ""; }
    char at(size_t i) const { return (i < len_) ? data_[i] : 0; }
    const char* begin() const { return data_; }
    const char* end() const { return data_ + len_; }
    bool operator==(StringView other) const {
        if (len_ != other.len_) return false;
        if (len_ == 0) return true;
        return std::memcmp(data_, other.data_, len_) == 0;
    }
private:
    const char* data_ = nullptr;
    size_t len_ = 0;
};

class String {
public:
    String() = default;
    String(const char* s) : String(s, s ? std::strlen(s) : 0) {}
    String(const char* s, size_t len) { if(len){data_=static_cast<char*>(::operator new(len+1));std::memcpy(data_,s,len);data_[len]=0;len_=len;} }
    explicit String(char c) : String(&c, 1) {}
    String(const String& o) : String(o.data_, o.len_) {}
    String(String&& o) noexcept : data_(o.data_), len_(o.len_) { o.data_=nullptr; o.len_=0; }
    ~String() { ::operator delete(data_); }
    String& operator=(const String& o) { if(this==&o)return*this; String t(o); swap(t); return*this; }
    String& operator=(String&& o) noexcept { if(this==&o)return*this; ::operator delete(data_); data_=o.data_; len_=o.len_; o.data_=nullptr; o.len_=0; return*this; }

    String& operator+=(const String& o) { return append(o.data_, o.len_); }
    String& operator+=(const char* s) { return append(s, s?std::strlen(s):0); }
    String& operator+=(char c) { return append(&c, 1); }
    String& append(const char* s, size_t len) {
        if(!len)return*this;
        size_t nl=len_+len;
        char* nd=static_cast<char*>(::operator new(nl+1));
        if(data_){std::memcpy(nd,data_,len_);::operator delete(data_);}
        std::memcpy(nd+len_,s,len); nd[nl]=0;
        data_=nd; len_=nl; return*this;
    }

    StringView mid(size_t pos, size_t count=~0ULL) const {
        if(pos>=len_)return StringView();
        if(count>len_-pos)count=len_-pos;
        return StringView(data_+pos,count);
    }
    StringView left(size_t n) const { return mid(0,n); }
    StringView right(size_t n) const { return n>=len_?StringView(data_,len_):mid(len_-n,n); }

    int indexOf(char c, size_t from=0) const {
        for(size_t i=from;i<len_;++i)if(data_[i]==c)return(int)i;
        return -1;
    }
    int indexOf(const char* s, size_t from=0) const {
        size_t sl=std::strlen(s);
        if(!sl||sl>len_)return -1;
        for(size_t i=from;i<=len_-sl;++i)if(std::memcmp(data_+i,s,sl)==0)return(int)i;
        return -1;
    }
    int lastIndexOf(char c) const {
        for(size_t i=len_;i>0;--i)if(data_[i-1]==c)return(int)(i-1);
        return -1;
    }
    bool contains(char c) const { return indexOf(c)>=0; }
    bool contains(const char* s) const { return indexOf(s)>=0; }
    bool startsWith(const char* s) const { size_t sl=std::strlen(s); return sl<=len_&&std::memcmp(data_,s,sl)==0; }
    bool endsWith(const char* s) const { size_t sl=std::strlen(s); return sl<=len_&&std::memcmp(data_+len_-sl,s,sl)==0; }
    int count(char c) const { int n=0; for(size_t i=0;i<len_;++i)if(data_[i]==c)++n; return n; }

    size_t length() const { return len_; }
    bool isEmpty() const { return len_==0; }
    const char* data() const { return data_?data_:""; }
    char at(size_t i) const { return (i<len_)?data_[i]:0; }
    char operator[](size_t i) const { return at(i); }

    bool operator==(const String& o) const { return len_==o.len_&&(len_==0||std::memcmp(data_,o.data_,len_)==0); }
    bool operator==(const char* s) const { size_t sl=s?std::strlen(s):0; return sl==len_&&(sl==0||std::memcmp(data_,s,sl)==0); }
    bool operator!=(const String& o) const { return !(*this==o); }
    bool operator!=(const char* s) const { return !(*this==s); }
    bool operator<(const String& o) const {
        size_t ml=len_<o.len_?len_:o.len_;
        int c=ml>0?std::memcmp(data_,o.data_,ml):0;
        return c<0||(c==0&&len_<o.len_);
    }

    int toInt(bool* ok=nullptr) const {
        if(!data_){if(ok)*ok=false;return 0;}
        char* end=nullptr; long v=std::strtol(data_,&end,10);
        if(ok)*ok=(end!=data_); return(int)v;
    }
    double toDouble(bool* ok=nullptr) const {
        if(!data_){if(ok)*ok=false;return 0.0;}
        char* end=nullptr; double v=std::strtod(data_,&end);
        if(ok)*ok=(end!=data_); return v;
    }

    void swap(String& o) noexcept { char* td=o.data_;o.data_=data_;data_=td; size_t tl=o.len_;o.len_=len_;len_=tl; }
    static String number(int n) { char b[32]; std::snprintf(b,sizeof(b),"%d",n); return String(b); }
    static String number(double d, int dec=2) { char f[16],b[64]; std::snprintf(f,sizeof(f),"%%%d.%df",dec+1,dec); std::snprintf(b,sizeof(b),f,d); return String(b); }

private:
    char* data_ = nullptr;
    size_t len_ = 0;
};

inline String operator+(const String& a, const String& b) { String r(a); r+=b; return r; }
inline String operator+(const String& a, const char* b) { String r(a); r+=b; return r; }
inline String operator+(const char* a, const String& b) { String r(a); r+=b; return r; }

} // namespace ArtifactCore
